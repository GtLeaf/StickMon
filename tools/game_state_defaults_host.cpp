#include "game/GameState.h"

#include <cassert>
#include <cstddef>

int main() {
    Game::GameState state;
    assert(Game::INITIAL_COINS == 1000U);
    assert(state.coins == Game::INITIAL_COINS);
    assert(Game::INITIAL_GAME_MINUTES == 7U * 60U);
    assert(state.gameMinutesTotal == Game::INITIAL_GAME_MINUTES);
    assert(state.bag.revive == 2);
    assert(state.tutorialFlags == 0);
    static_assert(sizeof(Game::GameState) == 1560);
    static_assert(offsetof(Game::GameState, tutorialFlags) == 1559);
    static_assert(Game::tutorialMask(Game::TutorialStep::ROOM_FEED) == (1U << 0));
    static_assert(Game::tutorialMask(Game::TutorialStep::ROOM_PET) == (1U << 1));
    static_assert(Game::tutorialMask(Game::TutorialStep::OPEN_MENU) == (1U << 2));
    static_assert(Game::tutorialMask(Game::TutorialStep::MENU_NAV) == (1U << 3));
    static_assert(Game::tutorialMask(Game::TutorialStep::MENU_BACK) == (1U << 4));
    static_assert(Game::tutorialMask(Game::TutorialStep::EXPLORE_WALK) == (1U << 5));
    static_assert(Game::tutorialMask(Game::TutorialStep::EXPLORE_MENU) == (1U << 6));
    static_assert(Game::tutorialMask(Game::TutorialStep::BATTLE_ACTION) == (1U << 7));
    static_assert(Game::TUTORIAL_ALL_FLAGS == 0xFF);
    return 0;
}
