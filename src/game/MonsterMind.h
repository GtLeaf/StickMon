#pragma once

#include <cstdint>
#include "game/GameState.h"
#include "game/Species.h"

enum class MonsterDesire : uint8_t {
    EAT,
    REST,
    WANDER,
    STARE,
};

static constexpr uint8_t MONSTER_FEED_TARGET_SATIETY = 85;
static constexpr uint8_t MONSTER_SLEEP_FOOD_WAKE_SATIETY = 35;

constexpr bool monsterShouldWakeForFood(uint8_t satiety) {
    return satiety <= MONSTER_SLEEP_FOOD_WAKE_SATIETY;
}

enum class MonsterMovementMode : uint8_t {
    NORMAL,
    STATIONARY,
};

struct MonsterBehaviorProfile {
    MonsterMovementMode movementMode = MonsterMovementMode::NORMAL;
    float moveSpeedScale = 1.0f;
    uint16_t idleMinMs = 2600;
    uint16_t idleMaxMs = 6800;
    uint8_t wanderRadiusX = 28;
    uint8_t wanderRadiusY = 16;
    uint16_t turnPauseMs = 220;
    int8_t activityBias = 0;
    int8_t sociabilityBias = 0;
};

class MonsterMind {
public:
    void reset(uint32_t nowMs);
    void update(const Game::MonsterRuntime& monster, bool sleepTime,
                bool bowlHasFood, uint32_t nowMs);
    MonsterDesire topDesire() const { return topDesire_; }
    void onActivity(uint32_t nowMs);
    void onAte(uint32_t nowMs);
    void onRested(uint32_t nowMs);

private:
    MonsterDesire topDesire_ = MonsterDesire::STARE;
    uint8_t scores_[4] = {};
    uint32_t lastActivityMs_ = 0;

    static constexpr uint8_t INERTIA_BONUS = 22;
    static constexpr uint8_t SWITCH_MARGIN = 14;
};

MonsterBehaviorProfile behaviorProfileFor(const Species& species,
                                          const Game::MonsterRuntime& monster);
