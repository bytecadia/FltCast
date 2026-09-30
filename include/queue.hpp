#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <stop_token>
#include <utility>

template <typename T> // Class works with any type
class TSQueue
{
private:
    std::queue<T> _q;                // queue for any element
    mutable std::mutex _mtx;         // lock access to queue / Why mutable? -> so const methods can still lock
    std::condition_variable_any _cv; // Signal to other thread

public:
    TSQueue() = default;                          // Requaired because altering constructors makes cpp stop giving you the free default one and TSQueue<std::string> q; wouldn't compile
    TSQueue(const TSQueue &) = delete;            // Remove copy costructor
    TSQueue &operator=(const TSQueue &) = delete; // Remove copy assignment operator

    void push(const T &val)
    { // pass reference to not copy but make const so this function can't change shi
        {
            std::lock_guard<std::mutex> lock(_mtx); // lock mutex (lock guard releases at end of scope)
            _q.push(val);
        } // end mutex scope
        _cv.notify_one(); // Notiy one of waiting thread
    }

    // Allows pushing temporary variables
    void push(T &&val)
    {
        {
            std::lock_guard<std::mutex> lock(_mtx);
            _q.push(std::move(val));
        }
        _cv.notify_one();
    }

    std::optional<T> pop(std::stop_token st)
    {
        std::unique_lock<std::mutex> lock(_mtx); // unique lock for pop b/c wait needs to unlock and relock the mutex while thread sleeps

        _cv.wait(lock, st, [this]
                 { return !_q.empty(); });

        if (_q.empty())
            return std::nullopt;

        T t = std::move(_q.front()); // Retrieve first in line then move ownership
        _q.pop();
        return t;
    }

    std::optional<T> pop_until(std::stop_token st, std::chrono::steady_clock::time_point deadline)
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _cv.wait_until(lock, st, deadline, [this]
                       { return !_q.empty(); });

        if (_q.empty())
            return std::nullopt;

        T t = std::move(_q.front());
        _q.pop();
        return t;
    }

    std::optional<T> try_pop()
    {
        std::lock_guard<std::mutex> lock(_mtx);
        if (_q.empty())
            return std::nullopt;

        T t = std::move(_q.front());

        _q.pop();
        return t;
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lock(_mtx);
        return _q.empty();
    }
};