#pragma once

#include <algorithm>
#include <cmath>

#include "AmoledGeometry.h"
#include "assets/PokemonMotion.h"

namespace AmoledV1 {

inline constexpr int WILD_SPRITE_AREA_WIDTH = 176;
inline constexpr int PLAYER_SPRITE_AREA_WIDTH = 152;
inline constexpr int BATTLE_SPRITE_AREA_HEIGHT = 144;
inline constexpr int WILD_SPRITE_TARGET_HEIGHT = 96;
inline constexpr int PLAYER_SPRITE_TARGET_HEIGHT = 104;

struct BattleSpriteLayout {
    float scale = 0.0f;
    AmoledUi::Rect rect{};
};

inline int battleSpriteAirLift(uint16_t speciesId) {
    const auto air = PokemonMotion::airProfileForSpecies(speciesId);
    if (air.height <= 0.0f) return 0;
    return std::max(24, static_cast<int>(std::lround(
        air.height * AmoledUi::RESOURCE_SCALE)));
}

inline BattleSpriteLayout fitBattleSprite(int width, int height, int groundPadding,
                                         int centerX, int groundY, int areaWidth,
                                         int areaHeight, int targetHeight) {
    if (width <= 0 || height <= 0 || areaWidth <= 0 || areaHeight <= 0) return {};
    constexpr float MIN_SCALE = 0.8f;
    constexpr float MAX_SCALE = 2.0f;
    const int visibleHeight = std::max(1, height - std::clamp(groundPadding, 0, height - 1));
    float scale = std::clamp(static_cast<float>(targetHeight) / visibleHeight,
                             MIN_SCALE, MAX_SCALE);
    scale = std::min(scale, static_cast<float>(areaWidth) / width);
    scale = std::min(scale, static_cast<float>(areaHeight) / height);
    // Match the renderer's destination extents, including fractional upscaling.
    auto extent = [](int size, float value) {
        return std::max(1, static_cast<int>(value >= 1.0f
            ? std::ceil(size * value) : std::floor(size * value)));
    };
    if (extent(width, scale) > areaWidth || extent(height, scale) > areaHeight) {
        scale = std::nextafter(scale, 0.0f);
    }
    const int drawnWidth = extent(width, scale);
    const int drawnHeight = extent(height, scale);
    const int drawnVisibleHeight = extent(visibleHeight, scale);
    return {scale, {centerX - drawnWidth / 2, groundY - drawnVisibleHeight,
                    drawnWidth, drawnHeight}};
}

}  // namespace AmoledV1
