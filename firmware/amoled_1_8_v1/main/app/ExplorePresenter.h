#pragma once

#include "app/ExploreSession.h"
#include "ui/models/ScreenModels.h"

namespace AmoledV1 {

// Route presentation only. No map ownership, game mutations or device services.
class ExplorePresenter {
public:
    struct State {
        ExploreEcology::Simulation exploreEcology;
        ExploreGrass::Simulation exploreGrass;
        ExploreDaylight::Tone exploreRouteDaylight{};
        uint8_t exploreRouteDirection = 0;
        uint8_t exploreRoutePetFrame = 0;
        uint8_t exploreRouteFollowerDirection = 0;
        uint8_t exploreRouteFollowerFrame = 0;
        float exploreRouteWorldX = 0.0f;
        float exploreRouteWorldY = 0.0f;
        float exploreRouteFromX = 0.0f;
        float exploreRouteFromY = 0.0f;
        float exploreRouteTargetX = 0.0f;
        float exploreRouteTargetY = 0.0f;
        float exploreRouteFollowerWorldX = 0.0f;
        float exploreRouteFollowerWorldY = 0.0f;
        float exploreRouteFollowerFromX = 0.0f;
        float exploreRouteFollowerFromY = 0.0f;
        float exploreRouteFollowerTargetX = 0.0f;
        float exploreRouteFollowerTargetY = 0.0f;
        int16_t exploreRouteCameraX = 0;
        int16_t exploreRouteCameraY = 0;
        uint32_t exploreRouteMoveStartedMs = 0;
        uint32_t nextExploreRouteFrameMs = 0;
        uint32_t nextExploreRouteMapFrameMs = 0;
        uint32_t nextExploreRouteBossFrameMs = 0;
        uint8_t exploreRouteMapFrame = 0;
        bool exploreRouteFollowerMoving = false;
        uint32_t exploreRouteExitStartedMs = 0;
        uint32_t exploreRouteExitDurationMs = 0;
        bool exploreRouteBossWasMoving = false;
    };
    enum class Motion : uint8_t { NONE, STEP_FINISHED, EXIT_FINISHED };
    struct Damage {
        bool valid = false;
        AmoledUi::Rect bounds{};
    };
    const State& state() const { return state_; }
    void reset();
    void updateDaylight(uint32_t gameMinutes, uint8_t area);
    void startRun(uint32_t nowMs);
    void resetMap(const ExploreSession::State& route, const Game::GameState& gameState,
        uint8_t area, uint8_t hiddenPoolMask, uint32_t nowMs);
    void beginStep(const ExploreSession::State& route, uint32_t nowMs);
    void beginExit(const ExploreSession::State& route, bool hasFollower, uint32_t nowMs);
    Motion updateMotion(const ExploreSession::State& route, const Game::GameState& gameState,
        uint8_t area, uint32_t nowMs);
    void updateEcology(const ExploreSession::State& route, const Game::GameState& gameState,
        uint8_t area, bool active, uint32_t nowMs);
    void resume(const ExploreSession::State& route, uint32_t paused, uint32_t nowMs);
    void revealSpecies(uint16_t speciesId) { state_.exploreEcology.revealSpecies(speciesId); }
    ExploreRouteViewModel model(const ExploreSession::State& route,
        const Game::GameState& gameState, uint8_t area, uint32_t nowMs,
        uint8_t fadeAlpha = 0, const char* toast = nullptr) const;
    void requestDynamicDamage(const ExploreRouteViewModel& model);
    Damage takeDamage();
    // Records the caller's draw, not a hardware/display acknowledgement.
    void recordDrawn(const ExploreRouteViewModel& model) const;
    void invalidateDrawn() const { drawnValid_ = false; }
private:
    void updateCamera();
    bool advanceWalkFrames(const ExploreSession::State& route, uint32_t nowMs);
    void requestBossDamage(const ExploreSession::State& route);
    void requestMapAnimationDamage(const ExploreSession::State& route);
    void requestRect(int left, int top, int right, int bottom);
    void requestRows(int top, int bottom);
    State state_;
    Damage damage_;
    mutable bool drawnValid_ = false;
    mutable AmoledUi::Rect drawnBounds_{};
    mutable int16_t drawnCameraX_ = 0, drawnCameraY_ = 0;
    mutable uint32_t drawnMapSeed_ = 0;
    friend struct AmoledAppTestAccess;
    friend struct AmoledEcologyTestAccess;
};

}  // namespace AmoledV1
