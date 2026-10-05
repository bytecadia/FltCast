#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <cerrno>
#include <string>
#include <chrono>
#include <mutex>
#include <condition_variable>

#include "socket.hpp"

constexpr std::size_t MAX_BUFFER = 64 * 1024;

bool set_timeout(int fd)
{
    timeval t{};
    t.tv_sec = 1;  // Seconds
    t.tv_usec = 0; // Microseconds

    return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t)) == 0;
}

int conn_sock(const char *host, const char *port)
{
    // ADSB feed, as a service string for getaddrinfo

    // Hints to resolver what kind of address is needed
    struct addrinfo hints{};         // value-init to all zero
    hints.ai_family = AF_UNSPEC;     // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP

    struct addrinfo *res = nullptr;            // resolver outputs linked list (free with freeaddrinfo)
    if (getaddrinfo(host, port, &hints, &res)) // 0 if success
    {
        spdlog::error("Failed to resolve '{}:{}'", host, port);
        return -1;
    }

    int fd = -1;
    for (addrinfo *p = res; p != nullptr; p = p->ai_next)
    {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol); // AF_INET or AF_INET6 (Internet), STREAM, TCP

        if (fd < 0)
        {
            spdlog::error("Failed to create socket (family: {}, type: {}, protocol: {}) with fd '{}'", p->ai_family, p->ai_socktype, p->ai_protocol, fd);
            continue; // Try next
        }

        if (!set_timeout(fd) || connect(fd, p->ai_addr, p->ai_addrlen) != 0) // fd, binary address, length
        {
            spdlog::error("Failed to connect to '{}:{}' with error '{}'", host, port, errno);
            close(fd);
            fd = -1;
            continue;
        }
        break;
    }

    freeaddrinfo(res); // Free linked list
    return fd;
}

// This closes the fd upon return
void recv_sock(int fd, TSQueue<std::string> &q, std::stop_token st)
{
    std::string buffer;
    char chunk[4096]; // common chunk size

    // Outer loop to read chunk message and add to buffer
    bool resync = false; // Handle partial messages when buffer exceeds max size
    while (!st.stop_requested())
    {
        ssize_t n = recv(fd, chunk, sizeof(chunk), 0); // receive bytes from socket and save into   chuck with default behavior (flags = 0)

        if (n == 0) // Means peer performed a shutdown
            break;

        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) // Timeout try agin (need so that stop request check can be reached)
                continue;

            spdlog::error("recv() failed with error {}", errno);
            break;
        }

        buffer.append(chunk, n); // Add chunk of message to buffer

        if (buffer.size() > MAX_BUFFER)
        {
            buffer.clear();
            resync = true;
        }

        size_t pos;
        // Inner loop to divide buffer into messages using delimeter
        while ((pos = buffer.find("\r\n")) != std::string::npos) // npos means not found
        {
            std::string msg = buffer.substr(0, pos); // Save message

            buffer.erase(0, pos + 2); // 2 for both \r and \n
            // Handle msg
            if (resync)
            {
                resync = false;
                continue;
            }

            q.push(msg);
        }
    }

    close(fd);
}

// Sleep returns true if wait was not interupted by stop
bool try_sleep(std::stop_token st, std::chrono::milliseconds time)
{
    // Mutex here is not meaningful, as in, it does not protected any shared data
    // The only reason it is here is use the wait_until function which follows
    // condition variable semantics. THe only meaningful parts is the stoptoken
    // and the time which wait until will use to stop waiting if the time is up
    // of the stop token received a stop request
    std::mutex mtx;
    std::condition_variable_any cv;         // any is need because it expose wait until api with stop_token
                                            // Regulare condition variable doesn't have it.
    std::unique_lock<std::mutex> lock(mtx); // not meaningfull

    cv.wait_for(lock, st, time, []
                { return false; }); // Predicate not meaningful

    return !st.stop_requested();
}

void socket_reader(std::stop_token st, TSQueue<std::string> &q, const Config &cfg)
{
    int fd;

    int attempt = 0;
    while (!st.stop_requested())
    {
        if (attempt > 0)
            if (!try_sleep(st, std::chrono::milliseconds(1000 << attempt)))
                return;

        fd = conn_sock(cfg.host.c_str(), cfg.port.c_str());

        if (fd < 0)
        {
            attempt = std::min(attempt + 1, 5); // Limit backoff to 32 seconds
            continue;
        }

        auto connected_at = std::chrono::steady_clock::now();

        recv_sock(fd, q, st);

        auto connection_time =
            std::chrono::steady_clock::now() - connected_at;

        // TODO: What do to do when hanging continuously - show something to screen?
        if (connection_time >= std::chrono::seconds(5))
            attempt = 0;
        else // Handle immediate disconnect and add backoff
            attempt = std::min(attempt + 1, 5);
    }
}