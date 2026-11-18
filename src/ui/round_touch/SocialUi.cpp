// SPDX-License-Identifier: Apache-2.0
#include "ui/round_touch/SocialUi.h"
#include "presentation/PixelRenderer.h"
#include "presentation/VisitMenu.h"
#include <algorithm>
namespace Ui {
namespace {
bool listState(SocialState state) {
    return state == SocialState::SEARCHING || state == SocialState::ROOM_LIST;
}
} // namespace
void SocialUi::enter(const Communication::VisitSessionService::ViewModel &model, uint32_t now) {
    SocialUiBase::enter(model, now);
    cancelTouch();
    offset_ = 0;
    displayed_ = false;
    lastUpdateMs_ = now;
}
void SocialUi::stateChanged(SocialState state, uint32_t now) {
    SocialUiBase::stateChanged(state, now);
    cancelTouch();
    if (state != SocialState::ROOM_LIST)
        offset_ = 0;
    displayed_ = false;
    lastUpdateMs_ = now;
}
void SocialUi::update(const Communication::VisitSessionService::ViewModel &model, uint32_t now,
                      RenderDemand &demand) {
    SocialUiBase::update(model, now, demand);
    if (!listState(model.state)) {
        motion_.cancel();
        offset_ = 0;
        lastUpdateMs_ = now;
        return;
    }
    const int maximum = VisitMenu::roomScrollMaximum(model.roomCount);
    if (displayed_ && !VisitMenu::sameRooms(displayedModel_, model)) {
        cancelTouch();
        demand.redraw();
    }
    const uint32_t elapsed = now - lastUpdateMs_;
    lastUpdateMs_ = now;
    if (elapsed > 250)
        motion_.cancel();
    if (motion_.update(std::min<uint32_t>(elapsed, 250), maximum, offset_))
        demand.redraw();
    if (motion_.moving())
        demand.wakeIn(33);
}
uint32_t SocialUi::visualFrameIntervalMs(uint32_t) const {
    return motion_.moving() || (context_ >= 0 && listState(downModel_.state)) ? 33 : 66;
}
void SocialUi::render() {
    const auto frame = host.socialFrame();
    const int clamped = std::clamp(offset_, 0, VisitMenu::roomScrollMaximum(frame.model.roomCount));
    if (clamped != offset_) {
        offset_ = clamped;
        cancelTouch();
    }
    PixelRenderer::clear(0);
    VisitMenu::render(PixelRenderer::canvas(), frame.model, true, frame.nowMs, pressed_, offset_);
    displayedModel_ = frame.model;
    displayedOffset_ = offset_;
    displayed_ = true;
}
bool SocialUi::onTouch(const Platform::IInputDevice::TouchEvent &event) {
    using Type = Platform::IInputDevice::TouchType;
    const auto model = host.socialFrame().model;
    const int key = int(model.state) * 16 + model.roomCount;
    const bool list = listState(model.state);
    const auto viewport = VisitMenu::roomViewport();
    const MarketListScroll::Bounds bounds =
        list ? MarketListScroll::Bounds{viewport.x, viewport.y, viewport.width, viewport.height}
             : MarketListScroll::Bounds{0, 0, 0, 0};
    const int maximum = list ? VisitMenu::roomScrollMaximum(model.roomCount) : 0;
    const int target = VisitMenu::backButton(true).contains(event.x, event.y)
                           ? 100
                           : VisitMenu::itemAt(event.x, event.y, model, true, offset_);
    if (event.type == Type::CANCEL) {
        const bool changed = context_ >= 0 || motion_.moving();
        cancelTouch();
        return changed;
    }
    if (event.type == Type::DOWN) {
        clearPress();
        const bool brake = motion_.touch(event, bounds, maximum, offset_, tap_);
        if (target != 100 &&
            (!displayed_ || !VisitMenu::sameTarget(displayedModel_, model, target) ||
             (target >= 0 && target < VisitMenu::CANCEL && list && displayedOffset_ != offset_))) {
            cancelTouch();
            return brake;
        }
        context_ = key;
        downX_ = event.x;
        downY_ = event.y;
        downModel_ = model;
        if (brake)
            return true;
        pressed_ = target;
    } else {
        if (context_ != key || !VisitMenu::sameTarget(downModel_, model, pressed_) ||
            (list && !VisitMenu::sameRooms(downModel_, model))) {
            cancelTouch();
            return true;
        }
        if (motion_.touch(event, bounds, maximum, offset_, tap_)) {
            pressed_ = -1;
            if (event.type == Type::UP)
                clearPress();
            return true;
        }
        const int dx = event.x - downX_, dy = event.y - downY_;
        if (dx * dx + dy * dy > 100) {
            cancelTouch();
            return true;
        }
    }
    const bool visual = pressed_ >= 0;
    const int hit = tap_.update(event, target);
    if (event.type == Type::UP)
        clearPress();
    if (hit < 0)
        return visual;
    if (hit == 100) {
        offset_ = 0;
        return host.activateSocial(SocialAction::BACK);
    }
    if (hit == VisitMenu::CANCEL) {
        offset_ = 0;
        return host.activateSocial(SocialAction::STOP);
    }
    switch (model.state) {
    case SocialState::IDLE:
        return host.activateSocial(hit ? SocialAction::SEARCH : SocialAction::HOST);
    case SocialState::ROOM_LIST:
        return host.activateSocial(SocialAction::SELECT_ROOM, uint8_t(hit));
    case SocialState::WAITING_HOST_DECISION:
        return host.activateSocial(SocialAction::DECIDE, hit == 0);
    case SocialState::ACTIVE:
        return host.activateSocial(SocialAction::END_VISIT);
    case SocialState::FAILED:
        return host.activateSocial(VisitMenu::error(model.error).adjustTeam
                                       ? SocialAction::ADJUST_TEAM
                                       : SocialAction::STOP);
    case SocialState::ENDED:
        return host.activateSocial(SocialAction::STOP);
    default:
        return false;
    }
}
} // namespace Ui
