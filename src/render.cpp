#include <algorithm>
#include <cctype>
#include <ctime>

#include "led-matrix.h"
#include "graphics.h"

#include "ui.hpp"
#include "image.hpp"
#include "canvas.hpp"
#include "render.hpp"
#include "colors.hpp"
#include "painter.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

// TODO:: Move this out of render
void draw_positions(const std::vector<Position> &positions, Canvas &canvas)
{
    for (const auto &pos : positions)
    {
        canvas.SetClipBounds(pos.l, pos.r);
        rgb_matrix::DrawText(&canvas,
                             *pos.text.font,
                             pos.x, pos.y,
                             *pos.text.color,
                             nullptr, // TODO:: Do I want background colors?
                             pos.text.items.c_str(),
                             0); // TODO: Add named parameters
    }
}

// TODO: Pass in a rect
void draw_radar(Canvas &c, Config cfg, const std::vector<DisplayData> &data, int w, int h, long elapsed)
{
    Painter p{c.GetRGBMatrix()};

    int cx = w / 2;
    int cy = h / 2;
    int radius = w / 2 - 2;

    // Frame
    const int corners[4][4] = {{0, 0, 1, 1}, {w - 1, 0, -1, 1}, {0, h - 1, 1, -1}, {w - 1, h - 1, -1, -1}};
    for (auto &k : corners)
    {
        p.line(k[0], k[1], k[0] + 3 * k[2], k[1], MID);
        p.line(k[0], k[1], k[0], k[1] + 3 * k[3], MID);
    }
    p.dotted(3, 0, w - 5, 0, FRAME);
    p.dotted(0, 3, 0, h - 5, FRAME);
    p.dotted(4, h - 1, w - 5, h - 1, FRAME);
    p.dotted(w - 1, 4, w - 1, h - 4, RING);
    p.line(cx, 0, cx, 1, WHITE);

    // Rings
    p.circle(cx, cy, radius / 3, RING);
    p.circle(cx, cy, (2 * radius) / 3, RING);
    p.circle(cx, cy, radius, MID);

    // Crosshairs
    p.dotted(2, cy, w - 2, cy, GRID);
    p.dotted(cx, 2, cx, h - 2, GRID);

    // Sweep
    const rgb_matrix::Color *trail[] = {&RING, &GRID, &SWEEP};
    double a = std::fmod(elapsed / 3000.0, 1.0) * 2 * std::numbers::pi;
    for (int i = 0; i < 3; i++)
    {
        double ta = a - (2 - i) * rads(5);
        p.line(cx, cy, cx + std::lround(radius * std::sin(ta)), cy - std::lround(radius * std::cos(ta)), *trail[i]);
    }

    // Featured aircraft
    std::size_t featured = data.empty() ? 0 : (elapsed / 5000) % std::min(std::size_t{5}, data.size()); // 5 represents the max rows that can fit

    // Tracks and heading tails
    double range = cfg.range;
    std::vector<std::pair<int, int>> pos;
    for (std::size_t i = 0; i < data.size(); i++)
    {
        auto &d = data[i];

        double r = d.distance / range * radius;
        auto b = rads(d.bearing);
        int x = std::lround(cx + r * sin(b));
        int y = std::lround(cy - r * cos(b));
        pos.emplace_back(x, y);

        auto t = rads(d.track);
        for (int k = 1; k <= (i == featured ? 1 : 2); k++)
            p.px(x - std::lround(k * std::sin(t)), y + std::lround(k * std::cos(t)), i == featured ? ORANGE_TAIL : GRID);
    }

    // Leader line
    if (!data.empty())
    {
        auto [fx, fy] = pos[featured];
        int row_y = 13 + 9 * static_cast<int>(featured); // Middle of the row in draw_planes
        p.dotted(fx, fy, w, row_y, LEADER);
        p.line(w - 1, row_y - 1, w - 1, row_y, GREY);
    }

    for (std::size_t i = 0; i < pos.size(); i++)
        p.px(pos[i].first, pos[i].second, i == featured ? ORANGE : WHITE);

    // Spotlight brackets
    if (!data.empty())
    {
        auto [fx, fy] = pos[featured];
        for (int sx : {-1, 1})
            for (int sy : {-1, 1})
            {
                int x = fx + 3 * sx, y = fy + 3 * sy;
                p.line(x, y, x - sx, y, ORANGE);
                p.line(x, y, x, y - sy, ORANGE);
            }
    }

    // Center
    p.line(cx - 2, cy, cx + 2, cy, GREY);
    p.line(cx, cy - 2, cx, cy + 2, GREY);
}

