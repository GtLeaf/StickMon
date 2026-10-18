#pragma once

#include <cstdint>

#include "ui/UiMetrics.h"

class Canvas565;

namespace AmoledV1 {

struct ExploreRouteViewModel;
class PixelCache565;

inline constexpr int EXPLORE_ROUTE_MAP_TOP = 0;
inline constexpr int EXPLORE_ROUTE_MAP_BOTTOM = UiMetrics::HOME_STATUS_TOP;

// Native framebuffer offset from the route centerline to the pickup shadow.
// Road actors share this ground anchor; their gameplay coordinates stay intact.
inline constexpr int EXPLORE_ROUTE_ACTOR_GROUND_OFFSET_Y = 18;
inline constexpr float exploreRouteActorFootWorldY(float worldY) {
    return worldY + EXPLORE_ROUTE_ACTOR_GROUND_OFFSET_Y /
        float(AmoledUi::RESOURCE_SCALE);
}

// Draw the map background and animated tiles within the caller's clip region.
// Actors, foreground tiles, HUD and overlays are rendered by the route screen.
bool drawExploreRouteMapLayer(Canvas565& canvas,
                              const ExploreRouteViewModel& model,
                              PixelCache565& exploreRouteWorldCache,
                              uint16_t rowBegin, uint16_t rowEnd);

// Draw map tiles that must appear in front of route actors, such as crystal
// tops, within the caller's clip region.
void drawExploreRouteMapForegroundLayer(Canvas565& canvas,
                                        const ExploreRouteViewModel& model,
                                        uint16_t rowBegin, uint16_t rowEnd);

// Restore grass, tree, shrub and rock pixels using the actor's ground depth.
// Swimming actors also stay below bridge decks regardless of their ground row.
// The rectangle and dirty rows use native framebuffer coordinates. Restores
// the caller's route-map clip after drawing.
void drawExploreRouteMapVegetationForActor(
    Canvas565& canvas, const ExploreRouteViewModel& model,
    float actorFootWorldY, int actorLeft, int actorTop,
    int actorRight, int actorBottom, uint16_t rowBegin, uint16_t rowEnd,
    bool denseGrassCover = false, bool airborne = false, bool swimming = false);

}  // namespace AmoledV1
