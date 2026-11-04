#pragma once

#include <cstdint>
#include "game/ExploreEditorTiles.h"
#include "game/ExploreRotatedTiles.h"
#include "game/ExploreAncientValleyLayouts.h"
#include "game/ExploreValleyData.h"
#include "game/ExploreMistForestData.h"

// Tile IDs refer to the shared Outside atlas, not GameAssets::Kind values.
// Objects are drawn in the map base, then restored inside each actor's bounds
// only when that actor's ground anchor is behind the object's ground row.
namespace ExploreTileSemantics {

enum class ActorRole : uint8_t { WILDLIFE, BOSS, COMPANION, LEADER };

struct ActorDepth {
    ActorRole role;
    float groundWorldY;
    uint8_t index = 0;
    bool belowBridge = false;  // Restore decks before drawing their occupants.
};

inline void sortActorsByGroundDepth(ActorDepth* actors, uint8_t count) {
    for (uint8_t index = 1; index < count; ++index) {
        const ActorDepth current = actors[index];
        uint8_t insertion = index;
        while (insertion > 0) {
            const ActorDepth& previous = actors[insertion - 1];
            const bool comesBefore = current.belowBridge != previous.belowBridge ?
                current.belowBridge : (current.groundWorldY < previous.groundWorldY ||
                (current.groundWorldY == previous.groundWorldY &&
                 static_cast<uint8_t>(current.role) <
                     static_cast<uint8_t>(previous.role)));
            if (!comesBefore) break;
            actors[insertion] = previous;
            --insertion;
        }
        actors[insertion] = current;
    }
}

constexpr bool bridgeDeck(uint16_t tileId) {
    return tileId == 1627 || tileId == 1643 || ExploreValleyData::flag(tileId, 16);
}

constexpr int foregroundDepthOffsetRows(uint16_t tileId) {
    if (ExploreMistForestData::flag(tileId,32)) return ExploreMistForestData::tile(tileId)->depth;
    if (ExploreValleyData::flag(tileId, 32)) return ExploreValleyData::tile(tileId)->depth;
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

constexpr bool denseGrass(uint16_t tileId) {
    return tileId == 390 || tileId == 399;
}

constexpr int objectDepthOffsetRows(uint16_t tileId, uint8_t layer) {
    if (const auto* tile = ExploreRotatedTiles::tile(tileId)) return (layer == 1 || (layer == 2 && tileId >= 13099 && tileId <= 13106)) ? tile->depth : 0;
    if (const auto* tile = ExploreEditorTiles::tile(tileId)) return layer == 1 ? tile->depth : 0;
    if (ExploreMistForestData::tile(tileId)) return layer ? ExploreMistForestData::tile(tileId)->depth : 0;
    if (ExploreValleyData::tile(tileId)) return layer ? ExploreValleyData::tile(tileId)->depth : 0;
    const int vegetation = foregroundDepthOffsetRows(tileId);
    if (vegetation) return vegetation;
    if (layer == 0) return 0;
    const int valleyDepth = ExploreAncientValley::depthRows(tileId);
    if (valleyDepth) return valleyDepth;
    if (tileId == 4772 && layer == 2) return 2;
    if (tileId == 4773 && layer == 1) return 1;
    if (tileId >= 4512 && tileId <= 4523) return 1;
    if (layer == 2 && tileId == 4543) return 2;
    if (tileId == 859 || tileId == 1231 || tileId == 1532 ||
        (tileId >= 4507 && tileId <= 4510) ||
        (tileId >= 4542 && tileId <= 4544)) return 1;
    if (tileId == 1506 || tileId == 1507 || tileId == 4541) return 2;
    if (tileId == 1514 || tileId == 1515) return 1;
    if (tileId >= 4532 && tileId <= 4540) return 3 - (tileId - 4532) / 3;
    return 0;
}

constexpr bool upwardLadder(uint16_t tileId, uint8_t layer) {
    return (tileId == 4772 && layer == 2) || (tileId == 4773 && layer == 1);
}

constexpr bool objectOccludes(uint16_t tileId, uint8_t layer, int tileRow,
                              float groundY, int tileSize) {
    const int rows = objectDepthOffsetRows(tileId, layer);
    // Both native ladder pieces share the center of the lower landing as
    // their ground anchor. Neither piece is an unconditional foreground tile.
    const bool ladder = upwardLadder(tileId, layer);
    const float groundRow = tileRow + rows - (ladder ? 0.5f : 0.0f);
    return rows && groundY < groundRow * tileSize;
}

// These decorations leave ground behind their visible contour. Large rock
// hills, water rocks, cliffs, portals and all other overlays remain blocked.
constexpr bool groundBehindDecoration(uint16_t tileId, uint8_t layer) {
    if (ExploreRotatedTiles::tile(tileId)) return layer == 1 || (layer == 2 && tileId >= 13099 && tileId <= 13106);
    if (ExploreEditorTiles::tile(tileId)) return layer == 1;
    if (ExploreMistForestData::tile(tileId)) return layer && ExploreMistForestData::flag(tileId,1);
    if (ExploreValleyData::tile(tileId)) return layer && ExploreValleyData::flag(tileId, 1);
    return (layer != 0 && ExploreAncientValley::caveRock(tileId)) ||
        (layer == 1 && foregroundDepthOffsetRows(tileId) != 0) ||
        (layer != 0 && (tileId == 859 || tileId == 1231 ||
            (tileId >= 4507 && tileId <= 4510) ||
            (tileId >= 4542 && tileId <= 4544) || tileId == 4541));
}

static_assert(objectOccludes(4772, 2, 3, 116.99f, 26));
static_assert(objectOccludes(4773, 1, 4, 116.99f, 26));
static_assert(!objectOccludes(4772, 2, 3, 117.0f, 26));
static_assert(!objectOccludes(4773, 1, 4, 117.0f, 26));
static_assert(vegetationOccludes(391, 1, 3, 129.0f, 26));
static_assert(!vegetationOccludes(391, 1, 3, 130.0f, 26));
static_assert(foregroundDepthOffsetRows(804) == 3 &&
              foregroundDepthOffsetRows(812) == 2 &&
              foregroundDepthOffsetRows(820) == 1);
static_assert(!depthSortedVegetation(399, 0) &&
              !depthSortedVegetation(391, 0));

}  // namespace ExploreTileSemantics
