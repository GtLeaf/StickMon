#pragma once

#include <cstdint>

#include "ui/UiMetrics.h"

class Canvas565;

namespace AmoledV1::UiCommon {

inline constexpr int TOAST_TOP = 282;
inline constexpr int TOAST_HEIGHT = 52;
inline constexpr int TOAST_BOTTOM = TOAST_TOP + TOAST_HEIGHT;

// A top-level page owns clipping for its render call. Local clips are always
// intersected with the requested rows; returning from the page clears both
// the supplied canvas and the shared asset canvas, including early returns.
class PageClip {
public:
    PageClip(Canvas565& canvas, uint16_t rowBegin, uint16_t rowEnd);
    ~PageClip();
    PageClip(const PageClip&) = delete;
    PageClip& operator=(const PageClip&) = delete;

    void setRect(int x, int y, int width, int height);
    void reset();

private:
    Canvas565& canvas_;
    Canvas565& assets_;
    int top_;
    int bottom_;
};

uint16_t rgb(uint8_t red, uint8_t green, uint8_t blue);
int textWidth(const char* value);
void text(Canvas565& canvas, int x, int y, const char* value,
          uint16_t color, int scale = 1);
void drawSceneFadeOverlay(Canvas565& canvas, uint8_t alpha,
                          uint16_t rowBegin, uint16_t rowEnd);
void drawHeaderButton(Canvas565& canvas, int x, bool pressed = false,
                      int height = UiMetrics::HOME_HEADER_HEIGHT);
void drawBackIcon(Canvas565& canvas, int height = UiMetrics::HOME_HEADER_HEIGHT);
void drawHeader(Canvas565& canvas, int height, bool backButton = false);
void drawHeaderText(Canvas565& canvas, int x, const char* value,
                    uint16_t color, int height);
bool pageHeaderBackAt(int x, int y);
void drawPageHeader(Canvas565& canvas, const char* title,
                    const char* trailing = nullptr,
                    bool backButton = true);
void drawPageHeaderCenteredText(Canvas565& canvas, const char* value,
                                uint16_t color);
void drawToast(Canvas565& canvas, const char* value);

}  // namespace AmoledV1::UiCommon
