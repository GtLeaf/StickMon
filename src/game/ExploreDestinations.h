#pragma once

#include "game/ExploreItemProgression.h"

// Display order is independent of persistent exploration-area IDs.
namespace ExploreDestinations {
constexpr uint8_t MARKET = 0xfe, INVALID = 0xff;
// Reorder this table to move destinations without migrating saves.
inline constexpr uint8_t ORDER[] = {MARKET, 0, 1, 2, 3, 4, 5};
constexpr uint8_t COUNT = sizeof(ORDER) / sizeof(ORDER[0]);
constexpr uint8_t id(uint8_t index) { return index < COUNT ? ORDER[index] : INVALID; }
constexpr uint8_t indexOf(uint8_t value) {
    for (uint8_t i = 0; i < COUNT; ++i) if (ORDER[i] == value) return i;
    return COUNT;
}
inline bool visible(uint8_t value, const Game::GameState& state) {
    return value == MARKET || value < ExploreItemProgression::visibleAreaCount(state);
}
inline uint8_t visibleCount(const Game::GameState& state) {
    uint8_t count = 0;
    for (auto value : ORDER) count += visible(value, state);
    return count;
}
inline uint8_t visibleId(uint8_t index, const Game::GameState& state) {
    for (auto value : ORDER) if (visible(value, state)) {
        if (index == 0) return value;
        --index;
    }
    return INVALID;
}
inline uint8_t visibleIndex(uint8_t value, const Game::GameState& state) {
    uint8_t index = 0;
    for (auto item : ORDER) if (visible(item, state)) {
        if (item == value) return index;
        ++index;
    }
    return COUNT;
}
inline bool unlocked(uint8_t value, const Game::GameState& state) {
    return value == MARKET || ExploreItemProgression::isAreaUnlocked(value, state);
}
} // namespace ExploreDestinations
