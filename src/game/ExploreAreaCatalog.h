#pragma once

#include <cstdint>
#include "game/ContentData.h"

#include "assets/GameAssets.h"
#include "game/GameState.h"

namespace ExploreAreaCatalog {

struct Area { uint8_t minMapCount,maxMapCount,recommendedLevel,depthSpread;
    uint16_t encounterChance,fieldColor; GameAssets::Kind battleBackground; };
template<class V> inline void contentFields(V& v, Area& x) {
    v(x.minMapCount,x.maxMapCount,x.recommendedLevel,x.depthSpread,x.encounterChance,x.fieldColor,x.battleBackground);
}
#if STICKMON_EXTERNAL_CONTENT
inline const ContentData::Table<Area,6> AREAS{27};
inline uint8_t minMapCount(uint8_t area) { return AREAS[area<6?area:0].minMapCount; }
inline uint8_t maxMapCount(uint8_t area) { return AREAS[area<6?area:0].maxMapCount; }
inline uint8_t recommendedLevel(uint8_t area) { return AREAS[area<6?area:0].recommendedLevel; }
inline uint8_t depthSpread(uint8_t area) { return AREAS[area<6?area:0].depthSpread; }
inline uint16_t encounterChance(uint8_t area) { return AREAS[area<6?area:0].encounterChance; }
inline uint16_t fieldColor(uint8_t area) { return AREAS[area<6?area:0].fieldColor; }
inline GameAssets::Kind battleBackground(uint8_t area) { return AREAS[area<6?area:0].battleBackground; }
#else
inline constexpr uint8_t minMapCount(uint8_t area) {
    return area == 1 || area == 2 ? 4
         : area == 3 ? 5
         : area == 4 ? 6
         : area == 5 ? 7
                     : 3;
}

inline constexpr uint8_t maxMapCount(uint8_t area) {
    return area == 1 ? 5
         : area == 2 ? 6
         : area == 3 ? 7
         : area == 4 ? 8
         : area == 5 ? 9
                     : 4;
}

inline constexpr uint16_t encounterChance(uint8_t area) {
    return area == 1 ? 600
         : area == 2 ? 700
         : area == 3 ? 900
         : area == 4 ? 1100
         : area == 5 ? 1300
                     : 500;
}

inline constexpr uint8_t recommendedLevel(uint8_t area) {
    return area == 1 ? 12
         : area == 2 ? 22
         : area == 3 ? 34
         : area == 4 ? 47
         : area == 5 ? 60
                     : 5;
}

inline constexpr uint8_t depthSpread(uint8_t area) {
    return area == 1 || area == 2 ? 3
         : area == 3 || area == 4 ? 4
         : area == 5 ? 5
                     : 2;
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
static_assert(depthSpread(0) == 2 && depthSpread(1) == 3 &&
                  depthSpread(2) == 3 && depthSpread(3) == 4 &&
                  depthSpread(4) == 4 && depthSpread(5) == 5,
              "explore area depth spreads must match progression tuning");
static_assert(fieldColor(0) == 0x2227 && fieldColor(5) == 0x1987,
              "explore field colors must match route rendering");
static_assert(battleBackground(0) == GameAssets::Kind::BATTLE_BG_GRASS &&
                  battleBackground(1) == GameAssets::Kind::BATTLE_BG_RIVERSIDE &&
                  battleBackground(2) == GameAssets::Kind::BATTLE_BG_GRASS &&
                  battleBackground(3) == GameAssets::Kind::BATTLE_BG_SNOW &&
                  battleBackground(4) == GameAssets::Kind::BATTLE_BG_DEEP_FOREST &&
                  battleBackground(5) == GameAssets::Kind::BATTLE_BG_RIVERSIDE,
              "all six explore areas must have a battle backdrop");

#endif
}  // namespace ExploreAreaCatalog
