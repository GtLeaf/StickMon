#pragma once

#include <cstddef>
#include <cstdint>

#include "game/GameState.h"

namespace FriendshipPity {

enum class Tier : uint8_t {
    NONE = 0,
    UNCOMMON,
    RARE,
    SPECIAL,
};

struct TrackedSpecies {
    uint16_t speciesId;
    Tier tier;
};

static constexpr uint8_t MAX_FAIL_COUNT = 5;
static constexpr uint16_t OFFER_CHANCE_CAP_PERMILLE = 250;

static constexpr TrackedSpecies TRACKED[] = {
    {172, Tier::UNCOMMON}, {280, Tier::UNCOMMON},
    {322, Tier::UNCOMMON}, {41, Tier::UNCOMMON},
    {17, Tier::UNCOMMON}, {184, Tier::UNCOMMON},
    {279, Tier::UNCOMMON}, {18, Tier::UNCOMMON},
    {26, Tier::UNCOMMON}, {92, Tier::UNCOMMON},
    {282, Tier::UNCOMMON}, {362, Tier::UNCOMMON},

    {133, Tier::RARE}, {134, Tier::RARE},
    {135, Tier::RARE}, {136, Tier::RARE},
    {196, Tier::RARE}, {197, Tier::RARE},
    {123, Tier::RARE}, {147, Tier::RARE},
    {148, Tier::RARE}, {149, Tier::RARE},
    {281, Tier::RARE}, {130, Tier::RARE},
    {25, Tier::RARE},

    {1, Tier::RARE}, {2, Tier::RARE}, {3, Tier::RARE},
    {4, Tier::RARE}, {5, Tier::RARE}, {6, Tier::RARE},
    {7, Tier::RARE}, {8, Tier::RARE}, {9, Tier::RARE},

    {143, Tier::SPECIAL}, {151, Tier::SPECIAL},
    {380, Tier::SPECIAL}, {381, Tier::SPECIAL},
};

static_assert(sizeof(TRACKED) / sizeof(TRACKED[0]) ==
                  Game::FRIENDSHIP_PITY_TRACKED_COUNT,
              "friendship pity save slots must match tracked species");

constexpr int8_t indexFor(uint16_t speciesId, size_t index = 0) {
    return index == Game::FRIENDSHIP_PITY_TRACKED_COUNT
        ? -1
        : TRACKED[index].speciesId == speciesId
            ? static_cast<int8_t>(index)
            : indexFor(speciesId, index + 1);
}

constexpr Tier tierAt(uint8_t index) {
    return index < Game::FRIENDSHIP_PITY_TRACKED_COUNT
        ? TRACKED[index].tier
        : Tier::NONE;
}

constexpr uint16_t stepPermille(Tier tier) {
    return tier == Tier::UNCOMMON ? 5
        : tier == Tier::RARE ? 10
        : tier == Tier::SPECIAL ? 15
        : 0;
}

constexpr uint8_t clampFailCount(uint8_t failCount) {
    return failCount > MAX_FAIL_COUNT ? MAX_FAIL_COUNT : failCount;
}

constexpr uint16_t bonusPermille(Tier tier, uint8_t failCount) {
    return static_cast<uint16_t>(
        stepPermille(tier) * clampFailCount(failCount));
}

constexpr uint16_t chanceWithBonus(uint16_t baseChancePermille,
                                   Tier tier, uint8_t failCount) {
    return baseChancePermille >= OFFER_CHANCE_CAP_PERMILLE
        ? OFFER_CHANCE_CAP_PERMILLE
        : baseChancePermille + bonusPermille(tier, failCount) >=
                  OFFER_CHANCE_CAP_PERMILLE
            ? OFFER_CHANCE_CAP_PERMILLE
            : static_cast<uint16_t>(
                  baseChancePermille + bonusPermille(tier, failCount));
}

// 原结交判定失败后，以条件概率补足到目标最终概率。
constexpr uint16_t conditionalBonusPermille(uint16_t baseChancePermille,
                                            uint16_t finalChancePermille) {
    return finalChancePermille <= baseChancePermille ||
                   baseChancePermille >= 1000
        ? 0
        : static_cast<uint16_t>(
              (static_cast<uint32_t>(
                   finalChancePermille - baseChancePermille) * 1000U +
               (1000U - baseChancePermille) / 2U) /
              (1000U - baseChancePermille));
}

} // namespace FriendshipPity
