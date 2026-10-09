#pragma once

#include <cstdint>

namespace Home {

enum class VisitArrivalPhase : uint8_t {
    NONE,
    HOST_APPROACH,
    DOOR_OPEN,
    HOST_CLEAR,
    GUEST_ENTER,
    MEETING,
    TALK,
    COMPLETE,
};

enum class VisitArrivalEvent : uint8_t {
    HOST_READY,
    DOOR_OPENED,
    GUEST_ENTERING,
    GUEST_INSIDE,
    TALK_STARTED,
    TALK_FINISHED,
    TALK_SKIPPED,
};

class VisitArrivalFlow {
public:
    static constexpr uint32_t DOOR_OPEN_MS = 420U;
    static constexpr uint32_t MOTION_TIMEOUT_MS = 8000U;

    void begin(uint32_t nowMs) {
        phase_ = VisitArrivalPhase::HOST_APPROACH;
        phaseStartedMs_ = nowMs;
    }

    void reset() {
        phase_ = VisitArrivalPhase::NONE;
        phaseStartedMs_ = 0;
    }

    VisitArrivalPhase phase() const { return phase_; }

    bool doorOpenReady(uint32_t nowMs) const {
        return phase_ == VisitArrivalPhase::DOOR_OPEN &&
               nowMs - phaseStartedMs_ >= DOOR_OPEN_MS;
    }

    bool advance(VisitArrivalEvent event, uint32_t nowMs) {
        VisitArrivalPhase next = phase_;
        switch (event) {
        case VisitArrivalEvent::HOST_READY:
            if (phase_ != VisitArrivalPhase::HOST_APPROACH) return false;
            next = VisitArrivalPhase::DOOR_OPEN;
            break;
        case VisitArrivalEvent::DOOR_OPENED:
            if (!doorOpenReady(nowMs)) return false;
            next = VisitArrivalPhase::HOST_CLEAR;
            break;
        case VisitArrivalEvent::GUEST_ENTERING:
            if (phase_ != VisitArrivalPhase::HOST_CLEAR) return false;
            next = VisitArrivalPhase::GUEST_ENTER;
            break;
        case VisitArrivalEvent::GUEST_INSIDE:
            if (phase_ != VisitArrivalPhase::GUEST_ENTER) return false;
            next = VisitArrivalPhase::MEETING;
            break;
        case VisitArrivalEvent::TALK_STARTED:
            if (phase_ != VisitArrivalPhase::MEETING) return false;
            next = VisitArrivalPhase::TALK;
            break;
        case VisitArrivalEvent::TALK_FINISHED:
            if (phase_ != VisitArrivalPhase::TALK) return false;
            next = VisitArrivalPhase::COMPLETE;
            break;
        case VisitArrivalEvent::TALK_SKIPPED:
            if (phase_ != VisitArrivalPhase::MEETING &&
                phase_ != VisitArrivalPhase::TALK) return false;
            next = VisitArrivalPhase::COMPLETE;
            break;
        }
        phase_ = next;
        phaseStartedMs_ = nowMs;
        return true;
    }

private:
    VisitArrivalPhase phase_ = VisitArrivalPhase::NONE;
    uint32_t phaseStartedMs_ = 0;
};

}  // namespace Home
