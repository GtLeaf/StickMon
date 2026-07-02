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
    struct BagRow {
        uint8_t source;
        uint8_t count;
        uint16_t color;
    };

    enum MenuItem : uint8_t {
        ITEM_TEAM = 0,
        ITEM_BAG,
        ITEM_EXPLORE,
        ITEM_SHOP,
        ITEM_COMPUTER,
        ITEM_SETTINGS,
        ITEM_DEBUG,
        ITEM_BACK,
        ITEM_COUNT,
    };

    enum class ViewMode : uint8_t {
        MENU,
        TEAM,
        STATUS,
        BAG,
        COMPUTER,
        STORAGE,
        DEBUG,
    };

    static constexpr uint8_t STATUS_PAGE_COUNT = 7;
    static constexpr uint8_t BAG_ITEM_COUNT = 8;
    static constexpr uint8_t COMPUTER_ITEM_COUNT = 3;
    static constexpr uint8_t DEBUG_ITEM_COUNT = 4;
    static constexpr uint8_t DEBUG_SWITCH_FOCUS_COUNT = 5;

    static int8_t lastCursor;
    int8_t cursor = 0;
    float animCursor = 0.0f;
    uint32_t toastUntil = 0;
    const char* toast = nullptr;
    ViewMode viewMode = ViewMode::MENU;
    uint8_t statusPage = 0;
    uint8_t statusMonsterIndex = 0;
    uint8_t teamCursor = 0;
    uint8_t teamActionCursor = 0;
    bool teamActionOpen = false;
    uint8_t bagCursor = 0;
    uint8_t bagConfirmSource = 0;
    bool bagConfirmOpen = false;
    bool bagConfirmYes = true;
    float bagScroll = 0.0f;
    uint8_t computerCursor = 0;
    uint8_t storageCursor = 0;
    float storageScroll = 0.0f;
    uint8_t debugCursor = 0;
    bool debugSwitchOpen = false;
    uint8_t debugSwitchFocus = 0;
    uint8_t debugSwitchDigits[3] = {0, 0, 1};
    int descScrollKey = -1;
    float descScroll = 0.0f;
    uint32_t descScrollLastMs = 0;

    void renderMenu();
    void renderToast();
    void renderTeamPage();
    void renderTeamActionPopup();
    void renderStatusPage();
    void renderEggStatusPage();
    void renderBagPage();
    void renderBagConfirmPopup();
    void renderComputerPage();
    void renderStoragePage();
    void renderDebugPage();
    void renderDebugSwitchPopup();
    void openDebugSwitchPopup();
    uint16_t debugSwitchTargetId() const;
    uint8_t collectVisibleBagRows(BagRow* rows, uint8_t maxRows) const;
    void renderSplitList(const BagRow* rows, uint8_t count);
    void renderBagDetail(const BagRow& row);
    void renderPageIndicator(uint8_t page, uint8_t count);
    void updateDescriptionScroll(int scrollKey, int maxScroll);
    void renderScrollableDescription(const char* const* lines, int lineCount,
                                     int x, int y, int w, uint16_t color,
                                     int scrollKey);
};
