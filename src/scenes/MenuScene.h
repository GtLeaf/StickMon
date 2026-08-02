#pragma once

#include "core/BuildConfig.h"
#include "core/Scene.h"
#include "game/GameState.h"
#include "game/Species.h"

class MenuScene : public Scene {
public:
    enum class BattleBagResult : uint8_t {
        NONE,
        POTION,
        SUPER_POTION,
        ANTIDOTE,
        FOOD_THROWN,
        PARALYZE_HEAL,
        AWAKENING,
        BURN_HEAL,
        ICE_HEAL,
        MAX_POTION,
        FULL_RESTORE,
        FULL_HEAL,
        REVIVE,
    };

    void onEnter() override;
    void onExit() override;
    SceneUpdateResult update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;
    void openExploreTeamView();
    void openExploreBagView();
    void openBattleBagView(const char* targetName, uint8_t targetTeamSlot);
    BattleBagResult consumeBattleBagResult();
    uint8_t battleBagThrownFoodIndex() const { return battleBagFoodIndex; }
    bool exploreViewClosed() const;

private:
    struct BagRow {
        uint8_t source;
        uint8_t count;
    };

    enum MenuItem : uint8_t {
        ITEM_EXPLORE = 0,
        ITEM_TEAM,
        ITEM_ROOM,
        ITEM_BAG,
        ITEM_SHOP,
        ITEM_COMPUTER,
        ITEM_SETTINGS,
#if STICKMON_ENABLE_DEBUG_FEATURES
        ITEM_DEBUG,
#endif
        ITEM_BACK,
        ITEM_COUNT,
    };

    enum class ViewMode : uint8_t {
        MENU,
        TEAM,
        STATUS,
        MOVES,
        ROOM,
        FOOD,
        BAG,
        COMPUTER,
        STORAGE,
#if STICKMON_ENABLE_DEBUG_FEATURES
        DEBUG,
#endif
    };

#if STICKMON_ENABLE_DEBUG_FEATURES
    enum class DebugCategory : uint8_t {
        ROOT,
        MONSTER,
        RESOURCE,
        ENV,
        MOTION,
        BATTLE,
        CONTACT_EVENT,
    };
#endif

    enum class MovePageMode : uint8_t {
        MANAGE,
        RECALL_SELECT,
        RECALL_REPLACE,
    };

    enum class TeamAction : uint8_t {
        STATUS,
        FIRST,
        MOVES,
        LEAVE,
        BACK,
    };

    enum class StorageAction : uint8_t {
        STATUS,
        INVITE,
        DELETE,
        BACK,
    };

    static constexpr uint8_t STATUS_PAGE_COUNT = 5;
    static constexpr uint8_t BAG_ITEM_COUNT = 26;
    static constexpr uint8_t ROOM_ITEM_COUNT = 3;
    static constexpr uint8_t FOOD_ITEM_COUNT = Game::ROOM_FOOD_COUNT + 1;
    static constexpr uint8_t COMPUTER_ITEM_COUNT = 3;
#if STICKMON_ENABLE_DEBUG_FEATURES
    static constexpr uint8_t DEBUG_ROOT_ITEM_COUNT = 7;
    static constexpr uint8_t DEBUG_BATTLE_ROOT_INDEX = 4;
    static constexpr uint8_t DEBUG_CONTACT_EVENT_ROOT_INDEX = 5;
    static constexpr uint8_t DEBUG_MONSTER_ITEM_COUNT = 4;
    static constexpr uint8_t DEBUG_MONSTER_RECOVER_INDEX = 0;
    static constexpr uint8_t DEBUG_MONSTER_LEVEL_UP_INDEX = 1;
    static constexpr uint8_t DEBUG_MONSTER_SWITCH_INDEX = 2;
    static constexpr uint8_t DEBUG_RESOURCE_ITEM_COUNT = 2;
    static constexpr uint8_t DEBUG_ENV_ITEM_COUNT = 3;
    static constexpr uint8_t DEBUG_MOTION_ITEM_COUNT = 3;
    static constexpr uint8_t DEBUG_BATTLE_ITEM_COUNT = 3;
    static constexpr uint8_t DEBUG_BATTLE_RANDOM_INDEX = 0;
    static constexpr uint8_t DEBUG_BATTLE_DRAW_BOUNDS_INDEX = 1;
    static constexpr uint8_t DEBUG_CONTACT_EVENT_ITEM_COUNT = 4;
    static constexpr uint8_t DEBUG_SWITCH_FOCUS_COUNT = 5;
    static constexpr uint8_t DEBUG_TIME_FOCUS_COUNT = 6;
#endif
    static constexpr uint8_t NAV_STACK_CAP = 8;

