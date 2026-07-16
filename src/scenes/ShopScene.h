#pragma once

#include "core/Scene.h"
#include "game/GameState.h"

class ShopScene : public Scene {
public:
    void onEnter() override;
    void onExit() override {}
    void update(uint32_t nowMs, float dtSeconds) override;
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
        BALL = 0,
        GREAT_BALL,
        HEAVY_BALL,
        TIMER_BALL,
        POTION,
        SUPER_POTION,
        ANTIDOTE,
        FOOD,
        CANDY,
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

    char toastBuffer[24] = {};
};
