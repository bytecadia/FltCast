#include <algorithm>
#include <ctime>

#include "led-matrix.h"
#include "graphics.h"

#include "layout.hpp"
#include "image.hpp"
#include "canvas.hpp"
#include "render.hpp"
#include "colors.hpp"

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
void draw_radar(Canvas &c, const std::vector<DisplayData> &data, int w, int h, long elapsed)
{
    int cx = w / 2;
    int cy = h / 2;
    int radius = w / 2 - 2;

    // Rings
    rgb_matrix::DrawCircle(c.GetRGBMatrix(), cx, cy, radius, GREY);
    rgb_matrix::DrawCircle(c.GetRGBMatrix(), cx, cy, (2 * radius) / 3, GREY);
    rgb_matrix::DrawCircle(c.GetRGBMatrix(), cx, cy, radius / 3, GREY);

    // Crosshairs
    rgb_matrix::DrawLine(c.GetRGBMatrix(), 0, cy, w, cy, GREY);
    rgb_matrix::DrawLine(c.GetRGBMatrix(), cx, 0, cx, h, GREY);

    // Center
    rgb_matrix::DrawLine(c.GetRGBMatrix(), cx - 2, cy, cx + 2, cy, ORANGE);
    rgb_matrix::DrawLine(c.GetRGBMatrix(), cx, cy - 2, cx, cy + 2, ORANGE);

    // Draw sweep
    auto a = std::fmod(elapsed / 3000.0, 1.0) * 2 * std::numbers::pi;
    double x = cx + radius * sin(a);
    double y = cy - radius * cos(a);
    DrawLine(c.GetRGBMatrix(), cx, cy, std::lround(x), std::lround(y), DIM);

    // Tracks
    double range = 100;
    for (std::size_t i = 0; i < data.size(); i++)
    {
        auto &d = data[i];

        if (d.distance > range)
            continue;

        double r = d.distance / range * radius;
        auto b = rads(data[i].bearing);
        int x = std::lround(cx + r * sin(b));
        int y = std::lround(cy - r * cos(b));

        // Track
        c.SetPixel(x, y, WHITE.r, WHITE.g, WHITE.b);

        // Draw spotlight
        if (i == 0)
        {
            rgb_matrix::DrawLine(c.GetRGBMatrix(), x - 3, y + 3, x + 3, y + 3, ORANGE);
            rgb_matrix::DrawLine(c.GetRGBMatrix(), x - 3, y - 3, x + 3, y - 3, ORANGE);
            rgb_matrix::DrawLine(c.GetRGBMatrix(), x - 3, y + 3, x - 3, y - 3, ORANGE);
            rgb_matrix::DrawLine(c.GetRGBMatrix(), x + 3, y + 3, x + 3, y - 3, ORANGE);
        }

        // TODO: Draw lines from list to trk?
    }
}

void draw_planes(Canvas &c, const std::vector<DisplayData> &data, const rgb_matrix::Font &font,
                 Rect content)
{
    // Helpers
    auto split = [&](Element left, Element right)
    {
        int gap = std::max(1, (content.rght() - content.lft()) - msr(left) - msr(right));
        return Row{Mode::Scroll, {left, right}, font.height(), gap};
    };

    auto draw_row = [&](const Row &row, int y)
    {
        draw_positions(lay_row(content.lft(), y, content.lft(), content.rght(), row, 0), c);
    };

    // Start y
    int head_y = content.tp() + font.baseline();
    int foot_y = content.btm() - (font.height() - font.baseline());

    // Header
    draw_row(split(Element{Mode::Fit, 4, {Text{"TRK", &font, &ORANGE}, Text{"ID", &font, &ORANGE}}},
                   Element{Mode::Fit, 0, {Text{"DIST", &font, &GREY}}}),
             head_y);

    // Planes
    int y = head_y + font.height();
    for (std::size_t i = 0; i < data.size() && y <= foot_y - font.height(); i++, y += font.height())
    {
        const auto &d = data[i];

        draw_row(split(Element{Mode::Fit, 4, {Text{std::format("{:02}", i + 1), &font, &GREY}, Text{d.sub_header, &font, i == 0 ? &ORANGE : &WHITE}}},
                       Element{Mode::Fit, 0, {Text{std::to_string(d.distance), &font, &GREY}}}),
                 y);
    }

    // Footer
    char clock[9];
    std::time_t now = std::time(nullptr);
    std::strftime(clock, sizeof clock, "%H:%M:%S", std::localtime(&now));
    draw_row(split(Element{Mode::Fit, 0, {Text{"SCAN", &font, &ORANGE}}},
                   Element{Mode::Fit, 0, {Text{clock, &font, &GREY}}}),
             foot_y);
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
    options.pixel_mapper_config = cfg.pixel_mapper;

    rgb_matrix::RuntimeOptions runtime_opt;
    runtime_opt.gpio_slowdown = cfg.gpio_slowdown;
    runtime_opt.drop_privileges = -1;

    rgb_matrix::RGBMatrix *mtrx = rgb_matrix::CreateMatrixFromOptions(options, runtime_opt);
    if (mtrx == NULL)
        return;

    Canvas canvas(mtrx->CreateFrameCanvas(), 0, cfg.cols);

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
        draw_radar(canvas, aircrafts, 64, 64, elapsed);

        Rect plane_frame{64, 0, 64, 64};
        plane_frame.inset(2);
        draw_planes(canvas, aircrafts, lrg, plane_frame);

        auto next = mtrx->SwapOnVSync(canvas.GetRGBMatrix());
        canvas.SetRGBMatrix(next);
    }

    delete mtrx;
#endif
}