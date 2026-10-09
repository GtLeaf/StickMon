#include <cassert>

#include "game/HomeVisitDeparture.h"

int main() {
    using Home::VisitDepartureFlow;
    using Home::VisitDeparturePhase;

    assert(VisitDepartureFlow::advance(
        VisitDeparturePhase::NONE, true, 20000) == VisitDeparturePhase::NONE);
    assert(VisitDepartureFlow::advance(
        VisitDeparturePhase::WALK_TO_DOOR, false, 12000) ==
           VisitDeparturePhase::WALK_TO_DOOR);
    assert(VisitDepartureFlow::advance(
        VisitDeparturePhase::WALK_TO_DOOR, false, 12001) ==
           VisitDeparturePhase::CROSS_DOOR);
    assert(VisitDepartureFlow::advance(
        VisitDeparturePhase::WALK_TO_DOOR, true, 0) ==
           VisitDeparturePhase::CROSS_DOOR);
    assert(VisitDepartureFlow::advance(
        VisitDeparturePhase::CROSS_DOOR, false, 5000) ==
           VisitDeparturePhase::CROSS_DOOR);
    assert(VisitDepartureFlow::advance(
        VisitDeparturePhase::CROSS_DOOR, false, 5001) ==
           VisitDeparturePhase::AWAY);
    assert(VisitDepartureFlow::advance(
        VisitDeparturePhase::CROSS_DOOR, true, 0) ==
           VisitDeparturePhase::AWAY);
    assert(VisitDepartureFlow::advance(
        VisitDeparturePhase::AWAY, true, 20000) == VisitDeparturePhase::AWAY);
}
