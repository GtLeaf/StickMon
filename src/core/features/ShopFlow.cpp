#include "core/features/ShopFlow.h"
#include "game/ItemInventory.h"

namespace Features {

void ShopFlow::reset(bool categoryList) {
    viewMode = categoryList ? ViewMode::CATEGORY : ViewMode::DAILY;
    activeCategory = CATEGORY_DAILY;
    cursor = 0;
}

bool ShopFlow::activateCategory() {
    switch (cursor) {
    case CATEGORY_DAILY:
        activeCategory = CATEGORY_DAILY;
        viewMode = ViewMode::DAILY;
        cursor = 0;
        break;
    case CATEGORY_EXPLORE:
        activeCategory = CATEGORY_EXPLORE;
        viewMode = ViewMode::EXPLORE;
        cursor = 0;
        break;
    case CATEGORY_SELL:
        activeCategory = CATEGORY_SELL;
        viewMode = ViewMode::SELL;
        cursor = 0;
        break;
    default:
        return false;
    }
    return true;
}

void ShopFlow::returnToCategories() {
    viewMode = ViewMode::CATEGORY;
    cursor = static_cast<uint8_t>(activeCategory);
}

ShopFlow::Item ShopFlow::currentItem() const {
    return itemAtIndex(cursor);
}

ShopFlow::Item ShopFlow::selectedItem() const {
    return viewMode == ViewMode::SELL ? sellItemAtIndex(cursor) : currentItem();
}

ShopFlow::Item ShopFlow::itemForCategory(Category category, uint8_t index) const {
    if (category == CATEGORY_SELL) return sellItemAtIndex(index);
    if (category == CATEGORY_BACK) return BACK;

    // ShopService owns the catalog and progression rules; selection keeps
    // the existing item ordering used by both UI families.
    Game::ShopService::Category serviceCategory =
        category == CATEGORY_DAILY
            ? Game::ShopService::Category::DAILY
            : Game::ShopService::Category::EXPLORE;
    uint8_t visibleCount = Game::ShopService::buyItemCount(
        serviceCategory, state());
    if (index < visibleCount) {
        return itemFromGameItemId(Game::ShopService::buyItemAt(
            serviceCategory, state(), index));
    }
    return BACK;
}

ShopFlow::Item ShopFlow::itemFromGameItemId(Game::ItemId item) {
    if (item >= Game::ItemId::HP_UP && item < Game::ItemId::COUNT) {
        return static_cast<Item>(MARKET_FIRST + static_cast<uint8_t>(item) -
                                 static_cast<uint8_t>(Game::ItemId::HP_UP));
    }
    switch (item) {
    case Game::ItemId::POTION: return POTION;
    case Game::ItemId::SUPER_POTION: return SUPER_POTION;
    case Game::ItemId::ANTIDOTE: return ANTIDOTE;
    case Game::ItemId::PARALYZE_HEAL: return PARALYZE_HEAL;
    case Game::ItemId::AWAKENING: return AWAKENING;
    case Game::ItemId::BURN_HEAL: return BURN_HEAL;
    case Game::ItemId::ICE_HEAL: return ICE_HEAL;
    case Game::ItemId::NORMAL_FOOD: return FOOD;
    case Game::ItemId::TASTY_FOOD: return TASTY_FOOD;
    case Game::ItemId::SWEET_FOOD: return SWEET_FOOD;
    case Game::ItemId::SPICY_FOOD: return SPICY_FOOD;
    case Game::ItemId::SOUR_FOOD: return SOUR_FOOD;
    case Game::ItemId::BITTER_FOOD: return BITTER_FOOD;
    case Game::ItemId::DRY_FOOD: return DRY_FOOD;
    case Game::ItemId::CANDY: return CANDY;
    case Game::ItemId::MAX_POTION: return MAX_POTION;
    case Game::ItemId::FULL_RESTORE: return FULL_RESTORE;
    case Game::ItemId::FULL_HEAL: return FULL_HEAL;
    case Game::ItemId::FIRE_STONE: return FIRE_STONE;
    case Game::ItemId::WATER_STONE: return WATER_STONE;
    case Game::ItemId::THUNDER_STONE: return THUNDER_STONE;
    case Game::ItemId::REVIVE: return REVIVE;
    case Game::ItemId::MAX_REPEL: return MAX_REPEL;
    case Game::ItemId::HONEY: return HONEY;
    case Game::ItemId::NUGGET: return NUGGET;
    case Game::ItemId::BIG_PEARL: return BIG_PEARL;
    case Game::ItemId::STAR_PIECE: return STAR_PIECE;
    case Game::ItemId::SOAP_0: return SOAP_0;
    case Game::ItemId::SOAP_1: return SOAP_1;
    case Game::ItemId::SOAP_2: return SOAP_2;
    case Game::ItemId::HEART_SCALE:
    case Game::ItemId::COUNT:
        return BACK;
    default: return BACK;
    }
    return BACK;
}

ShopFlow::Item ShopFlow::itemAtIndex(uint8_t index) const {
    return itemForCategory(activeCategory, index);
}

ShopFlow::Item ShopFlow::sellItemAtIndex(uint8_t index) const {
    const Game::GameState& game = state();
    uint8_t visibleCount = Game::ShopService::sellItemCount(game);
    if (index < visibleCount) {
        return itemFromGameItemId(Game::ShopService::sellItemAt(game, index));
    }
    return BACK;
}

uint8_t ShopFlow::currentItemCount() const {
    return viewMode == ViewMode::CATEGORY
        ? static_cast<uint8_t>(CATEGORY_COUNT) : itemCountForCategory(activeCategory, true);
}

uint8_t ShopFlow::itemCountForCategory(Category category, bool includeBack) const {
    if (category == CATEGORY_SELL) {
        uint8_t count = Game::ShopService::sellItemCount(
            state());
        return includeBack ? static_cast<uint8_t>(count + 1) : count;
    }
    if (category != CATEGORY_DAILY && category != CATEGORY_EXPLORE) return 0;

    Game::ShopService::Category serviceCategory =
        category == CATEGORY_DAILY
            ? Game::ShopService::Category::DAILY
            : Game::ShopService::Category::EXPLORE;
    uint8_t count = Game::ShopService::buyItemCount(
        serviceCategory, state());
    return includeBack ? static_cast<uint8_t>(count + 1) : count;
}

uint8_t ShopFlow::sellItemCount() const {
    return static_cast<uint8_t>(
        Game::ShopService::sellItemCount(state()) + 1);
}

uint8_t ShopFlow::ownedCountFor(Item item) const {
    Game::ItemId gameItem = gameItemIdFor(item);
    if (gameItem == Game::ItemId::COUNT) return 0;
    return Game::ItemInventory::count(state(), gameItem);
}

uint16_t ShopFlow::priceFor(Item item) {
    return Game::ShopService::buyPrice(gameItemIdFor(item));
}

uint16_t ShopFlow::sellPriceFor(Item item) {
    return Game::ShopService::sellPrice(gameItemIdFor(item));
}

Game::ItemId ShopFlow::gameItemIdFor(Item item) {
    if (item >= MARKET_FIRST && item <= MARKET_LAST) {
        return static_cast<Game::ItemId>(static_cast<uint8_t>(Game::ItemId::HP_UP) +
                                         item - MARKET_FIRST);
    }
    switch (item) {
    case POTION: return Game::ItemId::POTION;
    case SUPER_POTION: return Game::ItemId::SUPER_POTION;
    case ANTIDOTE: return Game::ItemId::ANTIDOTE;
    case PARALYZE_HEAL: return Game::ItemId::PARALYZE_HEAL;
    case AWAKENING: return Game::ItemId::AWAKENING;
    case BURN_HEAL: return Game::ItemId::BURN_HEAL;
    case ICE_HEAL: return Game::ItemId::ICE_HEAL;
    case FOOD: return Game::ItemId::NORMAL_FOOD;
    case TASTY_FOOD: return Game::ItemId::TASTY_FOOD;
    case SWEET_FOOD: return Game::ItemId::SWEET_FOOD;
    case SPICY_FOOD: return Game::ItemId::SPICY_FOOD;
    case SOUR_FOOD: return Game::ItemId::SOUR_FOOD;
    case BITTER_FOOD: return Game::ItemId::BITTER_FOOD;
    case DRY_FOOD: return Game::ItemId::DRY_FOOD;
    case CANDY: return Game::ItemId::CANDY;
    case MAX_POTION: return Game::ItemId::MAX_POTION;
    case FULL_RESTORE: return Game::ItemId::FULL_RESTORE;
    case FULL_HEAL: return Game::ItemId::FULL_HEAL;
    case FIRE_STONE: return Game::ItemId::FIRE_STONE;
    case WATER_STONE: return Game::ItemId::WATER_STONE;
    case THUNDER_STONE: return Game::ItemId::THUNDER_STONE;
    case REVIVE: return Game::ItemId::REVIVE;
    case MAX_REPEL: return Game::ItemId::MAX_REPEL;
    case HONEY: return Game::ItemId::HONEY;
    case NUGGET: return Game::ItemId::NUGGET;
    case BIG_PEARL: return Game::ItemId::BIG_PEARL;
    case STAR_PIECE: return Game::ItemId::STAR_PIECE;
    case SOAP_0: return Game::ItemId::SOAP_0;
    case SOAP_1: return Game::ItemId::SOAP_1;
    case SOAP_2: return Game::ItemId::SOAP_2;
    default: return Game::ItemId::COUNT;
    }
}

Game::ShopService::BuyResult ShopFlow::buy(Game::GameState& state, Item item) {
    return Game::ShopService::buy(state, gameItemIdFor(item));
}

Game::ShopService::SellResult ShopFlow::sell(Game::GameState& state, Item item) {
    return Game::ShopService::sell(state, gameItemIdFor(item));
}

}  // namespace Features
