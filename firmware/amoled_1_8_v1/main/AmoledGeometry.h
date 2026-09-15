#pragma once

#include <cstdint>

// All AMOLED layout, touch events and dirty rectangles use panel pixels.
namespace AmoledUi {

struct Rect {
    int x;
    int y;
    int width;
    int height;

    constexpr bool contains(int px, int py) const {
        return px >= x && px < x + width &&
               py >= y && py < y + height;
    }
};

inline constexpr int WIDTH = 368;
inline constexpr int HEIGHT = 448;
// Source artwork and shared world geometry keep their authored resolution.
inline constexpr int RESOURCE_SCALE = 2;

}  // namespace AmoledUi
