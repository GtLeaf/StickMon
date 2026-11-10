// SPDX-License-Identifier: Apache-2.0
#include "ui/round_touch/HatchUi.h"

#include <cstdio>
#include "core/UiStrings.h"
#include "presentation/PixelRenderer.h"
#include "platform/api/PlatformServices.h"
#include "ui/common/HatchUiContent.h"
#include "ui/round_touch/RoomControls.h"
#include "ui/round_touch/BatteryHud.h"

namespace Ui {
void HatchUi::enter(const HatchFrame& frame) {
    HatchUiBase::enter(frame);
    shakeDetector.reset();
    nextMotionMs = frame.nowMs;
    reacting = tapping = lastHeart = false;
    lastOffset = 0;
    lastNight = frame.night;
    lastSecond = frame.remainingSeconds;
}

void HatchUi::leave() {
    Platform::imu().setAccelerationEnabled(false);
    shakeDetector.reset();
    reacting = tapping = false;
}

HatchAction HatchUi::readMotion(uint32_t nowMs, RenderDemand& demand) {
    if (Platform::display().locked()) {
        leave();
        nextMotionMs = nowMs;
        demand.wakeIn(100);
        return HatchAction::NONE;
    }
    if (static_cast<int32_t>(nowMs - nextMotionMs) < 0) {
        demand.wakeAt(nowMs, nextMotionMs);
        return HatchAction::NONE;
    }
    nextMotionMs = nowMs + 20;
    // Re-enable after a power-key lock; the device implementation is idempotent.
    if (!Platform::imu().setAccelerationEnabled(true)) {
        shakeDetector.reset();
        nextMotionMs = nowMs + 1000;
    } else {
        float x = 0, y = 0, z = 0;
        if (Platform::imu().readAcceleration(x, y, z) &&
            shakeDetector.sample(x, y, z, nowMs)) {
            react(nowMs, false);
            demand.wakeAt(nowMs, nextMotionMs);
            return HatchAction::WIPE;
        }
    }
    demand.wakeAt(nowMs, nextMotionMs);
    return HatchAction::NONE;
}

void HatchUi::react(uint32_t nowMs, bool tap) {
    reactionStartMs = nowMs;
    reacting = true;
    tapping = tap;
}

int8_t HatchUi::eggOffset(uint32_t nowMs) const {
    return reacting ? Game::HatchShakeDetector::animationOffset(nowMs - reactionStartMs) : 0;
}

bool HatchUi::heartVisible(uint32_t nowMs) const {
    return reacting && tapping && nowMs - reactionStartMs < 700;
}

void HatchUi::updateVisuals(const HatchFrame& frame, RenderDemand& demand) {
    const auto offset = eggOffset(frame.nowMs);
    const bool heart = heartVisible(frame.nowMs);
    demand.changed(offset != lastOffset || heart != lastHeart ||
        frame.night != lastNight || frame.remainingSeconds != lastSecond);
    lastOffset = offset;
    lastHeart = heart;
    lastNight = frame.night;
    lastSecond = frame.remainingSeconds;
    if (reacting) {
        const uint32_t age = frame.nowMs - reactionStartMs;
        if (age >= Game::HatchShakeDetector::ANIMATION_MS) reacting = tapping = false;
        else demand.wakeIn(40 - age % 40);
    }
}

void HatchUi::render(const HatchFrame& frame) {
    HatchUiContent::drawWorld(frame, layout(), eggOffset(frame.nowMs));
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
    RoundRoom::drawMenuButton();
    if (heartVisible(frame.nowMs)) {
        constexpr uint8_t heart[]{0x66, 0xff, 0xff, 0x7e, 0x3c, 0x18};
        const auto center = HatchUiContent::eggCenter(layout());
        for (int y = 0; y < 6; ++y) for (int x = 0; x < 8; ++x)
            if (heart[y] & (0x80 >> x))
                c.fillRect(center.x - 8 + x * 2, center.y - 42 + y * 2, 2, 2,
                    PixelRenderer::rgb(255, 112, 144));
    }
    if (!RoundRoom::lastSaveOk) {
        c.fillRoundRect(76, 204, 208, 28, 6, PixelRenderer::rgb(23, 37, 49));
        MenuGrid::label(c, 180, 210, RoundRoom::SAVE_FAILED, 200, 0xf800);
    }
}
int HatchUi::touchTarget(int16_t x, int16_t y) const {
    if (RoundRoom::HOME_MENU_RECT.contains(x, y)) return 1;
    const auto center = HatchUiContent::eggCenter(layout());
    return x >= center.x - 24 && x <= center.x + 24 && y >= center.y - 27 && y <= center.y + 27 ? 0 : -1;
}
HatchAction HatchUi::onTap(int target, uint32_t nowMs) {
    if (target == 1) return HatchAction::MENU;
    if (target != 0) return HatchAction::NONE;
    react(nowMs, true);
    return HatchAction::POKE;
}
}  // namespace Ui
