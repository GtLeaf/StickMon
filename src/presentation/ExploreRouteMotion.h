#pragma once

#include <cmath>
#include <cstdint>
#include "assets/PokemonSprites.h"

namespace ExploreRouteMotion {

inline uint8_t directionForDelta(float dx, float dy, uint8_t fallback) {
    if (fabsf(dx) < 0.01f && fabsf(dy) < 0.01f) return fallback;
    if (fabsf(dx) >= fabsf(dy)) {
        return static_cast<uint8_t>(
            dx >= 0.0f ? PokemonSprites::WalkDirection::RIGHT
                       : PokemonSprites::WalkDirection::LEFT);
    }
    return static_cast<uint8_t>(
        dy >= 0.0f ? PokemonSprites::WalkDirection::DOWN
                   : PokemonSprites::WalkDirection::UP);
}

}  // namespace ExploreRouteMotion
