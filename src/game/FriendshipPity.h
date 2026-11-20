#pragma once

#include <cstdint>

#include "game/ExploreSpecialEncounter.h"
#include "game/GameState.h"

namespace FriendshipPity {

static constexpr uint8_t MAX_FAIL_COUNT = 5;
static constexpr uint16_t BONUS_PER_FAILURE_PERMILLE = 10;
static constexpr uint16_t OFFER_CHANCE_CAP_PERMILLE = 250;

constexpr uint8_t clampFailCount(uint8_t failures) {
    return failures > MAX_FAIL_COUNT ? MAX_FAIL_COUNT : failures;
}

inline uint8_t indexFor(const Game::FriendshipPityState& pity, uint16_t speciesId) {
    for (uint8_t index = 0; index < Game::FRIENDSHIP_PITY_CAPACITY; ++index) {
        const auto& entry = pity.entries[index];
        if (entry.speciesId == speciesId && entry.failures != 0) return index;
    }
    return Game::FRIENDSHIP_PITY_CAPACITY;
}

inline uint8_t failuresFor(const Game::FriendshipPityState& pity, uint16_t speciesId) {
    const uint8_t index = indexFor(pity, speciesId);
    return index < Game::FRIENDSHIP_PITY_CAPACITY ? pity.entries[index].failures : 0;
}

inline bool recordFailure(Game::FriendshipPityState& pity, uint16_t speciesId) {
    if (!speciesId) return false;
    uint8_t index = indexFor(pity, speciesId);
    uint8_t failures = 1;
    if (index < Game::FRIENDSHIP_PITY_CAPACITY) {
        failures = clampFailCount(pity.entries[index].failures);
        if (failures < MAX_FAIL_COUNT) ++failures;
        if (index == 0 && failures == pity.entries[0].failures) return false;
    } else {
        index = Game::FRIENDSHIP_PITY_CAPACITY - 1;
    }
    for (; index > 0; --index) pity.entries[index] = pity.entries[index - 1];
    pity.entries[0] = {speciesId, failures};
    return true;
}

inline bool clear(Game::FriendshipPityState& pity, uint16_t speciesId) {
    uint8_t index = indexFor(pity, speciesId);
    if (index == Game::FRIENDSHIP_PITY_CAPACITY) return false;
    for (; index + 1 < Game::FRIENDSHIP_PITY_CAPACITY; ++index)
        pity.entries[index] = pity.entries[index + 1];
    pity.entries[index] = {};
    return true;
}

inline bool sanitize(Game::FriendshipPityState& pity) {
    Game::FriendshipPityState clean{};
    uint8_t count = 0;
    for (const auto& entry : pity.entries) {
        if (!entry.speciesId || !entry.failures || failuresFor(clean, entry.speciesId)) continue;
        clean.entries[count++] = {entry.speciesId, clampFailCount(entry.failures)};
    }
    bool changed = false;
    for (uint8_t i = 0; i < Game::FRIENDSHIP_PITY_CAPACITY; ++i) {
        changed |= clean.entries[i].speciesId != pity.entries[i].speciesId ||
                   clean.entries[i].failures != pity.entries[i].failures;
    }
    if (changed) pity = clean;
    return changed;
}

inline Game::FriendshipPityState migrateLegacy(
    const uint8_t (&counts)[Game::LEGACY_FRIENDSHIP_PITY_COUNT]) {
    // Frozen v1–v10 indices. Those saves contain counts but no recency.
    const Game::FriendshipPityEntry legendary[] = {
        {ExploreSpecial::MEW, counts[35]},
        {ExploreSpecial::LATIAS, counts[36]},
        {ExploreSpecial::LATIOS, counts[37]},
    };
    Game::FriendshipPityState result{};
    uint8_t index = 0;
    for (const auto& entry : legendary) {
        if (entry.failures) result.entries[index++] = {entry.speciesId, clampFailCount(entry.failures)};
    }
    return result;
}

constexpr uint16_t bonusPermille(uint8_t failures) {
    return BONUS_PER_FAILURE_PERMILLE * clampFailCount(failures);
}

constexpr uint16_t chanceWithBonus(uint16_t baseChancePermille, uint8_t failures) {
    return baseChancePermille + bonusPermille(failures) >= OFFER_CHANCE_CAP_PERMILLE
        ? OFFER_CHANCE_CAP_PERMILLE
        : static_cast<uint16_t>(baseChancePermille + bonusPermille(failures));
}

// Roll this conditional bonus only if the ordinary offer checks failed.
constexpr uint16_t conditionalBonusPermille(uint16_t baseChancePermille,
                                            uint16_t finalChancePermille) {
    return finalChancePermille <= baseChancePermille || baseChancePermille >= 1000
        ? 0
        : static_cast<uint16_t>(
              (static_cast<uint32_t>(finalChancePermille - baseChancePermille) * 1000U +
               (1000U - baseChancePermille) / 2U) /
              (1000U - baseChancePermille));
}

} // namespace FriendshipPity
