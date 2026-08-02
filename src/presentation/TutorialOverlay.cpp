#include "presentation/TutorialOverlay.h"

#include "core/MathUtil.h"
#include "hardware/Hal.h"
#include "presentation/PixelRenderer.h"

namespace TutorialOverlay {

void draw(Button button, const char* text, bool longPress, int y) {
    if (!text || !text[0]) return;

    auto& canvas = PixelRenderer::canvas();
    static constexpr int PANEL_X = 4;
    static constexpr int PANEL_H = 27;
    static constexpr int PANEL_W = Hal::DISPLAY_W - PANEL_X * 2;
    y = MathUtil::clamp(y, 0, Hal::DISPLAY_H - PANEL_H - 2);

    PixelRenderer::fillRectAlpha(
        PANEL_X, y, PANEL_W, PANEL_H,
        PixelRenderer::rgb(10, 14, 20), 218);
    canvas.drawRect(PANEL_X, y, PANEL_W, PANEL_H,
                    PixelRenderer::rgb(94, 111, 124));

    const int keyX = PANEL_X + 15;
    const int keyY = y + PANEL_H / 2;
    const uint16_t keyColor = PixelRenderer::rgb(255, 216, 72);
    canvas.fillCircle(keyX, keyY, 9, keyColor);
    if (longPress) {
        canvas.drawCircle(keyX, keyY, 11,
                          PixelRenderer::rgb(135, 214, 238));
    }
    PixelRenderer::text(keyX - 4, keyY - 8,
                        button == Button::A ? "A" : "B",
                        PixelRenderer::rgb(24, 28, 34), 1);
    PixelRenderer::text(PANEL_X + 31, y + 5, text,
                        PixelRenderer::rgb(241, 242, 232), 1);
}

} // namespace TutorialOverlay
