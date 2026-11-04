#include "ui/common/ApplicationUiContent.h"

#include <algorithm>
#include <cstdio>
#include "core/UiStrings.h"
#include "platform/api/PlatformServices.h"
#include "presentation/PixelRenderer.h"

namespace Ui::ApplicationUiContent {
namespace {
void button(int x, int y, int w, const char* label) {
    auto& c = PixelRenderer::canvas();
    c.fillRoundRect(x, y, w, 26, 4, PixelRenderer::rgb(24, 30, 40));
    c.drawRoundRect(x, y, w, 26, 4, PixelRenderer::rgb(94, 111, 124));
    PixelRenderer::text(x + (w - 32) / 2, y + 5, label, 0xffff);
}
}  // namespace

void drawNavigation(const ApplicationLayout& layout) {
    if (!layout.navigationHeight || !Platform::input().touchAvailable()) return;
    auto& c = PixelRenderer::canvas();
    c.clearClipRect();
    c.fillRect(0, (layout.height - layout.navigationHeight), layout.width, layout.navigationHeight, 0);
    const char* labels[] = {Ui::BACK, "\xe9\x80\x89\xe6\x8b\xa9", "\xe7\xa1\xae\xe8\xae\xa4"};
    for (int i = 0; i < 3; ++i) {
        const int left = i * layout.width / 3;
        const int right = (i + 1) * layout.width / 3;
        button(left + 6, (layout.height - layout.navigationHeight) + 3, right - left - 12, labels[i]);
    }
}

void drawResourceAlert(const ApplicationResourceAlertFrame& frame, const ApplicationLayout& layout) {
    if (!frame.visible()) return;
    auto& c = PixelRenderer::canvas();
    static constexpr int POP_X = 24;
    static constexpr int POP_Y = 34;
    static constexpr int POP_W = 192;
    static constexpr int POP_H = 64;
    c.fillRect(0, 0, layout.width, layout.height, 0x0000);

    uint16_t bg = PixelRenderer::rgb(18, 22, 30);
    uint16_t border = PixelRenderer::rgb(241, 242, 232);
    uint16_t warn = PixelRenderer::rgb(255, 216, 72);
    uint16_t text = PixelRenderer::rgb(245, 246, 232);

    c.fillRect(POP_X, POP_Y, POP_W, POP_H, bg);
    c.drawRect(POP_X, POP_Y, POP_W, POP_H, border);
    PixelRenderer::text(POP_X + 50, POP_Y + 8, Ui::ResourceAlert::TITLE, warn);

    int y = POP_Y + 30;
    if (frame.roomMissing) {
        PixelRenderer::text(POP_X + 18, y, Ui::ResourceAlert::ROOM_MISSING, text);
        y += 16;
    }

    if (frame.missingSpecies > 0) {
        PixelRenderer::text(POP_X + 18, y, Ui::ResourceAlert::SPRITE_MISSING, text);
        if (frame.firstMissingSpecies != 0) {
            char idBuf[12];
            std::snprintf(idBuf, sizeof(idBuf), "#%03u", frame.firstMissingSpecies);
            PixelRenderer::text(POP_X + 130, y, idBuf, warn);
        }
        y += 16;
    }

    if (frame.fontMissing &&
        !frame.roomMissing && frame.missingSpecies == 0) {
        PixelRenderer::text(POP_X + 18, y, Ui::ResourceAlert::FONT_MISSING, text);
    }
}

void drawSceneFade(const ApplicationFadeFrame& frame) {
    if (!frame.active) return;
    if (frame.hold) {
        PixelRenderer::darken(255);
        return;
    }
    uint32_t progress = std::min<uint32_t>(frame.durationMs, frame.progressMs);
    uint8_t amount = frame.out
        ? (uint8_t)(progress * 255UL / frame.durationMs)
        : (uint8_t)(255UL - progress * 255UL / frame.durationMs);
    PixelRenderer::darken(amount);
}

}  // namespace Ui::ApplicationUiContent
