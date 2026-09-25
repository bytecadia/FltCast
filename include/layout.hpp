#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "ui.hpp"
#include "image.hpp"
#include "data.hpp"
#include "geometry.hpp"

struct DisplayLayout
{
    std::vector<Row> rows;
    Rect content;
    int row_gap;
    int img_span;

    DisplayLayout(const DisplayData &data,
                  const rgb_matrix::Font &sml,
                  const rgb_matrix::Font &med, const rgb_matrix::Font &lrg, int x, int y, int w, int h, int padding, int row_gap, int img_span);
};

int msr(const Text &t);

template <typename T>
int msr(const T &t)
{
    int w = 0;
    const size_t n = t.items.size();
    for (size_t i = 0; i < n; i++)
    {
        w += msr(t.items[i]);
        if (i + 1 < t.items.size())
            w += t.gap;
    }

    return w;
}

std::vector<Position> lay_elmnt(int x, int y, int l, int r, int &w, int gap, Element &elmnt);
std::pair<int, int> rght_scrl_plcmnt(int strt, int l, int r, int min_gap, int pps, int64_t time, const Row &rest);
std::pair<int, int> lft_scrl_plcmnt(int strt, int l, int r, int min_gap, int pps, int64_t time, const Row &rest);
std::vector<Position> lay_row(int x, int y, int l, int r, const Row &row, int64_t time);
std::vector<Position> layout(const DisplayLayout &disp, int64_t time, Image img);