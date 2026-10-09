#include <cassert>
#include <cstdint>

#include "game/HomeVisitArrival.h"

int main() {
    using Home::VisitArrivalEvent;
    using Home::VisitArrivalFlow;
    using Home::VisitArrivalPhase;

    VisitArrivalFlow arrival;
    assert(!arrival.advance(VisitArrivalEvent::GUEST_ENTERING, 0));
    arrival.begin(UINT32_MAX - 200U);
    assert(arrival.phase() == VisitArrivalPhase::HOST_APPROACH);
    assert(!arrival.advance(VisitArrivalEvent::DOOR_OPENED, 0));
    assert(arrival.advance(VisitArrivalEvent::HOST_READY, UINT32_MAX - 100U));
    assert(!arrival.doorOpenReady(318U));
    assert(!arrival.advance(VisitArrivalEvent::DOOR_OPENED, 318U));
    assert(arrival.doorOpenReady(319U));
    assert(arrival.advance(VisitArrivalEvent::DOOR_OPENED, 319U));
    assert(!arrival.advance(VisitArrivalEvent::TALK_STARTED, 321U));
    assert(arrival.advance(VisitArrivalEvent::GUEST_ENTERING, 321U));
    assert(!arrival.advance(VisitArrivalEvent::TALK_STARTED, 322U));
    assert(arrival.advance(VisitArrivalEvent::GUEST_INSIDE, 323U));
    assert(arrival.advance(VisitArrivalEvent::TALK_STARTED, 324U));
    assert(arrival.advance(VisitArrivalEvent::TALK_FINISHED, 325U));
    assert(arrival.phase() == VisitArrivalPhase::COMPLETE);
    assert(!arrival.advance(VisitArrivalEvent::GUEST_ENTERING, 326U));

    arrival.begin(1000U);
    assert(arrival.advance(VisitArrivalEvent::HOST_READY, 1000U));
    assert(arrival.advance(VisitArrivalEvent::DOOR_OPENED, 1420U));
    assert(arrival.advance(VisitArrivalEvent::GUEST_ENTERING, 1421U));
    assert(arrival.advance(VisitArrivalEvent::GUEST_INSIDE, 1422U));
    assert(arrival.advance(VisitArrivalEvent::TALK_SKIPPED, 1423U));
    arrival.begin(2000U);
    assert(arrival.advance(VisitArrivalEvent::HOST_READY, 2000U));
    assert(arrival.advance(VisitArrivalEvent::DOOR_OPENED, 2420U));
    assert(arrival.advance(VisitArrivalEvent::GUEST_ENTERING, 2421U));
    assert(arrival.advance(VisitArrivalEvent::GUEST_INSIDE, 2422U));
    assert(arrival.advance(VisitArrivalEvent::TALK_STARTED, 2423U));
    assert(arrival.advance(VisitArrivalEvent::TALK_SKIPPED, 2424U));
    arrival.reset();
    assert(arrival.phase() == VisitArrivalPhase::NONE);
    assert(!arrival.doorOpenReady(2000U));
}
