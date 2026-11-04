#pragma once

#include "game/ExploreTileSemantics.h"
#include "ui/common/ExploreRouteUiModel.h"

namespace Ui {

// Optional family-owned route overlays. Coordinates use the route viewport.
class ExploreRouteOverlay {
public:
    virtual ~ExploreRouteOverlay() = default;
    virtual bool drawHud(const ExploreRouteFrame&, const ExploreRouteLayout&) const {
        return false;
    }
    virtual uint8_t appendActors(ExploreTileSemantics::ActorDepth* actors,
                                uint8_t count, uint8_t capacity) const = 0;
    virtual void drawGround(const ExploreRouteFrame& frame, int cameraX, int cameraY,
                            const ExploreRouteLayout& layout) const = 0;
    virtual void drawWildlife(const ExploreRouteFrame& frame, uint8_t index,
                              int cameraX, int cameraY,
                              const ExploreRouteLayout& layout) const = 0;
    virtual void drawCover(const ExploreRouteFrame& frame, float groundY,
                           int left, int top, int right, int bottom,
                           int cameraX, int cameraY, const ExploreRouteLayout& layout,
                           bool airborne = false, bool swimming = false) const = 0;
};

}  // namespace Ui
