#include "game/ShopService.h"

#include <algorithm>
#include <cstddef>

#include "game/ExploreItemProgression.h"
#include "game/ItemInventory.h"

namespace Game {
namespace ShopService {
namespace {

constexpr ItemId DAILY_ITEMS[] = {
    ItemId::NORMAL_FOOD, ItemId::TASTY_FOOD, ItemId::SWEET_FOOD,
    ItemId::SPICY_FOOD, ItemId::SOUR_FOOD, ItemId::BITTER_FOOD,
    ItemId::DRY_FOOD, ItemId::CANDY, ItemId::FULL_HEAL,
    ItemId::SOAP_0, ItemId::SOAP_1, ItemId::SOAP_2,
};

constexpr ItemId EXPLORE_ITEMS[] = {
    ItemId::POTION, ItemId::SUPER_POTION, ItemId::ANTIDOTE,
    ItemId::PARALYZE_HEAL, ItemId::AWAKENING, ItemId::BURN_HEAL,
    ItemId::ICE_HEAL, ItemId::MAX_POTION, ItemId::FULL_RESTORE,
    ItemId::FIRE_STONE, ItemId::WATER_STONE, ItemId::THUNDER_STONE,
    ItemId::REVIVE, ItemId::MAX_REPEL, ItemId::HONEY,
};

constexpr ItemId SELL_ITEMS[] = {
    ItemId::NORMAL_FOOD, ItemId::TASTY_FOOD, ItemId::SWEET_FOOD,
    ItemId::SPICY_FOOD, ItemId::SOUR_FOOD, ItemId::BITTER_FOOD,
    ItemId::DRY_FOOD, ItemId::POTION, ItemId::SUPER_POTION,
    ItemId::ANTIDOTE, ItemId::PARALYZE_HEAL, ItemId::AWAKENING,
    ItemId::BURN_HEAL, ItemId::ICE_HEAL, ItemId::CANDY,
    ItemId::SOAP_0, ItemId::SOAP_1, ItemId::SOAP_2,
    ItemId::MAX_POTION, ItemId::FULL_RESTORE, ItemId::FULL_HEAL,
    ItemId::FIRE_STONE, ItemId::WATER_STONE, ItemId::THUNDER_STONE,
    ItemId::REVIVE, ItemId::MAX_REPEL, ItemId::HONEY,
    ItemId::NUGGET, ItemId::BIG_PEARL, ItemId::STAR_PIECE,
};

template <size_t N>
uint8_t visibleCount(const ItemId (&items)[N], const GameState& state,
                     bool requireStock) {
    uint8_t count = 0;
    for (ItemId item : items) {
        if (requireStock) {
            if (ItemInventory::count(state, item) > 0) ++count;
        } else if (ExploreItemProgression::isShopItemUnlocked(item, state)) {
            ++count;
        }
    }
    return count;
}

template <size_t N>
ItemId visibleItemAt(const ItemId (&items)[N], const GameState& state,
                     uint8_t visibleIndex, bool requireStock) {
    uint8_t visible = 0;
    for (ItemId item : items) {
        bool shown = requireStock
            ? ItemInventory::count(state, item) > 0
            : ExploreItemProgression::isShopItemUnlocked(item, state);
        if (shown && visible++ == visibleIndex) return item;
    }
    return ItemId::COUNT;
}

bool purchasable(ItemId item) {
    for (ItemId candidate : DAILY_ITEMS) if (candidate == item) return true;
    for (ItemId candidate : EXPLORE_ITEMS) if (candidate == item) return true;
    return false;
}

bool sellable(ItemId item) {
    for (ItemId candidate : SELL_ITEMS) if (candidate == item) return true;
    return false;
}

}  // namespace

uint16_t buyPrice(ItemId item) {
    switch (item) {
    case ItemId::NORMAL_FOOD: return 20;
    case ItemId::TASTY_FOOD: return 60;
    case ItemId::SWEET_FOOD:
    case ItemId::SPICY_FOOD:
    case ItemId::SOUR_FOOD:
    case ItemId::BITTER_FOOD:
    case ItemId::DRY_FOOD: return 45;
    case ItemId::POTION: return 60;
    case ItemId::SUPER_POTION: return 120;
    case ItemId::ANTIDOTE:
    case ItemId::PARALYZE_HEAL:
    case ItemId::AWAKENING:
    case ItemId::BURN_HEAL:
    case ItemId::ICE_HEAL: return 40;
    case ItemId::CANDY: return 2000;
    case ItemId::MAX_POTION: return 300;
    case ItemId::FULL_RESTORE: return 400;
    case ItemId::FULL_HEAL: return 150;
    case ItemId::FIRE_STONE:
    case ItemId::WATER_STONE:
    case ItemId::THUNDER_STONE: return 1000;
    case ItemId::REVIVE: return 300;
    case ItemId::MAX_REPEL: return 150;
    case ItemId::HONEY: return 100;
    case ItemId::SOAP_0:
    case ItemId::SOAP_1:
    case ItemId::SOAP_2: return 25;
    default: return 0;
    }
}

uint16_t sellPrice(ItemId item) {
    switch (item) {
    case ItemId::NUGGET: return 500;
    case ItemId::BIG_PEARL: return 1000;
    case ItemId::STAR_PIECE: return 2500;
    default: return buyPrice(item) / 2;
    }
}

const char* shortName(ItemId item) {
    switch (item) {
    case ItemId::NORMAL_FOOD: return "FOOD";
    case ItemId::TASTY_FOOD: return "TASTY FOOD";
    case ItemId::SWEET_FOOD: return "SWEET FOOD";
    case ItemId::SPICY_FOOD: return "SPICY FOOD";
    case ItemId::SOUR_FOOD: return "SOUR FOOD";
    case ItemId::BITTER_FOOD: return "BITTER FOOD";
    case ItemId::DRY_FOOD: return "DRY FOOD";
    case ItemId::POTION: return "POTION";
    case ItemId::SUPER_POTION: return "SUPER POTION";
    case ItemId::ANTIDOTE: return "ANTIDOTE";
    case ItemId::CANDY: return "CANDY";
    case ItemId::PARALYZE_HEAL: return "PARALYZE HEAL";
    case ItemId::AWAKENING: return "AWAKENING";
    case ItemId::BURN_HEAL: return "BURN HEAL";
    case ItemId::ICE_HEAL: return "ICE HEAL";
    case ItemId::MAX_POTION: return "MAX POTION";
    case ItemId::FULL_RESTORE: return "FULL RESTORE";
    case ItemId::FULL_HEAL: return "FULL HEAL";
    case ItemId::FIRE_STONE: return "FIRE STONE";
    case ItemId::WATER_STONE: return "WATER STONE";
    case ItemId::THUNDER_STONE: return "THUNDER STONE";
    case ItemId::REVIVE: return "REVIVE";
    case ItemId::MAX_REPEL: return "MAX REPEL";
    case ItemId::HONEY: return "HONEY";
    case ItemId::NUGGET: return "NUGGET";
    case ItemId::BIG_PEARL: return "BIG PEARL";
    case ItemId::STAR_PIECE: return "STAR PIECE";
    case ItemId::SOAP_0: return "BABY SOAP";
    case ItemId::SOAP_1: return "LEAF SOAP";
    case ItemId::SOAP_2: return "MINT SOAP";
    case ItemId::HEART_SCALE: return "HEART SCALE";
    default: return "ITEM";
    }
}

const char* shortDescription(ItemId item) {
    switch (item) {
    case ItemId::POTION: return "RESTORES 20 HP";
    case ItemId::SUPER_POTION: return "RESTORES 50 HP";
    case ItemId::MAX_POTION: return "RESTORES ALL HP";
    case ItemId::FULL_RESTORE: return "HP AND STATUS";
    case ItemId::FULL_HEAL: return "CLEARS STATUS";
    case ItemId::REVIVE: return "REVIVES AT HALF HP";
    case ItemId::ANTIDOTE: return "CURES POISON";
    case ItemId::PARALYZE_HEAL: return "CURES PARALYSIS";
    case ItemId::AWAKENING: return "CURES SLEEP";
    case ItemId::BURN_HEAL: return "CURES BURN";
    case ItemId::ICE_HEAL: return "CURES FREEZE";
    case ItemId::MAX_REPEL: return "AVOIDS ENCOUNTERS";
    case ItemId::HONEY: return "DRAWS ENCOUNTERS";
    case ItemId::CANDY: return "RARE GROWTH ITEM";
    case ItemId::FIRE_STONE:
    case ItemId::WATER_STONE:
    case ItemId::THUNDER_STONE: return "EVOLUTION ITEM";
    case ItemId::HEART_SCALE: return "RECALLS A MOVE";
    default:
        return foodIndexForItemId(item) >= 0 ? "ROOM FOOD" : "CARE ITEM";
    }
}

uint8_t buyItemCount(Category category, const GameState& state) {
    if (category == Category::DAILY) {
        return visibleCount(DAILY_ITEMS, state, false);
    }
    if (category == Category::EXPLORE) {
        return visibleCount(EXPLORE_ITEMS, state, false);
    }
    return 0;
}

ItemId buyItemAt(Category category, const GameState& state,
                 uint8_t visibleIndex) {
    if (category == Category::DAILY) {
        return visibleItemAt(DAILY_ITEMS, state, visibleIndex, false);
    }
    if (category == Category::EXPLORE) {
        return visibleItemAt(EXPLORE_ITEMS, state, visibleIndex, false);
    }
    return ItemId::COUNT;
}

uint8_t sellItemCount(const GameState& state) {
    return visibleCount(SELL_ITEMS, state, true);
}

ItemId sellItemAt(const GameState& state, uint8_t visibleIndex) {
    return visibleItemAt(SELL_ITEMS, state, visibleIndex, true);
}

BuyResult buy(GameState& state, ItemId item) {
    if (!purchasable(item)) return BuyResult::INVALID_ITEM;
    if (!ExploreItemProgression::isShopItemUnlocked(item, state)) {
        return BuyResult::LOCKED;
    }
    if (item == ItemId::CANDY &&
        state.candyPurchasesToday >= DAILY_CANDY_PURCHASE_CAP) {
        return BuyResult::DAILY_LIMIT;
    }
    uint16_t price = buyPrice(item);
    if (state.coins < price) return BuyResult::NOT_ENOUGH_COINS;
    if (!ItemInventory::add(state, item)) return BuyResult::BAG_FULL;
    state.coins -= price;
    if (item == ItemId::CANDY) ++state.candyPurchasesToday;
    return BuyResult::BOUGHT;
}

SellResult sell(GameState& state, ItemId item) {
    if (!sellable(item)) return SellResult::INVALID_ITEM;
    if (!ItemInventory::remove(state, item)) return SellResult::NO_STOCK;
    uint32_t value = static_cast<uint32_t>(state.coins) + sellPrice(item);
    state.coins = std::min<uint32_t>(value, 99999U);
    return SellResult::SOLD;
}

}  // namespace ShopService
}  // namespace Game