void draw_planes(Canvas &c, const std::vector<DisplayData> &data, const rgb_matrix::Font &lrg,
                 const rgb_matrix::Font &sml, Rect content, long elapsed)
{
    Painter p{c.GetRGBMatrix()};

    int lft = content.lft();
    int rgt = content.rght() - 1; // Last column inside the padding
    int cs_x = lft + 14; // Callsign column

    // Baselines
    int head_y = content.tp() + sml.baseline();
    int foot_y = content.btm(); // Digits have no descender so it can sit in the padding

    // Dividers
    int top_div = head_y + 1;
    int btm_div = foot_y - sml.baseline() - 3;

    // Header
    p.text(sml, lft, head_y, MID, "TRK");
    p.text(sml, cs_x, head_y, GREY, "ID");
    p.right(sml, rgt, head_y, MID, "MI");

    // Divider
    p.line(lft, top_div, rgt, top_div, GRID);

    // Planes
    int step = 9;
    int y = top_div + 8;

    // Calculate the featured aircraft
    int rows = (btm_div - 1 - y) / step + 1; // Max rows that can fit
    std::size_t featured = data.empty() ? 0 : (elapsed / 5000) % std::min<std::size_t>(rows, data.size());

    for (std::size_t i = 0; i < data.size() && y < btm_div; i++, y += step)
    {
        const auto &d = data[i];
        bool f = i == featured;
        p.text(sml, lft + 1, y, f ? ORANGE_DIM : MID, std::format("{:02}", i + 1));
        p.text(lrg, cs_x, y, f ? ORANGE : WHITE, d.sub_header);
        p.right(sml, rgt, y, MID, std::to_string(d.distance));
    }

    // Divider
    p.line(lft, btm_div, rgt, btm_div, GRID);

    // Footer
    p.text(sml, lft + 1, foot_y, ORANGE, "LIVE");
    if ((elapsed / 500) % 2 == 0) // Flashing dot
        p.fill(lft + 19, foot_y - 3, 2, 2, LIVE);

    char clock[6];
    std::time_t now = std::time(nullptr);
    std::strftime(clock, sizeof clock, "%H:%M", std::localtime(&now));
    p.right(sml, rgt, foot_y, MID, clock);
}

