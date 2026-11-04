#include "ui/common/ShowerUiBase.h"

#include <cmath>
#include <cstdio>
#include "core/MathUtil.h"
#include "core/UiStrings.h"
#include "hardware/Hal.h"

namespace Ui {
using namespace ShowerTiming;
namespace {
float clampFloat(float value, float lo, float hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}
}  // namespace

void ShowerUiBase::enter() {
    state.menuCursor = 0;
    state.menuAnimCursor = 0;
    state.gravityX = state.gravityY = state.gravityZ = 0;
    state.accelSeeded = false;
    const auto geometry = layout();
    state.soapX = state.brushX = geometry.centerX;
    state.soapY = state.brushY = geometry.centerY;
    state.waterSpawnCarry = 0;
    state.toast = nullptr;
    state.toastUntilMs = 0;
    for (auto& drop : state.water) drop = ShowerWaterDrop{};
    for (auto& floater : state.expFloats) floater = ShowerExpFloat{};
}

void ShowerUiBase::resetSession() {
    state.atmosphereAlpha = 0;
    state.hopStartMs = 0;
    state.wiggleStartMs = 0;
    state.turnUntilMs = 0;
    state.turnFlip = false;
    state.lastRubReactionMs = 0;
    state.lastRinseShakeMs = 0;
    state.exitConfirmYes = false;
}

UiMotion::StepResult ShowerUiBase::updateMenu() {
    float target = static_cast<float>(state.menuCursor);
    while (target - state.menuAnimCursor > SHOWER_MENU_COUNT / 2.0f)
        target -= SHOWER_MENU_COUNT;
    while (target - state.menuAnimCursor < -SHOWER_MENU_COUNT / 2.0f)
        target += SHOWER_MENU_COUNT;
    return UiMotion::lerp(state.menuAnimCursor, target, 0.25f, 0.04f);
}

ShowerMotion ShowerUiBase::readMotion(uint32_t nowMs, float dtSeconds) {
    float ax = 0, ay = 0, az = 0;
    const bool available = Hal::ins().readAccel(ax, ay, az);
    const float dt = dtSeconds > 0.05f ? 0.05f : dtSeconds;
    if (!state.accelSeeded) {
        state.gravityX = ax;
        state.gravityY = ay;
        state.gravityZ = az;
        state.accelSeeded = true;
    }
    const float alpha = 1.0f - expf(-GRAVITY_SMOOTHING_HZ * dt);
    state.gravityX += (ax - state.gravityX) * alpha;
    state.gravityY += (ay - state.gravityY) * alpha;
    state.gravityZ += (az - state.gravityZ) * alpha;
    ShowerMotion motion;
    motion.dynX = ax - state.gravityX;
    motion.dynY = ay - state.gravityY;
    const float dynZ = az - state.gravityZ;
    motion.shake = sqrtf(motion.dynX * motion.dynX + motion.dynY * motion.dynY + dynZ * dynZ);
    if (!available && Platform::input().pressed(Platform::InputButton::PRIMARY)) {
        const float phase = static_cast<float>(nowMs % 1600) * 0.003926991f;
        motion.dynX = sinf(phase) * 0.75f;
        motion.dynY = cosf(phase * 2.0f) * 0.55f;
        motion.shake = 0.8f;
    }
    return motion;
}

void ShowerUiBase::moveTool(bool soap, float dtSeconds, const ShowerMotion& motion) {
    const auto geometry = layout();
    const float dt = dtSeconds > 0.05f ? 0.05f : dtSeconds;
    const float smoothing = 1.0f - expf(-9.0f * dt);
    const float targetX = clampFloat(geometry.centerX + motion.dynX * TOOL_SHAKE_GAIN,
                                     geometry.toolLeftX, geometry.toolRightX);
    const float targetY = clampFloat(geometry.centerY + motion.dynY * TOOL_SHAKE_GAIN,
                                     geometry.toolTopY, geometry.toolBottomY);
    float& x = soap ? state.soapX : state.brushX;
    float& y = soap ? state.soapY : state.brushY;
    x += (targetX - x) * smoothing;
    y += (targetY - y) * smoothing;
}

bool ShowerUiBase::onButton(const ButtonEvent& event) {
    if (event.action != BtnAction::PRESSED && event.action != BtnAction::LONG_PRESS)
        return false;
    const auto mode = host.showerFrame().mode;
    if (mode == ShowerMode::EXIT_CONFIRM) {
        if (event.btn == 1 && event.action == BtnAction::PRESSED)
            state.exitConfirmYes = !state.exitConfirmYes;
        else if (event.btn == 0)
            return host.activateShower(ShowerAction::EXIT_CHOICE, state.exitConfirmYes);
        return true;
    }
    if (mode == ShowerMode::INCOMPLETE || mode == ShowerMode::COMPLETE) return true;
    if (mode == ShowerMode::SOAPING || mode == ShowerMode::BRUSHING || mode == ShowerMode::RINSING) {
        if (event.btn == 1) return host.activateShower(ShowerAction::MENU, 1);
        return true;
    }
    if (mode == ShowerMode::SOAP_SELECT) {
        if (event.btn == 1 && event.action == BtnAction::LONG_PRESS)
            return host.activateShower(ShowerAction::MENU);
        if (event.btn == 1) {
            const int8_t next = host.nextShowerSoap(state.soapCursor, 1);
            if (next >= 0) state.soapCursor = static_cast<uint8_t>(next);
        } else if (event.btn == 0)
            return host.activateShower(ShowerAction::BEGIN_SOAP);
        return true;
    }
    if (event.btn == 1 && event.action == BtnAction::LONG_PRESS)
        return host.activateShower(ShowerAction::REQUEST_EXIT);
    if (event.btn == 1) {
        state.menuCursor = (state.menuCursor + 1) % SHOWER_MENU_COUNT;
        return true;
    }
    if (event.btn != 0) return false;
    switch (state.menuCursor) {
    case 0: return host.activateShower(ShowerAction::CHOOSE_SOAP);
    case 1: return host.activateShower(ShowerAction::BEGIN_BRUSH);
    case 2: return host.activateShower(ShowerAction::BEGIN_RINSE);
    default: return host.activateShower(ShowerAction::REQUEST_EXIT);
    }
}

void ShowerUiBase::clearWater() {
    for (auto& drop : state.water) drop.active = false;
}

void ShowerUiBase::beginRinse(uint32_t nowMs) {
    clearWater();
    state.waterSpawnCarry = 0;
    state.lastRinseShakeMs = nowMs;
}

void ShowerUiBase::updateRinseWater(uint32_t nowMs, float dtSeconds, uint8_t spawnCursor) {
    const auto geometry = layout();
    const float dt = dtSeconds > 0.05f ? 0.05f : dtSeconds;
    state.waterSpawnCarry += dt * 96.0f;
    while (state.waterSpawnCarry >= 1.0f) {
        spawnWaterDrop(spawnCursor);
        state.waterSpawnCarry -= 1.0f;
    }
    for (auto& drop : state.water) {
        if (!drop.active) continue;
        drop.vy += 260.0f * dt;
        drop.x += drop.vx * dt;
        drop.y += drop.vy * dt;
        if (drop.y >= geometry.waterFloorY) {
            if (fabsf(drop.vy) > 35.0f && drop.x > geometry.waterBounceLeftX &&
                drop.x < geometry.waterBounceRightX) {
                drop.y = geometry.waterBounceY;
                drop.vy = -drop.vy * 0.24f;
                drop.vx += drop.x < geometry.centerX ? -18.0f : 18.0f;
            } else drop.active = false;
        }
        if (drop.x < 0.0f || drop.x >= geometry.waterRightX) drop.active = false;
    }
    if (nowMs - state.lastRinseShakeMs >= RINSE_SHAKE_INTERVAL_MS) {
        state.lastRinseShakeMs = nowMs;
        startWiggle(nowMs);
        for (uint8_t i = 0; i < 3; ++i) {
            const float direction = (i % 2 == 0) ? 1.0f : -1.0f;
            spawnSplashDrop(geometry.centerX + direction * (10 + i * 6),
                geometry.centerY - 12.0f, direction * (50.0f + i * 20.0f), -45.0f);
        }
    }
}

void ShowerUiBase::spawnWaterDrop(uint8_t spawnCursor) {
    const auto geometry = layout();
    for (uint8_t i = 0; i < SHOWER_WATER_CAP; ++i) {
        auto& drop = state.water[i];
        if (drop.active) continue;
        drop.active = true;
        drop.x = static_cast<float>(geometry.waterSpawnX) +
            static_cast<float>((i * 29 + spawnCursor * 7) % 139);
        drop.y = -4.0f - static_cast<float>((i * 7) % 42);
        drop.vx = static_cast<float>((static_cast<int>(i) % 7) - 3) * 4.0f;
        drop.vy = 125.0f + static_cast<float>((i * 13) % 90);
        return;
    }
}

void ShowerUiBase::spawnSplashDrop(float x, float y, float vx, float vy) {
    for (auto& drop : state.water) {
        if (drop.active) continue;
        drop.active = true;
        drop.x = x; drop.y = y; drop.vx = vx; drop.vy = vy;
        return;
    }
}

void ShowerUiBase::updateAtmosphere(bool targetVisible, float dtSeconds) {
    const float dt = dtSeconds > 0.05f ? 0.05f : dtSeconds;
    const float target = targetVisible ? 255.0f : 0.0f;
    const float speed = targetVisible ? 300.0f : static_cast<float>(ATMOSPHERE_FADE_OUT_PER_SECOND);
    if (state.atmosphereAlpha < target)
        state.atmosphereAlpha = MathUtil::min(target, state.atmosphereAlpha + speed * dt);
    else if (state.atmosphereAlpha > target)
        state.atmosphereAlpha = MathUtil::max(target, state.atmosphereAlpha - speed * dt);
}

bool ShowerUiBase::updateEffects(uint32_t nowMs, bool atmosphereTarget, RenderDemand& demand) {
    bool effectActive = false;
    for (auto& floater : state.expFloats) {
        if (!floater.active) continue;
        if (nowMs - floater.bornMs >= EXP_FLOAT_DURATION_MS) {
            floater.active = false;
            demand.redraw();
        } else effectActive = true;
    }
    if (demand.expired(state.toast != nullptr, nowMs, state.toastUntilMs)) state.toast = nullptr;
    const bool atmosphereMoving = atmosphereTarget
        ? state.atmosphereAlpha < 255.0f : state.atmosphereAlpha > 0.0f;
    const bool timedReaction =
        (state.hopStartMs != 0 && nowMs - state.hopStartMs < HOP_DURATION_MS) ||
        (state.wiggleStartMs != 0 && nowMs - state.wiggleStartMs < WIGGLE_DURATION_MS) ||
        nowMs < state.turnUntilMs;
    if (state.hopStartMs != 0 && nowMs - state.hopStartMs >= HOP_DURATION_MS) {
        state.hopStartMs = 0;
        demand.redraw();
    }
    if (state.wiggleStartMs != 0 && nowMs - state.wiggleStartMs >= WIGGLE_DURATION_MS) {
        state.wiggleStartMs = 0;
        demand.redraw();
    }
    if (state.turnUntilMs != 0 && nowMs >= state.turnUntilMs) {
        state.turnUntilMs = 0;
        demand.redraw();
    }
    return atmosphereMoving || effectActive || timedReaction;
}

void ShowerUiBase::startHop(float heightPx, uint32_t nowMs) {
    state.hopStartMs = nowMs;
    state.hopHeightPx = heightPx;
}

void ShowerUiBase::startWiggle(uint32_t nowMs) { state.wiggleStartMs = nowMs; }

void ShowerUiBase::startTurn(uint32_t nowMs) {
    if (nowMs < state.turnUntilMs) return;
    state.turnFlip = !state.turnFlip;
    state.turnUntilMs = nowMs + TURN_HOLD_MS;
}

void ShowerUiBase::spawnExpFloat(uint32_t amount, uint32_t nowMs) {
    uint8_t slot = 0;
    uint32_t oldest = UINT32_MAX;
    for (uint8_t i = 0; i < SHOWER_EXP_FLOAT_CAP; ++i) {
        if (!state.expFloats[i].active) { slot = i; break; }
        if (state.expFloats[i].bornMs < oldest) { oldest = state.expFloats[i].bornMs; slot = i; }
    }
    auto& floater = state.expFloats[slot];
    const auto geometry = layout();
    floater.x = geometry.expFloatBaseX + static_cast<float>((slot % 2) * 14);
    floater.y = geometry.centerY + 16.0f;
    floater.bornMs = nowMs;
    std::snprintf(floater.text, sizeof(floater.text),
                  Ui::Shower::EXP_GAIN_FMT, static_cast<unsigned>(amount));
    floater.heart = false;
    floater.active = true;
}

void ShowerUiBase::spawnHeartFloat(uint32_t nowMs) {
    uint8_t slot = 0;
    uint32_t oldest = UINT32_MAX;
    for (uint8_t i = 0; i < SHOWER_EXP_FLOAT_CAP; ++i) {
        if (!state.expFloats[i].active) { slot = i; break; }
        if (state.expFloats[i].bornMs < oldest) { oldest = state.expFloats[i].bornMs; slot = i; }
    }
    auto& floater = state.expFloats[slot];
    const auto geometry = layout();
    floater.x = geometry.heartFloatX;
    floater.y = geometry.heartFloatY;
    floater.bornMs = nowMs;
    floater.text[0] = '\0';
    floater.heart = true;
    floater.active = true;
}

}  // namespace Ui
