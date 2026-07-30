#pragma once

#include "core/Scene.h"
#include "game/GameState.h"

class ShopScene : public Scene {
public:
    void onEnter() override;
    void onExit() override {}
    SceneUpdateResult update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;

private:
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
        BACK,
        COUNT,
    };

    ViewMode viewMode = ViewMode::CATEGORY;
    Category activeCategory = CATEGORY_DAILY;
    uint8_t cursor = 0;
    float itemColumnX = 128.0f;
    float itemAnimCursor = 0.0f;
    const char* toast = nullptr;
    uint32_t toastUntil = 0;

    void activateCategory();
    void returnToCategories();
    void buyCurrent();
    void sellCurrent();
    Item currentItem() const;
    Item selectedItem() const;
    Item itemAtIndex(uint8_t index) const;
    Item itemForCategory(Category category, uint8_t index) const;
    Item sellItemAtIndex(uint8_t index) const;
    uint8_t currentItemCount() const;
    uint8_t itemCountForCategory(Category category, bool includeBack) const;
    uint8_t sellItemCount() const;
    uint8_t ownedCountFor(Item item) const;
    bool itemColumnSettled() const;
    void renderList();
    void renderCategoryList();
    void renderSubmenu();
    void renderIconColumn(int centerX, bool dimmed, bool selectable);
    void renderItemDetail();
    void drawItemIcon(Item item, int centerX, int centerY, float scale,
                      uint16_t fallbackColor) const;
    void renderToast();
    static uint16_t priceFor(Item item);
    static uint16_t sellPriceFor(Item item);
    static Game::ItemId gameItemIdFor(Item item);
    static const char* nameFor(Item item);
    static const char* descFor(Item item);
    static constexpr Item EXPLORE_ITEMS[] = {
        POTION,
        SUPER_POTION,
        ANTIDOTE,
        PARALYZE_HEAL,
        AWAKENING,
        BURN_HEAL,
        ICE_HEAL,
        MAX_POTION,
        FULL_RESTORE,
        FIRE_STONE,
        WATER_STONE,
        THUNDER_STONE,
        REVIVE,
        MAX_REPEL,
        HONEY,
        BACK,
    };
    static constexpr Item DAILY_ITEMS[] = {
        FOOD,
        TASTY_FOOD,
        SWEET_FOOD,
        SPICY_FOOD,
        SOUR_FOOD,
        BITTER_FOOD,
        DRY_FOOD,
        CANDY,
        FULL_HEAL,
        SOAP_0,
        SOAP_1,
        SOAP_2,
        BACK,
    };
    static constexpr Item SELL_ITEMS[] = {
        FOOD,
        TASTY_FOOD,
        SWEET_FOOD,
        SPICY_FOOD,
        SOUR_FOOD,
        BITTER_FOOD,
        DRY_FOOD,
        POTION,
        SUPER_POTION,
        ANTIDOTE,
        PARALYZE_HEAL,
        AWAKENING,
        BURN_HEAL,
        ICE_HEAL,
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
    };
    static constexpr uint8_t EXPLORE_ITEM_COUNT =
        sizeof(EXPLORE_ITEMS) / sizeof(EXPLORE_ITEMS[0]);
    static constexpr uint8_t DAILY_ITEM_COUNT =
        sizeof(DAILY_ITEMS) / sizeof(DAILY_ITEMS[0]);
    static constexpr uint8_t SELL_ITEM_TYPE_COUNT =
        sizeof(SELL_ITEMS) / sizeof(SELL_ITEMS[0]);

    char toastBuffer[24] = {};
};