void draw_dossier(Canvas &c, const DisplayData &d, std::size_t count, const rgb_matrix::Font &med,
                  const rgb_matrix::Font &sml, const std::optional<Image> &icon)
{
    Painter p{c.GetRGBMatrix()};
    const rgb_matrix::Color off;

    // Header bar
    p.fill(0, 0, c.width(), 7, ORANGE);
    p.text(sml, 2, 6, off, std::format("NEAREST // 1 OF {}", count));
    for (int i = 0; i < 4; i++)
        p.fill(113 + 3 * i, 4 - i, 2, 2 + i, off); // TODO: Signal bars decoration

    // Aircraft panel
    p.panel(0, 10, 55, 63, sml, d.sub_header, ORANGE);
    if (icon)
    {
        static const int bayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
        int x0 = 3 + (50 - icon->w) / 2;
        for (int iy = 0; iy < icon->h; iy++)
            for (int ix = 0; ix < icon->w; ix++)
            {
                int a = icon->pixels[(iy * icon->w + ix) * 4 + 3];
                int x = x0 + ix, y = 13 + iy;
                if (a * 0.6 * 16 / 255 > bayer[(y & 3) * 4 + (x & 3)])
                    p.px(x, y, WHITE);
                else if (a > 128)
                    p.px(x, y, MID);
            }
    }

    std::string airline = d.header.substr(0, d.header.find(' '));
    std::ranges::transform(airline, airline.begin(), [](unsigned char ch)
                           { return std::toupper(ch); });
    p.text(med, 3, 48, WHITE, airline.substr(0, 12));
    p.text(sml, 3, 56, MID, "TYPE");
    p.right(sml, 53, 56, WHITE, d.model.substr(0, 8));
    p.text(sml, 3, 62, MID, "ICAO");
    p.right(sml, 53, 62, WHITE, d.icao);

    // Altitude panel
    p.panel(58, 10, 127, 34, sml, "ALT", MID);
    int adv = p.text(med, 61, 20, WHITE, std::to_string(d.alt));
    p.text(sml, 61 + adv + 1, 20, MID, "FT");
    p.right(sml, 125, 20, MID, std::format("FL{:03}", d.alt / 100));
    p.meter(61, 124, 21, d.alt / 45000.0);

    // Altitude history
    if (!d.alt_hist.empty())
    {
        auto [lo, hi] = std::ranges::minmax(d.alt_hist);
        double span = std::max(hi - lo, 1000);
        int x = 124 - static_cast<int>(d.alt_hist.size());
        for (int v : d.alt_hist)
        {
            int h = 1 + std::lround((v - lo) / span * 6);
            ++x;
            if (h > 1)
                p.line(x, 32, x, 32 - h + 2, GRID);
            p.px(x, 32 - h + 1, MID);
        }
    }

    // Navigation panel
    p.panel(58, 38, 127, 63, sml, "NAV", MID);

    // Artificial horizon
    int cx = 70, cy = 51, r = 10;
    double b = rads(d.bank);
    double pitch = std::clamp(d.pitch / 2.5, -6.0, 6.0);
    for (int y = cy - r; y <= cy + r; y++)
        for (int x = cx - r; x <= cx + r; x++)
        {
            if (std::hypot(x - cx, y - cy) > r - 0.6)
                continue;
            double s = (y - cy - pitch) * std::cos(b) + (x - cx) * std::sin(b);
            if (std::abs(s) < 0.5)
                p.px(x, y, WHITE);
            else if (s > 0 && (x + y) % 2 == 0)
                p.px(x, y, GRID);
        }
    p.circle(cx, cy, r, MID);
    for (int deg : {-30, 30}) // Roll scale
        p.px(cx + std::lround((r + 1) * std::sin(rads(deg))), cy - std::lround((r + 1) * std::cos(rads(deg))), MID);
    p.px(cx, cy - r - 1, WHITE);
    p.line(cx - 7, cy, cx - 3, cy, ORANGE);
    p.line(cx + 3, cy, cx + 7, cy, ORANGE);
    p.px(cx, cy, ORANGE);

    // Heading tape
    int tx0 = 85, tx1 = 124, mid = (tx0 + tx1) / 2;
    for (int deg = (d.track + 320) / 10 * 10 - 360; deg <= d.track + 40; deg += 10)
    {
        int x = mid + std::lround((deg - d.track) / 2.0);
        if (x < tx0 || x > tx1)
            continue;
        bool major = (deg + 360) % 30 == 0;
        p.line(x, major ? 46 : 47, x, 47, major ? MID : GRID);
        if (!major)
            continue;
        int h = (deg + 360) % 360;
        std::string lab = h == 0 ? "N" : h == 90 ? "E"
                                     : h == 180  ? "S"
                                     : h == 270  ? "W"
                                                 : std::format("{:02}", h / 10);
        int w = rgb_matrix::MeasureText(sml, lab.c_str()) - 1;
        int lx = x - w / 2;
        if (lx >= tx0 && lx + w - 1 <= tx1)
            p.text(sml, lx, 45, std::abs(x - mid) <= 2 ? WHITE : MID, lab);
    }
    p.line(tx0, 48, tx1, 48, GRID);
    p.px(mid, 49, ORANGE);
    p.line(mid - 1, 50, mid + 1, 50, ORANGE);

    p.text(sml, tx0, 57, MID, "SPD");
    p.right(sml, tx1, 57, WHITE, std::format("{}KT", d.speed));
    p.text(sml, tx0, 63, MID, "VS");
    p.right(sml, tx1, 63, WHITE, std::format("{:+}", d.vs));
}

