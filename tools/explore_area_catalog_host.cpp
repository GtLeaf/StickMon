#include <cassert>

#include "game/ExploreAreaCatalog.h"
#include "game/ExploreItemProgression.h"
#include "game/ExploreRouteGeometry.h"

int main() {
    static_assert(Game::EXPLORE_AREA_COUNT == 6,
                  "update the shared explore catalog for new areas");

    const uint8_t expectedLevels[] = {5, 12, 22, 34, 47, 60};
    const uint16_t expectedFieldColors[] = {
        0x2227, 0x224A, 0x2A66, 0xB6DB, 0x1945, 0x1987,
    };
    const GameAssets::Kind expectedBattleBackgrounds[] = {
        GameAssets::Kind::BATTLE_BG_GRASS,
        GameAssets::Kind::BATTLE_BG_RIVERSIDE,
        GameAssets::Kind::BATTLE_BG_GRASS,
        GameAssets::Kind::BATTLE_BG_SNOW,
        GameAssets::Kind::BATTLE_BG_DEEP_FOREST,
        GameAssets::Kind::BATTLE_BG_RIVERSIDE,
    };
    for (uint8_t area = 0; area < Game::EXPLORE_AREA_COUNT; ++area) {
        assert(ExploreAreaCatalog::recommendedLevel(area) ==
               expectedLevels[area]);
        assert(ExploreAreaCatalog::fieldColor(area) ==
               expectedFieldColors[area]);
        assert(ExploreAreaCatalog::battleBackground(area) ==
               expectedBattleBackgrounds[area]);
    }
    assert(ExploreAreaCatalog::recommendedLevel(255) == expectedLevels[0]);

    Game::GameState state;
    assert(ExploreItemProgression::unlockedArea(state) == 0);
    assert(ExploreItemProgression::visibleAreaCount(state) == 2);
    assert(ExploreItemProgression::isAreaUnlocked(0, state));
    assert(!ExploreItemProgression::isAreaUnlocked(1, state));

    state.explorePoolRerollCounts[0] = 1;
    assert(ExploreItemProgression::unlockedArea(state) == 1);
    assert(ExploreItemProgression::visibleAreaCount(state) == 3);
    assert(ExploreItemProgression::isAreaUnlocked(1, state));

    state.explorePoolRerollCounts[2] = 1;
    assert(ExploreItemProgression::unlockedArea(state) == 1);

    for (uint8_t area = 0; area + 1 < Game::EXPLORE_AREA_COUNT; ++area) {
        state.explorePoolRerollCounts[area] = 1;
    }
    assert(ExploreItemProgression::unlockedArea(state) == 5);
    assert(ExploreItemProgression::visibleAreaCount(state) == 6);
    assert(!ExploreItemProgression::isAreaUnlocked(6, state));

    ExploreMapGenerator::Path path;
    path.pointCount = 3;
    path.points[0] = {2, 1};
    path.points[1] = {2, 2};
    path.points[2] = {3, 2};
    ExploreRouteGeometry::WorldPoint first =
        ExploreRouteGeometry::pathPoint(path, 0);
    ExploreRouteGeometry::WorldPoint second =
        ExploreRouteGeometry::pathPoint(path, 1);
    ExploreRouteGeometry::WorldPoint third =
        ExploreRouteGeometry::pathPoint(path, 2);
    assert(first.x == 78.0f && first.y == 39.0f);
    assert(second.x == 78.0f && second.y == 65.0f);
    assert(third.x == 91.0f && third.y == 65.0f);
    return 0;
}
