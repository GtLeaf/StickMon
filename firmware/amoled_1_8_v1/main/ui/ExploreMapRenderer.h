#pragma once

#include "game/ExploreRouteGeometry.h"

#include <cstdint>
#include <cmath>

#include "ui/UiMetrics.h"
#include "game/ExploreMapGenerator.h"

class Canvas565;

namespace AmoledV1 {

struct ExploreRouteViewModel;
class PixelCache565;

// The existing world cache holds night-colored static cells. Cells touched by
// actors or animation are composed in original RGB565, then tinted exactly once.
struct ExploreRouteTonePass {
    bool active = false;
    bool cells[ExploreMapGenerator::CELL_COUNT]{};
    void include(const ExploreRouteViewModel& model, AmoledUi::Rect bounds);
    void finish(Canvas565& canvas, const ExploreRouteViewModel& model) const;
};

inline constexpr int EXPLORE_ROUTE_MAP_TOP = 0;
inline constexpr int EXPLORE_ROUTE_MAP_BOTTOM = UiMetrics::HOME_STATUS_TOP;
// Exploration has its own pixel scale; room art and other screens remain 2x.
inline constexpr float EXPLORE_ROUTE_SCALE = 1.5f;

// Round the world and camera separately so every map and actor shares the
// same pixel origin, including odd camera coordinates and offscreen actors.
inline int exploreRoutePixel(float world) {
    return static_cast<int>(std::lround(world * EXPLORE_ROUTE_SCALE));
}
inline int exploreRouteScreen(float world, int camera) {
    return exploreRoutePixel(world) - exploreRoutePixel(float(camera));
}

// Native framebuffer offset from the route centerline to the pickup shadow.
// Road actors share this ground anchor; their gameplay coordinates stay intact.
inline constexpr int EXPLORE_ROUTE_ACTOR_GROUND_OFFSET_Y =
    static_cast<int>(9 * EXPLORE_ROUTE_SCALE + 0.5f);
inline constexpr float exploreRouteActorFootWorldY(const ExploreMapGenerator::Map* map, float worldY) {
    return worldY + (ExploreRouteGeometry::hasValleyForestCenterline(map) ? 0.0f : 9.0f);
}
inline int exploreRouteActorGroundScreenY(const ExploreMapGenerator::Map* map, float worldY, int cameraY) {
    return EXPLORE_ROUTE_MAP_TOP + exploreRouteScreen(worldY, cameraY) +
        (ExploreRouteGeometry::hasValleyForestCenterline(map) ? 0 : EXPLORE_ROUTE_ACTOR_GROUND_OFFSET_Y);
}

// Draw the map background and animated tiles within the caller's clip region.
// Actors, HUD and overlays are rendered by the route screen; depth-sorted map
// objects are restored around each actor by the function below.
bool drawExploreRouteMapLayer(Canvas565& canvas,
                              const ExploreRouteViewModel& model,
                              PixelCache565& exploreRouteWorldCache,
                              uint16_t rowBegin, uint16_t rowEnd,
                              ExploreRouteTonePass* tonePass = nullptr);

// Restore vegetation, rocks and complete ladder modules by actor ground depth.
// Swimming actors also stay below bridge decks regardless of their ground row.
// The rectangle and dirty rows use native framebuffer coordinates. Restores
// the caller's route-map clip after drawing.
void drawExploreRouteMapVegetationForActor(
    Canvas565& canvas, const ExploreRouteViewModel& model,
    float actorFootWorldY, int actorLeft, int actorTop,
    int actorRight, int actorBottom, uint16_t rowBegin, uint16_t rowEnd,
    bool denseGrassCover = false, bool airborne = false, bool swimming = false);

}  // namespace AmoledV1
