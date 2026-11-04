#include "presentation/HudRenderer.h"

#include "assets/HudAssets.h"
#include "platform/api/FlashStorage.h"
#include "presentation/Canvas565.h"
#include "presentation/PixelRenderer.h"

namespace HudRenderer {

void drawBattery(Canvas565& canvas, int x, int y, int percent,
                 uint8_t pixelScale, bool charging) {
    if (!pixelScale) return;
    const int s = pixelScale;
    const uint16_t outline = PixelRenderer::rgb(222, 234, 229);
    canvas.drawRoundRect(x, y, 16 * s, 8 * s, 2 * s, outline);
    canvas.fillRect(x + 16 * s, y + 2 * s, 2 * s, 4 * s, outline);
    if (percent < 0 || percent > 100) {
        canvas.fillRect(x + 6 * s, y + 7 * s / 2, 4 * s, s,
            PixelRenderer::rgb(142, 157, 155));
    } else {
        const int fillWidth = percent * (12 * s) / 100;
        canvas.fillRect(x + 2 * s, y + 2 * s, fillWidth, 4 * s,
            percent < 20 ? PixelRenderer::rgb(238, 91, 91) : PixelRenderer::rgb(92, 213, 139));
    }
    if (charging) {
        // A dark inset keeps the lightning legible over every battery level.
        canvas.fillRect(x + 5 * s, y + s, 6 * s, 6 * s,
                        PixelRenderer::rgb(20, 32, 30));
        const uint16_t bolt = PixelRenderer::rgb(255, 220, 88);
        constexpr uint8_t rows[] = {0x06, 0x0C, 0x1F, 0x06, 0x0C, 0x08};
        for (int row = 0; row < 6; ++row) {
            for (int column = 0; column < 5; ++column) {
                if (rows[row] & (1U << (4 - column))) {
                    canvas.fillRect(x + (6 + column) * s, y + (1 + row) * s,
                                    s, s, bolt);
                }
            }
        }
    }
}

void drawHungerIcon(Canvas565& canvas, int x, int y, uint8_t hunger,
                    uint8_t pixelScale) {
    if (pixelScale == 0) return;
    uint8_t visibleRows = static_cast<uint8_t>(
        (static_cast<uint16_t>(HudAssets::HUNGER_ICON_H) * hunger + 99) /
        100);
    if (visibleRows > HudAssets::HUNGER_ICON_H) {
        visibleRows = HudAssets::HUNGER_ICON_H;
    }
    uint8_t hiddenRows = HudAssets::HUNGER_ICON_H - visibleRows;
    const uint16_t emptyColor = 0x4229;
    static constexpr int8_t CUT_JITTER[] = {
        -1, 0, 1, 0, 2, 1, 0, -1, 1, 0, 2, 0, -1, 1,
    };

    const uint32_t total = static_cast<uint32_t>(HudAssets::HUNGER_ICON_W) *
                           HudAssets::HUNGER_ICON_H;
    uint32_t index = 0;
    uint32_t pixel = 0;
    while (index < HudAssets::HUNGER_ICON_RLE_LEN && pixel < total) {
        uint16_t token = FlashStorage::readWord(
            &HudAssets::HUNGER_ICON_RLE[index++]);
        uint16_t run = token & 0x7FFFU;
        if (run == 0) continue;
        if ((token & 0x8000U) != 0) {
            pixel += run;
            if (pixel > total) pixel = total;
            continue;
        }

        for (uint16_t offset = 0;
             offset < run && index < HudAssets::HUNGER_ICON_RLE_LEN &&
             pixel < total;
             ++offset, ++pixel) {
            uint16_t color = FlashStorage::readWord(
                &HudAssets::HUNGER_ICON_RLE[index++]);
            uint8_t row = static_cast<uint8_t>(
                pixel / HudAssets::HUNGER_ICON_W);
            uint8_t column = static_cast<uint8_t>(
                pixel % HudAssets::HUNGER_ICON_W);
            int cutRow = hiddenRows == 0
                ? 0 : static_cast<int>(hiddenRows) + CUT_JITTER[column];
            if (cutRow < 0) cutRow = 0;
            if (cutRow > HudAssets::HUNGER_ICON_H) {
                cutRow = HudAssets::HUNGER_ICON_H;
            }
            canvas.fillAssetRect(x + column * pixelScale,
                                 y + row * pixelScale,
                                 pixelScale, pixelScale,
                                 row < cutRow ? emptyColor : color);
        }
    }
}

}  // namespace HudRenderer
