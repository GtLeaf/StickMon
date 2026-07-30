#include <cstdio>

#include "game/BondSystem.h"
#include "game/CareTicker.h"

namespace {

int fail(int code, const char* message) {
    std::printf("[care_ticker_host] FAIL %d: %s\n", code, message);
    return code;
}

}  // namespace

int main() {
    Game::GameState state;
    state.teamCount = 2;
    state.team[0].satiety = 75;
    state.team[1].satiety = 60;
    state.team[1].origin = Game::Origin::BEFRIENDED;

    Game::CareTickAccumulators acc{};
    Game::applyCareMinutes(state, acc, 1, 1.0f, true);
    if (state.team[0].satiety != 74 ||
        state.team[1].satiety != 59) {
        return fail(1, "both formal team members consume hunger");
    }

    state.team[1].origin = Game::Origin::VISITOR;
    state.team[1].satiety = 60;
    acc = Game::CareTickAccumulators{};
    Game::applyCareMinutes(state, acc, 1, 1.0f, true);
    if (state.team[1].satiety != 60) {
        return fail(2, "temporary visitors must not consume hunger");
    }

    state.storageCount = 1;
    state.careDay = 0;
    state.gameMinutesTotal = Game::GAME_MINUTES_PER_DAY;
    state.pairMoodRewardsToday = 3;
    state.storage[0].petCountToday =
        Game::Bond::inviteLockMarker(1);
    if (!Game::resetDailyCareCounters(state) ||
        state.pairMoodRewardsToday != 0 ||
        state.storage[0].petCountToday !=
            Game::Bond::inviteLockMarker(1)) {
        return fail(3, "daily reset must preserve invitation lock marker");
    }

    std::printf("[care_ticker_host] all tests passed\n");
    return 0;
}
