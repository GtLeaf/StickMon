#pragma once

#include "ui/common/ExploreUiModel.h"

namespace Ui {

class ExploreUiBase {
public:
    explicit ExploreUiBase(ExploreHost& host) : host(host) {}
    virtual ~ExploreUiBase() = default;
    ExploreUiState state;
    ExploreRouteVisualState routeVisual;
    bool onButton(const ButtonEvent& event);
    virtual ExploreCursorUpdate updateAreaCursor(uint8_t areaCount, uint8_t visibleCount,
        uint32_t nowMs, uint32_t previewStarted, bool hasPreview);
    virtual void render() = 0;
    virtual int touchTarget(int16_t, int16_t) { return -1; }
    virtual bool onTap(int) { return false; }
    virtual bool onTouch(const Platform::IInputDevice::TouchEvent&) { return false; }
    virtual void cancelTouch() {}
    virtual void resetRouteVisual(const ExplorePool::Pool&, uint8_t) {}
    virtual void updateRouteVisual(uint32_t, bool) {}
    virtual void revealRouteSpecies(uint16_t) {}
    virtual uint8_t routeSpriteSpecies(const ExplorePool::Pool&, uint16_t*, uint8_t) const {
        return 0;
    }

protected:
    ExploreHost& host;
    virtual void drawTutorial(const ExploreTutorialFrame& frame) const;
    virtual const ExploreRouteOverlay* routeOverlay() const { return nullptr; }
    ExploreRouteFrame routeFrame() const;
    void updateAreaPreviewVisual(ExploreCursorUpdate& result, uint8_t areaCount,
        uint32_t nowMs, uint32_t previewStarted, bool hasPreview);
    void renderWith(const ExploreRouteLayout& routeLayout,
                    const ExploreBattleLayout& battleLayout);
};

}  // namespace Ui
