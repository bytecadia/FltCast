#include <algorithm>
#include <string>

#include "graphics.h"
#include "layout.hpp"

int msr(const Text &t)
{
    return MeasureText(*t.font, t.items.c_str(), 0);
}

std::vector<Position> lay_elmnt(int x, int y, int l, int r, int &w,
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
            x += elmnt.gap;

        pos.push_back(np);
    }
    w = x - start;
    return pos;
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

    for (size_t i = 0; i < row.items.size(); i++)
    {
        int w;
        Element elmnt = row.items[i];
        std::vector<Position> np = lay_elmnt(x, y, l, r, w, elmnt);

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
                auto [x_first, x_rollover] = lft_scrl_plcmnt(x, l, r, 3, 15, time, Row{row.mode, rest, row.h, row.gap}); // TODO: Hard code gap and pps for now

                np = lay_elmnt(x_first, y, l, r, w, elmnt);
                pos.insert(pos.end(), np.begin(), np.end());

                np = lay_elmnt(x_rollover, y, l, r, w, elmnt);
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
