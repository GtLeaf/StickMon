#pragma once

#include "assets/PokemonSprites.h"
#include "core/RoomNavigator.h"
#include "game/GameState.h"
#include "game/HomeSimulation.h"

namespace AmoledV1 {

// Room-action lifecycle. Actors/navigation remain authoritative in Simulation;
// the caller applies animation, camera, damage and persistence effects.
class HomeFlow {
public:
    using Direction = PokemonSprites::WalkDirection;
    enum class Action : uint8_t {
        NONE, ATTENTION_APPROACH, ATTENTION_WAIT, LOOK_AROUND, FEED_FINISH,
        CIRCLE, DASH, STEP_BACK, QUIET_GAZE, WINDOW_APPROACH, WINDOW_WAIT,
    };
    struct State {
        Action action = Action::NONE;
        uint8_t phase = 0;
        uint32_t startedMs = 0, untilMs = 0;
        uint32_t nextAttentionMs = 0, nextSpecialActionMs = 0;
        float originX = 0.0f, originY = 0.0f, radius = 0.0f;
        Direction baseDirection = Direction::DOWN;
        Direction windowDirection = Direction::UP;
    };
    struct Layout {
        const RoomResource::Point* polygon = nullptr;
        uint8_t polygonCount = 0;
        RoomNavigator::Bounds bounds{};
        float foodX = 0.0f, foodY = 0.0f;
        bool hasWindow = false;
        float windowX = 0.0f, windowY = 0.0f;
        Direction windowDirection = Direction::UP;
    };
    enum class Heart : uint8_t { NONE, SHOW, CLEAR };
    struct Effects {
        bool save = false, roomChanged = false, scheduleDecision = false;
        bool stopAnimation = false, resetFrame = false, startWalk = false;
        bool directionChanged = false;
        Direction direction = Direction::DOWN;
        Heart heart = Heart::NONE;
        uint32_t heartsUntilMs = 0;
    };
    struct Update {
        bool active = false, moved = false;
        float previousY = 0.0f;
        Home::RouteStep step = Home::RouteStep::NO_ROUTE;
    };

    HomeFlow(Game::GameState& game, Home::Actor& main,
             const Home::Actor& companion, Home::Simulation& simulation)
        : gameState(game), homeMainActor(main), homeCompanionActor(companion),
          homeRuntime(simulation) {}
    HomeFlow(const HomeFlow&) = delete;
    HomeFlow& operator=(const HomeFlow&) = delete;
    const State& state() const { return state_; }
    bool needsWindowAnchor() const;
    void reset();
    void scheduleAttention(uint32_t nowMs, bool initial = false);
    void scheduleSpecialAction(uint32_t nowMs);
    bool tryAmbient(const Layout& layout, Direction direction, uint32_t nowMs);
    void startFeedFinish(uint32_t nowMs);
    Update update(const Layout& layout, uint32_t nowMs, float elapsedSeconds, bool blocksFood);
    // Called after the caller advances camera/frame presentation for this step.
    void completeMovement(const Layout& layout, Home::RouteStep step, bool blocksFood, uint32_t nowMs);
    void cancel(bool blocksFood, uint32_t nowMs);
    Effects takeEffects();

private:
    bool contains(const Layout& layout, float x, float y) const;
    bool pathInside(const Layout& layout, float fromX, float fromY,
                    float toX, float toY) const;
    bool chooseNearbyPose(const Layout& layout, float originX, float originY,
                          float minDistance, float maxDistance,
                          float preferredAngle, float angleSpread,
                          float& x, float& y) const;
    bool chooseAttentionPose(const Layout& layout, float& x, float& y) const;
    bool chooseCirclePose(const Layout& layout, uint8_t phase, float& x, float& y) const;
    bool chooseWindowGazePose(const Layout& layout, float& x, float& y) const;
    bool beginLeg(float x, float y, uint32_t nowMs);
    bool beginMove(Action action, float x, float y, uint32_t nowMs);
    bool startAttention(const Layout& layout, Direction direction, uint32_t nowMs);
    bool startWindowGaze(const Layout& layout, uint32_t nowMs);
    bool startAutonomousAction(const Layout& layout, Direction direction, uint32_t nowMs);
    void finishMovement(const Layout& layout, bool blocksFood, uint32_t nowMs);
    void finish(bool blocksFood, uint32_t nowMs);
    void face(Direction direction);
    friend struct AmoledCareTestAccess;
    Game::GameState& gameState;
    Home::Actor& homeMainActor;
    const Home::Actor& homeCompanionActor;
    Home::Simulation& homeRuntime;
    State state_;
    Effects effects_;
};

}  // namespace AmoledV1
