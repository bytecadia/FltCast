#pragma once

#include <cmath>
#include <numbers>

inline double rads(double deg)
{
    return deg * (std::numbers::pi / 180);
}

inline double calc_bearing(double base_lat, double base_lon,
                           double targ_lat, double targ_lon)
{
    double phi1 = rads(base_lat);
    double phi2 = rads(targ_lat);

    double lambda1 = rads(base_lon);
    double lambda2 = rads(targ_lon);
    double delta_lambda = lambda2 - lambda1;

    double y = std::sin(delta_lambda) * std::cos(phi2);
    double x = std::cos(phi1) * std::sin(phi2) -
               std::sin(phi1) * std::cos(phi2) * std::cos(delta_lambda);

    double theta = std::atan2(y, x);
    double degrees = theta * 180.0 / std::numbers::pi;

    return std::fmod(degrees + 360.0, 360.0);
}

inline std::string cardinal_dir(double bearing)
{
    static const char *dirs[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    return dirs[static_cast<int>((bearing + 22.5) / 45.0) % 8];
}

// Equirectangular approximation
inline double calc_dist(double base_lat, double base_lon,
                        double targ_lat, double targ_lon)
{
    double phi1 = rads(base_lat);
    double phi2 = rads(targ_lat);

    double lambda1 = rads(base_lon);
    double lambda2 = rads(targ_lon);

    double sclr = std::cos((phi1 + phi2) / 2.0);

    double y = phi2 - phi1;
    double x = (lambda2 - lambda1) * sclr;

    return std::sqrt(x * x + y * y) * 3959.0;
}