#pragma once

#include <cstdint>

namespace ExploreRunRules {

inline constexpr uint8_t COOLDOWN_STEPS = 5;
inline constexpr uint8_t MAX_ENCOUNTERS_PER_MAP = 2;
inline constexpr uint16_t MAP_PICKUP_CHANCE = 6500;
inline constexpr uint32_t MAP_GENERATION_SAFE_SEED = 1;
inline constexpr uint32_t MAP_GENERATION_RETRY_SALTS[] = {
    0, 0x6D2B79F5U, 0x9E3779B9U, 0x85EBCA6BU,
};

constexpr uint8_t mapCountForRoll(uint8_t minimum, uint8_t maximum, uint8_t roll) {
    return maximum <= minimum
        ? minimum
        : (maximum == minimum + 1
            ? (roll < 50 ? minimum : maximum)
            : (roll < 25 ? minimum : (roll < 75 ? minimum + 1 : maximum)));
}

constexpr uint8_t cooldownAfterCompletedStep(uint8_t cooldown) {
    return cooldown > 0 ? cooldown - 1 : 0;
}

constexpr uint8_t cooldownAfterCompletedSteps(uint8_t cooldown, uint8_t steps) {
    for (uint8_t i = 0; i < steps; ++i) cooldown = cooldownAfterCompletedStep(cooldown);
    return cooldown;
}

constexpr bool encounterGateOpen(uint8_t cooldown, uint8_t encounterCount) {
    return cooldown == 0 && encounterCount < MAX_ENCOUNTERS_PER_MAP;
}

constexpr bool encounterGateAllows(uint8_t cooldown, uint8_t encounterCount, bool bypassGate) {
    return bypassGate || encounterGateOpen(cooldown, encounterCount);
}

constexpr bool repelAllowsEncounter(bool guaranteed, bool repelActiveThisStep) {
    return guaranteed || !repelActiveThisStep;
}

constexpr bool canScheduleGuaranteedEncounter(uint8_t pointCount, uint8_t cooldown) {
    return pointCount >= 3 && cooldown + 1 <= pointCount - 2;
}

constexpr uint8_t guaranteedEncounterIndex(uint8_t pointCount, uint8_t cooldown) {
    return pointCount * 2 / 3 < cooldown + 1
        ? cooldown + 1
        : (pointCount * 2 / 3 > pointCount - 2 ? pointCount - 2 : pointCount * 2 / 3);
}

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
