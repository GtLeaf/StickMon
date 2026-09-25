#include "ui/UiCommon.h"

#include <algorithm>

#include "AmoledGeometry.h"
#include "core/FontResource.h"
#include "presentation/PixelRenderer.h"
#include "ui/UiMetrics.h"

namespace AmoledV1::UiCommon {

PageClip::PageClip(Canvas565& canvas, uint16_t rowBegin, uint16_t rowEnd)
    : canvas_(canvas), assets_(PixelRenderer::canvas()),
      top_(std::min<int>(rowBegin, AmoledUi::HEIGHT)),
      bottom_(std::max(top_, std::min<int>(rowEnd, AmoledUi::HEIGHT))) {
    reset();
}

PageClip::~PageClip() {
    canvas_.clearClipRect();
    if (&assets_ != &canvas_) assets_.clearClipRect();
}

void PageClip::setRect(int x, int y, int width, int height) {
    const int left = std::clamp(x, 0, AmoledUi::WIDTH);
    const int right = std::clamp(x + std::max(0, width), left, AmoledUi::WIDTH);
    const int top = std::clamp(y, top_, bottom_);
    const int bottom = std::clamp(y + std::max(0, height), top, bottom_);
    canvas_.setClipRect(left, top, right - left, bottom - top);
    if (&assets_ != &canvas_) {
        assets_.setClipRect(left, top, right - left, bottom - top);
    }
}

void PageClip::reset() {
    setRect(0, top_, AmoledUi::WIDTH, bottom_ - top_);
}

uint16_t rgb(uint8_t red, uint8_t green, uint8_t blue) {
    return static_cast<uint16_t>(((red & 0xF8U) << 8) |
                                 ((green & 0xFCU) << 3) | (blue >> 3));
}

int textWidth(const char* value) {
    if (!value) return 0;
    int width = 0;
    const uint8_t* it = reinterpret_cast<const uint8_t*>(value);
    while (*it) {
        if (*it < 0x80) {
            width += 16;
            ++it;
            continue;
        }
        width += 32;
        ++it;
        while (*it && (*it & 0xC0) == 0x80) ++it;
    }
    return width;
}

void text(Canvas565& canvas, int x, int y, const char* value,
          uint16_t color, int scale) {
    if (!value || scale <= 0) return;
    PixelRenderer::text(canvas, x, y, value, color, scale);
}

void drawSceneFadeOverlay(Canvas565&, uint8_t alpha, uint16_t rowBegin,
                          uint16_t rowEnd) {
    if (alpha == 0) return;
    PixelRenderer::fillRectAlpha(0, rowBegin, AmoledUi::WIDTH,
                                 rowEnd - rowBegin, 0, alpha);
}

void drawHeaderButton(Canvas565& canvas, int x, bool pressed, int height) {
    constexpr int buttonWidth = 52;
    canvas.fillRoundRect(x + 4, 4, buttonWidth - 8,
                         height - 8, 8,
                         pressed ? rgb(53, 76, 83) : rgb(27, 43, 51));
    canvas.drawRoundRect(x + 4, 4, buttonWidth - 8,
                         height - 8, 8,
                         rgb(67, 97, 101));
}

void drawBackIcon(Canvas565& canvas, int height) {
    drawHeaderButton(canvas, 0, false, height);
    const uint16_t color = rgb(222, 234, 229);
    const int centerY = height / 2;
    canvas.drawLine(32, centerY - 10, 18, centerY, color);
    canvas.drawLine(18, centerY, 32, centerY + 10, color);
}

void drawHeader(Canvas565& canvas, int height, bool backButton) {
    canvas.fillRect(0, 0, AmoledUi::WIDTH, height, rgb(19, 31, 39));
    canvas.fillRect(0, height - 2, AmoledUi::WIDTH, 2, rgb(56, 87, 89));
    if (backButton) drawBackIcon(canvas, height);
}

void drawHeaderText(Canvas565& canvas, int x, const char* value,
                    uint16_t color, int height) {
    text(canvas, x, (height - FontResource::LARGE_GLYPH_H) / 2, value, color);
}

bool pageHeaderBackAt(int x, int y) {
    return x >= 0 && x < UiMetrics::PAGE_HEADER_BACK_HIT_WIDTH &&
           y >= 0 && y < UiMetrics::PAGE_HEADER_HEIGHT;
}

void drawPageHeader(Canvas565& canvas, const char* title,
                    const char* trailing, bool backButton) {
    constexpr int backCenterX = 40;
    constexpr int rightPadding = 28;
    const int centerY = UiMetrics::PAGE_HEADER_HEIGHT / 2;
    const int textY =
        (UiMetrics::PAGE_HEADER_HEIGHT - FontResource::LARGE_GLYPH_H) / 2;

    canvas.fillRect(0, 0, AmoledUi::WIDTH, UiMetrics::PAGE_HEADER_HEIGHT,
                    rgb(0, 0, 0));
    if (backButton) {
        canvas.fillCircle(backCenterX, centerY, 26, rgb(27, 43, 51));
        canvas.drawCircle(backCenterX, centerY, 26, rgb(67, 97, 101));
        const uint16_t backColor = rgb(235, 183, 239);
        canvas.drawLine(47, centerY - 14, 29, centerY, backColor);
        canvas.drawLine(29, centerY, 47, centerY + 14, backColor);
    }

    const int right = AmoledUi::WIDTH - rightPadding;
    if (trailing && *trailing) {
        text(canvas, backButton ? 96 : rightPadding, textY, title,
             rgb(102, 176, 245));
        text(canvas, right - textWidth(trailing), textY, trailing,
             rgb(248, 210, 105));
    } else if (title && *title) {
        text(canvas, right - textWidth(title), textY, title,
             rgb(102, 176, 245));
    }
}

void drawPageHeaderCenteredText(Canvas565& canvas, const char* value,
                                uint16_t color) {
    if (!value || !*value) return;
    text(canvas, (AmoledUi::WIDTH - textWidth(value)) / 2,
         (UiMetrics::PAGE_HEADER_HEIGHT - FontResource::LARGE_GLYPH_H) / 2,
         value, color);
}

void drawToast(Canvas565& canvas, const char* value) {
    if (!value || !*value) return;
    int width = textWidth(value) + 28;
    width = std::min(width, AmoledUi::WIDTH - 16);
    int x = (AmoledUi::WIDTH - width) / 2;
    canvas.fillRoundRect(x, TOAST_TOP, width, TOAST_HEIGHT, 8,
                         rgb(20, 31, 38));
    canvas.drawRoundRect(x, TOAST_TOP, width, TOAST_HEIGHT, 8,
                         rgb(92, 139, 137));
    text(canvas, x + 14,
         TOAST_TOP + (TOAST_HEIGHT - FontResource::LARGE_GLYPH_H) / 2,
         value, rgb(234, 240, 235));
}

}  // namespace AmoledV1::UiCommon
