// SPDX-License-Identifier: Apache-2.0
#include "ui/round_touch/HatchUi.h"

#include <cstdio>
#include "core/UiStrings.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TouchUi.h"
#include "ui/common/HatchUiContent.h"
#include "ui/round_touch/RoomControls.h"
#include "ui/round_touch/BatteryHud.h"

namespace Ui {
namespace {
constexpr MenuGrid::Rect WIPE{90, 268, 80, 34}, POKE{190, 268, 80, 34};
}
void HatchUi::render(const HatchFrame& frame) {
    HatchUiContent::draw(frame, layout(), nullptr);
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, 360, 99, 0);
    c.fillRect(0, 260, 360, 100, 0);
    c.fillRect(0, 99, 60, 161, 0);
    c.fillRect(300, 99, 60, 161, 0);
    char countdown[16];
    RoundBattery::draw(180, 24);
    std::snprintf(countdown, sizeof(countdown), "%02u:%02u", frame.remainingSeconds / 60, frame.remainingSeconds % 60);
    MenuGrid::label(c, 180, 50, "等待新伙伴孵化", 200, 0xffff);
    MenuGrid::label(c, 180, 76, countdown, 180, 0xffe0);
    c.fillRoundRect(112, 238, 136, 8, 3, 0x39c7);
    if (frame.progress) c.fillRoundRect(112, 238, 136 * frame.progress / 100, 8, 3, 0x77b6);
    TouchUi::action(WIPE.x, WIPE.y, WIPE.width, Ui::Hatch::AQUA);
    TouchUi::action(POKE.x, POKE.y, POKE.width, Ui::Hatch::WARM);
    TouchUi::action(RoundRoom::MENU_RECT.x, RoundRoom::MENU_RECT.y, RoundRoom::MENU_RECT.width, Ui::MENU);
    c.fillRoundRect(76, 204, 208, 28, 6, PixelRenderer::rgb(23, 37, 49));
    MenuGrid::label(c, 180, 210, !RoundRoom::lastSaveOk ? RoundRoom::SAVE_FAILED :
        notice() ? notice() : "擦一擦，或戳一戳", 200,
        RoundRoom::lastSaveOk ? 0xffff : 0xf800);
}
int HatchUi::touchTarget(int16_t x, int16_t y) const {
    if (RoundRoom::MENU_RECT.contains(x, y)) return 1;
    if (WIPE.contains(x, y)) return 2;
    if (POKE.contains(x, y)) return 0;
    const auto center = HatchUiContent::eggCenter(layout());
    return x >= center.x - 24 && x <= center.x + 24 && y >= center.y - 27 && y <= center.y + 27 ? 0 : -1;
}
HatchAction HatchUi::onTap(int target, uint32_t nowMs) {
    if (target == 2) return onButton({0, BtnAction::PRESSED}, nowMs);
    return HatchUiBase::onTap(target, nowMs);
}
}  // namespace Ui
