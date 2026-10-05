#pragma once

#include <filesystem>
#include <format>
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

// Sprites are made with ft/tools/sprite_maker (outside the repo)
// FAA spells every variant differently (737-823, 737-7H4) so match on prefixes - first match wins
struct SpriteFamily
{
    const char *mfc;  // Manufacturer starts with
    const char *mdl;  // Model starts with
    const char *name; // assets/sprites/<name>.png
};

inline constexpr SpriteFamily SPRITE_FAMILIES[] = {
    {"BOEING", "737", "b737"},
    {"BOEING", "757", "b757"},
    {"BOEING", "767", "b767"},
    {"BOEING", "777", "b777"},
    {"BOEING", "787", "b787"},
    {"AIRBUS", "A31", "a320"}, // A318, A319
    {"AIRBUS", "A32", "a320"}, // A320, A321
    {"AIRBUS", "A33", "a330"},
    {"AIRBUS", "A35", "a350"},
    {"AIRBUS", "BD-500", "a220"},
    {"EMBRAER", "ERJ 170", "e175"},
    {"EMBRAER", "ERJ 190", "e190"},
    {"EMBRAER", "EMB-145", "erj145"},
    {"LEARJET", "", "learjet"},         // Every Learjet model
    {"GATES LEARJET", "", "learjet"},   // Older Learjets are registered under Gates
    {"BOMBARDIER", "CL-600-2C", "crj"}, // CRJ700 (CL-600-2B16 is a Challenger business jet)
    {"BOMBARDIER", "CL-600-2D", "crj"}, // CRJ900
    {"CESSNA", "172", "c172"},
    {"CESSNA", "152", "c172"}, // High-wing singles share one sprite
    {"CESSNA", "182", "c172"},
    {"PIPER", "PA-28", "pa28"},
    {"CIRRUS", "SR2", "sr22"},
    {"ROBINSON", "R44", "r44"},
    {"ROBINSON", "R22", "r44"},
};

inline std::string sprite_for(const AircraftInfo &info)
{
    for (const auto &f : SPRITE_FAMILIES)
    {
        if (info.mfc.starts_with(f.mfc) && info.mdl.starts_with(f.mdl))
        {
            auto path = std::format("{}/sprites/{}.png", ASSETS_PATH, f.name);
            if (std::filesystem::exists(path))
                return path;
            break;
        }
    }

    // No family - use the most common model of the same kind (unknown is mostly foreign airliners)
    const char *fallback = info.type == AircraftType::Prop ? "c172" : info.type == AircraftType::Heli ? "r44"
                                                                                                      : "b737";
    return std::format("{}/sprites/{}.png", ASSETS_PATH, fallback);
}

// TODO: Should this be returning optional, does it make sense for partials here
inline AircraftInfo lookup_aircraft(SQLite::Database &db, std::string icao)
{
    try
    {
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

inline std::pair<std::string, std::string> lookup_airline(SQLite::Database &db, std::string callsign)
{
    std::string code = parse_chars(callsign);

    if (code.empty())
        return {};

    try
    {
        SQLite::Statement query(db,
                                "SELECT "
                                "name "
                                "FROM airlines "
                                "WHERE icao = ?;");

        query.bind(1, code);

        if (!query.executeStep())
            return {};

        return {code, query.getColumn(0).getString()};
    }
    catch (const SQLite::Exception &e)
    {
        spdlog::error("Lookup airline failed for callsign '{}' and ICAO '{}' with error '{}'", callsign, code, e.what());
    }
    return {};
}

struct DisplayData
{
    std::string header; // Airline or manufacturer - this is why database is needed
    std::string img_path;
    std::string sub_header;
    std::string icao;
    std::string model;
    std::string sprite_path;
    int alt;
    int speed;
    int distance;
    int bearing;
    int track;
    int vs;
    double bank;
    double pitch;
    std::vector<int> alt_hist;

    // TODO: Need to add check to process.cpp to ensure below is true
    DisplayData(SQLite::Database &db, const Aircraft &a,
                const Config &cfg) // Assumes aircraft has all info
    {
        auto [code, airline] = lookup_airline(db, a.callsign);
        AircraftInfo info = lookup_aircraft(db, a.icao);

        sprite_path = sprite_for(info);
        img_path = sprite_path;

        if (!airline.empty())
        {
            header = airline;
            auto try_path = std::format("{}/airlines/{}.png", ASSETS_PATH, code);
            if (std::filesystem::exists(try_path))
                img_path = try_path;
        }
        else
            header = std::format("{} {}", info.mfc, info.mdl);

        sub_header = a.icao;

        if (!a.callsign.empty())
            sub_header = trim(a.callsign);

        alt = a.alt.value_or(0);
        speed = a.gs.value_or(0);
        distance = static_cast<int>(calc_dist(cfg.lat, cfg.lon, *a.lat, *a.lon));
        track = a.trk.value_or(0);
        bearing = static_cast<int>(calc_bearing(cfg.lat, cfg.lon, *a.lat, *a.lon));
        icao = a.icao;
        model = info.mdl;
        vs = a.vs.value_or(0);
        bank = a.bank;
        pitch = speed > 0 ? std::atan(vs * 0.00508 / (speed * 0.514444)) * 180 / std::numbers::pi : 0;
        alt_hist.assign(a.alt_hist.begin(), a.alt_hist.end());
    }
};