#include "ui/UiCommon.h"

#include <algorithm>
#include <cstdio>

#include "AmoledGeometry.h"
#include "core/FontResource.h"
#include "presentation/PixelRenderer.h"
#include "ui/UiMetrics.h"

namespace AmoledV1::UiCommon {

PageClip::PageClip(Canvas565& canvas, uint16_t rowBegin, uint16_t rowEnd,
                   uint16_t columnBegin, uint16_t columnEnd)
    : canvas_(canvas), assets_(PixelRenderer::canvas()),
      left_(std::min<int>(columnBegin, AmoledUi::WIDTH)),
      right_(std::max(left_, std::min<int>(columnEnd, AmoledUi::WIDTH))),
      top_(std::min<int>(rowBegin, AmoledUi::HEIGHT)),
      bottom_(std::max(top_, std::min<int>(rowEnd, AmoledUi::HEIGHT))) {
    reset();
}

PageClip::~PageClip() {
    canvas_.clearClipRect();
    if (&assets_ != &canvas_) assets_.clearClipRect();
}

void PageClip::setRect(int x, int y, int width, int height) {
    const int left = std::clamp(x, left_, right_);
    const int right = std::clamp(x + std::max(0, width), left, right_);
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

void drawFallbackPet(Canvas565& canvas, int centerX, int groundY,
                     bool drawShadow) {
    const int dx = centerX - 184;
    const int dy = groundY - 302;
    const uint16_t shadow = rgb(39, 57, 63);
    const uint16_t body = rgb(79, 185, 140);
    const uint16_t bodyLight = rgb(126, 218, 166);
    const uint16_t belly = rgb(224, 238, 194);
    const uint16_t dark = rgb(22, 38, 44);
    const uint16_t cheek = rgb(242, 112, 103);

    if (drawShadow) {
        canvas.fillEllipse((184 + dx), (298 + dy), (62), (16), shadow);
    }
    canvas.fillTriangle((138 + dx), (188 + dy), (158 + dx), (148 + dy), (172 + dx), (198 + dy), body);
    canvas.fillTriangle((196 + dx), (198 + dy), (212 + dx), (148 + dy), (232 + dx), (190 + dy), body);
    canvas.fillEllipse((184 + dx), (232 + dy), (60), (70), body);
    canvas.fillEllipse((184 + dx), (256 + dy), (40), (42), belly);
    canvas.fillEllipse((168 + dx), (208 + dy), (10), (14), dark);
    canvas.fillEllipse((202 + dx), (208 + dy), (10), (14), dark);
    canvas.fillRect((166 + dx), (204 + dy), (4), (4), bodyLight);
    canvas.fillRect((200 + dx), (204 + dy), (4), (4), bodyLight);
    canvas.fillEllipse((150 + dx), (230 + dy), (12), (8), cheek);
    canvas.fillEllipse((218 + dx), (230 + dy), (12), (8), cheek);
    canvas.drawFastHLine((176 + dx), (232 + dy), (18), dark);
    canvas.drawPixel((174 + dx), (230 + dy), dark);
    canvas.drawPixel((194 + dx), (230 + dy), dark);
    canvas.fillRoundRect((134 + dx), (264 + dy), (32), (20), (8), body);
    canvas.fillRoundRect((204 + dx), (264 + dy), (32), (20), (8), body);
}

namespace {

uint8_t shadowRgb565R(uint16_t color) {
    return static_cast<uint8_t>(((color >> 11) & 0x1F) * 255 / 31);
}

uint8_t shadowRgb565G(uint16_t color) {
    return static_cast<uint8_t>(((color >> 5) & 0x3F) * 255 / 63);
}

uint8_t shadowRgb565B(uint16_t color) {
    return static_cast<uint8_t>((color & 0x1F) * 255 / 31);
}

}  // namespace

uint16_t blendShadowRgb565(uint16_t background, uint16_t color,
                           uint8_t alpha) {
    uint8_t inverse = static_cast<uint8_t>(255 - alpha);
    return rgb(
        static_cast<uint8_t>((shadowRgb565R(color) * alpha +
                              shadowRgb565R(background) * inverse) / 255),
        static_cast<uint8_t>((shadowRgb565G(color) * alpha +
                              shadowRgb565G(background) * inverse) / 255),
        static_cast<uint8_t>((shadowRgb565B(color) * alpha +
                              shadowRgb565B(background) * inverse) / 255));
}

void fillSoftShadow(Canvas565& canvas, int centerX, int centerY,
                    int radiusX, int radiusY, uint16_t color,
                    uint8_t maxAlpha) {
    if (radiusX <= 0 || radiusY <= 0 || maxAlpha == 0) return;
    for (int py = centerY - radiusY; py <= centerY + radiusY; ++py) {
        float dy = static_cast<float>(py - centerY) / radiusY;
        for (int px = centerX - radiusX; px <= centerX + radiusX; ++px) {
            float dx = static_cast<float>(px - centerX) / radiusX;
            float distanceSq = dx * dx + dy * dy;
            if (distanceSq > 1.0f) continue;
            uint8_t alpha = static_cast<uint8_t>(
                maxAlpha * (1.0f - distanceSq));
            if (alpha == 0) continue;
            uint16_t background = canvas.readPixel((px), (py));
            canvas.drawPixel((px), (py), blendShadowRgb565(background, color, alpha));
        }
    }
}

void fillShadowCore(Canvas565& canvas, int centerX, int centerY,
                    int radiusX, int radiusY, uint16_t color,
                    uint8_t alpha) {
    if (radiusX <= 0 || radiusY <= 0 || alpha == 0) return;
    for (int py = centerY - radiusY; py <= centerY + radiusY; ++py) {
        float dy = static_cast<float>(py - centerY) / radiusY;
        for (int px = centerX - radiusX; px <= centerX + radiusX; ++px) {
            float dx = static_cast<float>(px - centerX) / radiusX;
            if (dx * dx + dy * dy > 1.0f) continue;
            uint16_t background = canvas.readPixel((px), (py));
            canvas.drawPixel((px), (py), blendShadowRgb565(background, color, alpha));
        }
    }
}

void drawHomeHpBar(Canvas565& canvas, int x, int y, int width,
                   uint8_t percent, bool fainted) {
    canvas.fillRect((x), (y), (width), (12), rgb(39, 45, 50));
    int filled = percent > 0
        ? std::max(2, (width - 4) * percent / 100) : 0;
    uint16_t fillColor = fainted ? rgb(156, 174, 181)
        : percent > 50 ? rgb(92, 222, 112)
        : (percent > 20 ? rgb(246, 204, 72) : rgb(232, 80, 84));
    if (filled > 0) canvas.fillRect((x + 2), (y + 2), (filled), (8), fillColor);
    canvas.drawRect((x), (y), (width), (12), rgb(220, 224, 218));
}

void drawHomeHudButton(Canvas565& canvas, int x) {
    canvas.fillRoundRect((x), (UiMetrics::HOME_HUD_BUTTON_Y), (UiMetrics::HOME_HUD_BUTTON_SIZE), (UiMetrics::HOME_HUD_BUTTON_SIZE), (8), rgb(27, 43, 51));
    canvas.drawRoundRect((x), (UiMetrics::HOME_HUD_BUTTON_Y), (UiMetrics::HOME_HUD_BUTTON_SIZE), (UiMetrics::HOME_HUD_BUTTON_SIZE), (8), rgb(67, 97, 101));
}

void drawHudMenuIcon(Canvas565& canvas, int x) {
    drawHomeHudButton(canvas, x);
    const uint16_t color = rgb(222, 234, 229);
    for (int row = 0; row < 3; ++row) {
        canvas.fillRoundRect(x + 16, UiMetrics::HOME_HUD_BUTTON_Y + 20 + row * 10,
                             32, 4, 2, color);
    }
}

void drawHomeMenuIcon(Canvas565& canvas) {
    drawHudMenuIcon(canvas, UiMetrics::HOME_MENU_BUTTON_X);
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

void drawBackChevron(Canvas565& canvas, int x, int centerY, int halfHeight,
                     uint16_t color) {
    // Match the reference SVG's 45-degree arms, bevelled ends and 69:257
    // inner-tip inset / half-height ratio, rounded to native pixels.
    if (halfHeight <= 0) return;
    const int inset = std::max(1, (halfHeight * 69 + 128) / 257);
    for (int dy = -halfHeight; dy <= halfHeight; ++dy) {
        const int distance = dy < 0 ? -dy : dy;
        const int right = std::min(distance + inset, 2 * halfHeight - distance);
        canvas.drawFastHLine(x + distance, centerY + dy,
                             right - distance + 1, color);
    }
}

void drawBackIcon(Canvas565& canvas, int height) {
    drawHeaderButton(canvas, 0, false, height);
    const uint16_t color = rgb(255, 255, 255);
    const int centerY = height / 2;
    drawBackChevron(canvas, 18, centerY, 12, color);
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
        const uint16_t backColor = rgb(255, 255, 255);
        drawBackChevron(canvas, 29, centerY, 16, backColor);
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

void drawCoins(Canvas565& canvas, uint32_t value, int anchorX, int y, bool centered, bool backdrop) {
    char label[16];
    std::snprintf(label, sizeof(label), "C%lu", static_cast<unsigned long>(value));
    const int width = textWidth(label);
    const int x = anchorX - (centered ? width / 2 : width);
    if (backdrop) canvas.fillRoundRect(x - 8, y - 4, width + 16, 40, 6, rgb(248, 248, 224));
    text(canvas, x, y, label, backdrop ? rgb(40, 56, 80) : rgb(248, 210, 105));
}

}  // namespace AmoledV1::UiCommon
