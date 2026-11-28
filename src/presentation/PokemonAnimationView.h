#pragma once

#include "assets/PokemonSprites.h"
#include "game/PokemonAnimationTiming.h"

namespace PokemonAnimationView {

// Keep the directional registration shared by drawing and dirty bounds.
inline bool animationFor(uint16_t species, PokemonSprites::WalkDirection direction,
                         bool walking, PokemonSprites::WalkingAnimation& animation) {
    if (!walking && PokemonAnimationTiming::hasTiming(species)) {
        PokemonSprites::PetAnimationProfile profile{};
        if (PokemonSprites::petAnimationProfile(species, profile) && profile.idleFrames) {
            constexpr uint8_t directions[] = {0, 2, 4, 6, 1, 3, 5, 7};
            uint8_t index = directions[static_cast<uint8_t>(direction) % 8];
            const bool flip = profile.mirrorRightDirections && index >= 5;
            if (flip) index = 8 - index;
            animation = {static_cast<PokemonSprites::SpriteKind>(
                static_cast<uint16_t>(profile.idleBase) + index * profile.idleFrames),
                profile.idleFrames, flip};
            return true;
        }
    }
    return PokemonSprites::walkingAnimation(species, direction, animation);
}

}  // namespace PokemonAnimationView
