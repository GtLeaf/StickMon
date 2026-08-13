#include <cassert>

#include "game/ExploreBossPity.h"

int main() {
    static_assert(ExploreBossPity::chanceForMisses(0) == 4000,
                  "first normal boss roll must be 40 percent");
    static_assert(ExploreBossPity::chanceForMisses(1) == 6000,
                  "second normal boss roll must be 60 percent");
    static_assert(ExploreBossPity::chanceForMisses(2) == 8000,
                  "third normal boss roll must be 80 percent");
    static_assert(ExploreBossPity::chanceForMisses(3) == 10000,
                  "fourth normal boss roll must be guaranteed");
    static_assert(ExploreBossPity::chanceForMisses(255) == 10000,
                  "corrupt counters must saturate safely");
    static_assert(!ExploreBossPity::requiresGuaranteedEligibleRun(2) &&
                      ExploreBossPity::requiresGuaranteedEligibleRun(3),
                  "100 percent pity must also guarantee an eligible route");

    Game::GameState state;
    state.normalBossPitySlotIndex = 12;
    state.normalBossMissCount[1] = 2;
    assert(!ExploreBossPity::syncSlot(state, 12));
    assert(state.normalBossMissCount[1] == 2);

    assert(ExploreBossPity::syncSlot(state, 13));
    for (uint8_t area = 0; area < Game::EXPLORE_AREA_COUNT; ++area) {
        assert(state.normalBossMissCount[area] == 0);
    }

    for (uint8_t i = 0; i < 8; ++i) {
        ExploreBossPity::increment(state, 4);
    }
    assert(state.normalBossMissCount[4] == ExploreBossPity::MAX_MISSES);
    state.normalBossMissCount[3] = 2;
    ExploreBossPity::resetArea(state, 4);
    assert(state.normalBossMissCount[4] == 0);
    assert(state.normalBossMissCount[3] == 2);
    return 0;
}
