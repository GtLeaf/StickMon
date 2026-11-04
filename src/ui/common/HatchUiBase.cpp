#include "ui/common/HatchUiBase.h"

#include <cstdlib>
#include "core/UiStrings.h"
#include "game/HatchService.h"
#include "presentation/TouchUi.h"
#include "ui/common/HatchUiContent.h"

namespace Ui {
void HatchUiBase::enter(const HatchFrame& frame) {
    lastRemainingSecond = frame.remainingSeconds;
    lastEggOffset = Game::HatchService::shakeOffset(frame.progress, frame.nowMs);
    lastNight = frame.night;
}

void HatchUiBase::updateVisuals(const HatchFrame& frame, RenderDemand& demand) {
    const int8_t offset = Game::HatchService::shakeOffset(frame.progress, frame.nowMs);
    demand.changed(offset != lastEggOffset || frame.night != lastNight);
    lastEggOffset = offset;
    lastNight = frame.night;
    uint16_t interval = 2400 - static_cast<uint16_t>(frame.progress) * 19;
    if (interval < 360) interval = 360;
    const uint16_t phaseMs = frame.nowMs % interval;
    demand.wakeIn(phaseMs <= 230 ? 46 - (phaseMs % 46) : interval - phaseMs);
    demand.changed(frame.remainingSeconds != lastRemainingSecond);
    lastRemainingSecond = frame.remainingSeconds;
}

void HatchUiBase::updateToast(uint32_t nowMs, RenderDemand& demand) {
    if (demand.expired(toast != nullptr, nowMs, toastUntil)) toast = nullptr;
}

HatchAction HatchUiBase::onButton(const ButtonEvent& event, uint32_t nowMs) {
    if (event.btn == 0 && event.action == BtnAction::LONG_PRESS) return HatchAction::MENU;
    if (event.action != BtnAction::PRESSED) return HatchAction::NONE;
    toast = event.btn == 0 ? Ui::Hatch::AQUA : Ui::Hatch::WARM;
    toastUntil = nowMs + 700;
    return event.btn == 0 ? HatchAction::WIPE : HatchAction::POKE;
}

HatchAction HatchUiBase::onTap(int target, uint32_t nowMs) {
    if (target == 1) return onButton({0, BtnAction::LONG_PRESS}, nowMs);
    if (target == 0) return onButton({1, BtnAction::PRESSED}, nowMs);
    return HatchAction::NONE;
}

int HatchUiBase::touchTarget(int16_t x, int16_t y) const {
    if (TouchUi::homeMenuHit(x, y)) return 1;
    const auto center = HatchUiContent::eggCenter(layout());
    return std::abs(x - center.x) <= 24 && std::abs(y - center.y) <= 27 ? 0 : -1;
}
}  // namespace Ui
