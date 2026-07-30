#include <cassert>

#include "game/ExploreItemEffects.h"

int main() {
    Game::ExploreItemEffects effects;

    assert(effects.activateMaxRepel());
    assert(!effects.activateMaxRepel());
    for (uint8_t step = 1; step <= Game::ExploreItemEffects::MAX_REPEL_STEPS; ++step) {
        assert(effects.repelStepsRemaining() ==
               Game::ExploreItemEffects::MAX_REPEL_STEPS - step + 1);
        effects.completeWalkStep();
    }
    assert(effects.repelStepsRemaining() == 0);
    assert(effects.activateMaxRepel());

    assert(effects.activateHoney());
    assert(!effects.activateHoney());
    assert(effects.honeyEncounterPending());
    effects.consumeHoneyEncounter();
    assert(!effects.honeyEncounterPending());
    assert(effects.activateHoney());

    effects.reset();
    assert(effects.repelStepsRemaining() == 0);
    assert(!effects.honeyEncounterPending());
    return 0;
}
