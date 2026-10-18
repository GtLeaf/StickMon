#include "app/HatchController.h"

#include <algorithm>
#include <cstdlib>

#include "game/CareTicker.h"
#include "game/HatchService.h"
#include "ui/HatchScreens.h"

namespace AmoledV1 {

void HatchController::reset(uint32_t nowMs) {
    state_ = State{};
    state_.lastUpdateMs = state_.completionRetryMs = state_.imuRetryMs = nowMs;
    eggHitBounds_ = {130, 136, 108, 128};
    cancelTouch();
}

void HatchController::restoreProgress(const Game::HatchProgress& progress,
                                      uint16_t totalSeconds, uint32_t nowMs) {
    state_.elapsedSeconds = std::min(progress.elapsedSeconds, totalSeconds);
    state_.pokeCount = progress.pokeCount;
    state_.wipeCount = progress.wipeCount;
    state_.lastSavedSecond = static_cast<uint16_t>(state_.elapsedSeconds);
    state_.lastRemainingSecond = Game::HatchService::remainingSeconds(
        state_.elapsedSeconds, totalSeconds);
    state_.lastShakeOffset = 0;
    resume(nowMs);
}

void HatchController::advance(uint32_t nowMs, uint16_t totalSeconds,
                              float speed, bool active) {
    const uint32_t elapsedMs = nowMs - state_.lastUpdateMs;
    state_.lastUpdateMs = nowMs;
    if (!active) return;
    state_.elapsedSeconds = std::min<float>(totalSeconds,
        state_.elapsedSeconds + elapsedMs * speed / 1000.0f);
}

HatchController::Feedback HatchController::updateFeedback(
    uint32_t nowMs, uint16_t totalSeconds) {
    Feedback feedback;
    if (state_.heartVisible && nowMs - state_.heartStartedMs >= 700) {
        state_.heartVisible = false;
        feedback.roomChanged = true;
    }
    const uint16_t remaining = Game::HatchService::remainingSeconds(
        state_.elapsedSeconds, totalSeconds);
    const int8_t offset = state_.shaking
        ? Game::HatchShakeDetector::animationOffset(nowMs - state_.shakeStartedMs) : 0;
    if (state_.shaking && nowMs - state_.shakeStartedMs >=
                              Game::HatchShakeDetector::ANIMATION_MS) {
        state_.shaking = false;
    }
    feedback.statusChanged = remaining != state_.lastRemainingSecond;
    feedback.roomChanged = feedback.roomChanged || offset != state_.lastShakeOffset;
    state_.lastRemainingSecond = remaining;
    state_.lastShakeOffset = offset;
    return feedback;
}

bool HatchController::finished(uint16_t totalSeconds) const {
    return state_.elapsedSeconds >= totalSeconds;
}

bool HatchController::interact(bool shake, uint32_t nowMs, uint16_t totalSeconds,
                               bool completionPending) {
    if (completionPending || finished(totalSeconds)) return false;
    uint16_t& count = shake ? state_.wipeCount : state_.pokeCount;
    if (count < UINT16_MAX) ++count;
    state_.shaking = true;
    state_.shakeStartedMs = nowMs;
    if (!shake) {
        state_.heartVisible = true;
        state_.heartStartedMs = nowMs;
    }
    return true;
}

bool HatchController::progressToSave(bool force, Game::HatchProgress& progress) const {
    const uint16_t seconds = static_cast<uint16_t>(state_.elapsedSeconds);
    if (!force && static_cast<uint32_t>(seconds) <
                      static_cast<uint32_t>(state_.lastSavedSecond) + 15) return false;
    progress = Game::HatchProgress{};
    progress.elapsedSeconds = seconds;
    progress.pokeCount = state_.pokeCount;
    progress.wipeCount = state_.wipeCount;
    return true;
}

bool HatchController::beginCompletionAttempt(uint32_t nowMs) {
    if (static_cast<int32_t>(nowMs - state_.completionRetryMs) < 0) return false;
    state_.completionRetryMs = nowMs + 1000;
    return true;
}

uint8_t HatchController::starterStyle() const {
    return Game::HatchService::starterStyle(state_.pokeCount, state_.wipeCount);
}

HatchController::ImuRequest HatchController::syncImu(bool enabled, uint32_t nowMs) {
    ImuRequest request;
    request.enabled = enabled;
    if (state_.imuRequested != enabled) {
        state_.imuRequested = enabled;
        state_.imuStateKnown = false;
        state_.imuRetryMs = nowMs;
        state_.lastImuPollMs = nowMs;
        state_.shakeDetector.reset();
        request.roomChanged = state_.shaking || state_.heartVisible ||
                              state_.lastShakeOffset != 0;
        state_.shaking = state_.heartVisible = false;
        state_.lastShakeOffset = 0;
    }
    request.apply = !state_.imuStateKnown &&
        static_cast<int32_t>(nowMs - state_.imuRetryMs) >= 0;
    return request;
}

void HatchController::didApplyImu(bool succeeded, uint32_t nowMs) {
    state_.imuStateKnown = succeeded;
    state_.imuEnabled = state_.imuRequested && succeeded;
    state_.imuRetryMs = nowMs + 1000;
}

bool HatchController::pollImu(uint32_t nowMs) {
    if (!state_.imuEnabled || nowMs - state_.lastImuPollMs < 20) return false;
    state_.lastImuPollMs = nowMs;
    return true;
}

bool HatchController::sampleAcceleration(float x, float y, float z, uint32_t nowMs) {
    return state_.shakeDetector.sample(x, y, z, nowMs);
}

HatchController::TouchAction HatchController::handleTouch(const TouchEvent& event) {
    constexpr int TAP_SLOP = 12;
    if (event.type == TouchEventType::DOWN) {
        pointerDown_ = true;
        touchStartX_ = event.x;
        touchStartY_ = event.y;
        state_.pressedItem = eggHitBounds_.contains(event.x, event.y) ? 0 : -1;
    } else if (event.type == TouchEventType::MOVE) {
        if (std::max(std::abs(event.x - touchStartX_),
                     std::abs(event.y - touchStartY_)) > TAP_SLOP) {
            state_.pressedItem = -1;
        }
    } else if (pointerDown_) {
        const bool tap = std::max(std::abs(event.x - touchStartX_),
            std::abs(event.y - touchStartY_)) <= TAP_SLOP;
        TouchAction action = TouchAction::NONE;
        if (tap && hatchMenuAt(touchStartX_, touchStartY_) &&
                   hatchMenuAt(event.x, event.y)) {
            action = TouchAction::OPEN_MENU;
        } else if (tap && state_.pressedItem == 0 &&
                          eggHitBounds_.contains(event.x, event.y)) {
            action = TouchAction::POKE;
        }
        cancelTouch();
        return action;
    }
    return TouchAction::NONE;
}

void HatchController::cancelTouch() {
    pointerDown_ = false;
    state_.pressedItem = -1;
}

HatchViewModel HatchController::model(uint32_t gameMinutes, const char* toast) const {
    HatchViewModel model;
    model.remainingSeconds = state_.lastRemainingSecond;
    model.shakeOffset = state_.lastShakeOffset;
    model.showHeart = state_.heartVisible;
    model.gameMinutesOfDay = gameMinutes % Game::GAME_MINUTES_PER_DAY;
    model.night = model.gameMinutesOfDay < 6 * 60 || model.gameMinutesOfDay >= 18 * 60;
    model.toast = toast;
    return model;
}

}  // namespace AmoledV1
