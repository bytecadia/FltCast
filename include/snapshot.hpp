#pragma once

#include <mutex>
#include <optional>

#include "aircraft.hpp"

template <typename T>
class Snapshot
{
private:
    T _v;
    mutable std::mutex _mtx;

public:
    void write(T a)
    {
        std::lock_guard<std::mutex> lock(_mtx);
        _v = std::move(a);
    }

    T read() const
    {
        std::lock_guard<std::mutex> lock(_mtx);
        return _v;
    }
};