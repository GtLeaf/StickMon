#pragma once

#include <cstdint>

#include "ui/UiMetrics.h"

class Canvas565;

namespace AmoledV1 {

struct ExploreRouteViewModel;
class PixelCache565;

inline constexpr int EXPLORE_ROUTE_MAP_TOP = 0;
inline constexpr int EXPLORE_ROUTE_MAP_BOTTOM = UiMetrics::HOME_STATUS_TOP;

// Draw the map background and animated tiles within the caller's clip region.
// Actors, HUD and overlays are rendered by the route screen after this layer.
bool drawExploreRouteMapLayer(Canvas565& canvas,
                              const ExploreRouteViewModel& model,
                              PixelCache565& exploreRouteWorldCache,
                              uint16_t rowBegin, uint16_t rowEnd);

}  // namespace AmoledV1
