#include "ui/compact_touch/ExploreUi.h"

#include "ui/PageProfile.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include "core/AppSceneFlow.h"
#include "hardware/Hal.h"
#include "ui/compact_touch/ExploreHudUi.h"
#include "ui/compact_touch/HudControls.h"
#if STICKMON_UI_ROUND_TOUCH
#include "ui/round_touch/TouchTutorial.h"
#include "ui/round_touch/ExploreRouteUi.h"
#include "ui/round_touch/ExploreBattleUi.h"
#endif

namespace Ui {
namespace {
ExploreRouteLayout routeLayout(ExplorePhase phase) {
#if STICKMON_UI_ROUND_TOUCH
    return RoundExplore::routeLayout(phase);
#else
    (void)phase;
    return {Ui::Page::WIDTH, Ui::Page::HEIGHT, true, true, 1.0f / 6.0f};
#endif
}
ExploreBattleLayout battleLayout() {
#if STICKMON_UI_ROUND_TOUCH
    return RoundExplore::battleLayout();
#else
    return {Ui::Page::WIDTH, Ui::Page::HEIGHT, 28, true, 10, true};
#endif
}
enum Target {
    TARGET_WALK = 100, TARGET_COMMAND = 110, TARGET_MENU = 120, TARGET_SPECIAL = 130, TARGET_FRIEND = 132,
    TARGET_PICKUP = 140, TARGET_RESULT, TARGET_END, TARGET_DEFEAT, TARGET_OPEN_MENU
};
int touchContext(const ExploreInputFrame& frame) {
    return static_cast<int>(frame.phase) | (int(frame.menuOpen) << 8) |
        (int(frame.battleBusy) << 9) | (int(frame.defeatAwaitInput) << 10) |
        (int(frame.friendshipConfirmation) << 11) | (int(frame.iceSliding) << 12) |
        (int(frame.routeBlocked) << 13);
}
}

void ExploreUi::render() {
#if STICKMON_UI_ROUND_TOUCH
    if (host.exploreInputFrame().phase == ExplorePhase::SELECT) {
        selection.render(host.exploreRouteFrame());
        return;
    }
    selection.release();
#endif
    renderWith(routeLayout(host.exploreInputFrame().phase), battleLayout());
}

bool ExploreUi::onButton(const ButtonEvent& event) {
    cancelTouch();
#if STICKMON_UI_ROUND_TOUCH
    const auto frame = host.exploreInputFrame();
    if (frame.phase == ExplorePhase::SELECT) {
        if (event.action == BtnAction::LONG_PRESS && (event.btn == 0 || event.btn == 1))
            return selection.tap(host, state, RoundExplore::BACK);
        if (event.action != BtnAction::PRESSED) return false;
        if (event.btn == 0) return selection.tap(host, state, RoundExplore::DEPART);
        if (event.btn == 1 && frame.visibleAreaCount) {
            state.areaCursor = (state.areaCursor + 1) % frame.visibleAreaCount;
            state.areaAnimCursor = state.areaCursor;
            return host.activateExplore(ExploreAction::AREA_CHANGED, state.areaCursor);
        }
        return false;
    }
#endif
    return ExploreUiBase::onButton(event);
}

void ExploreUi::drawTutorial(const ExploreTutorialFrame& frame) const {
#if STICKMON_UI_ROUND_TOUCH
    RoundTutorial::explore(frame);
#else
    ExploreUiBase::drawTutorial(frame);
#endif
}

int ExploreUi::touchTarget(int16_t x, int16_t y) {
    const auto frame = host.exploreInputFrame();
#if STICKMON_UI_ROUND_TOUCH
    if (frame.phase == ExplorePhase::SELECT) return selection.target(frame, state, x, y);
#endif
    auto layout = routeLayout(frame.phase);
#if STICKMON_UI_ROUND_TOUCH
    if (RoundExplore::nativeBattle(frame.phase)) {
        layout.width = 360; layout.height = 360;
    }
#endif
    if (x < 0 || x >= layout.width || y < 0 || y >= layout.height) return -1;
#if STICKMON_UI_ROUND_TOUCH
    if ((RoundExplore::nativeRoute(frame.phase) || RoundExplore::nativeBattle(frame.phase)) &&
        !RoundExplore::routePoint(x, y)) return -1;
#endif
    const auto battle = battleLayout();
    if (frame.phase == ExplorePhase::WALKING) {
        if (frame.iceSliding || frame.routeBlocked) return -1;
#if STICKMON_UI_ROUND_TOUCH
        if (RoundExplore::ROUTE_MENU.contains(x, y)) return TARGET_OPEN_MENU;
        if (frame.menuOpen) {
            for (int i = 0; i < AppSceneFlow::exploreMenuItemCount(); ++i)
                if (RoundExplore::routeMenuItem(i).contains(x, y)) return TARGET_MENU + i;
            return -1;
        }
        return RoundExplore::routeHudHit(x, y, host.exploreRouteFrame().teamHudCount)
            ? -1 : TARGET_WALK;
#else
        if (!frame.menuOpen)
            return HudControls::menuBounds().contains(x, y) ? TARGET_OPEN_MENU : TARGET_WALK;
        for (int i = 0; i < AppSceneFlow::exploreMenuItemCount(); ++i)
            if (ExploreHudUi::menuItem(i).contains(x, y)) return TARGET_MENU + i;
        return -1;
#endif
    }
    if (frame.phase == ExplorePhase::ENCOUNTER) {
        if (frame.battleBusy) return -1;
        if (frame.defeatAwaitInput) return TARGET_DEFEAT;
        if (host.exploreBattleFrame().battleLogVisibleCount > 0) return -1;
        for (int i = 0; i < 4; ++i)
            if (battle.commandTarget(i).contains(x, y)) return TARGET_COMMAND + i;
        return -1;
    }
    if (frame.phase == ExplorePhase::SPECIAL_PROMPT) {
        const auto panel = layout.specialPrompt();
        if (!panel.contains(x, y) || y < panel.y + 50) return -1;
        return TARGET_SPECIAL + (x >= panel.x + panel.width / 2);
    }
    if (frame.phase == ExplorePhase::FRIENDSHIP && frame.friendshipConfirmation) {
        const auto panel = battle.friendshipPrompt();
        for (int i = 0; i < 2; ++i) {
            const MenuGrid::Rect row{panel.x + 2, panel.y + 3 + i * battle.friendshipRowHeight,
                                     panel.width - 4, battle.friendshipRowHeight - 2};
            if (row.contains(x, y)) return TARGET_FRIEND + i;
        }
        return -1;
    }
    if (frame.phase == ExplorePhase::PICKUP)
        return layout.pickupPrompt().contains(x, y) ? TARGET_PICKUP : -1;
    if (frame.phase == ExplorePhase::RESULT)
        return layout.resultPrompt().contains(x, y) ? TARGET_RESULT : -1;
    if (frame.phase == ExplorePhase::ENDING)
        return layout.endPrompt().contains(x, y) ? TARGET_END : -1;
    if (frame.phase != ExplorePhase::SELECT || x >= ExploreRouteLayout::AREA_LIST_WIDTH) return -1;
    for (uint8_t i = 0; i <= frame.visibleAreaCount; ++i) {
        if (fabsf(i - state.areaAnimCursor) > ExploreRouteLayout::AREA_VISIBLE_DISTANCE) continue;
        const int center = layout.areaRowCenter(i, state.areaAnimCursor);
        // Leave four pixels between adjacent targets; only drawn rows can be tapped.
        if (y >= center - 15 && y < center + 15)
            return i < frame.visibleAreaCount ? i : frame.areaCount;
    }
    return -1;
}

bool ExploreUi::onTap(int target) {
    const auto frame = host.exploreInputFrame();
#if STICKMON_UI_ROUND_TOUCH
    if (frame.phase == ExplorePhase::SELECT) return selection.tap(host, state, target);
#endif
    if (frame.phase == ExplorePhase::WALKING && !frame.iceSliding && !frame.routeBlocked) {
#if STICKMON_UI_ROUND_TOUCH
        if (frame.menuOpen && target == TARGET_OPEN_MENU)
            return host.activateExplore(ExploreAction::MENU_RETURN_PAUSE);
#endif
        if (!frame.menuOpen && target == TARGET_OPEN_MENU) {
            state.exploreMenuCursor = 0;
            return host.activateExplore(ExploreAction::OPEN_MENU);
        }
        if (!frame.menuOpen && target == TARGET_WALK) return host.activateExplore(ExploreAction::WALK);
        if (frame.menuOpen && target >= TARGET_MENU && target < TARGET_MENU + AppSceneFlow::exploreMenuItemCount()) {
            state.exploreMenuCursor = static_cast<uint8_t>(target - TARGET_MENU);
            if (AppSceneFlow::exploreMenuEntry(state.exploreMenuCursor).item ==
                AppSceneFlow::ExploreMenuItem::BACK)
                return host.activateExplore(ExploreAction::MENU_RETURN_PAUSE);
            return host.activateExplore(ExploreAction::MENU_ITEM, state.exploreMenuCursor);
        }
    }
    if (frame.phase == ExplorePhase::ENCOUNTER && !frame.battleBusy) {
        if (frame.defeatAwaitInput && target == TARGET_DEFEAT)
            return host.activateExplore(ExploreAction::BATTLE_DEFEAT);
        if (!frame.defeatAwaitInput && host.exploreBattleFrame().battleLogVisibleCount == 0 &&
            target >= TARGET_COMMAND && target < TARGET_COMMAND + 4) {
            state.battleCursor = static_cast<uint8_t>(target - TARGET_COMMAND);
            return host.activateExplore(ExploreAction::BATTLE_COMMAND, state.battleCursor);
        }
    }
    if (frame.phase == ExplorePhase::SPECIAL_PROMPT && (target == TARGET_SPECIAL || target == TARGET_SPECIAL + 1)) {
        state.specialChallengeYes = target == TARGET_SPECIAL;
        return host.activateExplore(ExploreAction::SPECIAL_CONFIRM, state.specialChallengeYes);
    }
    if (frame.phase == ExplorePhase::FRIENDSHIP && frame.friendshipConfirmation &&
        (target == TARGET_FRIEND || target == TARGET_FRIEND + 1)) {
        state.friendshipConfirmYes = target == TARGET_FRIEND;
        return host.activateExplore(ExploreAction::FRIENDSHIP_CONFIRM, state.friendshipConfirmYes);
    }
    if ((frame.phase == ExplorePhase::PICKUP && target == TARGET_PICKUP) ||
        (frame.phase == ExplorePhase::RESULT && target == TARGET_RESULT) ||
        (frame.phase == ExplorePhase::ENDING && target == TARGET_END))
        return onButton({0, BtnAction::PRESSED});
    if (frame.phase != ExplorePhase::SELECT || target < 0 ||
        (target >= frame.visibleAreaCount && target != frame.areaCount)) return false;
    if (target == frame.areaCount) return host.activateExplore(ExploreAction::AREA_BACK);
    if (target == state.areaCursor)
        return onButton({0, BtnAction::PRESSED});
    state.areaCursor = static_cast<uint8_t>(target);
    return host.activateExplore(ExploreAction::AREA_CHANGED, state.areaCursor);
}

void ExploreUi::clearAreaTouch() {
    areaTap.cancel();
    areaTouchActive = areaDragging = areaDragDirty = false;
}

void ExploreUi::cancelTouch() {
#if STICKMON_UI_ROUND_TOUCH
    selection.cancel();
#endif
    if (areaTouchActive && areaDragging) state.areaAnimCursor = areaTouchAnchor;
    clearAreaTouch();
    routeTap.cancel();
    routeTouchContext = -1;
}

bool ExploreUi::onRouteTouch(const Platform::IInputDevice::TouchEvent& event,
                            const ExploreInputFrame& frame) {
    using Type = Platform::IInputDevice::TouchType;
    if (event.type == Type::DOWN) {
        cancelTouch();
        routeTouchContext = touchContext(frame);
    } else if (event.type == Type::CANCEL || routeTouchContext != touchContext(frame)) {
        cancelTouch();
        return false;
    }
    const int tap = routeTap.update(event, touchTarget(event.x, event.y));
    if (event.type != Type::UP) return false;
    // Clear before dispatch: return, defeat and debug actions may destroy this UI.
    routeTouchContext = -1;
    return tap >= 0 && onTap(tap);
}

ExploreCursorUpdate ExploreUi::updateAreaCursor(uint8_t areaCount,
    uint8_t visibleCount, uint32_t nowMs, uint32_t previewStarted, bool hasPreview) {
#if STICKMON_UI_ROUND_TOUCH
    ExploreCursorUpdate roundResult;
    if (state.areaCursor >= visibleCount) {
        state.areaCursor = 0;
        roundResult.redraw = roundResult.moved = true;
    }
    state.areaAnimCursor = state.areaCursor;
    updateAreaPreviewVisual(roundResult, areaCount, nowMs, previewStarted, hasPreview);
    return roundResult;
#endif
    if (!areaTouchActive)
        return ExploreUiBase::updateAreaCursor(areaCount, visibleCount,
            nowMs, previewStarted, hasPreview);
    ExploreCursorUpdate result;
    result.moved = result.redraw = areaDragDirty;
    areaDragDirty = false;
    updateAreaPreviewVisual(result, areaCount, nowMs, previewStarted, hasPreview);
    return result;
}

bool ExploreUi::onTouch(const Platform::IInputDevice::TouchEvent& event) {
    using Type = Platform::IInputDevice::TouchType;
    const auto frame = host.exploreInputFrame();
#if STICKMON_UI_ROUND_TOUCH
    if (frame.phase == ExplorePhase::SELECT) return selection.touch(host, state, event);
#endif
    const float target = state.areaCursor < frame.areaCount
        ? state.areaCursor : frame.visibleAreaCount;
    const bool interrupted = areaTouchActive &&
        (areaDragging || state.areaAnimCursor != target);
    if (frame.phase != ExplorePhase::SELECT) {
        if (areaTouchActive) cancelTouch();
        const bool changed = onRouteTouch(event, frame);
        return changed || interrupted;
    }
    routeTap.cancel();
    routeTouchContext = -1;
    if (event.type == Type::CANCEL ||
        event.x < 0 || event.x >= ExploreRouteLayout::AREA_LIST_WIDTH ||
        event.y < 0 || event.y >= Ui::Page::HEIGHT) {
        cancelTouch();
        return interrupted;
    }
    if (event.type == Type::DOWN) {
        cancelTouch();
        areaTouchActive = true;
        areaTouchX = event.x;
        areaTouchY = event.y;
        areaTouchAnchor = state.areaAnimCursor;
        areaTap.update(event, touchTarget(event.x, event.y));
        return interrupted;
    }
    if (!areaTouchActive) return false;
    const int dx = event.x - areaTouchX;
    const int dy = event.y - areaTouchY;
    if (!areaDragging && std::abs(dx) > 10 && std::abs(dx) >= std::abs(dy)) {
        cancelTouch();
        return interrupted;
    }
    if (!areaDragging && std::abs(dy) > 10 && std::abs(dy) > std::abs(dx)) {
        areaDragging = true;
        areaTap.cancel();
    }
    bool changed = false;
    if (areaDragging) {
        const float position = std::clamp(areaTouchAnchor -
            float(dy) / ExploreRouteLayout::AREA_ROW_SPACING, 0.0f, float(frame.visibleAreaCount));
        changed = position != state.areaAnimCursor;
        state.areaAnimCursor = position;
        areaDragDirty = areaDragDirty || changed;
    }
    const int tap = areaTap.update(event, touchTarget(event.x, event.y));
    if (event.type != Type::UP) return changed;
    const bool dragged = areaDragging;
    // Keep the release position for snapping; cancel/lock instead restores the anchor.
    clearAreaTouch();
    if (dragged) {
        const int next = std::clamp(int(std::lround(state.areaAnimCursor)),
            0, int(frame.visibleAreaCount));
        const uint8_t target = next < frame.visibleAreaCount ? uint8_t(next) : frame.areaCount;
        if (target == state.areaCursor) return true;
        state.areaCursor = target;
        return host.activateExplore(ExploreAction::AREA_CHANGED, target);
    }
    // Clear all gesture state before an activation that may destroy the scene.
    const bool resume = state.areaAnimCursor != target;
    return (tap >= 0 && onTap(tap)) || resume;
}
}  // namespace Ui
