#pragma once

#include "ui/common/BagUi.h"
#include "ui/common/TeamUi.h"
#include "ui/StatusUi.h"
#include "ui/MovesUi.h"
#include "ui/RoomUi.h"
#include "ui/FoodUi.h"
#include "ui/ComputerUi.h"
#include "ui/ContactsUi.h"
#include "ui/MainMenuUi.h"
#include "ui/DebugUi.h"
#include "ui/MenuOverlayUi.h"

#include "core/BuildConfig.h"
#include "core/AppSceneFlow.h"
#include "core/Scene.h"
#include "game/GameState.h"
#include "game/Species.h"
#if STICKMON_UI_ROUND_TOUCH
#include "core/ProgressionUi.h"
#endif

class MenuScene : public Scene, private Ui::TeamHost, private Ui::MovesHost, private Ui::RoomHost, private Ui::FoodHost, private Ui::ComputerHost, private Ui::ContactsHost, private Ui::MainMenuHost
#if STICKMON_ENABLE_DEBUG_FEATURES
    , private Ui::DebugHost
#endif
{
public:
    using BattleBagResult = Features::Bag::BattleResult;

    MenuScene() : mainMenuUi(*this), teamUi(*this), statusUi(*this), movesUi(*this), roomUi(*this), foodUi(*this), computerUi(*this), contactsUi(*this)
#if STICKMON_ENABLE_DEBUG_FEATURES
        , debugUi(*this)
#endif
    {}
    ~MenuScene() override;
    void onEnter() override;
    void onExit() override;
    SceneUpdateResult update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;
    bool touchNavigation() const override;
    bool onTouch(const Platform::IInputDevice::TouchEvent& event) override;
    void cancelTouch() override;
    int touchTarget(int16_t x, int16_t y) override;
    bool onTap(int target) override;
    void openExploreTeamView();
    void openExploreBagView();
    void openBattleBagView(const char* targetName, uint8_t targetTeamSlot);
    BattleBagResult consumeBattleBagResult();
    uint8_t battleBagThrownFoodIndex() const { return battleBagFoodIndex; }
    bool exploreViewClosed() const;

private:
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



    using MovePageMode = Features::Moves::PageMode;
    using TeamAction = Ui::TeamAction;


    static constexpr uint8_t NAV_STACK_CAP = 8;

    Ui::MenuOverlayUi overlayUi;
    Ui::MainMenuUi mainMenuUi;
    BagUi bagUi;
    Ui::TeamUi teamUi;
    Ui::StatusUi statusUi;
    Ui::MovesUi movesUi;
    Ui::RoomUi roomUi;
    Ui::FoodUi foodUi;
    Ui::ComputerUi computerUi;
    Ui::ContactsUi contactsUi;
    uint32_t toastUntil = 0;
    const char* toast = nullptr;
    char toastBuffer[48] = {};
    ViewMode viewMode = ViewMode::MENU;
    bool exploreContextMode = false;
    bool battleBagMode = false;
    const char* battleTargetName = nullptr;
    uint8_t battleTargetTeamSlot = 0;
    BattleBagResult battleBagResult = BattleBagResult::NONE;
    uint8_t battleBagFoodIndex = 0;
#if STICKMON_UI_ROUND_TOUCH
    enum class ItemProgression : uint8_t { NONE, LEVEL_UP, EVOLUTION, CANCELLED, MOVE, REPLACED };
    Game::ItemId pendingTeamItem = Game::ItemId::COUNT;
    bool itemFeedback = false;
    uint8_t itemTargetSlot = 0;
    uint16_t itemHpFrom = 0;
    uint16_t itemHpTo = 0;
    uint32_t itemFeedbackStarted = 0;
    ItemProgression itemProgression = ItemProgression::NONE;
    uint16_t itemCancelledSpecies = 0;
    ProgressionUi::MoveLearnState itemMoveLearn{};
    TouchTap itemProgressionTap;
    bool selectingTeamItem() const override;
    void useTeamItemOn(uint8_t slot) override;
    uint16_t teamItemHp(uint8_t slot) const override;
    void finishTeamItem();
    void advanceItemProgression();
    void renderItemProgression();
    bool handleItemProgressionButton(const ButtonEvent& event);
    bool handleItemProgressionTouch(const Platform::IInputDevice::TouchEvent& event);
    void useDirectBagItem(Game::ItemId item);
#endif
#if STICKMON_ENABLE_DEBUG_FEATURES
    Ui::DebugUi debugUi;
#endif
    ViewMode navStack[NAV_STACK_CAP] = {};
    uint8_t navDepth = 0;

    const Game::GameState& pageState() const override;
    bool pageExploreContext() const override;
    void pageBack() override;
    void pageHome() override;
    void pageNotice(const char* text, uint32_t durationMs = 1100) override;
    void pageBackTutorial() override;
    void pageNavigationTutorial() override;
    void openTeamStatus(uint8_t slot) override;
    void openTeamMoves(uint8_t slot) override;
    bool makeTeamFirst(uint8_t slot) override;
    bool leaveTeam(uint8_t slot) override;
    bool forgetPageMove(uint8_t slot, uint8_t moveSlot) override;
    bool recallPageMove(uint8_t slot, Game::MoveId moveId, uint8_t replacementSlot) override;
    uint8_t collectPageRecallable(uint8_t slot, Game::MoveId* moves, uint8_t capacity) override;

    bool activateMainMenu(uint8_t index) override;
    void openMainMenuFood() override;
    void leaveMainMenu() override;
    static bool nativeSurfaceView(ViewMode view);
    bool directTouchPage() const;
    Scene* delegatedPage();
    void openRoomFood() override;
    void openRoomShower() override;
    bool selectPageFood(uint8_t index) override;
    void openComputerSocial() override;
    void openComputerContacts() override;
    bool contactVisiting(uint8_t slot) const override;
    bool contactInviteLocked(uint8_t slot) const override;
    void openContactStatus(uint8_t slot) override;
    Features::Contacts::InviteResult invitePageContact(uint8_t slot) override;
    void contactJoined() override;
    bool deletePageContact(uint8_t slot) override;
    Ui::MenuOverlayFrame menuOverlayFrame() const;
    void renderTutorial();
    void renderToast();
    void renderBagPage();
    Features::Bag::Context bagContext() const;
    bool handleBagAction(const BagUi::Action& action);
#if STICKMON_ENABLE_DEBUG_FEATURES
    Ui::DebugFrame debugFrame() const override;
    bool activateDebug(Ui::DebugAction action, uint16_t value = 0) override;
#endif
    void resetNavigation();
    void openExploreView(ViewMode next);
    void pushView(ViewMode next);
    void popView();
};
