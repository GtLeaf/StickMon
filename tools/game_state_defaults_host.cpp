#include "game/GameState.h"

#include <cassert>

int main() {
    Game::GameState state;
    assert(Game::INITIAL_GAME_MINUTES == 7U * 60U);
    assert(state.gameMinutesTotal == Game::INITIAL_GAME_MINUTES);
    return 0;
}
