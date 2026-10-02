#pragma once

#include <spdlog/spdlog.h>
#include <INIReader.h>
#include <string>

struct Config
{
    std::string host;
    std::string port;

    double lat;
    double lon;

    std::string sml_fnt;
    std::string med_fnt;
    std::string lrg_fnt;

    int cols;
    int rows;
    int chain_length;
    int parallel;
    std::string pixel_mapper;
    int limit_refresh;
    int gpio_slowdown;
    int padding;
    int row_gap;
    int img_span;
    int img_h;

    double range;
};

inline Config parse_cfg(std::string path)
{
    INIReader reader(path); // TODO: what if reader fails

    if (reader.ParseError() != 0)
        spdlog::error("Failed to parse config '{}' with error code {}", path, reader.ParseError());

    // Logging wrappers for INIReader
    auto get = [&](const std::string &section, const std::string &key, const std::string &def)
    {
        auto v = reader.Get(section, key, def);
        if (v == def && reader.Get(section, key, "") == "")
            spdlog::error("{}.{} missing using default {}", section, key, def);
        return v;
    };

    auto getReal = [&](const std::string &section, const std::string &key, double def)
    {
        auto v = reader.GetReal(section, key, def);
        if (v == def && reader.Get(section, key, "") == "")
            spdlog::error("{}.{} missing using default {}", section, key, std::to_string(def));
        return v;
    };

    Config config;
    config.host = get("network", "host", "127.0.0.1");
    config.port = get("network", "port", "30003");

    config.lat = getReal("location", "lat", 40.7); // NYC Default
    config.lon = getReal("location", "lon", -74.0);

    config.sml_fnt = get("font", "sml", "4x6");
    config.med_fnt = get("font", "med", "5x8");
    config.lrg_fnt = get("font", "lrg", "6x10");

    config.rows = static_cast<int>(getReal("matrix", "rows", 32));
    config.cols = static_cast<int>(getReal("matrix", "cols", 64));
    config.chain_length = static_cast<int>(getReal("matrix", "chain_length", 1));
    config.parallel = static_cast<int>(getReal("matrix", "parallel", 1));
    config.limit_refresh = static_cast<int>(getReal("matrix", "limit_refresh", 300));
    config.gpio_slowdown = static_cast<int>(getReal("matrix", "gpio_slowdown", 5));
    config.pixel_mapper = get("matrix", "pixel_mapper", "U-mapper");
    config.padding = static_cast<int>(getReal("matrix", "padding", 2));
    config.row_gap = static_cast<int>(getReal("matrix", "row_gap", 2));
    config.img_span = static_cast<int>(getReal("matrix", "img_span", 2));
    config.img_h = static_cast<int>(getReal("matrix", "img_height", 64));

    config.range = getReal("radar", "range", 1000);

    return config;
}