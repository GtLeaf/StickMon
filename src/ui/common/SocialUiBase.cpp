#include "ui/common/SocialUiBase.h"

namespace Ui {
using State = SocialState;

void SocialUiBase::enter(const Communication::VisitSessionService::ViewModel& model,
                         uint32_t nowMs) {
    state = SocialUiState{};
    state.lastRoomCount = model.roomCount;
    state.stateStartedMs = nowMs;
}

void SocialUiBase::stateChanged(SocialState next, uint32_t nowMs) {
    state.stateStartedMs = nowMs;
    state.roomCursor = 0;
    if (next == State::WAITING_HOST_DECISION) state.cursor = 0;
}

void SocialUiBase::update(const Communication::VisitSessionService::ViewModel& model,
                          uint32_t nowMs, RenderDemand& demand) {
    if (state.lastRoomCount != model.roomCount) {
        state.lastRoomCount = model.roomCount;
        demand.redraw();
    }
    if (model.state == State::HOSTING || model.state == State::SEARCHING ||
        model.state == State::JOINING || model.state == State::SYNCING ||
        model.state == State::WAITING_ACCEPT ||
        model.state == State::WAITING_HOST_DECISION) {
        const uint32_t visualTick = nowMs / SOCIAL_DOTS_INTERVAL_MS;
        if (visualTick != state.lastVisualTick) {
            state.lastVisualTick = visualTick;
            demand.redraw();
        }
        demand.wakeIn(66);
    } else if (model.state == State::ROOM_LIST ||
               model.state == State::ACTIVE || model.state == State::ENDING) {
        demand.wakeIn(200);
    }
}

bool SocialUiBase::onButton(const ButtonEvent& event) {
    const auto model = host.socialFrame().model;
    if ((event.btn == 0 || event.btn == 1) &&
        event.action == BtnAction::LONG_PRESS) {
        return host.activateSocial(SocialAction::BACK);
    }
    if (event.action != BtnAction::PRESSED) return false;

    if (model.state == State::IDLE) {
        if (event.btn == 1) {
            state.cursor = (state.cursor + 1) % 3;
        } else if (event.btn == 0) {
            if (state.cursor == 0) return host.activateSocial(SocialAction::HOST);
            if (state.cursor == 1) return host.activateSocial(SocialAction::SEARCH);
            return host.activateSocial(SocialAction::BACK);
        }
        return true;
    }
    if (model.state == State::ROOM_LIST) {
        if (event.btn == 1) {
            state.roomCursor = (state.roomCursor + 1) % (model.roomCount + 1);
        } else if (event.btn == 0) {
            if (state.roomCursor == model.roomCount) return host.activateSocial(SocialAction::STOP);
            return host.activateSocial(SocialAction::SELECT_ROOM, state.roomCursor);
        }
        return true;
    }
    if (model.state == State::WAITING_HOST_DECISION) {
        if (event.btn == 1) state.cursor = (state.cursor + 1) % 2;
        else if (event.btn == 0) return host.activateSocial(SocialAction::DECIDE, state.cursor == 0);
        return true;
    }
    if (event.btn == 0) {
        if (model.state == State::ACTIVE) return host.activateSocial(SocialAction::END_VISIT);
        if (model.state != State::ENDING) return host.activateSocial(SocialAction::STOP);
        return true;
    }
    return false;
}

}  // namespace Ui
