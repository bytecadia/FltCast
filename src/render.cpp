#include "led-matrix.h"
#include "graphics.h"

#include "layout.hpp"
#include "image.hpp"
#include "canvas.hpp"
#include "render.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

// TODO:: Move this out of render

void draw(const std::vector<Position> &positions, Canvas &canvas)
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

void render(std::stop_token st, Snapshot &snap, const Config &cfg, SQLite::Database &db)
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

    // TODO: Fix hard coding here
    options.chain_length = 1;
    options.parallel = 1;
    options.limit_refresh_rate_hz = 300;
    options.show_refresh_rate = true;

    rgb_matrix::RuntimeOptions runtime_opt;
    runtime_opt.gpio_slowdown = 5;
    runtime_opt.drop_privileges = -1;

    rgb_matrix::RGBMatrix *mtrx = rgb_matrix::CreateMatrixFromOptions(options, runtime_opt);
    if (mtrx == NULL)
        return;

    Canvas canvas(mtrx->CreateFrameCanvas(), 0, cfg.cols);

    ImgCache cache;
    auto start = std::chrono::steady_clock::now();
    while (!st.stop_requested())
    {
        auto a = snap.read(); // TODO!: what did I mean by this -> (TODO: Make this blocking)
                              // ^ Ahh I see it checks and returns immediately if there
                              // is no aircraft to read and will keep looping
                              //  if (st.stop_requested())
                              //      return;
                              // There needs to be a way to tell if update is necessary
                              // so display data obect doesn't need to be written
                              // every refresh
        // TODO: Create an idle screen?

        if (!a)
            continue;

        DisplayData data{db, *a, cfg};
        DisplayLayout disp{data, sml, med, lrg, 0, 0, cfg.cols, cfg.rows, cfg.padding, cfg.row_gap, cfg.img_span};

        canvas.Clear();

        std::optional<Image> img = cache.get_image(data.img_path, cfg.img_h);

        if (img)
            draw_image(canvas, disp.content.lft(), disp.content.tp(), *img);

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

        if (!img) // TODO: Fix, this is ugly
            img = Image{};

        auto positions = layout(disp, elapsed, *img);

        draw(positions, canvas);
        auto next = mtrx->SwapOnVSync(canvas.GetRGBMatrix());
        canvas.SetRGBMatrix(next);
    }

    delete mtrx;
#endif
}