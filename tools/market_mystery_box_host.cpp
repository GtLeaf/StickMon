#include "core/SaveCodec.h"
#include "game/MarketShopService.h"
#include "save_schema1_fabricator.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace Game;
static GameState fresh(unsigned day = 7, bool unlocked = true) {
    GameState state{};
    state.coins = 5000;
    state.gameMinutesTotal = day * 1440;
    if (unlocked) for (auto& clear : state.explorePoolRerollCounts) clear = 1;
    MarketShop::ensurePeriod(state);
    return state;
}
int main() {
    static_assert(MarketShop::ROW_COUNT == 8);
    // Only the initial eligible market leader contributes to the frozen draw.
    auto leader = fresh();
    leader.marketSeason = GameState::Season{};
    leader.teamCount = 2;
    leader.team[0].fainted = true;
    leader.team[1].hpCur = 10;
    leader.team[1].ivPacked = 12345;
    leader.team[1].metAt = 7;
    auto reference = leader;
    assert(MarketShop::ensurePeriod(reference));
    for (unsigned identity = 1; identity <= 10; ++identity) {
        auto changed = leader;
        changed.team[0].ivPacked = identity;
        assert(MarketShop::ensurePeriod(changed));
        assert(changed.marketSeason.boxRoll == reference.marketSeason.boxRoll);
    }
    bool activeIdentityChangesDraw = false;
    for (unsigned identity = 1; identity <= 10; ++identity) {
        auto changed = leader;
        changed.team[1].ivPacked = identity;
        changed.team[1].metAt = 100 + identity;
        assert(MarketShop::ensurePeriod(changed));
        activeIdentityChangesDraw |= changed.marketSeason.boxRoll != reference.marketSeason.boxRoll;
    }
    assert(activeIdentityChangesDraw);
    const auto frozenRoll = reference.marketSeason.boxRoll;
    reference.team[1].ivPacked ^= 0x1234;
    reference.team[1].metAt += 1;
    assert(!MarketShop::ensurePeriod(reference) && reference.marketSeason.boxRoll == frozenRoll);

    auto state = fresh();
    for (bool progressed : {false,true}) {
      auto weighted = fresh(0,progressed);
      std::array<unsigned,7> buckets{};
      for (unsigned roll = 0; roll < 100; ++roll) {
        const auto reward = MarketShop::boxRewardForRoll(weighted, roll);
        unsigned index = 5;
        switch (reward.item) {
        case ItemId::HONEY: index = 0; assert(reward.amount == 3); break;
        case ItemId::SUPER_POTION: index = 1; assert(reward.amount == 3); break;
        case ItemId::MAX_REPEL: index = 2; assert(reward.amount == 2); break;
        case ItemId::REVIVE: index = 3; assert(reward.amount == 1); break;
        case ItemId::HEART_SCALE: index = 4; assert(reward.amount == 1); break;
        case ItemId::CANDY: index = 6; assert(reward.amount == 1); break;
        default: assert((reward.item == ItemId::FIRE_STONE || reward.item == ItemId::WATER_STONE || reward.item == ItemId::THUNDER_STONE) && reward.amount == 1);
        }
        ++buckets[index];
        auto purchase = weighted;
        purchase.marketSeason.boxRoll = uint8_t(roll+1);
        const auto count = ItemInventory::count(purchase,reward.item);
        MarketShop::Reward granted;
        assert(MarketShop::buy(purchase,MarketShop::BOX,0,&granted) == MarketShop::Result::OK);
        assert(granted.item == reward.item && granted.amount == reward.amount);
        assert(ItemInventory::count(purchase,reward.item) == count+reward.amount);
        assert(purchase.coins == 4700 && purchase.marketSeason.stock[MarketShop::BOX] == 0);
        assert(purchase.marketSeason.boxRoll == roll+1);
      }
      assert((buckets == std::array<unsigned,7>{25,25,15,15,12,6,2}));
    }
    // Box candy is a bonus independent of the ordinary-shop daily counter.
    state.candyPurchasesToday = DAILY_CANDY_PURCHASE_CAP;
    assert(MarketShop::boxRewardForRoll(state,99).item == ItemId::CANDY);
    state.marketSeason.boxRoll = 100;
    const auto candyBefore = state.bag.candy;
    MarketShop::Reward reward{};
    assert(MarketShop::buy(state,MarketShop::BOX,7,&reward) == MarketShop::Result::OK);
    assert(state.coins == 4700 && reward.item == ItemId::CANDY && reward.amount == 1);
    assert(state.bag.candy == candyBefore+1 && state.candyPurchasesToday == DAILY_CANDY_PURCHASE_CAP);
    assert(state.marketSeason.stock[MarketShop::BOX] == 0);
    const auto saved = state;
    assert(MarketShop::buy(state,MarketShop::BOX,7) == MarketShop::Result::SOLD_OUT);
    assert(std::memcmp(&saved,&state,sizeof(state)) == 0);
    assert(!MarketShop::ensurePeriod(state));
    assert(MarketShop::buy(state,MarketShop::CANDY,7) == MarketShop::Result::OK);
    assert(state.bag.candy == candyBefore+2 && state.candyPurchasesToday == DAILY_CANDY_PURCHASE_CAP);
    assert(MarketShop::buy(state,MarketShop::CANDY,7) == MarketShop::Result::SOLD_OUT);
    std::vector<uint8_t> bytes(SaveCodec::MAX_ENCODED_BYTES);
    size_t written = 0;
    SaveCodec::Snapshot decoded;
    assert(SaveCodec::encode(state,{},7,bytes.data(),bytes.size(),written));
    assert(SaveCodec::decode(bytes.data(),written,decoded));
    assert(decoded.state.marketSeason.stock[MarketShop::BOX] == 0);
    assert(decoded.state.marketSeason.boxRoll == 100 && decoded.state.candyPurchasesToday == DAILY_CANDY_PURCHASE_CAP);
    assert(!MarketShop::ensurePeriod(decoded.state));

    state = fresh(); state.marketSeason.boxRoll = 1; state.bag.honey = 98;
    const auto full = state;
    assert(MarketShop::buy(state,MarketShop::BOX,7) == MarketShop::Result::BAG_FULL);
    assert(std::memcmp(&full,&state,sizeof(state)) == 0); // whole 3-unit reward or nothing
    state = fresh(); state.marketSeason.boxRoll = 1; state.bag.honey = 96;
    assert(MarketShop::buy(state,MarketShop::BOX,7,&reward) == MarketShop::Result::OK);
    assert(state.bag.honey == ITEM_STACK_CAP && reward.amount == 3);
    state = fresh(); state.coins = 299;
    const auto poor = state;
    assert(MarketShop::buy(state,MarketShop::BOX,7) == MarketShop::Result::NO_COINS);
    assert(std::memcmp(&poor,&state,sizeof(state)) == 0);
    state = fresh(); state.gameMinutesTotal = 8 * 1440;
    const auto closed = state;
    assert(MarketShop::buy(state,MarketShop::BOX,7) == MarketShop::Result::STALE);
    assert(!MarketShop::ensurePeriod(state));
    assert(std::memcmp(&closed,&state,sizeof(state)) == 0);
    state.gameMinutesTotal = 10 * 1440;
    assert(MarketShop::ensurePeriod(state) && state.marketSeason.stock[MarketShop::BOX] == 1);
    for (auto& stock : state.marketSeason.stock) stock = 0;
    assert(MarketShop::soldOut(state));
    state.marketSeason.stock[MarketShop::BOX] = 1;
    assert(!MarketShop::soldOut(state));
    std::puts("market-mystery-box: first-day exact weights, frozen leader draw, bonus candy, period quota, refusals and save OK");
}
