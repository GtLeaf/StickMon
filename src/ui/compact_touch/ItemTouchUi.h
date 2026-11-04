#pragma once

#include "presentation/MenuGrid.h"

namespace ItemTouchUi {
// Section order is visual only; targets retain the scene's inventory indices.
struct Grid {
    MenuGrid::Layout layout;
    int firstCount, secondCount;
    // Extra scroll room without clipping the visible viewport.
    int scrollBottomInset = 0;
    int targets[64];
    Grid(MenuGrid::Rect viewport, int first, int second = -1);
    int count() const;
    int sectionTop(int section) const;
    int contentHeight() const;
    float maxScroll() const;
    MenuGrid::Rect cell(int index, float scroll = 0) const;
    MenuGrid::Rect targetCell(int target, float scroll = 0) const;
    int itemAt(int x, int y, float scroll) const;
    float reveal(int target, float scroll) const;
    void section(int index, float scroll, const char* label) const;
};
MenuGrid::Rect body(bool shop = false);
MenuGrid::Rect rail(int index);
MenuGrid::Rect action();
MenuGrid::Rect back();
void detailButton(MenuGrid::Rect rect, const char* label, bool pressed, bool cancel = false);
}  // namespace ItemTouchUi
