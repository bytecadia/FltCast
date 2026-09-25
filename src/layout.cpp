#include <string>
#include <SQLiteCpp/SQLiteCpp.h>

#include "config.hpp" // TODO: Pretty sure config should be used here?
#include "aircraft.hpp"
#include "colors.hpp"
#include "strings.hpp"
#include "graphics.h"
#include "data.hpp"
#include "layout.hpp"

DisplayLayout::DisplayLayout(const DisplayData &data,
                             const rgb_matrix::Font &sml,
                             const rgb_matrix::Font &med, const rgb_matrix::Font &lrg, int x, int y, int w, int h, int padding, int row_gap, int img_span)
    : content(x, y, w, h), row_gap(row_gap), img_span(img_span)
{
    rows.push_back(Row{
        Mode::Scroll,
        {Element{Mode::Scroll, 0, {Text{data.header, &lrg, &RED}}}},
        lrg.height(), // TODO: Made it largest font height for now, should be good?
        0});

    rows.push_back(Row{
        Mode::Clip,
        {Element{Mode::Clip, 0, {Text{data.sub_header, &sml, &RED}}}}, // TODO: Mode unused in element
        lrg.height(),
        0});

    rows.push_back(Row{
        Mode::Fit,
        {Element{Mode::Fit, 2, {Text{std::to_string(data.speed), &sml, &BLUE}, Text{"mph", &sml, &LIGHT_BLUE}}},
         Element{Mode::Fit, 2, {Text{std::to_string(data.alt), &sml, &BLUE}, Text{"ft", &sml, &LIGHT_BLUE}}}},
        lrg.height(),
        2});

    rows.push_back(Row{
        Mode::Fit,
        {Element{Mode::Fit, 2, {Text{std::to_string(data.distance), &med, &YELLOW}, Text{"mi", &sml, &LIGHT_YELLOW}}},
         Element{Mode::Fit, 0, {Text{std::to_string(data.bearing), &med, &YELLOW}}},
         Element{Mode::Fit, 2, {Text{"-", &med, &YELLOW}, Text{std::to_string(data.track), &med, &YELLOW}, Text{"°", &sml, &LIGHT_YELLOW}}}},
        lrg.height(),
        2});

    content.inset(padding);
}

int msr(const Text &t)
{
    return MeasureText(*t.font, t.items.c_str(), 0);
}

std::vector<Position> lay_elmnt(int x, int y, int l, int r, int &w, int gap,
                                Element &elmnt)
{
    int start = x;
    std::vector<Position> pos;
    for (size_t i = 0; i < elmnt.items.size(); i++)
    {
        Text itm = elmnt.items[i];
        Position np{itm, x, y, l, r};

        x += msr(itm);
        if (i < elmnt.items.size() - 1)
            x += gap;

        pos.push_back(np);
    }
    w = x - start;
    return pos;
}

std::pair<int, int> rght_scrl_plcmnt(int strt, int l, int r, int min_gap, int pps, int64_t time, const Row &rest)
{
    int period = std::max(r - l, msr(rest) + min_gap);
    strt -= ((time * pps) / 1000) % period;

    return {strt, strt + period};
}

std::pair<int, int> lft_scrl_plcmnt(int strt, int l, int r, int min_gap, int pps, int64_t time, const Row &rest)
{
    int period = std::max(r - l, msr(rest) + min_gap);
    strt -= ((time * pps) / 1000) % period;

    return {strt, strt + period};
}

std::vector<Position> lay_row(int x, int y, int l, int r, const Row &row, int64_t time)
{
    std::vector<Position> pos;
    int start = x;

    for (size_t i = 0; i < row.items.size(); i++)
    {
        int w;
        Element elmnt = row.items[i];
        std::vector<Position> np = lay_elmnt(x, y, l, r, w, row.gap, elmnt);

        if (x + w > r)
        {
            switch (row.mode)
            {
            case Mode::Fit:
                return pos;

            case Mode::Clip:
                pos.insert(pos.end(), np.begin(), np.end());
                return pos;

            case Mode::Scroll: // TODO: Only scrows when the text overflows, is that what I want?
            {
                std::vector<Element> rest(row.items.begin() + i, row.items.end());
                auto [x_first, x_rollover] = lft_scrl_plcmnt(x, l, r, 3, 5, time, Row{row.mode, rest, row.h, row.gap}); // TODO: Hard code gap and pps for now

                np = lay_elmnt(x_first, y, l, r, w, row.gap, elmnt);
                pos.insert(pos.end(), np.begin(), np.end());

                np = lay_elmnt(x_rollover, y, l, r, w, row.gap, elmnt);
                pos.insert(pos.end(), np.begin(), np.end());

                x += w + row.gap;
                continue;
            }
            }
        }

        pos.insert(pos.end(), np.begin(), np.end());
        x += w + row.gap;
    }

    return pos;
}

std::vector<Position> layout(const DisplayLayout &disp, int64_t time, Image img)
{
    const int y1 = disp.rows[0].h;
    const int y2 = y1 + disp.rows[1].h + disp.row_gap;
    const int y4 = disp.content.btm();
    const int y3 = y4 - disp.rows[3].h - disp.row_gap;

    const int x = disp.content.lft();
    const int x1 = (1 <= disp.img_span) ? x + 1 + img.w : x; // TODO: Fix this hardcoded gap here
    const int x2 = (2 <= disp.img_span) ? x + 1 + img.w : x;
    const int x3 = (3 <= disp.img_span) ? x + 1 + img.w : x;
    const int x4 = (4 <= disp.img_span) ? x + 1 + img.w : x;

    std::vector<Position> pos, row_pos;

    auto add_row = [&](int x, int y, int i)
    {
        auto row = lay_row(x, y, x, disp.content.rght(), disp.rows[i], time);
        pos.insert(pos.end(), row.begin(), row.end());
    };

    add_row(x1, y1, 0);
    add_row(x2, y2, 1);
    add_row(x3, y3, 2);
    add_row(x4, y4, 3);

    return pos;
}