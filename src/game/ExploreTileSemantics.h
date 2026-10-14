#pragma once

#include <cstdint>

// Tile IDs refer to the shared Outside atlas, not GameAssets::Kind values.
// Vegetation is drawn in the map base, then restored inside each actor's
// bounds only when that actor's feet are behind the vegetation's ground row.
namespace ExploreTileSemantics {

enum class ActorRole : uint8_t { BOSS, COMPANION, LEADER };

struct ActorDepth {
    ActorRole role;
    float groundWorldY;
};

inline void sortActorsByGroundDepth(ActorDepth* actors, uint8_t count) {
    for (uint8_t index = 1; index < count; ++index) {
        const ActorDepth current = actors[index];
        uint8_t insertion = index;
        while (insertion > 0) {
            const ActorDepth& previous = actors[insertion - 1];
            const bool comesBefore = current.groundWorldY < previous.groundWorldY ||
                (current.groundWorldY == previous.groundWorldY &&
                 static_cast<uint8_t>(current.role) <
                     static_cast<uint8_t>(previous.role));
            if (!comesBefore) break;
            actors[insertion] = previous;
            --insertion;
        }
        actors[insertion] = current;
    }
}

constexpr int foregroundDepthOffsetRows(uint16_t tileId) {
    if (tileId == 391) return 2;
    if ((tileId >= 800 && tileId <= 805) ||
        (tileId >= 824 && tileId <= 826)) return 3;
    if ((tileId >= 808 && tileId <= 813) ||
        (tileId >= 832 && tileId <= 834)) return 2;
    if ((tileId >= 816 && tileId <= 821) ||
        (tileId >= 840 && tileId <= 842)) return 1;
    return 0;
}

constexpr bool depthSortedVegetation(uint16_t tileId, uint8_t layer) {
    return layer == 1 && foregroundDepthOffsetRows(tileId) != 0;
}

constexpr bool vegetationOccludes(uint16_t tileId, uint8_t layer,
                                  int tileRow, float actorFootWorldY,
                                  int tileSize) {
    return depthSortedVegetation(tileId, layer) &&
           actorFootWorldY <
               (tileRow + foregroundDepthOffsetRows(tileId)) * tileSize;
}

static_assert(vegetationOccludes(391, 1, 3, 129.0f, 26));
static_assert(!vegetationOccludes(391, 1, 3, 130.0f, 26));
static_assert(foregroundDepthOffsetRows(804) == 3 &&
              foregroundDepthOffsetRows(812) == 2 &&
              foregroundDepthOffsetRows(820) == 1);
static_assert(!depthSortedVegetation(399, 0) &&
              !depthSortedVegetation(391, 0));

}  // namespace ExploreTileSemantics
