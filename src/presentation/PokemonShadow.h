#pragma once
#include <algorithm>
#include <cmath>
#include "assets/PokemonSprites.h"

namespace PokemonShadow {

struct Geometry {
    int x, y, radiusX, radiusY;
};

struct Anchor { int x, y; };

// groundY uses the existing actor/visible-bottom anchor. Body hops and flight
// offsets are deliberately excluded; registration only moves the shadow.
inline Geometry atGround(const PokemonSprites::SpriteFrame* frame,
                         int centerX, int groundY, float scale,
                         bool flipX = false) {
    const auto shadow = PokemonSprites::frameShadow(frame, flipX);
    return {centerX + int(std::lround(shadow.offsetX * scale)),
            groundY + int(std::lround((shadow.offsetY -
                PokemonSprites::frameGroundOffsetY(frame)) * scale)),
            std::max(1, int(std::lround(shadow.radiusX * scale))),
            std::max(1, int(std::lround(shadow.radiusY * scale)))};
}

// Position the sprite so its registered shadow lands on the requested point.
inline Anchor spriteAnchorForShadow(const PokemonSprites::SpriteFrame* frame,
                                    int x, int y, float scale, bool flipX) {
    const auto shadow = atGround(frame, 0, 0, scale, flipX);
    return {x - shadow.x, y - shadow.y};
}

} // namespace PokemonShadow
