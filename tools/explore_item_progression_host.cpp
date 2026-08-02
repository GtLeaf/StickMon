#include "game/ExploreItemProgression.h"

#include <cassert>

int main() {
    Game::GameState state;

    assert(ExploreItemProgression::unlockAreaForItem(
        Game::ItemId::POTION) == 0);
    assert(ExploreItemProgression::unlockAreaForItem(
        Game::ItemId::ANTIDOTE) == 0);
    assert(ExploreItemProgression::unlockAreaForItem(
        Game::ItemId::HONEY) == 0);
    assert(ExploreItemProgression::unlockAreaForItem(
        Game::ItemId::CANDY) == 3);
    assert(ExploreItemProgression::unlockAreaForItem(
        Game::ItemId::MAX_REPEL) == 1);
    assert(ExploreItemProgression::unlockAreaForItem(
        Game::ItemId::SUPER_POTION) == 2);
    assert(ExploreItemProgression::unlockAreaForItem(
        Game::ItemId::REVIVE) == 2);
    assert(ExploreItemProgression::unlockAreaForItem(
        Game::ItemId::FULL_HEAL) == 3);
    assert(ExploreItemProgression::unlockAreaForItem(
        Game::ItemId::MAX_POTION) == 4);
    assert(ExploreItemProgression::unlockAreaForItem(
        Game::ItemId::FULL_RESTORE) == 4);

    assert(ExploreItemProgression::unlockedArea(state) == 0);
    assert(ExploreItemProgression::shopUnlockedArea(state) == 0);
    assert(ExploreItemProgression::isAreaUnlocked(0, state));
    assert(!ExploreItemProgression::isAreaUnlocked(1, state));
    assert(ExploreItemProgression::visibleAreaCount(state) == 2);
    assert(ExploreItemProgression::isShopItemUnlocked(
        Game::ItemId::POTION, state));
    assert(!ExploreItemProgression::isShopItemUnlocked(
        Game::ItemId::MAX_REPEL, state));

    state.explorePoolRerollCounts[0] = 1;
    assert(ExploreItemProgression::unlockedArea(state) == 1);
    assert(ExploreItemProgression::shopUnlockedArea(state) == 1);
    assert(ExploreItemProgression::isAreaUnlocked(1, state));
    assert(!ExploreItemProgression::isAreaUnlocked(2, state));
    assert(ExploreItemProgression::visibleAreaCount(state) == 3);
    assert(ExploreItemProgression::isShopItemUnlocked(
        Game::ItemId::MAX_REPEL, state));
    assert(!ExploreItemProgression::isShopItemUnlocked(
        Game::ItemId::SUPER_POTION, state));
    assert(!ExploreItemProgression::isShopItemUnlocked(
        Game::ItemId::CANDY, state));

    state.explorePoolRerollCounts[2] = 1;
    assert(ExploreItemProgression::unlockedArea(state) == 1);
    assert(ExploreItemProgression::shopUnlockedArea(state) == 3);
    assert(ExploreItemProgression::isShopItemUnlocked(
        Game::ItemId::CANDY, state));

    state.explorePoolRerollCounts[1] = 1;
    assert(ExploreItemProgression::unlockedArea(state) == 3);
    assert(ExploreItemProgression::visibleAreaCount(state) == 5);
    assert(ExploreItemProgression::isShopItemUnlocked(
        Game::ItemId::CANDY, state));

    state.explorePoolRerollCounts[0] = 0;
    state.explorePoolRerollCounts[3] = 1;
    assert(ExploreItemProgression::unlockedArea(state) == 0);
    assert(ExploreItemProgression::shopUnlockedArea(state) == 4);
    assert(ExploreItemProgression::isShopItemUnlocked(
        Game::ItemId::MAX_POTION, state));
    assert(ExploreItemProgression::isShopItemUnlocked(
        Game::ItemId::FULL_RESTORE, state));

    state.explorePoolRerollCounts[0] = 1;
    state.explorePoolRerollCounts[4] = 1;
    state.explorePoolRerollCounts[5] = 1;
    assert(ExploreItemProgression::unlockedArea(state) == 5);
    assert(ExploreItemProgression::shopUnlockedArea(state) == 5);
    assert(ExploreItemProgression::visibleAreaCount(state) == 6);
    assert(ExploreItemProgression::isShopItemUnlocked(
        Game::ItemId::FULL_RESTORE, state));
    return 0;
}
