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
        CATEGORY_EXPLORE = 0,
        CATEGORY_DAILY,
        CATEGORY_SELL,
        CATEGORY_BACK,
        CATEGORY_COUNT,
    };

    enum Item : uint8_t {
        BALL = 0,
        GREAT_BALL,
        POTION,
        SUPER_POTION,
        ANTIDOTE,
        FOOD,
        CANDY,
        BACK,
        COUNT,
    };

    ViewMode viewMode = ViewMode::CATEGORY;
    uint8_t cursor = 0;
    const char* toast = nullptr;
    uint32_t toastUntil = 0;

    void activateCategory();
    void buyCurrent();
    void sellCurrent();
    Item currentItem() const;
    Item itemAtIndex(uint8_t index) const;
    Item sellItemAtIndex(uint8_t index) const;
    uint8_t currentItemCount() const;
    uint8_t sellItemCount() const;
    uint8_t ownedCountFor(Item item) const;
    void renderList();
    void renderCategoryList();
    void renderItemList();
    void renderSellPage();
    void renderToast();
    static uint16_t priceFor(Item item);
    static uint16_t sellPriceFor(Item item);
    static Game::ItemId gameItemIdFor(Item item);
    static const char* nameFor(Item item);
    static const char* descFor(Item item);

    char toastBuffer[24] = {};
};
