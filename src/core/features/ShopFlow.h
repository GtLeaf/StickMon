#pragma once

#include "game/ShopService.h"

namespace Features {

// UI-independent catalog, selection, and transaction flow.
class ShopFlow {
public:
    enum class ViewMode : uint8_t {
        CATEGORY,
        EXPLORE,
        DAILY,
        SELL,
    };

    enum Category : uint8_t {
        CATEGORY_DAILY = 0,
        CATEGORY_EXPLORE,
        CATEGORY_SELL,
        CATEGORY_BACK,
        CATEGORY_COUNT,
    };

    enum Item : uint8_t {
        POTION = 0,
        SUPER_POTION,
        ANTIDOTE,
        PARALYZE_HEAL,
        AWAKENING,
        BURN_HEAL,
        ICE_HEAL,
        FOOD,
        TASTY_FOOD,
        SWEET_FOOD,
        SPICY_FOOD,
        SOUR_FOOD,
        BITTER_FOOD,
        DRY_FOOD,
        CANDY,
        SOAP_0,
        SOAP_1,
        SOAP_2,
        MAX_POTION,
        FULL_RESTORE,
        FULL_HEAL,
        FIRE_STONE,
        WATER_STONE,
        THUNDER_STONE,
        REVIVE,
        MAX_REPEL,
        HONEY,
        NUGGET,
        BIG_PEARL,
        STAR_PIECE,
        MARKET_FIRST,
        MARKET_LAST = MARKET_FIRST + 16,
        BACK,
        COUNT,
    };

    ViewMode viewMode = ViewMode::CATEGORY;
    Category activeCategory = CATEGORY_DAILY;
    uint8_t cursor = 0;

    void bind(const Game::GameState& state) { state_ = &state; }
    void reset(bool categoryList);
    bool activateCategory();
    void returnToCategories();
    Item currentItem() const;
    Item selectedItem() const;
    Item itemAtIndex(uint8_t index) const;
    Item itemForCategory(Category category, uint8_t index) const;
    Item sellItemAtIndex(uint8_t index) const;
    uint8_t currentItemCount() const;
    uint8_t itemCountForCategory(Category category, bool includeBack) const;
    uint8_t sellItemCount() const;
    uint8_t ownedCountFor(Item item) const;
    static Item itemFromGameItemId(Game::ItemId item);
    static Game::ItemId gameItemIdFor(Item item);
    static uint16_t priceFor(Item item);
    static uint16_t sellPriceFor(Item item);
    static Game::ShopService::BuyResult buy(Game::GameState& state, Item item);
    static Game::ShopService::SellResult sell(Game::GameState& state, Item item);
protected:
    const Game::GameState& state() const { return *state_; }
private:
    const Game::GameState* state_ = nullptr;
};

}  // namespace Features
