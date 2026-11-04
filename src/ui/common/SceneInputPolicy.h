#pragma once

#include "core/ButtonDispatcher.h"

namespace Ui {
template<int Width, int Height, int NavigationHeight>
struct SceneInputPolicy {
    static constexpr int CONTENT = -2;
    static constexpr int NAVIGATION = 0x7000;
    static bool contains(int x, int y) {
        return x >= 0 && x < Width && y >= 0 && y < Height;
    }
    static int target(int x, int y, bool navigation) {
        if (!contains(x, y)) return -1;
        if (navigation && y >= Height - NavigationHeight) return NAVIGATION + x * 3 / Width;
        return CONTENT;
    }
    static bool button(int target, ButtonEvent& event) {
        if (target == NAVIGATION) { event = {1, BtnAction::LONG_PRESS}; return true; }
        if (target == NAVIGATION + 1) { event = {1, BtnAction::PRESSED}; return true; }
        if (target == NAVIGATION + 2) { event = {0, BtnAction::PRESSED}; return true; }
        return false;
    }
};
}  // namespace Ui
