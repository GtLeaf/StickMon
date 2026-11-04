#pragma once

#include "ui/common/ExploreUiBase.h"
#include "ui/common/ExploreRouteOverlay.h"
#include "game/ExploreEcology.h"
#include "game/ExploreGrass.h"
#if STICKMON_UI_ROUND_TOUCH
#include "ui/round_touch/ExploreSelectUi.h"
#endif

namespace Ui {
class ExploreUi : public ExploreUiBase, private ExploreRouteOverlay {
public:
    explicit ExploreUi(ExploreHost& host) : ExploreUiBase(host) {}
    void render() override;
#if STICKMON_UI_ROUND_TOUCH
    void releaseSelectionCache() { selection.release(); }
#endif
    bool onButton(const ButtonEvent& event);
    int touchTarget(int16_t x, int16_t y) override;
    bool onTap(int target) override;
    bool onTouch(const Platform::IInputDevice::TouchEvent& event) override;
    void cancelTouch() override;
    ExploreCursorUpdate updateAreaCursor(uint8_t areaCount, uint8_t visibleCount,
        uint32_t nowMs, uint32_t previewStarted, bool hasPreview) override;
    void resetRouteVisual(const ExplorePool::Pool& pool, uint8_t hiddenMask) override;
    void updateRouteVisual(uint32_t nowMs, bool active) override;
    void revealRouteSpecies(uint16_t species) override;
    uint8_t routeSpriteSpecies(const ExplorePool::Pool& pool, uint16_t* ids,
                              uint8_t capacity) const override;
private:
#if STICKMON_UI_ROUND_TOUCH
    RoundExplore::Selection selection;
#endif
    void drawTutorial(const ExploreTutorialFrame& frame) const override;
    ExploreEcology::Simulation ecology;
    ExploreGrass::Simulation grass;
    bool routeVisualActive = false;
    const ExploreRouteOverlay* routeOverlay() const override { return this; }
    bool drawHud(const ExploreRouteFrame& frame, const ExploreRouteLayout& layout) const override;
    uint8_t appendActors(ExploreTileSemantics::ActorDepth* actors, uint8_t count,
                         uint8_t capacity) const override;
    void drawGround(const ExploreRouteFrame& frame, int cameraX, int cameraY,
                    const ExploreRouteLayout& layout) const override;
    void drawWildlife(const ExploreRouteFrame& frame, uint8_t index, int cameraX,
                      int cameraY, const ExploreRouteLayout& layout) const override;
    void drawCover(const ExploreRouteFrame& frame, float groundY,
                   int left, int top, int right, int bottom, int cameraX, int cameraY,
                   const ExploreRouteLayout& layout, bool airborne, bool swimming) const override;
    TouchTap areaTap;
    TouchTap routeTap;
    int routeTouchContext = -1;
    bool areaTouchActive = false, areaDragging = false, areaDragDirty = false;
    int16_t areaTouchX = 0, areaTouchY = 0;
    float areaTouchAnchor = 0;
    void clearAreaTouch();
    bool onRouteTouch(const Platform::IInputDevice::TouchEvent& event,
                      const ExploreInputFrame& frame);
};
}  // namespace Ui
