#include "game/MarketShopService.h"
#include "game/MarketSchedule.h"
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace Game;
static GameState fresh(unsigned day = 0) {
    GameState s{}; s.coins = 20000; s.gameMinutesTotal = day * 1440;
    MarketShop::ensurePeriod(s); return s;
}
int main() {
    static_assert(MarketShop::ROW_COUNT == 8);
    auto s = fresh(); unsigned total = 0;
    auto daily = s;
    assert(ShopService::buy(daily,ItemId::CANDY) == ShopService::BuyResult::INVALID_ITEM);
    for (auto stock : s.marketSeason.stock) total += stock;
    assert(total == 16 && MarketShop::item(s, MarketShop::CANDY) == ItemId::CANDY);
    assert(MarketShop::item(s, MarketShop::DISCOUNT_A) != MarketShop::item(s, MarketShop::DISCOUNT_B));
    const auto nutrient=MarketShop::item(s,MarketShop::NUTRIENT);
    const auto berry=MarketShop::item(s,MarketShop::RESET_BERRY);
    MarketShop::Reward reward;
    assert(MarketShop::buy(s, MarketShop::NUTRIENT, 0, &reward) == MarketShop::Result::OK && reward.item==nutrient);
    assert(MarketShop::buy(s, MarketShop::NUTRIENT, 0, &reward) == MarketShop::Result::OK);
    const auto used = s;
    assert(MarketShop::buy(s, MarketShop::NUTRIENT, 0, &reward) == MarketShop::Result::SOLD_OUT && std::memcmp(&used,&s,sizeof(s)) == 0);
    assert(MarketShop::buy(s, MarketShop::RESET_BERRY, 0, &reward) == MarketShop::Result::OK);
    assert(MarketShop::buy(s, MarketShop::RESET_BERRY, 0, &reward) == MarketShop::Result::OK);
    assert(MarketShop::buy(s, MarketShop::RESET_BERRY, 0, &reward) == MarketShop::Result::OK);
    const auto resetUsed = s;
    assert(MarketShop::buy(s, MarketShop::RESET_BERRY, 0, &reward) == MarketShop::Result::SOLD_OUT && std::memcmp(&resetUsed,&s,sizeof(s)) == 0);
    s = fresh(); s.marketSeason.boxRoll = 100;
    assert(MarketShop::buy(s, MarketShop::BOX, 0, &reward) == MarketShop::Result::OK && reward.item == ItemId::CANDY);
    assert(MarketShop::buy(s, MarketShop::CANDY, 0) == MarketShop::Result::OK && s.bag.candy == 3);
    assert(s.candyPurchasesToday == 0); // ordinary-shop daily counter is independent.
    assert(!MarketShop::ensurePeriod(s));
    s.gameMinutesTotal = 1440; const auto closed = s;
    assert(MarketShop::buy(s, MarketShop::DISCOUNT_A, 0) == MarketShop::Result::STALE && std::memcmp(&closed,&s,sizeof(s)) == 0);
    s.gameMinutesTotal = 3 * 1440; assert(MarketShop::ensurePeriod(s) && s.marketSeason.stock[MarketShop::CANDY] == 1);
    assert(MarketItemCatalog::nutrient(nutrient) && MarketItemCatalog::resetBerry(berry));
    std::puts("market-season: eight slots/16 units, concrete frozen goods, bonus candy, stale/closed periods and refresh OK");
}
