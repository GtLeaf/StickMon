// Host checks for the Kecleon stall shelf rules and their save round trip.
#include <cassert>
#include <cstdio>
#include <cstring>
#include "core/SaveCodec.h"
#include "game/MarketShopService.h"

using namespace Game;

static GameState fresh(uint32_t day, int clearedAreas) {
    GameState s{};
    s.coins = 5000; s.gameMinutesTotal = day * 1440 + 30;
    for (int i = 0; i < clearedAreas && i < EXPLORE_AREA_COUNT; ++i) s.explorePoolRerollCounts[i] = 1;
    return s;
}

int main() {
    // New save: every slot filled with unlocked goods, honey stands in for the scale.
    for (uint32_t day = 0; day < 60; ++day) {
        auto s = fresh(day, 0);
        assert(MarketShop::ensurePeriod(s));
        assert(MarketShop::item(s, MarketShop::RARE) == ItemId::HONEY);
        for (uint8_t i = 0; i < MARKET_SHELF_SLOTS; ++i) {
            const ItemId id = MarketShop::item(s, i);
            assert(id != ItemId::COUNT && s.marketShop.stock[i] == 1);
            assert(id == ItemId::HONEY || MarketShop::unlocked(s, id));
            // No arbitrage: never below the shop's sell price.
            assert(MarketShop::price(id) > ShopService::sellPrice(id));
            for (uint8_t j = 0; j < i; ++j) assert(MarketShop::item(s, j) != id);
        }
    }
    // Late game: heart scale rare slot, stones/candy sale slot, all distinct.
    bool sawCandy = false, sawStone = false;
    for (uint32_t day = 0; day < 200; ++day) {
        auto s = fresh(day, 6);
        MarketShop::ensurePeriod(s);
        assert(MarketShop::item(s, MarketShop::RARE) == ItemId::HEART_SCALE);
        const ItemId sale = MarketShop::item(s, MarketShop::SALE);
        sawCandy |= sale == ItemId::CANDY;
        sawStone |= sale == ItemId::FIRE_STONE || sale == ItemId::WATER_STONE || sale == ItemId::THUNDER_STONE;
        assert(MarketShop::price(sale) < ShopService::buyPrice(sale));
    }
    assert(sawCandy && sawStone);
    // Same day keeps the shelf; next day redraws.
    {
        auto s = fresh(7, 6);
        MarketShop::ensurePeriod(s);
        const auto first = s.marketShop;
        s.gameMinutesTotal += 600;
        assert(!MarketShop::ensurePeriod(s) && std::memcmp(&first, &s.marketShop, sizeof(first)) == 0);
        s.gameMinutesTotal = 8 * 1440;
        assert(MarketShop::ensurePeriod(s) && s.marketShop.periodDay == 8);
    }
    // Buying: one unit, charges once, sold out afterwards, failures keep state.
    {
        auto s = fresh(3, 6);
        MarketShop::ensurePeriod(s);
        const uint32_t period = s.marketShop.periodDay;
        auto poor = s; poor.coins = 599;
        assert(MarketShop::buy(poor, MarketShop::RARE, period) == MarketShop::Result::NO_COINS && poor.coins == 599);
        auto full = s; full.bag.heartScale = ITEM_STACK_CAP;
        assert(MarketShop::buy(full, MarketShop::RARE, period) == MarketShop::Result::BAG_FULL);
        assert(MarketShop::buy(s, MarketShop::RARE, period + 1) == MarketShop::Result::STALE);
        assert(MarketShop::buy(s, MarketShop::RARE, period) == MarketShop::Result::OK);
        assert(s.coins == 4400 && s.bag.heartScale == 1 && !s.marketShop.stock[0]);
        assert(MarketShop::buy(s, MarketShop::RARE, period) == MarketShop::Result::SOLD_OUT && s.coins == 4400);
        // A day rollover while the confirm page is open never sells the old quote.
        auto later = s; later.gameMinutesTotal += 1440;
        assert(MarketShop::buy(later, MarketShop::SUPPLY, period) == MarketShop::Result::STALE);
        assert(MarketShop::check(s, 7, period) == MarketShop::Result::BAD_SLOT);
    }
    // Candy shares the shop's daily limit.
    {
        auto s = fresh(0, 6);
        s.marketShop.periodDay = 0; s.marketShop.items[2] = uint8_t(ItemId::CANDY); s.marketShop.stock[2] = 1;
        s.candyPurchasesToday = DAILY_CANDY_PURCHASE_CAP;
        assert(MarketShop::buy(s, MarketShop::SALE, 0) == MarketShop::Result::DAILY_LIMIT);
        s.candyPurchasesToday = 0;
        assert(MarketShop::buy(s, MarketShop::SALE, 0) == MarketShop::Result::OK && s.candyPurchasesToday == 1 && s.coins == 3200);
    }
    // Save codec: schema 5 round trip.
    {
        auto s = fresh(9, 6);
        MarketShop::ensurePeriod(s); s.marketShop.stock[1] = 0;
        static uint8_t encoded[SaveCodec::MAX_ENCODED_BYTES];
        size_t length = 0; MainSceneViewState view{};
        assert(SaveCodec::encode(s, view, 5, encoded, sizeof(encoded), length));
        auto* decoded = new SaveCodec::Snapshot{};
        assert(SaveCodec::decode(encoded, length, *decoded));
        assert(std::memcmp(&decoded->state.marketShop, &s.marketShop, sizeof(s.marketShop)) == 0);
        delete decoded;
    }
    std::puts("market-shop: shelf draw unlock price buy stale candy save OK");
}
