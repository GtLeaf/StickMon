#pragma once

#include <cstdint>

#include "assets/GameAssets.h"
#include "game/GameState.h"

namespace ExploreAreaCatalog {

inline constexpr uint8_t recommendedLevel(uint8_t area) {
    return area == 1 ? 12
         : area == 2 ? 22
         : area == 3 ? 34
         : area == 4 ? 47
         : area == 5 ? 60
                     : 5;
}

inline constexpr uint16_t fieldColor(uint8_t area) {
    return area == 1 ? 0x224A
         : area == 2 ? 0x2A66
         : area == 3 ? 0xB6DB
         : area == 4 ? 0x1945
         : area == 5 ? 0x1987
                     : 0x2227;
}

// The six exploration areas intentionally share four battle backdrops. Keep
// this mapping beside the shared area progression data so every frontend uses
// the same visual scene for a given area.
inline constexpr GameAssets::Kind battleBackground(uint8_t area) {
    return area == 1 || area == 5 ? GameAssets::Kind::BATTLE_BG_RIVERSIDE
         : area == 3 ? GameAssets::Kind::BATTLE_BG_SNOW
         : area == 4 ? GameAssets::Kind::BATTLE_BG_DEEP_FOREST
                     : GameAssets::Kind::BATTLE_BG_GRASS;
}

static_assert(recommendedLevel(0) == 5 && recommendedLevel(5) == 60,
              "explore area levels must match progression tuning");
static_assert(fieldColor(0) == 0x2227 && fieldColor(5) == 0x1987,
              "explore field colors must match route rendering");
static_assert(battleBackground(0) == GameAssets::Kind::BATTLE_BG_GRASS &&
                  battleBackground(1) == GameAssets::Kind::BATTLE_BG_RIVERSIDE &&
                  battleBackground(2) == GameAssets::Kind::BATTLE_BG_GRASS &&
                  battleBackground(3) == GameAssets::Kind::BATTLE_BG_SNOW &&
                  battleBackground(4) == GameAssets::Kind::BATTLE_BG_DEEP_FOREST &&
                  battleBackground(5) == GameAssets::Kind::BATTLE_BG_RIVERSIDE,
              "all six explore areas must have a battle backdrop");

}  // namespace ExploreAreaCatalog
