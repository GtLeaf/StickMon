#pragma once

#include <cstdint>
#include "game/ExploreItemProgression.h"
#include "game/GameState.h"
#include "game/ItemInventory.h"
#include "game/ShopService.h"

// Kecleon stall (奇珍小铺): three fixed slots per game day (rare / supply /
// sale), one unit each. Prices stay below the shop's buy price and above its
// sell price, so the stall never creates an arbitrage loop. Pure rules; the
// caller saves and restores its copy on failure.
namespace Game::MarketShop {

constexpr uint16_t STALL_SPECIES = 352;
enum Slot : uint8_t { RARE = 0, SUPPLY = 1, SALE = 2 };
enum class Result : uint8_t { OK, STALE, BAD_SLOT, SOLD_OUT, LOCKED, NO_COINS, BAG_FULL, DAILY_LIMIT };

inline uint32_t periodDay(const GameState& state) { return state.gameMinutesTotal / (24U * 60U); }
inline ItemId item(const GameState& state, uint8_t slot) {
    return slot < MARKET_SHELF_SLOTS && state.marketShop.items[slot] < uint8_t(ItemId::COUNT)
        ? ItemId(state.marketShop.items[slot]) : ItemId::COUNT;
}

inline uint16_t price(ItemId id) {
    switch (id) {
    case ItemId::HEART_SCALE: return 600;
    case ItemId::CANDY: return 1800;
    case ItemId::FIRE_STONE: case ItemId::WATER_STONE: case ItemId::THUNDER_STONE: return 900;
    default: {
        // Supply/fallback goods: 90% of the shop, rounded down to 10 coins.
        const uint16_t base = ShopService::buyPrice(id);
        const uint16_t value = uint16_t(base * 9 / 10 / 10 * 10);
        return value ? value : base;
    }
    }
}

inline bool unlocked(const GameState& state, ItemId id) {
    return id < ItemId::COUNT && ExploreItemProgression::isShopItemUnlocked(id, state);
}

inline uint32_t mix(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x ? x : 1;
}

template <unsigned N>
inline ItemId pick(const GameState& state, const ItemId (&list)[N], uint32_t& seed,
                   ItemId avoidA = ItemId::COUNT, ItemId avoidB = ItemId::COUNT) {
    ItemId candidates[N]; unsigned n = 0;
    for (ItemId id : list)
        if (unlocked(state, id) && id != avoidA && id != avoidB) candidates[n++] = id;
    if (!n) return ItemId::COUNT;
    seed = mix(seed + 0x9e3779b9U);
    return candidates[seed % n];
}

// Draw the period shelf once per game day with a private seed (GameRandom is
// untouched). Returns true when the stored shelf changed.
inline bool ensurePeriod(GameState& state) {
    auto& shop = state.marketShop;
    const uint32_t day = periodDay(state);
    if (shop.periodDay == day) return false;
    shop = MarketShopState{};
    shop.periodDay = day;
    uint32_t seed = mix(day ^ 0x4b45434cU);
    static constexpr ItemId SUPPLY_LIST[] = {ItemId::FULL_RESTORE, ItemId::MAX_POTION, ItemId::REVIVE,
                                             ItemId::FULL_HEAL, ItemId::SUPER_POTION, ItemId::POTION};
    static constexpr ItemId SALE_LIST[] = {ItemId::CANDY, ItemId::FIRE_STONE, ItemId::WATER_STONE, ItemId::THUNDER_STONE};
    static constexpr ItemId FALLBACK_LIST[] = {ItemId::MAX_REPEL, ItemId::TASTY_FOOD, ItemId::ANTIDOTE, ItemId::POTION};
    // Rare slot: heart scale once unlocked, otherwise honey keeps the shelf full.
    const ItemId rare = unlocked(state, ItemId::HEART_SCALE) ? ItemId::HEART_SCALE : ItemId::HONEY;
    // Supply favours the strongest unlocked tier.
    ItemId supply = ItemId::COUNT;
    for (ItemId id : SUPPLY_LIST) if (unlocked(state, id)) { supply = id; break; }
    if (supply == ItemId::FULL_RESTORE || supply == ItemId::MAX_POTION) {
        static constexpr ItemId TOP[] = {ItemId::FULL_RESTORE, ItemId::MAX_POTION, ItemId::REVIVE};
        supply = pick(state, TOP, seed);
    }
    ItemId sale = pick(state, SALE_LIST, seed);
    if (sale == ItemId::COUNT) sale = pick(state, FALLBACK_LIST, seed, rare, supply);
    const ItemId shelf[MARKET_SHELF_SLOTS] = {rare, supply, sale};
    for (uint8_t i = 0; i < MARKET_SHELF_SLOTS; ++i) {
        shop.items[i] = shelf[i] < ItemId::COUNT ? uint8_t(shelf[i]) : 255;
        shop.stock[i] = shelf[i] < ItemId::COUNT ? 1 : 0;
    }
    return true;
}

inline bool soldOut(const GameState& state) {
    for (uint8_t i = 0; i < MARKET_SHELF_SLOTS; ++i) if (state.marketShop.stock[i]) return false;
    return true;
}

// `period` is the shelf the confirmation page showed; a day rollover while
// the page is open never charges the old quote against a new shelf.
inline Result check(const GameState& state, uint8_t slot, uint32_t period) {
    const auto& shop = state.marketShop;
    if (shop.periodDay != period || shop.periodDay != periodDay(state)) return Result::STALE;
    const ItemId id = item(state, slot);
    if (id == ItemId::COUNT) return Result::BAD_SLOT;
    if (!shop.stock[slot]) return Result::SOLD_OUT;
    if (!unlocked(state, id) && id != ItemId::HONEY) return Result::LOCKED;
    if (id == ItemId::CANDY && state.candyPurchasesToday >= DAILY_CANDY_PURCHASE_CAP) return Result::DAILY_LIMIT;
    if (ItemInventory::count(state, id) >= ITEM_STACK_CAP) return Result::BAG_FULL;
    if (state.coins < price(id)) return Result::NO_COINS;
    return Result::OK;
}

inline Result buy(GameState& state, uint8_t slot, uint32_t period) {
    const Result r = check(state, slot, period);
    if (r != Result::OK) return r;
    const ItemId id = item(state, slot);
    if (!ItemInventory::add(state, id)) return Result::BAG_FULL;
    state.coins -= price(id);
    if (id == ItemId::CANDY) ++state.candyPurchasesToday;
    --state.marketShop.stock[slot];
    return Result::OK;
}

} // namespace Game::MarketShop
