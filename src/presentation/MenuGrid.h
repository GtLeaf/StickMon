#pragma once

#include <cstdint>

class Canvas565;

namespace MenuGrid {
struct Rect {
    int x, y, width, height;
    bool contains(int px, int py) const {
        return px >= x && py >= y && px < x + width && py < y + height;
    }
};

struct Layout {
    Rect viewport;
    int columns, cellWidth, cellHeight, gapX, gapY, left, top;
    int textScale = 1;
    int drawOffsetY = 0;
    Rect cell(int index, float scroll = 0) const;
    int itemAt(int x, int y, float scroll, int count) const;
    float maxScroll(int count) const;
    float reveal(int index, float scroll, int count) const;
};

Layout fit(Rect viewport, int minimumWidth = 82, int cellHeight = 72,
           int gapX = 8, int gapY = 6, int padding = 8,
           int top = 0, int maximumWidth = 0);
Layout mainLayout(int width, int height, int textScale = 1);
void card(Canvas565& canvas, Rect rect, bool pressed, bool selected = false);
void label(Canvas565& canvas, int centerX, int y, const char* text,
           int maxWidth, uint16_t color, int textScale = 1);
void mainMenu(Canvas565& canvas, const Layout& layout, int count,
              float scroll, int pressed, int selected = -1);
}  // namespace MenuGrid
