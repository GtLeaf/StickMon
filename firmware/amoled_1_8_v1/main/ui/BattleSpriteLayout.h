#pragma once

#include <algorithm>
#include <cmath>

#include "AmoledGeometry.h"
#include "assets/PokemonMotion.h"
#include "assets/PokemonBattleSizes.h"

namespace AmoledV1 {

inline constexpr int WILD_SPRITE_AREA_WIDTH = 176;
inline constexpr int PLAYER_SPRITE_AREA_WIDTH = 152;
inline constexpr int BATTLE_SPRITE_AREA_HEIGHT = 144;
inline constexpr int PLAYER_SPRITE_TARGET_HEIGHT = 104;

struct BattleSpriteLayout {
    float scale = 0.0f;
    AmoledUi::Rect rect{};
    int verticalOffset = 0;
};

inline int battleSpriteAirLift(uint16_t speciesId) {
    const auto air = PokemonMotion::airProfileForSpecies(speciesId);
    if (air.height <= 0.0f) return 0;
    return std::max(24, static_cast<int>(std::lround(
        air.height * AmoledUi::RESOURCE_SCALE)));
}

inline BattleSpriteLayout fitBattleSpriteAtScale(int width, int height, int groundPadding,
                                               int centerX, int groundY, int areaWidth,
                                               int areaHeight, float scale) {
    if (width <= 0 || height <= 0 || areaWidth <= 0 || areaHeight <= 0 ||
        !std::isfinite(scale) || scale <= 0.0f) return {};
    const int visibleHeight = std::max(1, height - std::clamp(groundPadding, 0, height - 1));
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

inline BattleSpriteLayout fitBattleSprite(int width, int height, int groundPadding,
                                         int centerX, int groundY, int areaWidth,
                                         int areaHeight, int targetHeight) {
    if (width <= 0 || height <= 0) return {};
    const int visibleHeight = std::max(1, height - std::clamp(groundPadding, 0, height - 1));
    const float scale = std::clamp(static_cast<float>(targetHeight) / visibleHeight,
                                   0.8f, 2.0f);
    return fitBattleSpriteAtScale(width, height, groundPadding, centerX, groundY,
                                 areaWidth, areaHeight, scale);
}

inline BattleSpriteLayout fitBattleFrontSprite(uint16_t speciesId, int width, int height,
                                              int groundPadding, int centerX, int groundY,
                                              int areaWidth, int areaHeight) {
    if (width <= 0 || height <= 0) return {};
    const auto natural = PokemonBattleSizes::front(speciesId);
    const int visibleHeight = std::max(1, height - std::clamp(groundPadding, 0, height - 1));
    // Keep one source-canvas scale across species instead of equalizing height.
    const float scale = natural.width && natural.height
        ? std::min(static_cast<float>(natural.width) / width,
                   static_cast<float>(natural.height) / visibleHeight)
        : 1.0f;
    const int lift = battleSpriteAirLift(speciesId);
    auto layout = fitBattleSpriteAtScale(width, height, groundPadding, centerX,
                                        groundY - lift, areaWidth, areaHeight - lift, scale);
    if (layout.scale <= 0.0f) return layout;
    // Split unused vertical space around the visible body. This keeps small
    // fronts near the area's center while retaining the airborne offset.
    const int unusedHeight = std::max(0, layout.rect.y - (groundY - areaHeight));
    layout.rect.y -= unusedHeight / 2;
    layout.verticalOffset = lift + unusedHeight / 2;
    return layout;
}

}  // namespace AmoledV1
