#include "ui/compact_touch/ItemTouchUi.h"

#include "ui/PageProfile.h"

#include <algorithm>
#include <cmath>
#include "presentation/PixelRenderer.h"
#include "presentation/TouchUi.h"

namespace ItemTouchUi {
namespace {
constexpr int SECTION_H = 28;
constexpr int SECTION_GAP = 8;
}
Grid::Grid(MenuGrid::Rect viewport, int first, int second)
    : layout(MenuGrid::fit(viewport, 82, 64, 8, 6, 8, SECTION_H)),
      firstCount(first), secondCount(second) {
    for (int i = 0; i < 64; ++i) targets[i] = i;
}
int Grid::count() const { return firstCount + std::max(0, secondCount); }
int Grid::sectionTop(int section) const {
    return section == 0 ? 0 : SECTION_H +
        ((firstCount + layout.columns - 1) / layout.columns) * (layout.cellHeight + layout.gapY) + SECTION_GAP;
}
int Grid::contentHeight() const {
    const int last = secondCount < 0 ? 0 : 1;
    const int n = last ? secondCount : firstCount;
    return sectionTop(last) + SECTION_H +
        ((n + layout.columns - 1) / layout.columns) * (layout.cellHeight + layout.gapY) + 8;
}
float Grid::maxScroll() const { return float(std::max(0, contentHeight() + scrollBottomInset - layout.viewport.height)); }
MenuGrid::Rect Grid::cell(int index, float scroll) const {
    const bool second = index >= firstCount;
    auto rect = layout.cell(second ? index - firstCount : index, scroll);
    rect.y += sectionTop(second ? 1 : 0);
    return rect;
}
MenuGrid::Rect Grid::targetCell(int target, float scroll) const {
    for (int i = 0; i < count(); ++i) if (targets[i] == target) return cell(i, scroll);
    return {0, 0, 0, 0};
}
int Grid::itemAt(int x, int y, float scroll) const {
    if (!layout.viewport.contains(x, y)) return -1;
    for (int i = 0; i < count(); ++i) if (cell(i, scroll).contains(x, y)) return targets[i];
    return -1;
}
float Grid::reveal(int target, float scroll) const {
    const auto rect = targetCell(target, scroll);
    if (rect.height == 0) return std::clamp(scroll, 0.0f, maxScroll());
    if (rect.y < layout.viewport.y) scroll -= layout.viewport.y - rect.y;
    const int bottom = layout.viewport.y + layout.viewport.height - scrollBottomInset;
    if (rect.y + rect.height > bottom)
        scroll += rect.y + rect.height - bottom;
    return std::clamp(scroll, 0.0f, maxScroll());
}
void Grid::section(int index, float scroll, const char* label) const {
    const auto& v = layout.viewport;
    const int y = v.y + sectionTop(index) - int(std::lround(scroll));
    if (y + SECTION_H <= v.y || y >= v.y + v.height) return;
    auto& c = PixelRenderer::canvas();
    c.fillRect(v.x + 8, y, v.width - 16, SECTION_H - 4, PixelRenderer::rgb(20, 31, 38));
    PixelRenderer::text(v.x + 12, y + 3, label, PixelRenderer::rgb(126, 175, 175));
    c.drawFastHLine(v.x + 12, y + SECTION_H - 4, v.width - 24, PixelRenderer::rgb(56, 87, 89));
}
MenuGrid::Rect body(bool shop) {
    const int left = shop ? 64 : 0;
    return {left, TouchUi::SAFE_HEADER_H, Ui::Page::WIDTH - left, Ui::Page::HEIGHT - TouchUi::SAFE_HEADER_H};
}
MenuGrid::Rect rail(int index) { return {8, TouchUi::SAFE_HEADER_H + 12 + index * 72, 48, 60}; }
MenuGrid::Rect action() { return {28, Ui::Page::HEIGHT - 48, Ui::Page::WIDTH / 2 - 36, 36}; }
MenuGrid::Rect back() { return {Ui::Page::WIDTH / 2 + 8, Ui::Page::HEIGHT - 48, Ui::Page::WIDTH / 2 - 36, 36}; }
void detailButton(MenuGrid::Rect rect, const char* label, bool pressed, bool cancel) {
    auto& c = PixelRenderer::canvas();
    const auto fill = cancel ? PixelRenderer::rgb(pressed ? 91 : 46, pressed ? 49 : 37, pressed ? 55 : 44)
        : PixelRenderer::rgb(pressed ? 48 : 36, pressed ? 74 : 54, pressed ? 68 : 61);
    const auto color = cancel ? PixelRenderer::rgb(239, 143, 148) : PixelRenderer::rgb(115, 226, 183);
    c.fillRoundRect(rect.x, rect.y, rect.width, rect.height, 6, fill);
    MenuGrid::label(c, rect.x + rect.width / 2, rect.y + 10, label, rect.width - 8, color);
}
}  // namespace ItemTouchUi
