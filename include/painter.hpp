#pragma once

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

#include "graphics.h"
#include "colors.hpp"

// Drawing helpers for the fixed layout screens - text y is the baseline
struct Painter
{
    rgb_matrix::Canvas *c; // Base canvas so a non matrix canvas works too

    void px(int x, int y, const rgb_matrix::Color &col) const
    {
        c->SetPixel(x, y, col.r, col.g, col.b);
    }

    void line(int x0, int y0, int x1, int y1, const rgb_matrix::Color &col) const
    {
        rgb_matrix::DrawLine(c, x0, y0, x1, y1, col);
    }

    void circle(int x, int y, int r, const rgb_matrix::Color &col) const
    {
        rgb_matrix::DrawCircle(c, x, y, r, col);
    }

    void fill(int x, int y, int w, int h, const rgb_matrix::Color &col) const
    {
        for (int iy = y; iy < y + h; iy++)
            line(x, iy, x + w - 1, iy, col);
    }

    // Every other pixel - library only draws solid lines
    void dotted(int x0, int y0, int x1, int y1, const rgb_matrix::Color &col) const
    {
        int n = std::max({std::abs(x1 - x0), std::abs(y1 - y0), 1});
        for (int i = 0; i <= n; i += 2)
            px(x0 + std::lround(double(x1 - x0) * i / n), y0 + std::lround(double(y1 - y0) * i / n), col);
    }

    // Returns the advance so more text can follow
    int text(const rgb_matrix::Font &f, int x, int y, const rgb_matrix::Color &col, const std::string &s) const
    {
        return rgb_matrix::DrawText(c, f, x, y, col, s.c_str());
    }

    // xr is the last lit column (+ 2 because every glyph ends in a blank column)
    void right(const rgb_matrix::Font &f, int xr, int y, const rgb_matrix::Color &col, const std::string &s) const
    {
        text(f, xr + 2 - rgb_matrix::MeasureText(f, s.c_str()), y, col, s);
    }

    // btop style panel with the title in the top border
    void panel(int x0, int y0, int x1, int y1, const rgb_matrix::Font &f, const std::string &title,
               const rgb_matrix::Color &tcol) const
    {
        int tx = x0 + 4;
        line(x0 + 1, y0, x0 + 2, y0, GRID);
        line(tx + rgb_matrix::MeasureText(f, title.c_str()) + 1, y0, x1 - 1, y0, GRID);
        line(x0, y0 + 1, x0, y1 - 1, GRID);
        line(x1, y0 + 1, x1, y1 - 1, GRID);
        line(x0 + 1, y1, x1 - 1, y1, GRID);
        text(f, tx, y0 + 3, tcol, title);
    }

    // 2px blocks with 1px gaps
    void meter(int x0, int x1, int y, double frac) const
    {
        int n = (x1 - x0 + 2) / 3;
        int lit = std::lround(std::clamp(frac, 0.0, 1.0) * n);
        for (int i = 0; i < n; i++)
            fill(x0 + 3 * i, y, 2, 3, i < lit ? GREY : RING);
    }
};
