#pragma once

#include <cstdint>

namespace ExploreRunRules {

constexpr bool allowsRegionalBoss(uint8_t mapCount, uint8_t maximumMapCount) {
    return maximumMapCount <= 2 ||
           mapCount > static_cast<uint8_t>(maximumMapCount - 2);
}

constexpr bool isRecoveryStep(uint16_t completedSteps) {
    return completedSteps > 0 && completedSteps % 10 == 0;
}

constexpr bool shouldPlaceFinalReward(uint8_t currentMapIndex,
                                      uint8_t mapCount,
                                      bool regionalBossPlaced) {
    return mapCount > 0 && currentMapIndex + 1 == mapCount &&
           !regionalBossPlaced;
}

constexpr uint16_t recoveryAmount(uint16_t maximumHp) {
    return maximumHp == 0
        ? 0
        : static_cast<uint16_t>((maximumHp + 9U) / 10U);
}

constexpr uint8_t leaderSlotForHealth(bool firstHealthy,
                                      bool secondHealthy) {
    return !firstHealthy && secondHealthy ? 1 : 0;
}

constexpr bool showsHealthyFollower(bool firstHealthy,
                                    bool secondHealthy) {
    return firstHealthy && secondHealthy;
}

static_assert(!allowsRegionalBoss(4, 6) &&
                  allowsRegionalBoss(5, 6) &&
                  allowsRegionalBoss(6, 6),
              "regional bosses require one of the two longest runs");
static_assert(!isRecoveryStep(9) && isRecoveryStep(10) &&
                  isRecoveryStep(20),
              "exploration recovery must occur every ten completed steps");
static_assert(shouldPlaceFinalReward(3, 4, false) &&
                  !shouldPlaceFinalReward(2, 4, false) &&
                  !shouldPlaceFinalReward(3, 4, true),
              "only a boss-free final map may place the completion reward");
static_assert(recoveryAmount(1) == 1 && recoveryAmount(19) == 2 &&
                  recoveryAmount(100) == 10,
              "step recovery must round ten percent upward");
static_assert(leaderSlotForHealth(true, true) == 0 &&
                  leaderSlotForHealth(false, true) == 1 &&
                  leaderSlotForHealth(false, false) == 0 &&
                  showsHealthyFollower(true, true) &&
                  !showsHealthyFollower(false, true),
              "a healthy reserve must replace a fainted exploration leader");

}  // namespace ExploreRunRules
