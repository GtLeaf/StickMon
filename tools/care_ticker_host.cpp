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
    if (Game::isScheduledSleepMinute(19U * 60U, 0) ||
        Game::isScheduledSleepMinute(21U * 60U + 59U, 0) ||
        !Game::isScheduledSleepMinute(22U * 60U, 0)) {
        return fail(1, "baseline sleep schedule must start at 22:00");
    }

    Game::GameState state;
    state.teamCount = 2;
    state.team[0].satiety = 75;
    state.team[1].satiety = 60;
    state.team[1].origin = Game::Origin::BEFRIENDED;

    Game::CareTickAccumulators acc{};
    Game::applyCareMinutes(state, acc, 1, 1, true);
    if (state.team[0].satiety != 74 ||
        state.team[1].satiety != 59) {
        return fail(2, "both formal team members consume hunger");
    }

    state.team[1].origin = Game::Origin::VISITOR;
    state.team[1].satiety = 60;
    acc = Game::CareTickAccumulators{};
    Game::applyCareMinutes(state, acc, 1, 1, true);
    if (state.team[1].satiety != 60) {
        return fail(3, "temporary visitors must not consume hunger");
    }

    state.team[0].hpMax = 100;
    state.team[0].hpCur = 50;
    state.team[0].satiety = 50;
    acc = Game::CareTickAccumulators{};
    Game::CareTickResult care = Game::applyCareMinutes(
        state, acc, 0, 1, true);
    if (state.team[0].hpCur != 51 || !care.hpChanged || !care.stateChanged) {
        return fail(4, "home recovery must restore one percent per game minute");
    }

    state.team[0].hpCur = 50;
    state.team[0].satiety = 0;
    acc = Game::CareTickAccumulators{};
    Game::applyCareMinutes(state, acc, 0, 1, true);
    if (state.team[0].hpCur != 51) {
        return fail(5, "empty hunger must keep slower home recovery");
    }

    state.team[0].hpMax = 157;
    state.team[0].hpCur = 100;
    state.team[0].satiety = 50;
    acc = Game::CareTickAccumulators{};
    Game::applyCareMinutes(state, acc, 0, 1, true);
    if (state.team[0].hpCur != 102) {
        return fail(6, "one-percent recovery must round up to a whole HP point");
    }

    state.team[0].hpCur = 100;
    acc = Game::CareTickAccumulators{};
    Game::applyCareMinutes(state, acc, 1, 0, true);
    if (state.team[0].hpCur != 100) {
        return fail(7, "real minutes alone must not trigger HP recovery");
    }

    state.team[0].hpCur = 154;
    acc = Game::CareTickAccumulators{};
    Game::applyCareMinutes(state, acc, 0, 3, true);
    if (state.team[0].hpCur != state.team[0].hpMax) {
        return fail(8, "multi-minute recovery must apply every tick and clamp to max HP");
    }

    state.storageCount = 1;
    state.careDay = 0;
    state.gameMinutesTotal = Game::GAME_MINUTES_PER_DAY;
    state.pairMoodRewardsToday = 3;
    state.candyPurchasesToday = Game::DAILY_CANDY_PURCHASE_CAP;
    state.storage[0].petCountToday =
        Game::Bond::inviteLockMarker(1);
    if (!Game::resetDailyCareCounters(state) ||
        state.pairMoodRewardsToday != 0 ||
        state.candyPurchasesToday != 0 ||
        state.storage[0].petCountToday !=
            Game::Bond::inviteLockMarker(1)) {
        return fail(9, "daily reset must preserve invitation lock marker");
    }

    std::printf("[care_ticker_host] all tests passed\n");
    return 0;
}
