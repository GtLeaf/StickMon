#pragma once

#include "core/Scene.h"

class MenuScene : public Scene {
public:
    void onEnter() override;
    void onExit() override;
    void update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;

private:
    enum MenuItem : uint8_t {
        ITEM_STATUS = 0,
        ITEM_BAG,
        ITEM_EXPLORE,
        ITEM_SHOP,
        ITEM_SOCIAL,
        ITEM_SETTINGS,
        ITEM_BACK,
        ITEM_COUNT,
    };

    enum class ViewMode : uint8_t {
        MENU,
        STATUS,
        BAG,
    };

    static constexpr uint8_t STATUS_PAGE_COUNT = 7;

    static int8_t lastCursor;
    int8_t cursor = 0;
    uint32_t toastUntil = 0;
    const char* toast = nullptr;
    ViewMode viewMode = ViewMode::MENU;
    uint8_t statusPage = 0;

    void renderMenu();
    void renderToast();
    void renderStatusPage();
    void renderEggStatusPage();
    void renderBagPage();
    void renderTitleBar(const char* title);
    void renderPageIndicator(uint8_t page, uint8_t count);
};
