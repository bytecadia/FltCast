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
        Mode::Fit,
        {Element{Mode::Scroll, 0, {Text{data.callsign, &lrg, &RED}}}},
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

std::pair<int, int> scrl_plcmnt(int strt, int l, int r, int min_gap, int pps, int64_t time, const Row &rest)
{
    // `(time * pps)` represents how many pixels that should be advancing.
    // But since time is in ms we need to divide by 1000 to get the value in secs.
    // Now, we are still not done since this number would continue
    // to grow and grow, we need it to wrap around once it reaches the end of the
    // canvas. We calculate that by getting the modulus of the total space alloted
    // by the clipping bounds `(r - 1)`.
    int scrl_ofst = ((time * pps) / 1000) % (r - l);

    // Here is where we begin to calculate where the new placement should be.
    // Subtracting the original start by our offset positions the text such
    // that it will scroll text to the left, <-
    strt -= scrl_ofst;

    // Next we calculate two possible placements for the rollover text:

    // 1) The ideal position which is the rollover begining exactly at the
    // left boundary for example:
    // EXAMPLE 1:
    //      0123456
    //  [T]|EXT   T|[EXT]
    //   ^x ^ Rollover (Note: the assumption is that clipping is handled
    //                  by the drawing step, we are not concerned with the
    //.                 fact that we are still accounting for the text that
    //                  is cut off, `T`, we need it for the caclulations.
    //                  Anything beyond the clip will later be cut off)

    // 2) The miniumum distance which accounts for the case when the
    // text is close to or greater than the size of the clipping areas.
    // In which case it would overwrite the first placement or not give enough of a gap:
    // EXAMPLE 2:
    //      012
    //  [T]|EXT|[EXT]
    //   ^x   ^ Avoiding this overwrite

    // We calculate the miniumum distance that the second placment has
    // to be to satisfy a minimum gap, lets say 2. This is irrespective
    // of the left clip. So if the clipping bounds has plenty of space:
    // EXAMPLE 3:
    //      0123456
    //     |TEXT..T|[EXT]   (Wrong - Extra letters)
    //      ^x  ^^ Gaps
    //
    // But if it doesn't:
    // EXAMPLE 4 ():
    //         012
    //  [TEXT]|..T|[EXT]   (Right - Fixed the overwrite)
    //   ^x    ^^ Gaps

    // The final step is to pick the one that is the most accurate.
    // If you compare the examples were the clipping size is the same.
    // examples 1 & 3 vs examples 2 & 4. You can clearly see that the
    // correct option is always whichever is the smallest value (x).

    // With that being said here is the actual logic:

    // `(r - strt)` is the distance from the current start of the
    // scrolling text to the right clipping boundary. Subtracting that
    // distance from `l` places the rollover copy the same distance to
    // the left of the left clipping boundary. This makes text clipped
    // at the right boundary appear to continue from the left boundary.
    int wrap_x = l - (r - strt);

    // For our next steps we need to get the width of the text that has to be
    // drawn.
    int w = msr(rest);

    // Calculate the rightmost position the rollover copy can occupy
    // without overlapping the original text. The rollover copy has width
    // `w`, so placing its start at `strt - w` would put its right edge
    // exactly at the original text's start. Subtracting `min_gap` guarantees
    // at least that much space between the two copies.
    int max_x = strt - w - min_gap;

    // Prefer the natural clipping-based wrap position, but if that
    // would place the rollover too close to the original text,
    // move it left far enough to preserve the minimum gap.
    int rollover_strt = std::min(wrap_x, max_x);

    // Return the first and second placemnet
    return {strt, rollover_strt};
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
                auto [x_first, x_rollover] = scrl_plcmnt(x, l, r, 3, 1, time, Row{row.mode, rest, row.h, row.gap}); // TODO: Hard code gap and pps for now

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
    const int x1 = (1 <= disp.img_span) ? x + img.w : x;
    const int x2 = (2 <= disp.img_span) ? x + img.w : x;
    const int x3 = (3 <= disp.img_span) ? x + img.w : x;
    const int x4 = (4 <= disp.img_span) ? x + img.w : x;

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