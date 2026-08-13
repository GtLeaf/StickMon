#include "presentation/HudRenderer.h"

#include "assets/HudAssets.h"
#include "platform/api/FlashStorage.h"
#include "presentation/Canvas565.h"

namespace HudRenderer {

void drawHungerIcon(Canvas565& canvas, int x, int y, uint8_t hunger) {
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
            canvas.drawPixel(x + column, y + row,
                             row < cutRow ? emptyColor : color);
        }
    }
}

}  // namespace HudRenderer
