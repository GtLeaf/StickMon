#pragma once

#include <algorithm>
#include <cstdint>

#include "game/GameState.h"

namespace ExploreEncounterRules {

inline constexpr uint8_t WILD_LEVEL_MIN = 1;
inline constexpr uint8_t WILD_LEVEL_VARIANCE = 2;

constexpr int8_t depthLevelOffset(uint16_t progressPermille, uint8_t spread) {
    return progressPermille >= 667
        ? static_cast<int8_t>(spread)
        : (progressPermille >= 333 ? 0 : -static_cast<int8_t>(spread));
}

constexpr uint8_t targetLevel(uint8_t averageLevel, uint8_t depthSpread,
                              uint8_t mapBlock, uint8_t mapBlockCount,
                              uint8_t routeIndex, uint8_t routePointCount) {
    int16_t target = averageLevel;
    if (mapBlockCount != 0 && mapBlock < mapBlockCount) {
        const uint16_t routeLength = routePointCount > 1
            ? routePointCount - 1 : 1;
        const uint16_t localProgress = std::min<uint16_t>(
            1000, static_cast<uint32_t>(routeIndex) * 1000U / routeLength);
        const uint16_t expeditionProgress = static_cast<uint16_t>(
            (static_cast<uint32_t>(mapBlock) * 1000U + localProgress) /
            mapBlockCount);
        target += depthLevelOffset(expeditionProgress, depthSpread);
    }
    return static_cast<uint8_t>(std::clamp<int16_t>(
        target, WILD_LEVEL_MIN, Game::LEVEL_MAX));
}

constexpr uint8_t levelForRoll(uint8_t minLevel, uint8_t maxLevel,
                               uint8_t targetLevel, uint8_t roll) {
    if (minLevel < WILD_LEVEL_MIN) minLevel = WILD_LEVEL_MIN;
    if (maxLevel > Game::LEVEL_MAX) maxLevel = Game::LEVEL_MAX;
    if (maxLevel < minLevel) maxLevel = minLevel;
    targetLevel = std::clamp(targetLevel, minLevel, maxLevel);

    int16_t level = targetLevel;
    if (roll < 10) level -= 2;
    else if (roll < 30) --level;
    else if (roll >= 90) level += 2;
    else if (roll >= 70) ++level;
    return static_cast<uint8_t>(std::clamp<int16_t>(
        level, minLevel, maxLevel));
}

}
