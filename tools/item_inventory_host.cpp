#include <cassert>

#include "game/ItemInventory.h"

int main() {
    using Game::ItemId;
    using Game::ItemInventory::UseResult;

    Game::GameState state;
    assert(Game::ItemInventory::count(state, ItemId::POTION) == 2);
    assert(Game::ItemInventory::add(state, ItemId::POTION, 3));
    assert(Game::ItemInventory::count(state, ItemId::POTION) == 5);
    assert(Game::ItemInventory::remove(state, ItemId::POTION, 2));
    assert(Game::ItemInventory::count(state, ItemId::POTION) == 3);
    state.bag.potion = Game::ITEM_STACK_CAP;
    assert(!Game::ItemInventory::add(state, ItemId::POTION));

    for (uint8_t i = 0; i < Game::ROOM_FOOD_COUNT; ++i) state.room.food[i] = 0;
    state.room.selectedFood = 0;
    assert(Game::ItemInventory::add(state, ItemId::SWEET_FOOD));
    assert(state.room.selectedFood == Game::ROOM_SWEET_FOOD_INDEX);
    assert(Game::ItemInventory::remove(state, ItemId::SWEET_FOOD));
    assert(state.room.selectedFood == 0);

    state.bag.potion = 1;
    state.team[0].hpCur = 4;
    state.team[0].hpMax = 50;
    assert(Game::ItemInventory::useOnTeam(state, ItemId::POTION, 0) ==
           UseResult::USED);
    assert(state.team[0].hpCur == 24 && state.bag.potion == 0);

    state.bag.superPotion = 1;
    assert(Game::ItemInventory::useOnTeam(state, ItemId::SUPER_POTION, 0) ==
           UseResult::USED);
    assert(state.team[0].hpCur == 50);
    assert(Game::ItemInventory::useOnTeam(state, ItemId::SUPER_POTION, 0) ==
           UseResult::NO_STOCK);

    state.bag.fullRestore = 1;
    state.team[0].hpCur = 10;
    state.team[0].majorStatus = Game::MajorStatus::BURN;
    assert(Game::ItemInventory::useOnTeam(state, ItemId::FULL_RESTORE, 0) ==
           UseResult::USED);
    assert(state.team[0].hpCur == state.team[0].hpMax);
    assert(state.team[0].majorStatus == Game::MajorStatus::NONE);

    state.bag.antidote = 1;
    state.team[0].majorStatus = Game::MajorStatus::TOXIC;
    assert(Game::ItemInventory::useOnTeam(state, ItemId::ANTIDOTE, 0) ==
           UseResult::USED);
    assert(state.team[0].majorStatus == Game::MajorStatus::NONE);

    state.teamCount = 2;
    state.team[1].hpMax = 31;
    state.team[1].hpCur = 0;
    state.team[1].fainted = true;
    state.bag.revive = 1;
    assert(Game::ItemInventory::preferredTarget(state, ItemId::REVIVE) == 1);
    assert(Game::ItemInventory::useOnTeam(state, ItemId::REVIVE, 1) ==
           UseResult::USED);
    assert(!state.team[1].fainted && state.team[1].hpCur == 15);

    state.bag.candy = 1;
    assert(!Game::ItemInventory::usableFromHomeBag(ItemId::CANDY));
    assert(Game::ItemInventory::useOnTeam(state, ItemId::CANDY, 0) ==
           UseResult::NOT_USABLE);
    assert(state.bag.candy == 1);

    state.bag.potion = 1;
    state.bag.antidote = 1;
    state.bag.paralyzeHeal = 1;
    state.bag.revive = 1;
    assert(Game::ItemInventory::homeBagItemAt(state, 0) == ItemId::POTION);
    assert(Game::ItemInventory::homeBagItemAt(state, 1) == ItemId::ANTIDOTE);
    assert(Game::ItemInventory::homeBagItemAt(state, 2) == ItemId::CANDY);
    assert(Game::ItemInventory::homeBagItemAt(state, 3) ==
           ItemId::PARALYZE_HEAL);
    return 0;
}
