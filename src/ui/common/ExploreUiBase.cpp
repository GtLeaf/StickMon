#include "ui/common/ExploreUiBase.h"

#include <cmath>
#include "core/AppSceneFlow.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TutorialOverlay.h"
#include "ui/common/ExploreRouteUiContent.h"
#include "ui/common/ExploreBattleUiContent.h"

namespace Ui {

ExploreCursorUpdate ExploreUiBase::updateAreaCursor(uint8_t areaCount,
    uint8_t visibleCount, uint32_t nowMs, uint32_t previewStarted, bool hasPreview) {
    ExploreCursorUpdate result;
    const float target = static_cast<float>(
        state.areaCursor < areaCount ? state.areaCursor : visibleCount);
    const float diff = target - state.areaAnimCursor;
    if (fabsf(diff) < 0.05f) {
        if (state.areaAnimCursor != target) {
            state.areaAnimCursor = target;
            result.moved = result.redraw = true;
        }
    } else {
        state.areaAnimCursor += diff * 0.5f;
        if (fabsf(target - state.areaAnimCursor) < 0.05f)
            state.areaAnimCursor = target;
        result.moved = result.redraw = true;
    }
    result.animating = fabsf(target - state.areaAnimCursor) >= 0.05f;
    updateAreaPreviewVisual(result, areaCount, nowMs, previewStarted, hasPreview);
    return result;
}

void ExploreUiBase::updateAreaPreviewVisual(ExploreCursorUpdate& result,
    uint8_t areaCount, uint32_t nowMs, uint32_t previewStarted, bool hasPreview) {
    if (state.areaCursor < areaCount && hasPreview) {
        const uint32_t cycle = (nowMs - previewStarted) / ExploreRouteTiming::PREVIEW_CYCLE_MS;
        if (cycle != state.areaPreviewVisualCycle) {
            state.areaPreviewVisualCycle = cycle;
            result.redraw = true;
        }
    }
}

bool ExploreUiBase::onButton(const ButtonEvent& event) {
    const ExploreInputFrame frame = host.exploreInputFrame();
    const bool pressed = event.action == BtnAction::PRESSED;
    const bool held = event.action == BtnAction::LONG_PRESS;
    if (frame.phase == ExplorePhase::ENDING) {
        if ((event.btn == 0 || event.btn == 1) && pressed)
            return host.activateExplore(ExploreAction::RETURN_HOME);
        return true;
    }
    if (frame.phase == ExplorePhase::EXITING) return true;
    if (frame.phase == ExplorePhase::PICKUP) {
        if (event.btn == 0 && pressed)
            return host.activateExplore(ExploreAction::RESUME_WALK);
        return true;
    }
    if (frame.phase == ExplorePhase::FRIENDSHIP) {
        if (!pressed) return true;
        if (event.btn == 1 && frame.friendshipConfirmation)
            state.friendshipConfirmYes = !state.friendshipConfirmYes;
        else if (event.btn == 0 && frame.friendshipConfirmation)
            return host.activateExplore(ExploreAction::FRIENDSHIP_CONFIRM,
                                        state.friendshipConfirmYes);
        return true;
    }
    if (frame.phase == ExplorePhase::SPECIAL_PROMPT) {
        if (!pressed) return true;
        if (event.btn == 1) state.specialChallengeYes = !state.specialChallengeYes;
        else if (event.btn == 0)
            return host.activateExplore(ExploreAction::SPECIAL_CONFIRM,
                                        state.specialChallengeYes);
        return true;
    }
    if (frame.phase == ExplorePhase::LEVEL_UP || frame.phase == ExplorePhase::EVOLUTION ||
        frame.phase == ExplorePhase::EVOLUTION_CANCELLED ||
        frame.phase == ExplorePhase::LEARN_MOVE || frame.phase == ExplorePhase::MOVE_REPLACED) {
        if (!pressed) return true;
    }
    if (frame.phase == ExplorePhase::ENCOUNTER && held) return true;
    if (frame.phase == ExplorePhase::WALKING && frame.iceSliding &&
        (event.btn == 0 || event.btn == 1)) return true;
    if (frame.phase == ExplorePhase::WALKING && frame.menuOpen) {
        if (held) return true;
        if (!pressed) return false;
        if (event.btn == 0)
            return host.activateExplore(ExploreAction::MENU_ITEM, state.exploreMenuCursor);
        if (event.btn == 1)
            state.exploreMenuCursor = (state.exploreMenuCursor + 1) %
                AppSceneFlow::exploreMenuItemCount();
        return true;
    }
    if (frame.phase == ExplorePhase::WALKING && event.btn == 1 && pressed) {
        state.exploreMenuCursor = 0;
        return host.activateExplore(ExploreAction::OPEN_MENU);
    }
    if (frame.phase != ExplorePhase::WALKING &&
        (event.btn == 0 || event.btn == 1) && held)
        return host.activateExplore(ExploreAction::EXIT);
    if (!pressed) return false;
    if (frame.phase == ExplorePhase::SELECT) {
        if (event.btn == 0)
            return host.activateExplore(state.areaCursor >= frame.areaCount
                ? ExploreAction::AREA_BACK : ExploreAction::DEPART, state.areaCursor);
        if (event.btn == 1) {
            if (state.areaCursor >= frame.areaCount) {
                state.areaCursor = 0;
                state.areaAnimCursor = 0;
            } else if (state.areaCursor + 1 < frame.visibleAreaCount) {
                ++state.areaCursor;
            } else state.areaCursor = frame.areaCount;
            return host.activateExplore(ExploreAction::AREA_CHANGED, state.areaCursor);
        }
    }
    if (frame.phase == ExplorePhase::WALKING && event.btn == 0)
        return host.activateExplore(ExploreAction::WALK);
    if (frame.phase == ExplorePhase::ENCOUNTER) {
        if (frame.battleBusy) return true;
        if (frame.defeatAwaitInput)
            return host.activateExplore(ExploreAction::BATTLE_DEFEAT);
        if (event.btn == 0)
            return host.activateExplore(ExploreAction::BATTLE_COMMAND, state.battleCursor);
        if (event.btn == 1) {
            state.battleCursor = (state.battleCursor + 1) % 4;
            return host.activateExplore(ExploreAction::BATTLE_CURSOR);
        }
    }
    if (frame.phase == ExplorePhase::LEVEL_UP || frame.phase == ExplorePhase::EVOLUTION ||
        frame.phase == ExplorePhase::EVOLUTION_CANCELLED ||
        frame.phase == ExplorePhase::LEARN_MOVE || frame.phase == ExplorePhase::MOVE_REPLACED)
        return host.handleExploreProgressionButton(event);
    if (frame.phase == ExplorePhase::RESULT) {
        if (frame.debugBattle && (event.btn == 0 || event.btn == 1))
            return host.activateExplore(ExploreAction::DEBUG_RETURN);
        if (event.btn == 0) return host.activateExplore(ExploreAction::RESULT_CONTINUE);
        if (event.btn == 1) return host.activateExplore(ExploreAction::EXIT);
    }
    return false;
}

ExploreRouteFrame ExploreUiBase::routeFrame() const {
    auto frame = host.exploreRouteFrame();
    frame.overlay = routeOverlay();
    return frame;
}

void ExploreUiBase::renderWith(const ExploreRouteLayout& routeLayout,
                               const ExploreBattleLayout& battleLayout) {
    const ExplorePhase phase = host.exploreInputFrame().phase;
    PixelRenderer::canvas().fillRect(0, 0, routeLayout.width, routeLayout.height,
                                     PixelRenderer::rgb(11, 22, 27));
    switch (phase) {
    case ExplorePhase::SELECT: {
        const auto frame = host.exploreRouteFrame();
        ExploreRouteUiContent::drawAreaMenu(frame, routeVisual, routeLayout);
        if (frame.toast) {
            auto& canvas = PixelRenderer::canvas();
            canvas.fillRoundRect(4, routeLayout.height - 29, routeLayout.width - 8, 25, 4,
                                 PixelRenderer::rgb(34, 39, 47));
            MenuGrid::label(canvas, routeLayout.width / 2, routeLayout.height - 24,
                            frame.toast, routeLayout.width - 16, 0xFFFF);
        }
        break;
    }
    case ExplorePhase::WALKING: case ExplorePhase::EXITING:
        ExploreRouteUiContent::drawWalking(routeFrame(), routeVisual, routeLayout);
        break;
    case ExplorePhase::ENCOUNTER:
        ExploreBattleUiContent::drawEncounter(host.exploreBattleFrame(), battleLayout);
        break;
    case ExplorePhase::FRIENDSHIP: {
        const auto frame = host.exploreBattleFrame();
        ExploreBattleUiContent::drawEncounter(frame, battleLayout);
        ExploreBattleUiContent::drawFriendshipPrompt(frame, battleLayout);
        break;
    }
    case ExplorePhase::SPECIAL_PROMPT: case ExplorePhase::PICKUP: case ExplorePhase::ENDING: {
        const auto frame = routeFrame();
        ExploreRouteUiContent::drawWalking(frame, routeVisual, routeLayout);
        if (phase == ExplorePhase::SPECIAL_PROMPT)
            ExploreRouteUiContent::drawSpecialPrompt(frame, routeLayout);
        else if (phase == ExplorePhase::PICKUP)
            ExploreRouteUiContent::drawPickupPrompt(frame, routeLayout);
        else ExploreRouteUiContent::drawEndPrompt(frame, routeLayout);
        break;
    }
    case ExplorePhase::RESULT:
        ExploreRouteUiContent::drawResult(host.exploreRouteFrame(), routeLayout);
        break;
    default: host.renderExploreProgression(); break;
    }
    drawTutorial(host.exploreTutorialFrame());
}

void ExploreUiBase::drawTutorial(const ExploreTutorialFrame& frame) const {
    if (frame.text && frame.kind != ExploreTutorialKind::BATTLE)
        TutorialOverlay::draw(static_cast<TutorialOverlay::Button>(frame.button), frame.text);
}

}  // namespace Ui
