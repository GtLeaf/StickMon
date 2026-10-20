#pragma once

#include <cstdint>

#include "AmoledGeometry.h"
#include "TouchEvent.h"
#include "game/GameState.h"
#include "game/HatchInteraction.h"
#include "ui/models/ScreenModels.h"

namespace AmoledV1 {

// Egg gestures and presentation timing. App applies hardware requests and
// commits progress/the completed game through its normal save boundary.
class HatchController {
public:
    enum class TouchAction : uint8_t { NONE, OPEN_MENU, POKE };

    struct State {
        float elapsedSeconds = 0.0f;
        uint16_t pokeCount = 0;
        uint16_t wipeCount = 0;
        uint16_t lastSavedSecond = 0;
        uint16_t lastRemainingSecond = 180;
        int8_t lastShakeOffset = 0;
        int8_t pressedItem = -1;
        uint32_t lastUpdateMs = 0;
        uint32_t completionRetryMs = 0;
        bool imuRequested = false;
        bool imuEnabled = false;
        bool imuStateKnown = false;
        uint32_t imuRetryMs = 0;
        uint32_t lastImuPollMs = 0;
        Game::HatchShakeDetector shakeDetector;
        uint32_t heartStartedMs = 0;
        uint32_t shakeStartedMs = 0;
        bool heartVisible = false;
        bool shaking = false;
    };

    struct Feedback {
        bool roomChanged = false;
        bool statusChanged = false;
    };

    struct ImuRequest {
        bool roomChanged = false;
        bool apply = false;
        bool enabled = false;
    };

    void reset(uint32_t nowMs);
    void restoreProgress(const Game::HatchProgress& progress,
                         uint16_t totalSeconds, uint32_t nowMs);
    void resume(uint32_t nowMs) { state_.lastUpdateMs = nowMs; }
    void advance(uint32_t nowMs, uint16_t totalSeconds, float speed, bool active);
    Feedback updateFeedback(uint32_t nowMs, uint16_t totalSeconds);
    bool finished(uint16_t totalSeconds) const;
    bool interact(bool shake, uint32_t nowMs, uint16_t totalSeconds,
                  bool completionPending);
    bool progressToSave(bool force, Game::HatchProgress& progress) const;
    void didSaveProgress(uint16_t seconds) { state_.lastSavedSecond = seconds; }
    bool beginCompletionAttempt(uint32_t nowMs);
    uint8_t starterStyle() const;

    ImuRequest syncImu(bool enabled, uint32_t nowMs);
    void didApplyImu(bool succeeded, uint32_t nowMs);
    bool pollImu(uint32_t nowMs);
    bool sampleAcceleration(float x, float y, float z, uint32_t nowMs);

    TouchAction handleTouch(const TouchEvent& event);
    void cancelTouch();
    HatchViewModel model(uint32_t gameMinutes, const char* toast) const;
    AmoledUi::Rect& eggHitBounds() const { return eggHitBounds_; }

    const State& state() const { return state_; }

private:
    State state_;
    bool pointerDown_ = false;
    int16_t touchStartX_ = 0;
    int16_t touchStartY_ = 0;
    mutable AmoledUi::Rect eggHitBounds_{130, 136, 108, 128};
};

}  // namespace AmoledV1
