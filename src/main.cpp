#include <csignal>
#include <functional>
#include <pthread.h>
#include <stop_token>
#include <thread>
#include <vector>
#include <SQLiteCpp/SQLiteCpp.h>
#include <spdlog/spdlog.h>

#include "socket.hpp"
#include "queue.hpp"
#include "config.hpp"
#include "process.hpp"
#include "snapshot.hpp"
#include "render.hpp"
#include "data.hpp"

int main()
{
    sigset_t s;
    sigemptyset(&s);
    sigaddset(&s, SIGINT);
    sigaddset(&s, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &s, nullptr);

    std::optional<SQLite::Database> db;
    try
    {
        db.emplace("aircraft.db", SQLite::OPEN_READONLY);
    }
    catch (const SQLite::Exception &e)
    {
        spdlog::error("Database 'aircraft.db' failed to open with error {}", e.what()); // Hardcoded for now
    }

    if (!db)
        return 0;

    Config cfg = parse_cfg(CONFIG_PATH);

    TSQueue<std::string> msg_q;
    Snapshot<std::vector<DisplayData>> snapshot;

    std::vector<std::jthread>
        threads;

    std::stop_source stp_src;
    threads.emplace_back(socket_reader, stp_src.get_token(), std::ref(msg_q), cfg); // config read only
    threads.emplace_back(process, stp_src.get_token(), std::ref(msg_q), std::ref(snapshot), std::ref(*db), std::cref(cfg));
    threads.emplace_back(render, stp_src.get_token(), std::ref(snapshot), std::cref(cfg));

    int sig;
    sigwait(&s, &sig);

    stp_src.request_stop();
}