void render(std::stop_token st, Snapshot<std::vector<DisplayData>> &snap, const Config &cfg)
{
#ifdef __APPLE__
// Not on pi
#else
    auto load_font = [&](rgb_matrix::Font &font, const std::string &path)
    {
        if (!font.LoadFont(path.c_str()))
        {
            spdlog::error("Couldn't load font '{}'", path.c_str());
            return false;
        }
        return true;
    };

    rgb_matrix::Font sml;
    rgb_matrix::Font med;
    rgb_matrix::Font lrg;
    load_font(sml, ((std::format("{}/{}.bdf", FONT_DIR, cfg.sml_fnt).c_str())));
    load_font(med, ((std::format("{}/{}.bdf", FONT_DIR, cfg.med_fnt).c_str())));
    load_font(lrg, ((std::format("{}/{}.bdf", FONT_DIR, cfg.lrg_fnt).c_str())));

    rgb_matrix::RGBMatrix::Options options;
    options.rows = cfg.rows;
    options.cols = cfg.cols;
    options.chain_length = cfg.chain_length;
    options.parallel = cfg.parallel;
    options.limit_refresh_rate_hz = cfg.limit_refresh;
    options.pixel_mapper_config = cfg.pixel_mapper.c_str();

    rgb_matrix::RuntimeOptions runtime_opt;
    runtime_opt.gpio_slowdown = cfg.gpio_slowdown;
    runtime_opt.drop_privileges = -1;

    rgb_matrix::RGBMatrix *mtrx = rgb_matrix::CreateMatrixFromOptions(options, runtime_opt);
    if (mtrx == NULL)
        return;

    Canvas canvas(mtrx->CreateFrameCanvas(), 0, cfg.cols);

    ImgCache cache;
    auto start = std::chrono::steady_clock::now();
    while (!st.stop_requested())
    {
        auto aircrafts = snap.read(); // TODO!: what did I mean by this -> (TODO: Make this blocking)
                                      // ^ Ahh I see it checks and returns immediately if there
                                      // is no aircraft to read and will keep looping
                                      //  if (st.stop_requested())
                                      //      return;
                                      // There needs to be a way to tell if update is necessary
                                      // so display data obect doesn't need to be written
                                      // every refresh
                                      // I still need looping because of the scrolling but I
                                      // don't need to make a new display data object if its
                                      // not any new data.
                                      // Also when plane queue is cleaned in process thread at some
                                      // point an idle screen should show theres no new planes nearby
        // TODO: Create an idle screen?

        canvas.Clear();

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        bool dossier = !aircrafts.empty() && (elapsed / 1000) % 30 >= 20;
        if (dossier)
            draw_dossier(canvas, aircrafts[0], aircrafts.size(), med, sml, cache.get_image(aircrafts[0].sprite_path, 25));
        else
        {
            draw_radar(canvas, cfg, aircrafts, 64, 64, elapsed);

            Rect plane_frame{64, 0, 64, 64};
            plane_frame.inset(2);
            draw_planes(canvas, aircrafts, lrg, sml, plane_frame, elapsed);
        }

        auto next = mtrx->SwapOnVSync(canvas.GetRGBMatrix());
        canvas.SetRGBMatrix(next);
    }

    delete mtrx;
#endif
}