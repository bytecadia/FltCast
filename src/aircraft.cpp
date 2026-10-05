#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ranges.h>

#include "aircraft.hpp"
#include "geometry.hpp"
#include "strings.hpp"

Aircraft::Aircraft(std::string icao) : icao(icao) {};

bool Aircraft::parse_msg(const std::vector<std::string> &msg)
{
    if (msg.size() != 22) // TODO: More checks for robustness?
    {
        spdlog::error("Failed to parse message (wrong size) with message '{}'", fmt::join(msg, ", "));
        return false;
    }

    // TODO: Should these not change the value of field if parsing failed
    // Example of check `if (auto v = parse_num<int>(msg[11])) alt = v;`
    int type = parse_num<int>(msg[1]).value_or(-1);
    switch (type)
    { // MSG type
    case 1:
        callsign = msg[10];
        break;
    case 2:
        alt = parse_num<int>(msg[11]);
        gs = parse_num<int>(msg[12]);
        trk = parse_num<int>(msg[13]);
        lat = parse_num<double>(msg[14]);
        lon = parse_num<double>(msg[15]);
        gnd = parse_num<int>(msg[21]);
        break;
    case 3:
        alt = parse_num<int>(msg[11]);
        lat = parse_num<double>(msg[14]);
        lon = parse_num<double>(msg[15]);
        gnd = parse_num<int>(msg[21]);
        break;
    case 4:
        gs = parse_num<int>(msg[12]);
        trk = parse_num<int>(msg[13]);
        vs = parse_num<int>(msg[16]);
        break;
    case 5:
        alt = parse_num<int>(msg[11]);
        gnd = parse_num<int>(msg[21]);
        break;
    case 6:
        alt = parse_num<int>(msg[11]);
        gnd = parse_num<int>(msg[21]);
        break;
    case 7:
        alt = parse_num<int>(msg[11]);
        gnd = parse_num<int>(msg[21]);
        break;
    case 8:
        gnd = parse_num<int>(msg[21]);
        break;
    default:
        spdlog::error("Failed to parse message (invalid message type) with type '{}'", type);
        return false;
    }
    return true;
}

void Aircraft::tick(double t)
{
    if (alt)
    {
        alt_hist.push_back(*alt);
        if (alt_hist.size() > 64)
            alt_hist.pop_front();
    }

    // ADS-B has no roll so estimate it from turn rate -> tan(bank) = v * w / g
    if (trk && gs && last_trk && t > 0)
    {
        double turn = std::remainder(*trk - *last_trk, 360.0); // Shortest way round (-180 to 180)
        double v = *gs * 0.514444;                             // Knots to m/s
        double w = rads(turn) / t;                             // Rad/s
        double b = std::clamp(std::atan(v * w / 9.81) * 180 / std::numbers::pi, -45.0, 45.0);
        bank = bank * 0.7 + b * 0.3; // TODO: Tune smoothing on real traffic
    }
    last_trk = trk;
}
