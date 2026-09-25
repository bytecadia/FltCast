#pragma once

#include <ranges>
#include <SQLiteCpp/SQLiteCpp.h>
#include <spdlog/spdlog.h>

#include "strings.hpp"
#include "aircraft.hpp"
#include "geometry.hpp"
#include "config.hpp"

// TODO: Make this not header only

enum class AircraftType
{
    Prop,
    Jet,
    Heli,
    Unk
};

inline std::string to_str(AircraftType type)
{
    switch (type)
    {
    case AircraftType::Prop:
        return "prop";
        break;
    case AircraftType::Jet:
        return "jet";
        break;
    case AircraftType::Heli:
        return "heli";
        break;
    default:
        return "unk";
        break;
    }
}

struct AircraftInfo
{
    std::string mfc;
    std::string mdl;
    AircraftType type;
};

// Caller must check if icao is not empty
inline std::string parse_icao(const std::vector<std::string> &msg)
{
    // TODO: Do I need more checks here?
    if (msg.size() != 22)
        return "";

    return msg[4];
}

inline AircraftType get_type(int type, int engine)
{
    if (type == 6) // Rotocraft
        return AircraftType::Heli;

    if (type == 4 || // Fixed-wing, singe-engine
        type == 5)   // Fixed-wing, multi-engine
    {
        if (engine == 4 || // Turbojet
            engine == 5)   // Turbofan
            return AircraftType::Jet;

        if (engine == 1 ||  // Reciprocating
            engine == 2 ||  // Turboprop
            engine == 7 ||  // 2-cycle
            engine == 8 ||  // 4-cycle
            engine == 10 || // Electric
            engine == 11)   // Rotary
            return AircraftType::Prop;
    }

    return AircraftType::Unk;
}

// TODO: Should this be returning optional, does it make sense for partials here
inline AircraftInfo lookup_aircraft(SQLite::Database &db, std::string icao)
{
    try
    {
        // TODO: Should I make this lower or upper in db
        std::ranges::transform(icao, icao.begin(), [](unsigned char c)
                               { return std::tolower(c); });

        // TODO: What more to add?
        SQLite::Statement query(db,
                                "SELECT "
                                "manufacturer, "
                                "model, "
                                "type_aircraft, "
                                "type_engine "
                                "FROM aircrafts "
                                "WHERE icao = ?;");

        query.bind(1, icao);

        if (!query.executeStep())
            return AircraftInfo{};

        AircraftInfo info;
        info.mfc = query.getColumn(0).getString();
        info.mdl = query.getColumn(1).getString();

        int type_aircraft = query.getColumn(2).getInt();
        int type_engine = query.getColumn(3).getInt();

        info.type = get_type(type_aircraft, type_engine);

        return info;
    }
    catch (const std::exception &e)
    {
        spdlog::error("Lookup aircraft failed for ICAO: '{}' with error '{}'",
                      icao, e.what());
    }

    return AircraftInfo{}; // TODO: Hacking here see todo above
}

inline std::string lookup_airline(SQLite::Database &db, std::string callsign)
{
    std::string code = parse_chars(callsign);

    if (code.empty())
        return "";

    try
    {
        SQLite::Statement query(db,
                                "SELECT "
                                "name "
                                "FROM airlines "
                                "WHERE icao = ?;");

        query.bind(1, code);

        if (!query.executeStep())
            return "";

        return query.getColumn(0).getString();
    }
    catch (const SQLite::Exception &e)
    {
        spdlog::error("Lookup airline failed for callsign '{}' and ICAO '{}' with error '{}'", callsign, code, e.what());
    }
    return "";
}

struct DisplayData
{
    std::string header; // Airline or manufacturer - this is why database is needed
    std::string img_path;
    std::string callsign;
    int alt;
    int speed;
    int distance;
    int bearing;
    int track;

    // TODO: Need to add check to process.cpp to ensure below is true
    DisplayData(SQLite::Database &db, const Aircraft &a,
                const Config &cfg) // Assumes aircraft has all info
    {
        std::string airline = lookup_airline(db, a.callsign);
        AircraftInfo info = lookup_aircraft(db, a.icao);

        img_path = std::format("{}/sprites/{}.png", ASSETS_PATH, to_str(info.type));

        if (!airline.empty())
        {
            header = airline;
            auto try_path = std::format("{}/airlines/{}.png", ASSETS_PATH, airline); // Hardcoded for now
            if (std::filesystem::exists(try_path))
                img_path = try_path;
        }
        else
            header = std::format("{} {}", info.mfc, info.mdl);

        callsign = a.callsign;
        alt = *a.alt;
        speed = *a.gs;
        distance = static_cast<int>(calc_dist(cfg.lat, cfg.lon, *a.lat, *a.lon));
        track = *a.trk;
        bearing = static_cast<int>(calc_bearing(cfg.lat, cfg.lon, *a.lat, *a.lon));
    }
};