#pragma once

#include <cstdint>

namespace Game {

struct ExploreItemEffects {
    static constexpr uint8_t MAX_REPEL_STEPS = 100;

    bool activateMaxRepel() {
        if (repelSteps > 0) return false;
        repelSteps = MAX_REPEL_STEPS;
        return true;
    }

    bool activateHoney() {
        if (honeyPending) return false;
        honeyPending = true;
        return true;
    }

    uint8_t repelStepsRemaining() const { return repelSteps; }
    bool honeyEncounterPending() const { return honeyPending; }

    void completeWalkStep() {
        if (repelSteps > 0) --repelSteps;
    }

    void consumeHoneyEncounter() { honeyPending = false; }

    void reset() {
        repelSteps = 0;
        honeyPending = false;
    }

private:
    uint8_t repelSteps = 0;
    bool honeyPending = false;
};

}  // namespace Game
