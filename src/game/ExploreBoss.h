#pragma once

#include <cstdint>

namespace ExploreBoss {

static constexpr uint8_t AREA_COUNT = 6;
static constexpr uint8_t CANDIDATE_COUNT = 4;
static constexpr uint16_t SPAWN_ROLL_MAX = 10000;
static constexpr uint16_t SPAWN_CHANCE = 4000;
static constexpr uint8_t VICTORY_COIN_REWARD = 10;

struct Config {
    uint16_t speciesIds[CANDIDATE_COUNT];
    uint8_t level;
    uint16_t experiencePercent;
};

static constexpr Config CONFIGS[AREA_COUNT] = {
    {{12, 162, 17, 25}, 10, 200},  // Butterfree, Furret, Pidgeotto, Pikachu
    {{8, 184, 195, 279}, 17, 200},  // Wartortle, Azumarill, Quagsire, Pelipper
    {{143, 3, 26, 282}, 27, 200},  // Snorlax, Venusaur, Raichu, Gardevoir
    {{362, 94, 76, 169}, 42, 200},  // Glalie, Gengar, Golem, Crobat
    {{197, 94, 169, 212}, 53, 200}, // Umbreon, Gengar, Crobat, Scizor
    {{149, 6, 76, 169}, 67, 200},  // Dragonite, Charizard, Golem, Crobat
};

inline const Config& configForArea(uint8_t area) {
    return CONFIGS[area < AREA_COUNT ? area : 0];
}

inline uint16_t speciesForRoll(uint8_t area, uint32_t roll) {
    const Config& config = configForArea(area);
    return config.speciesIds[roll % CANDIDATE_COUNT];
}

constexpr bool canPlaceOnPath(uint8_t pointCount) {
    return pointCount >= 3;
}

constexpr uint8_t routeIndex(uint8_t pointCount) {
    return canPlaceOnPath(pointCount) ? pointCount - 2 : 0;
}

constexpr uint8_t victoryCoinReward(bool bossBattle) {
    return bossBattle ? VICTORY_COIN_REWARD : 0;
}

static_assert(SPAWN_CHANCE > 0 && SPAWN_CHANCE < SPAWN_ROLL_MAX,
              "boss chance must be optional and non-zero");
static_assert(CANDIDATE_COUNT > 1,
              "each area must have more than one possible boss species");
static_assert(CONFIGS[0].level < CONFIGS[1].level &&
                  CONFIGS[1].level < CONFIGS[2].level &&
                  CONFIGS[2].level < CONFIGS[3].level &&
                  CONFIGS[3].level < CONFIGS[4].level &&
                  CONFIGS[4].level < CONFIGS[5].level,
              "boss levels must increase with area difficulty");
static_assert(routeIndex(3) == 1 && routeIndex(12) == 10,
              "bosses must stand one step before the exit");
static_assert(victoryCoinReward(false) == 0 &&
                  victoryCoinReward(true) == VICTORY_COIN_REWARD,
              "only boss victories may award battle coins");

} // namespace ExploreBoss
