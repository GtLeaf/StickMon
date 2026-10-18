#include "ShowerController.h"

#include <algorithm>
#include <cmath>

#include "HomeScreen.h"
#include "game/GameRandom.h"
#include "ui/UiMetrics.h"

namespace AmoledV1 {
namespace {
constexpr int HEADER_HEIGHT = UiMetrics::HEADER_HEIGHT;
constexpr int SHOWER_BODY_LEFT = 84;
constexpr int SHOWER_BODY_RIGHT = 284;
constexpr int SHOWER_BODY_TOP = 96;
constexpr int SHOWER_BODY_BOTTOM = 316;
constexpr int SHOWER_PET_X = 184;
constexpr int SHOWER_PET_Y = 212;
constexpr int SHOWER_TOOL_MIN_X = 24;
constexpr int SHOWER_TOOL_MAX_X = 344;
constexpr int SHOWER_TOOL_MIN_Y = 72;
constexpr int SHOWER_TOOL_MAX_Y = 420;
constexpr float SHOWER_FOAM_DISTANCE = 24.0f;
constexpr float SHOWER_BRUSH_DISTANCE = 8.0f;
constexpr uint32_t SHOWER_BRUSH_INTERVAL_MS = Game::BathInteraction::RUB_INTERVAL_MS;
}

void ShowerController::startHop(float height, uint32_t nowMs) {
    state_.showerHopStartedMs = nowMs;
    state_.showerHopHeight = height;
}

void ShowerController::addFloat(uint32_t experience, uint8_t heartCount, uint32_t nowMs) {
    heartCount = std::min<uint8_t>(heartCount, 3);
    if (heartCount > 0) {
        for (auto& floater : state_.showerFloats) {
            if (floater.heartCount > 0) floater.active = false;
        }
    }
    uint8_t slot = 0;
    for (uint8_t i = 0; i < 4; ++i) {
        if (!state_.showerFloats[i].active) { slot = i; break; }
        if (state_.showerFloats[i].bornMs < state_.showerFloats[slot].bornMs) slot = i;
    }
    state_.showerFloats[slot] = {nowMs, experience, true, heartCount};
}

void ShowerController::spawnWater(float x, float y, float vx, float vy) {
    for (auto& drop : state_.showerWater) {
        if (drop.active) continue;
        drop = {x, y, vx, vy, true};
        return;
    }
}

void ShowerController::update(uint32_t nowMs, bool dirtyRowsValid) {
    const float dt = std::min(0.05f, (nowMs - state_.showerLastUpdateMs) / 1000.0f);
    state_.showerLastUpdateMs = nowMs;
    bool moving = false;
    bool cleanup = false;
    if (state_.showerMode == ShowerMode::BRUSHING) {
        moving = updateFoamMotion(dt);
        checkAtmosphereThreshold(nowMs);
    }
    const float previousAlpha = state_.showerAtmosphereAlpha;
    state_.showerAtmosphereAlpha = state_.showerBath.atmosphereTarget
        ? std::min(255.0f, state_.showerAtmosphereAlpha + 300 * dt)
        : std::max(0.0f, state_.showerAtmosphereAlpha - 420 * dt);
    moving = moving || previousAlpha != state_.showerAtmosphereAlpha;
    if (state_.showerMode == ShowerMode::RINSING) {
        const uint32_t elapsed = nowMs - state_.showerModeStartedMs;
        // Progress is informational; foam visibility follows the simulation.
        state_.showerRinseProgress = static_cast<uint8_t>(std::min<uint32_t>(100, elapsed * 100 / 1800));
        state_.showerWaterCarry += dt * 96;
        while (state_.showerWaterCarry >= 1) {
            state_.showerWaterCarry -= 1;
            const int index = static_cast<int>(elapsed / 10 + state_.showerWaterCarry * 29);
            spawnWater(44 + (index * 29) % 278, 70 - (index * 7) % 84,
                             (index % 7 - 3) * 8, 250 + (index * 13) % 180);
        }
        for (auto& drop : state_.showerWater) {
            if (!drop.active) continue;
            drop.vy += 520 * dt;
            drop.x += drop.vx * dt;
            drop.y += drop.vy * dt;
            if (drop.y >= 322) {
                if (std::fabs(drop.vy) > 70 && drop.x > 50 && drop.x < 310) {
                    drop.y = 320; drop.vy *= -0.24f;
                    drop.vx += drop.x < SHOWER_PET_X ? -36 : 36;
                } else drop.active = false;
            }
            if (drop.x < 36 || drop.x >= 332) drop.active = false;
        }
        if (nowMs - state_.showerLastRinseShakeMs >= 800) {
            state_.showerLastRinseShakeMs = nowMs;
            state_.showerWiggleStartedMs = nowMs;
            for (int i = 0; i < 3; ++i) {
                const int direction = i % 2 == 0 ? 1 : -1;
                spawnWater(SHOWER_PET_X + direction * (20 + i * 12),
                    SHOWER_PET_Y - 24, direction * (100 + i * 40), -90);
            }
        }
        if (nowMs - state_.showerLastRinseFoamMs >= 350) {
            state_.showerLastRinseFoamMs = nowMs;
            rinseFoamOneStage();
        }
        moving = true;
        if (elapsed >= 1800 && !anyFoam() && state_.showerAtmosphereAlpha <= 1) {
            requestStage(Game::BathService::Stage::RINSE);
            state_.showerCompletionHearts = static_cast<uint8_t>(
                (state_.showerSoapRewarded ? 1 : 0) + (state_.showerBrushRewarded ? 1 : 0) +
                (state_.showerRinseRewarded ? 1 : 0));
            effects_.completionScore = state_.showerCompletionHearts;
            state_.showerSoapConsumed = false;
            state_.showerMode = ShowerMode::COMPLETE;
            state_.showerModeStartedMs = nowMs;
            for (auto& drop : state_.showerWater) drop = {};
            state_.showerHopStartedMs = 0;
            state_.showerWiggleStartedMs = 0;
            if (state_.showerCompletionHearts >= 3) effects_.cry = true;
            else if (state_.showerCompletionHearts == 2) startHop(20, nowMs);
            damageRows(HEADER_HEIGHT, 328);
        }
    } else if (state_.showerMode == ShowerMode::COMPLETE && nowMs - state_.showerModeStartedMs >= 1500) {
        reset(nowMs, state_.showerGroundY);
        effects_.sessionReset = true;
        damageRows(HEADER_HEIGHT, 448);
    }
    if (state_.showerHopStartedMs) {
        moving = true;
        if (nowMs - state_.showerHopStartedMs >= 320) { state_.showerHopStartedMs = 0; cleanup = true; }
    }
    if (state_.showerWiggleStartedMs) {
        moving = true;
        if (nowMs - state_.showerWiggleStartedMs >= 220) { state_.showerWiggleStartedMs = 0; cleanup = true; }
    }
    if (state_.showerTurnUntilMs && nowMs >= state_.showerTurnUntilMs) {
        state_.showerTurnUntilMs = 0;
        moving = cleanup = true;
    }
    for (auto& floater : state_.showerFloats) {
        if (!floater.active) continue;
        moving = true;
        if (nowMs - floater.bornMs >= 1100) { floater.active = false; cleanup = true; }
    }
    if (cleanup || (moving && (dirtyRowsValid || effects_.redraw ||
        static_cast<int32_t>(nowMs - state_.showerNextVisualMs) >= 0))) {
        state_.showerNextVisualMs = nowMs + 33;
        damage(36, HEADER_HEIGHT, 332, 328);
    }
}

void ShowerController::startTool(ShowerMode mode, uint32_t nowMs) {
    if (mode != ShowerMode::SOAPING && mode != ShowerMode::BRUSHING) return;

    state_.showerMode = mode;
    state_.showerModeStartedMs = nowMs;
    state_.showerToolDragging = false;
    state_.showerStrokeCarry = 0.0f;
    state_.showerToolX = SHOWER_PET_X;
    state_.showerToolY = SHOWER_PET_Y;
    state_.showerLastStrokeX = state_.showerToolX;
    state_.showerLastStrokeY = state_.showerToolY;
    state_.pressedShowerItem = -1;
    effects_.clearToast = true;
    damageRows(HEADER_HEIGHT, 448);
}

void ShowerController::dragTool(int x, int y, uint32_t nowMs) {
    if (!state_.showerToolDragging ||
        (state_.showerMode != ShowerMode::SOAPING &&
         state_.showerMode != ShowerMode::BRUSHING)) {
        return;
    }

    x = std::clamp(x, SHOWER_TOOL_MIN_X, SHOWER_TOOL_MAX_X);
    y = std::clamp(y, SHOWER_TOOL_MIN_Y, SHOWER_TOOL_MAX_Y);
    bool previousInside = state_.showerLastStrokeX >= SHOWER_BODY_LEFT &&
                          state_.showerLastStrokeX <= SHOWER_BODY_RIGHT &&
                          state_.showerLastStrokeY >= SHOWER_BODY_TOP &&
                          state_.showerLastStrokeY <= SHOWER_BODY_BOTTOM;
    bool currentInside = x >= SHOWER_BODY_LEFT && x <= SHOWER_BODY_RIGHT &&
                         y >= SHOWER_BODY_TOP && y <= SHOWER_BODY_BOTTOM;
    if (previousInside && currentInside) {
        float dx = static_cast<float>(x - state_.showerLastStrokeX);
        float dy = static_cast<float>(y - state_.showerLastStrokeY);
        state_.showerStrokeCarry += std::sqrt(dx * dx + dy * dy);
    }

    const int oldX = state_.showerToolX, oldY = state_.showerToolY;
    if (oldX == x && oldY == y) return;
    state_.showerToolX = static_cast<int16_t>(x);
    state_.showerToolY = static_cast<int16_t>(y);
    state_.showerLastStrokeX = state_.showerToolX;
    state_.showerLastStrokeY = state_.showerToolY;

    if (state_.showerMode == ShowerMode::SOAPING) {
        if (currentInside && state_.showerStrokeCarry >= SHOWER_FOAM_DISTANCE &&
            nowMs - state_.showerLastSoapMs >= Game::BathInteraction::SOAP_INTERVAL_MS) {
            state_.showerStrokeCarry = 0;
            spawnFoam(nowMs);
        }
    } else if (state_.showerMode == ShowerMode::BRUSHING) {
        if (currentInside && state_.showerStrokeCarry >= SHOWER_BRUSH_DISTANCE &&
            nowMs - state_.showerLastRubMs >= SHOWER_BRUSH_INTERVAL_MS) {
            state_.showerStrokeCarry = 0.0f;
            state_.showerLastRubMs = nowMs;
            rubFoamAt(x, y, nowMs);
        }
    }

    damage(std::min(oldX, x) - 64, std::min(oldY, y) - 64,
                      std::max(oldX, x) + 65, std::max(oldY, y) + 65);
}

void ShowerController::spawnFoam(uint32_t nowMs) {
    if (state_.showerBath.atmosphereTarget ||
        nowMs - state_.showerLastSoapMs < Game::BathInteraction::SOAP_INTERVAL_MS) return;
    state_.showerLastSoapMs = nowMs;
    if (Game::BathInteraction::spawn(state_.showerBath, bathLayout(), nowMs) == 0) return;
    if (!state_.showerHopStartedMs || nowMs - state_.showerHopStartedMs >= 900) startHop(12, nowMs);
    requestStage(Game::BathService::Stage::SOAP);
    damage(36, HEADER_HEIGHT, 332, 328);
}

ShowerFoamState* ShowerController::findFoamPartner(const ShowerFoamState& source) {
    return Game::BathInteraction::partner(state_.showerBath, bathLayout(), source);
}

bool ShowerController::tryMergeFoam(ShowerFoamState& source, uint32_t nowMs) {
    if (!Game::BathInteraction::merge(state_.showerBath, bathLayout(), source, nowMs)) return false;
    startHop(6, nowMs);
    checkAtmosphereThreshold(nowMs);
    return true;
}

void ShowerController::rubFoamAt(int x, int y, uint32_t nowMs) {
    const auto result = Game::BathInteraction::rub(state_.showerBath, bathLayout(), x, y, nowMs);
    if (!result.touched) return;
    if (result.merged) startHop(6, nowMs);
    checkAtmosphereThreshold(nowMs);
    if (nowMs - state_.showerLastReactionMs >= 1500) {
        state_.showerLastReactionMs = nowMs;
        if (GameRandom::random(100) < 35 && nowMs >= state_.showerTurnUntilMs) {
            state_.showerTurnFlip = !state_.showerTurnFlip;
            state_.showerTurnUntilMs = nowMs + 2600;
        }
    }
    damage(36, HEADER_HEIGHT, 332, 328);
}

void ShowerController::checkAtmosphereThreshold(uint32_t nowMs) {
    if (!Game::BathInteraction::checkAtmosphere(state_.showerBath, bathLayout())) return;
    requestStage(Game::BathService::Stage::BRUSH);
    startHop(6, nowMs);
    damage(36, HEADER_HEIGHT, 332, 328);
}

uint8_t ShowerController::foamLevelTotal() const {
    return Game::BathInteraction::lowerWeight(state_.showerBath, bathLayout());
}

bool ShowerController::anyFoam() const {
    return Game::BathInteraction::anyFoam(state_.showerBath);
}

void ShowerController::rinseFoamOneStage() {
    Game::BathInteraction::rinse(state_.showerBath);
}

Game::BathInteraction::Layout ShowerController::bathLayout() const {
    Game::BathInteraction::Layout layout;
    layout.centerX = SHOWER_PET_X; layout.centerY = SHOWER_PET_Y;
    layout.left = 80; layout.right = 288;
    layout.top = 124; layout.bottom = 282;
    layout.lowerY = 232; layout.groundY = state_.showerGroundY; layout.scale = 2;
    return layout;
}

bool ShowerController::updateFoamMotion(float dtSeconds) {
    return Game::BathInteraction::update(state_.showerBath, bathLayout(), dtSeconds);
}

void ShowerController::startRinse(uint32_t nowMs) {
    state_.showerMode = ShowerMode::RINSING;
    state_.showerModeStartedMs = nowMs;
    state_.showerLastRinseFoamMs = nowMs;
    state_.showerLastRinseShakeMs = nowMs;
    state_.showerWaterCarry = 0;
    for (auto& drop : state_.showerWater) drop = {};
    state_.showerRinseProgress = 0;
    state_.showerBath.atmosphereTarget = false;
    state_.showerToolDragging = false;
    state_.pressedShowerItem = -1;
    effects_.clearToast = true;
    effects_.fullRender = true;
}

void ShowerController::reset(uint32_t nowMs, int16_t groundY) {
    state_.showerMode = ShowerMode::MENU;
    state_.showerSoapIndex = 0;
    state_.showerRinseProgress = 0;
    state_.showerCompletionHearts = 0;
    state_.showerToolX = SHOWER_PET_X;
    state_.showerToolY = SHOWER_PET_Y;
    state_.showerLastStrokeX = state_.showerToolX;
    state_.showerLastStrokeY = state_.showerToolY;
    state_.showerStrokeCarry = 0.0f;
    state_.showerToolDragging = false;
    state_.showerSoapConsumed = false;
    state_.showerSoapRewarded = false;
    state_.showerBrushRewarded = false;
    state_.showerRinseRewarded = false;
    state_.showerExitConfirmYes = false;
    state_.showerModeStartedMs = nowMs;
    state_.showerLastRubMs = 0;
    state_.showerLastRinseFoamMs = nowMs;
    state_.showerBath = Game::BathInteraction::State{};
    state_.showerAtmosphereAlpha = 0.0f;
    state_.showerBath.atmosphereTarget = false;
    state_.pressedShowerItem = -1;
    state_.showerGroundY = groundY;
    state_.showerLastUpdateMs = nowMs;
    state_.showerLastSoapMs = nowMs - Game::BathInteraction::SOAP_INTERVAL_MS;
    state_.showerHopStartedMs = state_.showerWiggleStartedMs = state_.showerTurnUntilMs = 0;
    state_.showerTurnFlip = false;
    state_.showerLastReactionMs = nowMs;
    state_.showerLastRinseShakeMs = nowMs;
    state_.showerNextVisualMs = nowMs;
    state_.showerWaterCarry = 0;
    for (auto& drop : state_.showerWater) drop = {};
    for (auto& floater : state_.showerFloats) floater = {};
    effects_.clearToast = true;
}

ShowerController::Effects ShowerController::takeEffects() {
    const auto result = effects_;
    effects_ = {};
    return result;
}

void ShowerController::damage(int left, int top, int right, int bottom) {
    if (!effects_.redraw) {
        effects_.left = left; effects_.top = top;
        effects_.right = right; effects_.bottom = bottom;
    } else {
        effects_.left = std::min(effects_.left, left);
        effects_.top = std::min(effects_.top, top);
        effects_.right = std::max(effects_.right, right);
        effects_.bottom = std::max(effects_.bottom, bottom);
    }
    effects_.redraw = true;
}

void ShowerController::requestStage(Game::BathService::Stage stage) {
    bool* rewarded = nullptr;
    switch (stage) {
    case Game::BathService::Stage::SOAP: rewarded = &state_.showerSoapRewarded; break;
    case Game::BathService::Stage::BRUSH: rewarded = &state_.showerBrushRewarded; break;
    case Game::BathService::Stage::RINSE: rewarded = &state_.showerRinseRewarded; break;
    }
    if (!rewarded || *rewarded) return;
    *rewarded = true;
    effects_.rewardStageMask |= static_cast<uint8_t>(1U << static_cast<uint8_t>(stage));
}

void ShowerController::rewardFeedback(Game::BathService::Stage stage,
        uint32_t experience, uint8_t moodGain, uint32_t nowMs) {
    if (experience) addFloat(experience, 0, nowMs);
    if (moodGain) addFloat(0, moodGain <= 2 ? 1 : moodGain <= 5 ? 2 : 3, nowMs);
    // Brush completion already chose its small hop; a rinse completion
    // chooses its final reaction before reward feedback is applied.
    if ((experience || moodGain) && stage != Game::BathService::Stage::BRUSH &&
        state_.showerMode != ShowerMode::COMPLETE) startHop(20, nowMs);
}

void ShowerController::soapConsumed(uint8_t index, uint32_t nowMs) {
    state_.showerSoapIndex = index;
    state_.showerSoapConsumed = true;
    startTool(ShowerMode::SOAPING, nowMs);
}

bool ShowerController::beginTouch(int x, int y) {
    if ((state_.showerMode == ShowerMode::SOAPING ||
         state_.showerMode == ShowerMode::BRUSHING) &&
        showerToolAt(x, y, state_.showerToolX, state_.showerToolY)) {
        state_.showerToolDragging = true;
        state_.showerLastStrokeX = x;
        state_.showerLastStrokeY = y;
        damage(state_.showerToolX - 64, state_.showerToolY - 64,
               state_.showerToolX + 65, state_.showerToolY + 65);
        return true;
    }
    state_.pressedShowerItem = state_.showerMode == ShowerMode::SOAP_SELECT
        ? showerSoapItemAt(x, y)
        : state_.showerMode == ShowerMode::EXIT_CONFIRM
            ? showerExitChoiceAt(x, y)
            : (state_.showerMode == ShowerMode::MENU ||
               state_.showerMode == ShowerMode::SOAPING ||
               state_.showerMode == ShowerMode::BRUSHING)
                ? showerMenuItemAt(x, y) : -1;
    damageRows(HEADER_HEIGHT, 448);
    return false;
}

void ShowerController::cancelTouch() {
    state_.showerToolDragging = false;
    state_.pressedShowerItem = -1;
}

ShowerController::TapResult ShowerController::tap(
        const Game::GameState& game, int x, int y, uint32_t nowMs) {
    if (state_.showerMode == ShowerMode::EXIT_CONFIRM) {
        const int choice = showerExitChoiceAt(x, y);
        if (choice == 0) {
            state_.showerExitConfirmYes = true;
            return {Action::CLOSE};
        }
        if (choice == 1) {
            state_.showerExitConfirmYes = false;
            state_.showerMode = ShowerMode::MENU;
            damageRows(HEADER_HEIGHT, 448);
        }
        return {};
    }
    if (state_.showerMode == ShowerMode::SOAP_SELECT) {
        if (showerBackAt(x, y)) {
            state_.showerMode = ShowerMode::MENU;
            damageRows(HEADER_HEIGHT, 448);
            return {};
        }
        const int soap = showerSoapItemAt(x, y);
        if (soap >= 0) return {Action::SELECT_SOAP, static_cast<uint8_t>(soap)};
        return {};
    }
    if (showerBackAt(x, y)) return requestExit() ? TapResult{Action::CLOSE} : TapResult{};
    if (state_.showerMode != ShowerMode::MENU && state_.showerMode != ShowerMode::SOAPING &&
        state_.showerMode != ShowerMode::BRUSHING) return {};
    const int item = showerMenuItemAt(x, y);
    if (item == 0) {
        if (state_.showerSoapConsumed) return {Action::ALREADY_SOAPED};
        if (Game::BathService::nextOwnedSoap(game, -1) < 0) return {Action::NO_SOAP};
        state_.showerMode = ShowerMode::SOAP_SELECT;
        effects_.clearToast = true;
        damageRows(HEADER_HEIGHT, 448);
    } else if (item == 1) startTool(ShowerMode::BRUSHING, nowMs);
    else if (item == 2) startRinse(nowMs);
    return {};
}

bool ShowerController::requestExit() {
    cancelTouch();
    if (state_.showerSoapConsumed && !state_.showerRinseRewarded) {
        state_.showerMode = ShowerMode::EXIT_CONFIRM;
        state_.showerExitConfirmYes = false;
        effects_.clearToast = true;
        damageRows(HEADER_HEIGHT, 448);
        return false;
    }
    return true;
}

ShowerViewModel ShowerController::model(const Game::GameState& game,
        const char* toast, uint32_t nowMs, uint16_t columnBegin, uint16_t columnEnd) const {
    ShowerViewModel model;
    model.state = &game;
    model.mode = state_.showerMode;
    model.speciesId = game.team[0].speciesId;
    model.soapIndex = state_.showerSoapIndex;
    model.rinseProgress = state_.showerRinseProgress;
    model.foam = state_.showerBath.foam;
    model.foamCount = Game::BathInteraction::FOAM_CAP;
    model.atmosphereAlpha = static_cast<uint8_t>(std::clamp(state_.showerAtmosphereAlpha, 0.0f, 255.0f));
    model.completionHearts = state_.showerCompletionHearts;
    model.toolX = state_.showerToolX; model.toolY = state_.showerToolY;
    model.pressedItem = state_.pressedShowerItem;
    model.toolDragging = state_.showerToolDragging;
    model.exitConfirmYes = state_.showerExitConfirmYes;
    model.toast = toast;
    model.columnBegin = columnBegin; model.columnEnd = columnEnd;
    model.nowMs = nowMs; model.elapsedMs = nowMs - state_.showerModeStartedMs;
    model.groundY = state_.showerGroundY;
    if (state_.showerHopStartedMs && nowMs - state_.showerHopStartedMs < 320) {
        const float progress = (nowMs - state_.showerHopStartedMs) / 320.0f;
        model.petOffsetY = -static_cast<int16_t>(state_.showerHopHeight * 4 * progress * (1 - progress) + 0.5f);
    }
    model.petFlip = state_.showerWiggleStartedMs && nowMs - state_.showerWiggleStartedMs < 220
        ? ((nowMs - state_.showerWiggleStartedMs) / 70) % 2 == 1
        : nowMs < state_.showerTurnUntilMs && state_.showerTurnFlip;
    model.water = state_.showerWater; model.waterCount = 64;
    model.floats = state_.showerFloats; model.floatCount = 4;
    return model;
}

}  // namespace AmoledV1
