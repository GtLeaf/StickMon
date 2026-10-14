#include "game/HatchService.h"
#include "game/CareTicker.h"
#include "game/GameRandom.h"
#include "game/Species.h"

#include <cassert>
#include <cstring>

int main() {
    Game::GameState state;
    state.oobeDone = true;
    state.storageCount = 12;
    state.coins = 9876;
    Game::HatchService::initializeNewGame(state);
    assert(!state.oobeDone && state.teamCount == 0 && state.storageCount == 0);
    assert(state.coins == 0 && state.gameMinutesTotal == 7 * 60);
    assert(state.hatchSeconds == 180 && state.activeSlot == 0);
    assert(state.bag.potion == 0 && state.bag.candy == 0 && state.bag.revive == 0);
    for (auto food : state.room.food) assert(food == 0);

    using namespace Game::HatchService;
    assert(starterStyle(0, 0) == 0);
    assert(starterStyle(9, 0) == 0 && starterStyle(0, 9) == 0);
    assert(starterStyle(5, 5) == 0);
    assert(starterStyle(6, 4) == 1 && starterStyle(4, 6) == 2);
    assert(starterStyle(10, 0) == 1 && starterStyle(0, 10) == 2);
    assert(starterStyle(65535, 65535) == 0);
    assert(starterStyle(65535, 65534) == 1);

    for (uint8_t style : {0, 1, 2, 99}) {
        initializeNewGame(state);
        state.gameMinutesTotal = 543;
        state.settings.speedIndex = 2;
        GameRandom::seed(1234 + style);
        assert(finish(state, style));
        assert(state.oobeDone && state.teamCount == 1 && state.activeSlot == 0);
        const auto& monster = state.team[0];
        assert(monster.speciesId == (style == 1 ? 4 : style == 2 ? 7 : 1));
        assert(monster.level == 5 && monster.hpCur == monster.hpMax);
        assert(monster.hpMax == maxHpFor(*findSpecies(monster.speciesId), monster));
        assert(monster.ivPacked != 0 && monster.move1Id != 0);
        assert(monster.origin == Game::Origin::HATCHED);
        assert(monster.metArea == Game::MET_AREA_HATCHED);
        assert(monster.metAt == Game::gameSecondsForMinutes(543));
        assert(monster.lastSeenAt == monster.metAt);
        assert(monster.lastExploredAt == monster.metAt);
        assert(monster.lastWindowGazeAt == monster.metAt);
        assert(state.coins == 1000 && state.room.food[0] == 2);
        assert(state.bag.potion == 2 && state.bag.candy == 1 && state.bag.revive == 2);
        assert(state.settings.speedIndex == 2 && state.gameMinutesTotal == 543);
        const auto saved = state;
        assert(!finish(state, 2));
        assert(std::memcmp(&state, &saved, sizeof(state)) == 0);
    }

    assert(progressPercent(0, 180) == 0 && remainingSeconds(0, 180) == 180);
    assert(progressPercent(90, 180) == 50 && remainingSeconds(90, 180) == 90);
    assert(progressPercent(179.1f, 180) == 99 && remainingSeconds(179.1f, 180) == 1);
    assert(progressPercent(180, 180) == 100 && remainingSeconds(180, 180) == 0);
    assert(progressPercent(999, 180) == 100 && remainingSeconds(999, 180) == 0);
    assert(progressPercent(-1, 180) == 0 && remainingSeconds(-1, 180) == 180);
    assert(progressPercent(0, 0) == 100 && remainingSeconds(0, 0) == 0);
    assert(shakeOffset(0, 0) == -3 && shakeOffset(0, 46) == 3);
    assert(shakeOffset(0, 231) == 0 && shakeOffset(0, 2400) == -3);
    assert(shakeOffset(100, 500) == -3);
    return 0;
}
