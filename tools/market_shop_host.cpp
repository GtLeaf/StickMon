// Focused Kecleon period, shelf, purchase, migration and codec checks.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include "core/SaveCodec.h"
#include "game/MarketShopService.h"
#include "save_schema1_fabricator.h"

using namespace Game;

static GameState fresh(uint32_t day, unsigned clearedAreas = 6) {
    GameState state{};
    state.coins = 100000;
    state.gameMinutesTotal = day * 1440 + 30;
    for (unsigned i = 0; i < clearedAreas && i < EXPLORE_AREA_COUNT; ++i)
        state.explorePoolRerollCounts[i] = 1;
    return state;
}

static void firstDayHasAllMarketGoods() {
    auto state = fresh(0, 0);
    assert(MarketShop::ensurePeriod(state));
    const uint8_t capacities[] = {3,3,1,2,3,2,1,1};
    unsigned total = 0;
    for (uint8_t slot = 0; slot < MarketShop::ROW_COUNT; ++slot) {
        assert(state.marketSeason.stock[slot] == capacities[slot]);
        total += state.marketSeason.stock[slot];
    }
    assert(total == 16);
    assert(!MarketShop::unlocked(state, ItemId::COUNT));
    assert(!MarketShop::unlocked(state, ItemId(255)));

    // Every possible market good can be bought without defeating a boss.
    const struct { uint8_t slot; ItemId item; } goods[] = {
        {0,ItemId::POTION}, {0,ItemId::SUPER_POTION}, {0,ItemId::ANTIDOTE},
        {0,ItemId::PARALYZE_HEAL}, {0,ItemId::AWAKENING}, {0,ItemId::BURN_HEAL},
        {0,ItemId::ICE_HEAL}, {0,ItemId::MAX_POTION}, {0,ItemId::FULL_RESTORE},
        {0,ItemId::FULL_HEAL}, {0,ItemId::REVIVE}, {0,ItemId::MAX_REPEL}, {0,ItemId::HONEY},
        {MarketShop::CANDY,ItemId::CANDY},
        {MarketShop::NUTRIENT,ItemId::HP_UP}, {MarketShop::NUTRIENT,ItemId::PROTEIN},
        {MarketShop::NUTRIENT,ItemId::IRON}, {MarketShop::NUTRIENT,ItemId::CALCIUM},
        {MarketShop::NUTRIENT,ItemId::ZINC}, {MarketShop::NUTRIENT,ItemId::CARBOS},
        {MarketShop::RESET_BERRY,ItemId::POMEG_BERRY}, {MarketShop::RESET_BERRY,ItemId::KELPSY_BERRY},
        {MarketShop::RESET_BERRY,ItemId::QUALOT_BERRY}, {MarketShop::RESET_BERRY,ItemId::HONDEW_BERRY},
        {MarketShop::RESET_BERRY,ItemId::GREPA_BERRY}, {MarketShop::RESET_BERRY,ItemId::TAMATO_BERRY},
        {MarketShop::SPECIAL,ItemId::MAX_REVIVE}, {MarketShop::SPECIAL,ItemId::LAVA_COOKIE},
        {MarketShop::SPECIAL,ItemId::ORAN_BERRY}, {MarketShop::SPECIAL,ItemId::SITRUS_BERRY},
        {MarketShop::SPECIAL,ItemId::LUM_BERRY}, {MarketShop::RARE,ItemId::HEART_SCALE},
        {MarketShop::RARE,ItemId::FIRE_STONE}, {MarketShop::RARE,ItemId::WATER_STONE},
        {MarketShop::RARE,ItemId::THUNDER_STONE},
    };
    for (const auto& good : goods) {
        auto purchase = state;
        purchase.marketSeason.items[good.slot] = uint8_t(good.item);
        const auto count = ItemInventory::count(purchase,good.item);
        MarketShop::Reward reward;
        assert(MarketShop::buy(purchase,good.slot,0,&reward) == MarketShop::Result::OK);
        assert(reward.item == good.item && reward.amount == 1);
        assert(ItemInventory::count(purchase,good.item) == count+1);
        assert(purchase.coins == state.coins-MarketShop::price(good.item));
        assert(purchase.marketSeason.stock[good.slot] == capacities[good.slot]-1);
    }

    // Lifting market gates must not unlock the ordinary shop's higher tiers.
    auto ordinary = state;
    assert(ShopService::buyItemCount(ShopService::Category::DAILY,ordinary) == 2);
    assert(ShopService::buyItemCount(ShopService::Category::EXPLORE,ordinary) == 3);
    for (auto id : {ItemId::SUPER_POTION,ItemId::MAX_REPEL,ItemId::FULL_RESTORE,
                    ItemId::FIRE_STONE,ItemId::WATER_STONE,ItemId::THUNDER_STONE}) {
        const auto before = ordinary;
        assert(ShopService::buy(ordinary,id) == ShopService::BuyResult::LOCKED);
        assert(std::memcmp(&before,&ordinary,sizeof(ordinary)) == 0);
    }
    assert(ShopService::buy(ordinary,ItemId::CANDY) == ShopService::BuyResult::INVALID_ITEM);
    assert(ShopService::buy(ordinary,ItemId::POTION) == ShopService::BuyResult::BOUGHT);
    ordinary.explorePoolRerollCounts[0] = 1;
    assert(ShopService::buy(ordinary,ItemId::MAX_REPEL) == ShopService::BuyResult::BOUGHT);
    assert(ShopService::buy(ordinary,ItemId::SUPER_POTION) == ShopService::BuyResult::LOCKED);
    ordinary.explorePoolRerollCounts[1] = 1;
    assert(ShopService::buy(ordinary,ItemId::SUPER_POTION) == ShopService::BuyResult::BOUGHT);
}

