#include "game/GameState.h"

#include <cassert>

int main() {
    Game::GameState state;
    assert(Game::INITIAL_COINS == 1000U);
    assert(state.coins == Game::INITIAL_COINS);
    assert(Game::INITIAL_GAME_MINUTES == 7U * 60U);
    assert(state.gameMinutesTotal == Game::INITIAL_GAME_MINUTES);
    assert(state.bag.revive == 2);
    return 0;
}
