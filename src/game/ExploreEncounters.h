#pragma once

#include <cstddef>
#include <cstdint>

#include "game/ExplorePool.h"
#include "game/GameState.h"

namespace ExploreEncounters {

using Rarity = ExplorePool::Rarity;

struct Entry {
    uint16_t speciesId;
    uint8_t weight;
    uint8_t minLevel;
    uint8_t maxLevel;
    Rarity rarity;
};

template <size_t N>
constexpr uint16_t weightTotal(const Entry (&entries)[N], size_t index = 0) {
    return index == N
        ? 0
        : entries[index].weight + weightTotal(entries, index + 1);
}

template <size_t N>
constexpr bool levelRangesValid(const Entry (&entries)[N], size_t index = 0) {
    return index == N ||
           (entries[index].minLevel >= 1 &&
            entries[index].minLevel <= entries[index].maxLevel &&
            entries[index].maxLevel <= Game::LEVEL_MAX &&
            levelRangesValid(entries, index + 1));
}

static constexpr Entry GRASS_PATH[] = {
    {10, 18, 1, 6, Rarity::COMMON},
    {161, 19, 1, 9, Rarity::COMMON},
    {16, 17, 2, 9, Rarity::NORMAL},
    {261, 13, 2, 9, Rarity::NORMAL},
    {280, 8, 3, 9, Rarity::UNCOMMON},
    {172, 6, 3, 9, Rarity::UNCOMMON},
    {11, 15, 7, 9, Rarity::NORMAL},
    {133, 2, 3, 9, Rarity::RARE},
    {1, 1, 3, 9, Rarity::ULTRA_RARE},
    {4, 1, 3, 9, Rarity::ULTRA_RARE},
};

static constexpr Entry CREEK_SLOPE[] = {
    {194, 18, 7, 17, Rarity::COMMON},
    {298, 13, 7, 15, Rarity::NORMAL},
    {183, 8, 8, 17, Rarity::NORMAL},
    {278, 14, 7, 17, Rarity::NORMAL},
    {129, 12, 7, 17, Rarity::NORMAL},
    {74, 10, 8, 17, Rarity::NORMAL},
    {322, 6, 8, 17, Rarity::UNCOMMON},
    {41, 7, 8, 17, Rarity::UNCOMMON},
    {161, 3, 7, 14, Rarity::COMMON},
    {261, 2, 7, 17, Rarity::NORMAL},
    {16, 2, 7, 17, Rarity::NORMAL},
    {280, 2, 7, 17, Rarity::UNCOMMON},
    {147, 1, 10, 17, Rarity::RARE},
    {7, 1, 7, 15, Rarity::ULTRA_RARE},
    {5, 1, 16, 17, Rarity::ULTRA_RARE},
};

static constexpr Entry TALL_GRASS_PARK[] = {
    {12, 20, 17, 27, Rarity::COMMON},
    {285, 17, 17, 22, Rarity::COMMON},
    {25, 10, 17, 27, Rarity::VERY_RARE},
    {162, 8, 17, 27, Rarity::NORMAL},
    {281, 6, 20, 27, Rarity::RARE},
    {17, 6, 18, 27, Rarity::UNCOMMON},
    {184, 4, 18, 27, Rarity::UNCOMMON},
    {279, 4, 25, 27, Rarity::UNCOMMON},
    {130, 4, 20, 27, Rarity::RARE},
    {278, 3, 17, 24, Rarity::NORMAL},
    {322, 3, 17, 27, Rarity::UNCOMMON},
    {286, 3, 23, 27, Rarity::NORMAL},
    {26, 3, 17, 27, Rarity::UNCOMMON},
    {92, 3, 17, 24, Rarity::UNCOMMON},
    {133, 2, 17, 27, Rarity::RARE},
    {123, 1, 17, 27, Rarity::RARE},
    {2, 1, 17, 27, Rarity::ULTRA_RARE},
    {147, 1, 17, 27, Rarity::RARE},
    {8, 1, 17, 27, Rarity::ULTRA_RARE},
};

static constexpr Entry FROST_CRYSTAL_CAVE[] = {
    {361, 33, 28, 41, Rarity::COMMON},
    {42, 14, 28, 41, Rarity::NORMAL},
    {75, 12, 28, 41, Rarity::NORMAL},
    {93, 7, 28, 41, Rarity::NORMAL},
    {282, 6, 30, 42, Rarity::UNCOMMON},
    {92, 5, 28, 41, Rarity::UNCOMMON},
    {148, 5, 30, 41, Rarity::RARE},
    {41, 4, 28, 41, Rarity::UNCOMMON},
    {362, 4, 42, 42, Rarity::UNCOMMON},
    {18, 3, 36, 41, Rarity::UNCOMMON},
    {17, 3, 28, 35, Rarity::UNCOMMON},
    {281, 3, 28, 29, Rarity::RARE},
    {9, 1, 36, 41, Rarity::ULTRA_RARE},
};

static constexpr Entry MIST_FOREST_PATH[] = {
    {42, 27, 41, 53, Rarity::NORMAL},
    {286, 18, 41, 53, Rarity::NORMAL},
    {262, 15, 41, 53, Rarity::NORMAL},
    {282, 9, 41, 53, Rarity::UNCOMMON},
    {362, 7, 42, 53, Rarity::UNCOMMON},
    {93, 5, 41, 53, Rarity::NORMAL},
    {134, 3, 41, 53, Rarity::RARE},
    {135, 3, 41, 53, Rarity::RARE},
    {136, 3, 41, 53, Rarity::RARE},
    {196, 3, 41, 53, Rarity::RARE},
    {197, 3, 41, 53, Rarity::RARE},
    {148, 3, 41, 49, Rarity::RARE},
    {3, 1, 41, 53, Rarity::ULTRA_RARE},
};

static constexpr Entry ANCIENT_WATERFALL_VALLEY[] = {
    {42, 19, 53, 67, Rarity::NORMAL},
    {323, 9, 53, 67, Rarity::NORMAL},
    {195, 16, 53, 67, Rarity::COMMON},
    {130, 10, 53, 67, Rarity::RARE},
    {75, 8, 53, 67, Rarity::NORMAL},
    {279, 10, 53, 67, Rarity::UNCOMMON},
    {184, 10, 53, 67, Rarity::UNCOMMON},
    {362, 6, 53, 67, Rarity::UNCOMMON},
    {149, 3, 53, 67, Rarity::RARE},
    {262, 3, 53, 67, Rarity::NORMAL},
    {93, 3, 53, 67, Rarity::NORMAL},
    {6, 1, 53, 67, Rarity::ULTRA_RARE},
    {9, 1, 53, 67, Rarity::ULTRA_RARE},
    {3, 1, 53, 67, Rarity::ULTRA_RARE},
};

static_assert(weightTotal(GRASS_PATH) == 100, "grass path weights");
static_assert(weightTotal(CREEK_SLOPE) == 100, "creek slope weights");
static_assert(weightTotal(TALL_GRASS_PARK) == 100, "tall grass weights");
static_assert(weightTotal(FROST_CRYSTAL_CAVE) == 100, "frost cave weights");
static_assert(weightTotal(MIST_FOREST_PATH) == 100, "mist forest weights");
static_assert(weightTotal(ANCIENT_WATERFALL_VALLEY) == 100, "ancient valley weights");

static_assert(levelRangesValid(GRASS_PATH) &&
                  levelRangesValid(CREEK_SLOPE) &&
                  levelRangesValid(TALL_GRASS_PARK) &&
                  levelRangesValid(FROST_CRYSTAL_CAVE) &&
                  levelRangesValid(MIST_FOREST_PATH) &&
                  levelRangesValid(ANCIENT_WATERFALL_VALLEY),
              "explore encounter level range");

} // namespace ExploreEncounters
