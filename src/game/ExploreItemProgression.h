#pragma once

#include <cstdint>

#include "game/GameState.h"

namespace ExploreItemProgression {

// Area 0 goods are available after hatching. Defeating a boss unlocks the
// shop tier for the following area; clearing a deeper boss also implies the
// lower shop tiers so players are not forced to replay easier routes.
inline uint8_t unlockedArea(const Game::GameState& state) {
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

// Pickup-backed products use the first area whose pickup pool contains them.
// Care essentials stay in tier 0; other shop-only products fill thematic gaps.
inline uint8_t unlockAreaForItem(Game::ItemId item) {
    switch (item) {
    case Game::ItemId::NORMAL_FOOD:
    case Game::ItemId::TASTY_FOOD:
    case Game::ItemId::SWEET_FOOD:
    case Game::ItemId::SPICY_FOOD:
    case Game::ItemId::SOUR_FOOD:
    case Game::ItemId::BITTER_FOOD:
    case Game::ItemId::DRY_FOOD:
    case Game::ItemId::POTION:
    case Game::ItemId::ANTIDOTE:
    case Game::ItemId::HONEY:
    case Game::ItemId::SOAP_0:
    case Game::ItemId::SOAP_1:
    case Game::ItemId::SOAP_2:
        return 0;

    case Game::ItemId::PARALYZE_HEAL:
    case Game::ItemId::AWAKENING:
    case Game::ItemId::WATER_STONE:
    case Game::ItemId::MAX_REPEL:
        return 1;

    case Game::ItemId::SUPER_POTION:
    case Game::ItemId::BURN_HEAL:
    case Game::ItemId::THUNDER_STONE:
    case Game::ItemId::REVIVE:
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
    return unlockAreaForItem(item) <= unlockedArea(state);
}

} // namespace ExploreItemProgression
