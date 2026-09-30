#pragma once

#include <string>
#include <vector>
#include "graphics.h"

struct Text
{
    std::string items;
    const rgb_matrix::Font *font;
    const rgb_matrix::Color *color;
};

enum class Mode
{
    Clip,
    Fit,
    Scroll
};

struct Element
{
    Mode mode; // TODO: These fields left unused?
    int gap;
    std::vector<Text> items;
};

struct Row
{
    Mode mode;
    std::vector<Element> items;
    int h;
    int gap;
};

struct Position
{
    Text text;
    int x;
    int y;
    int l;
    int r;
};

class Rect
{
public:
    int x;
    int y;
    int h;
    int w;

    Rect(int x, int y, int w, int h) : x(x), y(y), h(h), w(w) {}

    void inset(int padding)
    {
        x += padding;
        y += padding;
        h -= padding * 2;
        w -= padding * 2;
    }

    int lft() const { return x; }
    int rght() const { return x + w; }
    int tp() const { return y; }
    int btm() const { return y + h; }
};