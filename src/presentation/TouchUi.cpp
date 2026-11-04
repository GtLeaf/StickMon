#include "presentation/TouchUi.h"
#include <algorithm>
#include "presentation/PixelRenderer.h"
#include "core/UiStrings.h"

namespace TouchUi {
namespace {
int labelWidth(const char* label) {
    int width = 0;
    for (const auto* p = reinterpret_cast<const uint8_t*>(label); p && *p; ++p)
        if ((*p & 0xc0) != 0x80) width += *p < 0x80 ? 8 : 16;
    return width;
}
}
bool enabled() { return NAV_H && Platform::input().touchAvailable(); }
MenuGrid::Layout listLayout(int rowHeight) {
    return MenuGrid::fit({0, PAGE_HEADER_H, Ui::Page::WIDTH, Ui::Page::HEIGHT - PAGE_HEADER_H},
        Ui::Page::WIDTH - 16, rowHeight, 0, 6, 8, 4);
}
void pageHeader(const char* title, bool pressed, bool selected) {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Ui::Page::WIDTH, PAGE_HEADER_H, 0);
    c.fillCircle(22, 19, 16, PixelRenderer::rgb(pressed ? 53 : 27, pressed ? 76 : 43, pressed ? 83 : 51));
    c.drawCircle(22, 19, 16, selected ? 0xffe0 : PixelRenderer::rgb(67, 97, 101));
    for (int offset = -1; offset <= 1; ++offset) {
        c.drawLine(24 + offset, 12, 17 + offset, 19, 0xffff);
        c.drawLine(17 + offset, 19, 24 + offset, 26, 0xffff);
    }
    PixelRenderer::text(Ui::Page::WIDTH - 12 - labelWidth(title), 11, title, 0xffff);
}
bool pageBackHit(int x, int y) { return x >= 0 && x < 46 && y >= 0 && y < PAGE_HEADER_H; }
MenuGrid::Rect safeHeaderBounds() { return {Ui::Page::HEADER_INSET, SAFE_HEADER_TOP, Ui::Page::WIDTH - 2 * Ui::Page::HEADER_INSET, 40}; }
MenuGrid::Rect pageBody() { return {0, SAFE_HEADER_H, Ui::Page::WIDTH, Ui::Page::HEIGHT - SAFE_HEADER_H - 16}; }
MenuGrid::Rect safeBack() { return {Ui::Page::HEADER_INSET, SAFE_HEADER_TOP, 40, 40}; }
void safePageHeader(const char* title, bool pressed, bool selected, const char* center) {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Ui::Page::WIDTH, SAFE_HEADER_H, 0);
    const auto back = safeBack();
    const int cx = back.x + 20, cy = back.y + 20;
    c.fillCircle(cx, cy, 16, PixelRenderer::rgb(pressed ? 53 : 27, pressed ? 76 : 43, pressed ? 83 : 51));
    c.drawCircle(cx, cy, 16, selected ? 0xffe0 : PixelRenderer::rgb(67, 97, 101));
    for (int offset = -1; offset <= 1; ++offset) {
        c.drawLine(cx + 2 + offset, cy - 7, cx - 5 + offset, cy, 0xffff);
        c.drawLine(cx - 5 + offset, cy, cx + 2 + offset, cy + 7, 0xffff);
    }
    const auto bounds = safeHeaderBounds();
    const int left = back.x + back.width + 8;
    const int right = bounds.x + bounds.width;
    c.setClipRect(left, bounds.y, right - left, bounds.height);
    PixelRenderer::text(std::max(left, right - labelWidth(title)), bounds.y + 12, title, 0xffff);
    c.clearClipRect();
    if (center) {
        const int end = right - labelWidth(title) - 8;
        const int width = end - left;
        c.setClipRect(left, bounds.y, width, bounds.height);
        PixelRenderer::text(std::max(left, std::min(Ui::Page::WIDTH / 2 - labelWidth(center) / 2,
            end - labelWidth(center))), bounds.y + 12, center, PixelRenderer::rgb(248, 210, 105));
        c.clearClipRect();
    }
}
void slider(int left, int right, int centerY, int value, int minimum, int maximum, bool pressed) {
    auto& c = PixelRenderer::canvas();
    const int knob = left + (std::clamp(value, minimum, maximum) - minimum) * (right - left) / std::max(1, maximum - minimum);
    const auto active = PixelRenderer::rgb(pressed ? 115 : 83, pressed ? 226 : 184, pressed ? 183 : 157);
    c.fillRoundRect(left, centerY - 4, right - left, 8, 4, PixelRenderer::rgb(48, 63, 70));
    if (knob > left) c.fillRoundRect(left, centerY - 4, knob - left, 8, 4, active);
    c.fillCircle(knob, centerY, pressed ? 9 : 8, 0xffff);
    c.fillCircle(knob, centerY, pressed ? 5 : 4, active);
}
void action(int x, int y, int width, const char* label, bool pressed) {
    auto& c = PixelRenderer::canvas();
    c.fillRoundRect(x, y, width, 34, 4, PixelRenderer::rgb(pressed ? 42 : 24, pressed ? 61 : 34, pressed ? 68 : 42));
    c.drawRoundRect(x, y, width, 34, 4, PixelRenderer::rgb(pressed ? 115 : 94, pressed ? 226 : 111, pressed ? 183 : 124));
    PixelRenderer::text(x + (width - labelWidth(label)) / 2, y + 9, label, 0xffff);
}
void header(const char* title, const char* trailing, bool pressed) {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Ui::Page::WIDTH, 38, 0);
    action(6, 2, 48, Ui::BACK, pressed);
    PixelRenderer::text(64, 10, title, 0xffff);
    if (trailing) PixelRenderer::text(Ui::Page::WIDTH - 12 - labelWidth(trailing), 10, trailing, 0xffe0);
}
bool backHit(int x, int y) { return x >= 4 && x < 58 && y >= 0 && y < 38; }
namespace {
void button(int x, int y, int w, const char* label) {
    auto& c = PixelRenderer::canvas();
    c.fillRoundRect(x, y, w, 26, 4, PixelRenderer::rgb(24, 30, 40));
    c.drawRoundRect(x, y, w, 26, 4, PixelRenderer::rgb(94, 111, 124));
    PixelRenderer::text(x + (w - 32) / 2, y + 5, label, 0xffff);
}
}
void drawHomeMenu() {
    if (!NAV_H || !Platform::input().touchAvailable()) return;
    button(Hal::DISPLAY_W - 70, Hal::DISPLAY_H - 32, 58, "\xe8\x8f\x9c\xe5\x8d\x95");
}
bool homeMenuHit(int x, int y) {
    return NAV_H && x >= Hal::DISPLAY_W - 76 && x < Hal::DISPLAY_W - 6 &&
        y >= Hal::DISPLAY_H - 36 && y < Hal::DISPLAY_H - 2;
}
}  // namespace TouchUi
