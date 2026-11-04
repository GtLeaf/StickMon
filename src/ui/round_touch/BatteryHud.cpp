// SPDX-License-Identifier: Apache-2.0
#include "ui/round_touch/BatteryHud.h"

#include <cstdio>
#include <cstring>
#include "hardware/Hal.h"
#include "presentation/HudRenderer.h"
#include "presentation/PixelRenderer.h"

namespace Ui::RoundBattery {
void draw(int centerX, int y) {
    const int level = Hal::ins().batteryLevel();
    const bool low = level >= 0 && level < 20;
    const uint16_t color = low ? PixelRenderer::rgb(255, 112, 96) : 0xffff;
    char text[32];
    if (level < 0) std::snprintf(text, sizeof(text), "--%%");
    else std::snprintf(text, sizeof(text), "%d%%", level);
    const int width = int(std::strlen(text)) * 8 + 26 + (low ? 54 : 0);
    const int x = centerX - width / 2;
    HudRenderer::drawBattery(PixelRenderer::canvas(), x, y + 4, level, 1,
        Platform::power().batteryCharging());
    PixelRenderer::text(x + 26, y, text, color);
    if (low) PixelRenderer::text(x + width - 48, y, "低电量", color);
}
}  // namespace Ui::RoundBattery
