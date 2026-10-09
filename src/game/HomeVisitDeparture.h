#pragma once

#include <cstdint>

namespace Home {

enum class VisitDeparturePhase : uint8_t {
    NONE,
    WALK_TO_DOOR,
    CROSS_DOOR,
    AWAY,
};

class VisitDepartureFlow {
public:
    static VisitDeparturePhase advance(VisitDeparturePhase phase,
                                       bool arrived, uint32_t elapsedMs) {
        if (phase == VisitDeparturePhase::WALK_TO_DOOR &&
            (arrived || elapsedMs > 12000U)) {
            return VisitDeparturePhase::CROSS_DOOR;
        }
        if (phase == VisitDeparturePhase::CROSS_DOOR &&
            (arrived || elapsedMs > 5000U)) {
            return VisitDeparturePhase::AWAY;
        }
        return phase;
    }
};

}  // namespace Home