    static int8_t lastCursor;
    int8_t cursor = 0;
    float animCursor = 0.0f;
    uint32_t toastUntil = 0;
    const char* toast = nullptr;
    char toastBuffer[48] = {};
    ViewMode viewMode = ViewMode::MENU;
    uint8_t statusPage = 0;
    uint8_t statusMonsterIndex = 0;
    bool statusFromStorage = false;
    bool exploreContextMode = false;
    bool battleBagMode = false;
    const char* battleTargetName = nullptr;
    uint8_t battleTargetTeamSlot = 0;
    BattleBagResult battleBagResult = BattleBagResult::NONE;
    uint8_t battleBagFoodIndex = 0;
    uint8_t teamCursor = 0;
    uint8_t teamActionCursor = 0;
    bool teamActionOpen = false;
    uint8_t moveMonsterIndex = 0;
    uint8_t moveCursor = 0;
    uint8_t moveForgetSlot = 0;
    bool moveForgetConfirmOpen = false;
    bool moveForgetConfirmYes = false;
    MovePageMode movePageMode = MovePageMode::MANAGE;
    Game::MoveId recallMoveIds[MAX_RECALLABLE_MOVE_COUNT] = {};
    uint8_t recallMoveCount = 0;
    uint8_t recallSelectedIndex = 0;
    float moveListScroll = 0.0f;
    uint8_t bagCursor = 0;
    uint8_t bagConfirmSource = 0;
    bool bagConfirmOpen = false;
    bool bagConfirmYes = true;
    float bagScroll = 0.0f;
    uint8_t roomCursor = 0;
    uint8_t foodCursor = 0;
    float foodScroll = 0.0f;
    uint8_t computerCursor = 0;
    uint8_t storageCursor = 0;
    uint8_t storageActionCursor = 0;
    bool storageActionOpen = false;
    bool storageInviteConfirmOpen = false;
    bool storageInviteConfirmYes = true;
    bool storageReleaseConfirmOpen = false;
    bool storageReleaseConfirmYes = false;
    float storageScroll = 0.0f;
#if STICKMON_ENABLE_DEBUG_FEATURES
    DebugCategory debugCategory = DebugCategory::ROOT;
    uint8_t debugCursor = 0;
    bool debugSwitchOpen = false;
    uint8_t debugSwitchFocus = 0;
    uint8_t debugSwitchDigits[3] = {0, 0, 1};
    bool debugTimeOpen = false;
    uint8_t debugTimeFocus = 0;
    uint8_t debugTimeDigits[4] = {0, 0, 0, 0};
    float debugScroll = 0.0f;
#endif
    int descScrollKey = -1;
    float descScroll = 0.0f;
    uint32_t descScrollLastMs = 0;
    int statusScrollKey = -1;
    float statusScroll = 0.0f;
    uint32_t statusScrollLastMs = 0;
    int lastBatteryLevel = -1;
    ViewMode navStack[NAV_STACK_CAP] = {};
    uint8_t navDepth = 0;

    void renderMenu();
    void renderToast();
    void renderTeamPage();
    void renderTeamActionPopup();
    void renderStatusPage();
    void renderMovesPage();
    void renderMoveForgetConfirmPopup();
    void renderEggStatusPage();
    void renderBagPage();
    void renderBagConfirmPopup();
    void renderRoomPage();
    void renderFoodPage();
    void renderComputerPage();
    void renderStoragePage();
    void renderStorageActionPopup();
    void renderStorageInviteConfirmPopup();
    void renderStorageReleaseConfirmPopup();
#if STICKMON_ENABLE_DEBUG_FEATURES
    void renderDebugPage();
    void renderDebugSwitchPopup();
    void renderDebugTimePopup();
    uint8_t debugItemCount() const;
    const char* debugItemLabel(uint8_t index) const;
    void handleDebugAction();
    void openDebugSwitchPopup();
    void openDebugTimePopup();
    uint16_t debugSwitchTargetId() const;
    uint16_t debugTimeTargetMinutes() const;
    void incrementDebugTimeDigit();
#endif
    void resetNavigation();
    void openExploreView(ViewMode next);
    void pushView(ViewMode next);
    void popView();
    uint8_t teamActionCount() const;
    TeamAction teamActionAt(uint8_t index) const;
    const char* teamActionLabel(uint8_t index) const;
    uint8_t storageActionCount() const;
    StorageAction storageActionAt(uint8_t index) const;
    const char* storageActionLabel(uint8_t index) const;
    uint8_t collectVisibleBagRows(BagRow* rows, uint8_t maxRows) const;
    uint8_t visibleFoodIndexOf(uint8_t foodIndex) const;
    bool isFoodBackIndex(uint8_t index) const;
    void drawSelectionDiamond(int cx, int cy, uint16_t color);
    void renderSplitList(const BagRow* rows, uint8_t count);
    void renderBagDetail(const BagRow& row);
    void renderPageIndicator(uint8_t page, uint8_t count);
    bool updateStatusScroll(int scrollKey, int maxScroll);
    bool updateDescriptionScroll(int scrollKey, int maxScroll);
    void renderScrollableDescription(const char* const* lines, int lineCount,
                                     int x, int y, int w, uint16_t color,
                                     int scrollKey);
};