int main() {
    static_assert(MarketShop::ROW_COUNT == 8);
    firstDayHasAllMarketGoods();
    const uint8_t capacities[] = {3,3,1,2,3,2,1,1};
    bool sawScale = false, sawStone = false;
    auto rotating = fresh(0);
    for (uint32_t day = 0; day < 200; ++day) {
        auto early = fresh(day, 0);
        rotating.gameMinutesTotal = day * 1440;
        if (!MarketSchedule::isOpenDay(day)) {
            const auto before = rotating;
            assert(!MarketShop::ensurePeriod(rotating));
            assert(std::memcmp(&before, &rotating, sizeof(before)) == 0);
            assert(!MarketShop::ensurePeriod(early));
            continue;
        }
        const auto previous = rotating.marketSeason;
        assert(MarketShop::ensurePeriod(rotating));
        assert(MarketShop::ensurePeriod(early));
        assert(rotating.marketSeason.periodDay == day);
        unsigned total = 0;
        for (uint8_t slot = 0; slot < MarketShop::ROW_COUNT; ++slot) {
            assert(rotating.marketSeason.stock[slot] == capacities[slot]);
            total += rotating.marketSeason.stock[slot];
            if (slot == MarketShop::BOX) continue;
            const auto id = MarketShop::item(rotating, slot);
            assert(id != ItemId::COUNT && MarketShop::unlocked(rotating, id));
            if (slot <= MarketShop::DISCOUNT_B) {
                assert(MarketShop::price(id) < ShopService::buyPrice(id));
                assert(MarketShop::price(id) > ShopService::sellPrice(id));
            }
        }
        assert(total == 16);
        assert(MarketShop::item(rotating, MarketShop::DISCOUNT_A) != MarketShop::item(rotating, MarketShop::DISCOUNT_B));
        const auto rare = MarketShop::item(rotating, MarketShop::RARE);
        sawScale |= rare == ItemId::HEART_SCALE;
        sawStone |= rare == ItemId::FIRE_STONE || rare == ItemId::WATER_STONE || rare == ItemId::THUNDER_STONE;
        if (previous.periodDay != MARKET_PERIOD_NONE) {
            assert(rotating.marketSeason.items[MarketShop::SPECIAL] != previous.items[MarketShop::SPECIAL]);
            assert(rotating.marketSeason.items[MarketShop::RARE] != previous.items[MarketShop::RARE]);
            const bool samePair = (rotating.marketSeason.items[0] == previous.items[0] && rotating.marketSeason.items[1] == previous.items[1]) ||
                                  (rotating.marketSeason.items[0] == previous.items[1] && rotating.marketSeason.items[1] == previous.items[0]);
            assert(!samePair);
        }
        for (uint8_t slot = 0; slot < MarketShop::ROW_COUNT; ++slot) {
            assert(early.marketSeason.stock[slot] == capacities[slot]);
            if (slot != MarketShop::BOX)
                assert(MarketShop::item(early,slot) != ItemId::COUNT);
        }
        const auto samePeriod = rotating;
        rotating.gameMinutesTotal += 600;
        assert(!MarketShop::ensurePeriod(rotating));
        assert(std::memcmp(&samePeriod.marketSeason, &rotating.marketSeason, sizeof(previous)) == 0);
    }
    assert(sawScale && sawStone);

    auto state = fresh(7);
    assert(MarketShop::ensurePeriod(state));
    const auto slot = MarketShop::RARE;
    const auto id = MarketShop::item(state, slot);
    const auto price = MarketShop::price(id);
    auto poor = state;
    poor.coins = price - 1;
    const auto beforePoor = poor;
    assert(MarketShop::buy(poor, slot, 7) == MarketShop::Result::NO_COINS);
    assert(std::memcmp(&poor, &beforePoor, sizeof(poor)) == 0);
    auto full = state;
    assert(ItemInventory::add(full, id, ITEM_STACK_CAP - ItemInventory::count(full,id)));
    const auto beforeFull = full;
    assert(MarketShop::buy(full, slot, 7) == MarketShop::Result::BAG_FULL);
    assert(std::memcmp(&full, &beforeFull, sizeof(full)) == 0);
    assert(MarketShop::buy(state, slot, 3) == MarketShop::Result::STALE);
    assert(MarketShop::check(state, MarketShop::ROW_COUNT, 7) == MarketShop::Result::BAD_SLOT);
    const auto count = ItemInventory::count(state,id);
    assert(MarketShop::buy(state, slot, 7) == MarketShop::Result::OK);
    assert(state.coins == 100000 - price && ItemInventory::count(state,id) == count+1);
    const auto bought = state;
    assert(MarketShop::buy(state, slot, 7) == MarketShop::Result::SOLD_OUT);
    assert(std::memcmp(&state, &bought, sizeof(state)) == 0);
    state.gameMinutesTotal = 8 * 1440;
    const auto closed = state;
    assert(MarketShop::buy(state, MarketShop::DISCOUNT_A, 7) == MarketShop::Result::STALE);
    assert(!MarketShop::ensurePeriod(state));
    assert(std::memcmp(&state,&closed,sizeof(state)) == 0);
    state.gameMinutesTotal = 10 * 1440;
    assert(MarketShop::ensurePeriod(state) && state.marketSeason.stock[slot] == 1);
    state.candyPurchasesToday = DAILY_CANDY_PURCHASE_CAP;
    const auto candy = state.bag.candy;
    assert(MarketShop::buy(state, MarketShop::CANDY, 10) == MarketShop::Result::OK);
    assert(state.bag.candy == candy+1 && state.candyPurchasesToday == DAILY_CANDY_PURCHASE_CAP);

    // Schema7's old same-period purchases reduce the new shared quotas once.
    state = fresh(7);
    auto expected = state;
    assert(MarketShop::ensurePeriod(expected));
    state.marketShop.periodDay = 7;
    state.marketShop.items[0] = expected.marketSeason.items[MarketShop::DISCOUNT_A];
    state.marketShop.items[1] = uint8_t(ItemId::CANDY);
    state.marketShop.items[2] = expected.marketSeason.items[MarketShop::RARE];
    state.marketShop.boxBought = 1;
    state.marketShop.boxRoll = 87;
    state.candyPurchasesToday = 1;
    state.marketCare.periodDay = 7;
    state.marketCare.used = 1;
    static uint8_t encoded[SaveCodec::MAX_ENCODED_BYTES];
    size_t length = 0;
    assert(SaveCodec::encode(state, {}, 5, encoded, sizeof(encoded), length));
    std::vector<uint8_t> legacy;
    assert(SaveTestUtil::fabricateLegacySchemaBlob(encoded,length,legacy,7));
    SaveCodec::Snapshot decoded;
    assert(SaveCodec::decode(legacy.data(),legacy.size(),decoded));
    assert(MarketShop::ensurePeriod(decoded.state));
    assert(decoded.state.marketSeason.stock[MarketShop::DISCOUNT_A] == 2);
    assert(decoded.state.marketSeason.stock[MarketShop::CANDY] == 0);
    assert(decoded.state.marketSeason.stock[MarketShop::RARE] == 0);
    assert(decoded.state.marketSeason.stock[MarketShop::BOX] == 0);
    assert(decoded.state.marketSeason.boxRoll == 87 && decoded.state.marketCare.used == 1);
    assert(!MarketShop::ensurePeriod(decoded.state));
    decoded.state.marketItemStock[0] = 23;
    decoded.state.marketTutorGifted = 1;
    const auto saved = decoded.state;
    assert(SaveCodec::encode(saved, {}, 6, encoded, sizeof(encoded), length));
    assert(SaveCodec::decode(encoded,length,decoded));
    assert(std::memcmp(&decoded.state.marketSeason,&saved.marketSeason,sizeof(saved.marketSeason)) == 0);
    assert(decoded.state.marketItemStock[0] == 23 && decoded.state.marketTutorGifted == 1);
    assert(!MarketShop::ensurePeriod(decoded.state));

    // An existing shelf may have sold or formerly locked rows. Loading that
    // same period must preserve every byte; only the next opening restocks.
    auto oldShelf = fresh(0,0);
    assert(MarketShop::ensurePeriod(oldShelf));
    oldShelf.marketSeason.stock[MarketShop::DISCOUNT_A] = 1;
    for (auto slot : {MarketShop::CANDY,MarketShop::NUTRIENT,MarketShop::RESET_BERRY,MarketShop::RARE,MarketShop::BOX})
        oldShelf.marketSeason.stock[slot] = 0;
    oldShelf.marketSeason.items[MarketShop::RARE] = 255;
    const auto preserved = oldShelf.marketSeason;
    assert(SaveCodec::encode(oldShelf,{},7,encoded,sizeof(encoded),length));
    assert(SaveCodec::decode(encoded,length,decoded));
    assert(!MarketShop::ensurePeriod(decoded.state));
    assert(std::memcmp(&preserved,&decoded.state.marketSeason,sizeof(preserved)) == 0);
    decoded.state.gameMinutesTotal = 3*1440;
    assert(MarketShop::ensurePeriod(decoded.state));
    for (uint8_t slot = 0; slot < MarketShop::ROW_COUNT; ++slot)
        assert(decoded.state.marketSeason.stock[slot] == capacities[slot]);
    assert(MarketShop::item(decoded.state,MarketShop::RARE) != ItemId::COUNT);
    std::puts("market-shop: first-day all goods, ordinary-shop gates, rotation, 8 rows/16 units, prices, refusals and migration/save OK");
}
