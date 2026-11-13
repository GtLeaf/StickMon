#include "game/ItemInventory.h"
#include "game/MarketItemCatalog.h"
#include "game/MarketWelcome.h"
#include "game/MonsterFactory.h"
#include "game/Species.h"
#include <cassert>
#include <cstdio>
using namespace Game;
int main() {
    GameState s{}; s.teamCount = 1; s.team[0] = MonsterFactory::create(25, 20);
    auto& m = s.team[0];
    assert(ItemInventory::add(s, ItemId::HP_UP, 2));
    m.ev.hp = 250;
    assert(ItemInventory::useOnTeam(s, ItemId::HP_UP, 0) == ItemInventory::UseResult::USED && m.ev.hp == 252);
    assert(ItemInventory::useOnTeam(s, ItemId::HP_UP, 0) != ItemInventory::UseResult::USED && ItemInventory::count(s, ItemId::HP_UP) == 1);
    m.ev.atk = 252; m.ev.def = 6;
    assert(ItemInventory::add(s, ItemId::CARBOS));
    assert(ItemInventory::useOnTeam(s, ItemId::CARBOS, 0) != ItemInventory::UseResult::USED);
    assert(ItemInventory::add(s, ItemId::KELPSY_BERRY));
    const auto affection = m.affection;
    assert(ItemInventory::useOnTeam(s, ItemId::KELPSY_BERRY, 0) == ItemInventory::UseResult::USED && m.ev.atk == 242 && m.affection == affection + 2);
    assert(ItemInventory::add(s, ItemId::MAX_REVIVE)); m.hpCur = 0; m.fainted = true;
    assert(ItemInventory::useOnTeam(s, ItemId::MAX_REVIVE, 0) == ItemInventory::UseResult::USED && m.hpCur == m.hpMax && !m.fainted);
    assert(ItemInventory::add(s, ItemId::ORAN_BERRY)); m.hpCur = 1;
    assert(ItemInventory::useOnTeam(s, ItemId::ORAN_BERRY, 0) == ItemInventory::UseResult::USED && m.hpCur == 11);
    assert(ItemInventory::add(s, ItemId::LUM_BERRY)); m.majorStatus = MajorStatus::POISON;
    assert(ItemInventory::useOnTeam(s, ItemId::LUM_BERRY, 0) == ItemInventory::UseResult::USED && m.majorStatus == MajorStatus::NONE);
    assert(!ItemInventory::usableInBattle(ItemId::PROTEIN) && ItemInventory::usableInBattle(ItemId::MAX_REVIVE));
    assert(MarketWelcome::grant(s) == MarketWelcome::Result::GRANTED && s.marketTutorGifted && s.bag.heartScale == 1);
    assert(MarketWelcome::grant(s) == MarketWelcome::Result::ALREADY && s.bag.heartScale == 1);
    GameState full{}; full.bag.heartScale = ITEM_STACK_CAP;
    assert(MarketWelcome::grant(full) == MarketWelcome::Result::BAG_FULL && !full.marketTutorGifted);
    std::puts("market-items: EV caps, no-effect consumption, reset berries, full revival and manual recovery OK");
}
