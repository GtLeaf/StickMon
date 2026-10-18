#pragma once

#include <cstdint>

#include "game/BathService.h"
#include "ui/models/ScreenModels.h"

namespace AmoledV1 {

// Owns the wash gesture and animation session; App applies gameplay, storage,
// navigation and sound effects reported by this controller.
class ShowerController {
public:
    struct State {
        int pressedShowerItem = -1;
        ShowerMode showerMode = ShowerMode::MENU;
        uint8_t showerSoapIndex = 0;
        uint8_t showerRinseProgress = 0;
        uint8_t showerCompletionHearts = 0;
        int16_t showerToolX = 184;
        int16_t showerToolY = 212;
        int16_t showerLastStrokeX = 184;
        int16_t showerLastStrokeY = 212;
        float showerStrokeCarry = 0.0f;
        bool showerToolDragging = false;
        bool showerSoapConsumed = false;
        bool showerSoapRewarded = false;
        bool showerBrushRewarded = false;
        bool showerRinseRewarded = false;
        bool showerExitConfirmYes = false;
        uint32_t showerModeStartedMs = 0;
        uint32_t showerLastRubMs = 0;
        uint32_t showerLastRinseFoamMs = 0;
        float showerAtmosphereAlpha = 0.0f;
        Game::BathInteraction::State showerBath;
        int16_t showerGroundY = 324;
        uint32_t showerLastUpdateMs = 0;
        uint32_t showerLastSoapMs = 0;
        uint32_t showerHopStartedMs = 0;
        float showerHopHeight = 0;
        uint32_t showerWiggleStartedMs = 0;
        uint32_t showerTurnUntilMs = 0;
        bool showerTurnFlip = false;
        uint32_t showerLastReactionMs = 0;
        uint32_t showerLastRinseShakeMs = 0;
        uint32_t showerNextVisualMs = 0;
        float showerWaterCarry = 0;
        ShowerWaterState showerWater[64] = {};
        ShowerFloatState showerFloats[4] = {};
    };
    struct Effects {
        uint8_t rewardStageMask = 0;
        uint8_t completionScore = 0;
        bool cry = false;
        bool clearToast = false;
        bool sessionReset = false;
        bool fullRender = false;
        bool redraw = false;
        int left = 0, top = 0, right = 0, bottom = 0;
    };
    enum class Action : uint8_t { NONE, CLOSE, SELECT_SOAP, ALREADY_SOAPED, NO_SOAP };
    struct TapResult { Action action = Action::NONE; uint8_t soapIndex = 0; };

    State& state() { return state_; }
    const State& state() const { return state_; }
    Effects takeEffects();
    void reset(uint32_t nowMs, int16_t groundY);
    bool beginTouch(int x, int y);
    void cancelTouch();
    TapResult tap(const Game::GameState& state, int x, int y, uint32_t nowMs);
    void soapConsumed(uint8_t index, uint32_t nowMs);
    void startTool(ShowerMode mode, uint32_t nowMs);
    void dragTool(int x, int y, uint32_t nowMs);
    void startRinse(uint32_t nowMs);
    bool requestExit();
    void update(uint32_t nowMs, bool dirtyRowsValid);
    void rewardFeedback(Game::BathService::Stage stage, uint32_t experience,
                        uint8_t moodGain, uint32_t nowMs);
    ShowerViewModel model(const Game::GameState& state, const char* toast,
                          uint32_t nowMs, uint16_t columnBegin,
                          uint16_t columnEnd) const;

    void spawnFoam(uint32_t nowMs);
    void rubFoamAt(int x, int y, uint32_t nowMs);
    bool tryMergeFoam(ShowerFoamState& source, uint32_t nowMs);
    ShowerFoamState* findFoamPartner(const ShowerFoamState& source);
    void rinseFoamOneStage();
    bool anyFoam() const;
    uint8_t foamLevelTotal() const;
    void checkAtmosphereThreshold(uint32_t nowMs);
    bool updateFoamMotion(float dtSeconds);
    Game::BathInteraction::Layout bathLayout() const;
    void startHop(float height, uint32_t nowMs);
    void addFloat(uint32_t experience, uint8_t heartCount, uint32_t nowMs);
    void spawnWater(float x, float y, float vx, float vy);

private:
    void requestStage(Game::BathService::Stage stage);
    void damage(int left, int top, int right, int bottom);
    void damageRows(int top, int bottom) { damage(0, top, 368, bottom); }
    State state_;
    Effects effects_;
};

}  // namespace AmoledV1
