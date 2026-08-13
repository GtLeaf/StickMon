#pragma once

#include <cstdint>

#include "game/GameState.h"

namespace ExploreItemProgression {

// Area 0 is always available. Each following area requires every preceding
// boss to be defeated, so corrupted/debug progress cannot skip a locked area.
inline uint8_t unlockedArea(const Game::GameState& state) {
    uint8_t area = 0;
    while (area + 1 < Game::EXPLORE_AREA_COUNT &&
           state.explorePoolRerollCounts[area] > 0) {
        ++area;
    }
    return area;
}

inline bool isAreaUnlocked(uint8_t area, const Game::GameState& state) {
    return area < Game::EXPLORE_AREA_COUNT && area <= unlockedArea(state);
}

inline uint8_t visibleAreaCount(const Game::GameState& state) {
    uint8_t count = static_cast<uint8_t>(unlockedArea(state) + 1);
    return count < Game::EXPLORE_AREA_COUNT
        ? static_cast<uint8_t>(count + 1)
        : count;
}

// Shop tiers follow the deepest defeated boss. This deliberately differs from
// route access: maps stay sequential, while a debug/imported deeper clear also
// unlocks every earlier product tier as specified by the item progression.
inline uint8_t shopUnlockedArea(const Game::GameState& state) {
    uint8_t area = 0;
    for (uint8_t defeatedArea = 0;
         defeatedArea < Game::EXPLORE_AREA_COUNT; ++defeatedArea) {
        if (state.explorePoolRerollCounts[defeatedArea] == 0) continue;
        uint8_t nextArea = static_cast<uint8_t>(defeatedArea + 1);
        if (nextArea >= Game::EXPLORE_AREA_COUNT) {
            nextArea = Game::EXPLORE_AREA_COUNT - 1;
        }
        if (nextArea > area) area = nextArea;
    }
    return area;
}

// The first shop tier contains only the care essentials needed to sustain a
// new save. Food variety and optional care products follow exploration
// progress instead of flooding the daily shelf on day one.
inline uint8_t unlockAreaForItem(Game::ItemId item) {
    switch (item) {
    case Game::ItemId::NORMAL_FOOD:
    case Game::ItemId::POTION:
    case Game::ItemId::ANTIDOTE:
    case Game::ItemId::HONEY:
    case Game::ItemId::SOAP_0:
        return 0;

    case Game::ItemId::TASTY_FOOD:
    case Game::ItemId::SWEET_FOOD:
    case Game::ItemId::SPICY_FOOD:
    case Game::ItemId::PARALYZE_HEAL:
    case Game::ItemId::AWAKENING:
    case Game::ItemId::WATER_STONE:
    case Game::ItemId::MAX_REPEL:
    case Game::ItemId::SOAP_1:
        return 1;

    case Game::ItemId::SOUR_FOOD:
    case Game::ItemId::BITTER_FOOD:
    case Game::ItemId::DRY_FOOD:
    case Game::ItemId::SUPER_POTION:
    case Game::ItemId::BURN_HEAL:
    case Game::ItemId::THUNDER_STONE:
    case Game::ItemId::REVIVE:
    case Game::ItemId::SOAP_2:
        return 2;

    case Game::ItemId::ICE_HEAL:
    case Game::ItemId::FULL_HEAL:
    case Game::ItemId::FIRE_STONE:
    case Game::ItemId::HEART_SCALE:
    case Game::ItemId::CANDY:
        return 3;

    case Game::ItemId::MAX_POTION:
    case Game::ItemId::FULL_RESTORE:
        return 4;

    case Game::ItemId::NUGGET:
    case Game::ItemId::BIG_PEARL:
    case Game::ItemId::STAR_PIECE:
        return 5;

    case Game::ItemId::COUNT:
        return Game::EXPLORE_AREA_COUNT;
    }
    return Game::EXPLORE_AREA_COUNT;
}

inline bool isShopItemUnlocked(Game::ItemId item,
                               const Game::GameState& state) {
    return unlockAreaForItem(item) <= shopUnlockedArea(state);
}

} // namespace ExploreItemProgression
