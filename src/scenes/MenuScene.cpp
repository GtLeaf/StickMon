#include "scenes/MenuScene.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "assets/GameAssets.h"
#include "core/UiStrings.h"
#include "core/GameEngine.h"
#include "core/MathUtil.h"
#include "core/UiMotion.h"
#include "game/BondSystem.h"
#include "game/MarketItemCatalog.h"
#include "game/Species.h"
#include "game/TeamRoster.h"
#include "hardware/Hal.h"
#include "platform/api/FlashStorage.h"
#include "ui/SceneSurface.h"

namespace {
bool applyBagItem(Game::ItemId item, uint8_t slot) {
    auto& engine = GameEngine::ins();
    if (Game::MarketItemCatalog::contains(item))
        return engine.useTeamItem(item, slot) == Game::ItemInventory::UseResult::USED;
    switch (item) {
    case Game::ItemId::POTION: return engine.usePotion(slot);
    case Game::ItemId::SUPER_POTION: return engine.useSuperPotion(slot);
    case Game::ItemId::ANTIDOTE: return engine.useAntidote(slot);
    case Game::ItemId::PARALYZE_HEAL: return engine.useParalyzeHeal(slot);
    case Game::ItemId::AWAKENING: return engine.useAwakening(slot);
    case Game::ItemId::BURN_HEAL: return engine.useBurnHeal(slot);
    case Game::ItemId::ICE_HEAL: return engine.useIceHeal(slot);
    case Game::ItemId::MAX_POTION: return engine.useMaxPotion(slot);
    case Game::ItemId::FULL_RESTORE: return engine.useFullRestore(slot);
    case Game::ItemId::FULL_HEAL: return engine.useFullHeal(slot);
    case Game::ItemId::FIRE_STONE:
    case Game::ItemId::WATER_STONE:
    case Game::ItemId::THUNDER_STONE: return engine.useEvolutionStone(slot, item);
    case Game::ItemId::REVIVE: return engine.useRevive(slot);
    case Game::ItemId::MAX_REPEL: return engine.useMaxRepel();
    case Game::ItemId::HONEY: return engine.useHoney();
    default: return Game::foodIndexForItemId(item) >= 0 && engine.removeItem(item);
    }
}





}

MenuScene::~MenuScene() = default;

#if STICKMON_ENABLE_DEBUG_FEATURES
Ui::DebugFrame MenuScene::debugFrame() const {
    auto& engine = GameEngine::ins();
    return {engine.gameMinutesOfDay(), engine.debugLightSourceLabel(),
            engine.debugTiltControlEnabled(), engine.debugWalkBoundaryVisible(),
            engine.debugBattleDrawBoundsVisible()};
}

bool MenuScene::activateDebug(Ui::DebugAction action, uint16_t value) {
    auto& engine = GameEngine::ins();
    const auto& state = engine.gameState();
    switch (action) {
    case Ui::DebugAction::RECOVER:
        if (state.teamCount == 0) pageNotice(Ui::Menu::HATCH_FIRST);
        else { engine.debugRecoverTeam(); pageNotice(Ui::Debug::RECOVERED); }
        break;
    case Ui::DebugAction::LEVEL_UP:
        if (state.teamCount == 0) pageNotice(Ui::Menu::HATCH_FIRST);
        else if (state.team[0].level >= Game::LEVEL_MAX) pageNotice(Ui::Debug::LEVEL_MAX);
        else if (engine.debugLevelUpActiveMonster()) {
            snprintf(toastBuffer, sizeof(toastBuffer), Ui::Debug::LEVEL_UP_FMT,
                     engine.activeMonster().level);
            pageNotice(toastBuffer);
        } else pageNotice(Ui::Debug::LEVEL_MAX);
        break;
    case Ui::DebugAction::SWITCH_SPECIES: {
        bool changed = engine.debugSetActiveSpecies(value);
        pageNotice(changed ? Ui::Debug::SWITCHED : Ui::Debug::INVALID_ID);
        return changed;
    }
    case Ui::DebugAction::ADD_COINS:
        engine.addCoins(1000);
        pageNotice(Ui::Debug::COINS_ADDED);
        break;
    case Ui::DebugAction::SET_TIME:
        engine.debugAdvanceToTimeOfDay(value);
        pageNotice(Ui::Debug::TIME_SET);
        break;
    case Ui::DebugAction::CYCLE_LIGHT: engine.cycleDebugLightSource(); break;
    case Ui::DebugAction::TOGGLE_TILT: engine.toggleDebugTiltControl(); break;
    case Ui::DebugAction::TOGGLE_WALK_BOUNDS: engine.toggleDebugWalkBoundary(); break;
    case Ui::DebugAction::PAIR_INTERACTION:
        if (!state.oobeDone || state.teamCount < 2)
            pageNotice(Ui::Debug::PAIR_NEEDS_TWO, 1400);
        else engine.requestDebugPairInteraction();
        break;
    case Ui::DebugAction::RANDOM_BATTLE:
        if (!state.oobeDone || state.teamCount == 0) pageNotice(Ui::Menu::HATCH_FIRST);
        else if (engine.activeMonster().fainted || engine.activeMonster().hpCur == 0)
            pageNotice(Ui::Menu::FAINTED_TOAST);
        else engine.beginDebugBattle();
        break;
    case Ui::DebugAction::TOGGLE_BATTLE_BOUNDS: engine.toggleDebugBattleDrawBounds(); break;
    case Ui::DebugAction::CONTACT_PLAY:
    case Ui::DebugAction::CONTACT_GIFT:
    case Ui::DebugAction::CONTACT_EXPLORE: {
        const auto kind = action == Ui::DebugAction::CONTACT_PLAY ? ContactVisitKind::PLAY
            : (action == Ui::DebugAction::CONTACT_GIFT ? ContactVisitKind::GIFT : ContactVisitKind::EXPLORE);
        switch (engine.debugTriggerContactVisit(kind)) {
        case DebugContactEventResult::STARTED:
            engine.requestScene(SceneID::MAIN);
            return true;
        case DebugContactEventResult::TEAM_NOT_SOLO: pageNotice(Ui::Social::HOST_TEAM_REQUIRED, 1400); break;
        case DebugContactEventResult::NO_CONTACT: pageNotice(Ui::Debug::NO_CONTACT, 1400); break;
        default: pageNotice(Ui::Debug::EVENT_BUSY, 1400); break;
        }
        break;
    }
    }
    return true;
}
#endif

void MenuScene::onEnter() {
#if STICKMON_UI_ROUND_TOUCH
    pendingTeamItem = Game::ItemId::COUNT;
    itemFeedback = false;
    itemProgression = ItemProgression::NONE;
#endif
    exploreContextMode = false;
    battleBagMode = false;
    battleTargetName = nullptr;
    battleTargetTeamSlot = 0;
    battleBagResult = BattleBagResult::NONE;
    resetNavigation();
    statusUi.reset();
    teamUi.reset();
    movesUi.reset();
    bagUi.reset();
    roomUi.reset();
    foodUi.reset();
    computerUi.reset();
    contactsUi.reset();
#if STICKMON_ENABLE_DEBUG_FEATURES
    debugUi.reset();
#endif
    mainMenuUi.enter(GameEngine::ins().previousScene() == GameEngine::ins().homeScene());
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (GameEngine::ins().consumeDebugMenuReturnRequest()) {
        pushView(ViewMode::DEBUG);
        debugUi.returnFromBattle();
    }
#endif
}

void MenuScene::openExploreTeamView() {
    openExploreView(ViewMode::TEAM);
}

void MenuScene::openExploreBagView() {
    openExploreView(ViewMode::BAG);
}

void MenuScene::openBattleBagView(const char* targetName,
                                  uint8_t targetTeamSlot) {
    openExploreView(ViewMode::BAG);
    battleBagMode = true;
    battleTargetName = targetName;
    battleTargetTeamSlot = targetTeamSlot;
}

MenuScene::BattleBagResult MenuScene::consumeBattleBagResult() {
    BattleBagResult result = battleBagResult;
    battleBagResult = BattleBagResult::NONE;
    return result;
}

bool MenuScene::nativeSurfaceView(ViewMode view) {
    return view == ViewMode::STATUS || view == ViewMode::MOVES || view == ViewMode::STORAGE
#if defined(STICKMON_UI_ROUND_TOUCH) && STICKMON_UI_ROUND_TOUCH
        || view == ViewMode::BAG
#endif
#if STICKMON_ENABLE_DEBUG_FEATURES
        || view == ViewMode::DEBUG
#endif
        ;
}

void MenuScene::openExploreView(ViewMode next) {
    onEnter();
    exploreContextMode = true;
    viewMode = next;
    navDepth = 0;
    Ui::SceneSurface::mainMenu(false, nativeSurfaceView(next));
}

bool MenuScene::exploreViewClosed() const {
    return exploreContextMode && viewMode == ViewMode::MENU;
}

void MenuScene::onExit() {
    cancelTouch();
    mainMenuUi.rememberCursor();
}

void MenuScene::resetNavigation() {
#if STICKMON_UI_ROUND_TOUCH
    pendingTeamItem = Game::ItemId::COUNT;
    itemFeedback = false;
    itemProgression = ItemProgression::NONE;
#endif
    cancelTouch();
    navDepth = 0;
    viewMode = ViewMode::MENU;
    Ui::SceneSurface::mainMenu(!exploreContextMode);
    bagUi.leave();
    mainMenuUi.resetNavigation();
}

void MenuScene::pushView(ViewMode next) {
    if (next == viewMode) return;
    cancelTouch();
    if (next == ViewMode::BAG) bagUi.enter();
    if (navDepth < NAV_STACK_CAP) {
        navStack[navDepth++] = viewMode;
    } else {
        for (uint8_t i = 1; i < NAV_STACK_CAP; ++i) {
            navStack[i - 1] = navStack[i];
        }
        navStack[NAV_STACK_CAP - 1] = viewMode;
    }
    viewMode = next;
    Ui::SceneSurface::mainMenu(false, nativeSurfaceView(next));
}

void MenuScene::popView() {
#if STICKMON_UI_ROUND_TOUCH
    if (selectingTeamItem()) {
        pendingTeamItem = Game::ItemId::COUNT;
        itemFeedback = false;
        toast = nullptr;
    }
#endif
    cancelTouch();
    bagUi.leave();
    teamUi.teamActionOpen = false;
    movesUi.moveForgetConfirmOpen = false;
    contactsUi.closePopups();
#if STICKMON_ENABLE_DEBUG_FEATURES
    debugUi.closePopups();
#endif

    if (navDepth > 0) {
        viewMode = navStack[--navDepth];
    } else {
        viewMode = ViewMode::MENU;
    }
    if (viewMode == ViewMode::MENU) {
        statusUi.statusFromStorage = false;
    }
    Ui::SceneSurface::mainMenu(viewMode == ViewMode::MENU && !exploreContextMode,
        nativeSurfaceView(viewMode));
}

SceneUpdateResult MenuScene::update(uint32_t nowMs, float dtSeconds) {
#if STICKMON_UI_ROUND_TOUCH
    if (itemFeedback) {
        if (nowMs - itemFeedbackStarted >= 700) {
            itemFeedback = false;
            advanceItemProgression();
            return SceneUpdateResult::frame();
        }
        return SceneUpdateResult::animate(33);
    }
    if (itemProgression == ItemProgression::EVOLUTION &&
        ProgressionUi::evolutionCancelHeld(nowMs))
        handleItemProgressionButton({1, BtnAction::PRESSED});
    if (itemProgression != ItemProgression::NONE) return SceneUpdateResult::animate(33);
#endif
    RenderDemand demand;
    Scene* page = delegatedPage();
    if (page || viewMode == ViewMode::BAG) {
        const auto result = page ? page->update(nowMs, dtSeconds)
            : bagUi.update(GameEngine::ins().gameState(), bagContext(), nowMs);
        demand.changed(result.redraw);
        demand.wakeIn(result.nextUpdateDelayMs);
        if (demand.expired(toast != nullptr, nowMs, toastUntil)) toast = nullptr;
        return demand.result();
    }

    if (demand.expired(toast != nullptr, nowMs, toastUntil)) toast = nullptr;
    return demand.result();
}

bool MenuScene::directTouchPage() const {
    if (viewMode == ViewMode::MENU) return !mainMenuUi.touchNavigation();
    if (viewMode == ViewMode::TEAM) return Ui::TeamUi::directTouch();
    if (viewMode == ViewMode::STATUS) return !statusUi.touchNavigation();
    if (viewMode == ViewMode::MOVES) return !movesUi.touchNavigation();
    if (viewMode == ViewMode::BAG) return BagUi::directTouch();
    if (viewMode == ViewMode::ROOM) return !roomUi.touchNavigation();
    if (viewMode == ViewMode::FOOD) return !foodUi.touchNavigation();
    if (viewMode == ViewMode::COMPUTER) return !computerUi.touchNavigation();
    if (viewMode == ViewMode::STORAGE) return !contactsUi.touchNavigation();
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (viewMode == ViewMode::DEBUG) return !debugUi.touchNavigation();
#endif
    return false;
}

bool MenuScene::touchNavigation() const {
    if (viewMode == ViewMode::MENU) return mainMenuUi.touchNavigation();
    if (viewMode == ViewMode::TEAM) return teamUi.touchNavigation();
    if (viewMode == ViewMode::STATUS) return statusUi.touchNavigation();
    if (viewMode == ViewMode::MOVES) return movesUi.touchNavigation();
    if (viewMode == ViewMode::ROOM) return roomUi.touchNavigation();
    if (viewMode == ViewMode::FOOD) return foodUi.touchNavigation();
    if (viewMode == ViewMode::COMPUTER) return computerUi.touchNavigation();
    if (viewMode == ViewMode::STORAGE) return contactsUi.touchNavigation();
    return overlayUi.touchNavigation(directTouchPage());
}

void MenuScene::cancelTouch() {
#if STICKMON_UI_ROUND_TOUCH
    ProgressionUi::cancelMoveLearnTouch();
#endif
    Scene::cancelTouch();
    bagUi.cancelTouch();
    teamUi.cancelTouch();
    statusUi.cancelTouch();
    movesUi.cancelTouch();
    roomUi.cancelTouch();
    foodUi.cancelTouch();
    computerUi.cancelTouch();
    contactsUi.cancelTouch();
    mainMenuUi.cancelTouch();
#if STICKMON_ENABLE_DEBUG_FEATURES
    debugUi.cancelTouch();
#endif
}

bool MenuScene::onTouch(const Platform::IInputDevice::TouchEvent& event) {
#if STICKMON_UI_ROUND_TOUCH
    if (itemFeedback) return true;
    if (itemProgression != ItemProgression::NONE) return handleItemProgressionTouch(event);
#endif
    if (auto* page = delegatedPage()) return page->onTouch(event);
    if (viewMode == ViewMode::BAG && BagUi::directTouch())
        return handleBagAction(bagUi.onTouch(event, GameEngine::ins().gameState(), bagContext()));
    return Scene::onTouch(event);
}

int MenuScene::touchTarget(int16_t x, int16_t y) {
#if STICKMON_UI_ROUND_TOUCH
    if (itemFeedback || itemProgression != ItemProgression::NONE) return -1;
#endif
    const int mode = int(viewMode) * 256;
    Scene* page = delegatedPage();
    if (page || viewMode == ViewMode::BAG) {
        const int target = page ? page->touchTarget(x, y)
            : bagUi.target(x, y, GameEngine::ins().gameState(), bagContext());
        return target < 0 ? -1 : mode + target;
    }
    return -1;
}

bool MenuScene::onTap(int target) {
#if STICKMON_UI_ROUND_TOUCH
    if (itemFeedback || itemProgression != ItemProgression::NONE) return true;
#endif
    if (target / 256 != int(viewMode)) return false;
    const int index = target % 256;
    if (auto* page = delegatedPage()) return page->onTap(index);
    if (viewMode == ViewMode::BAG)
        return handleBagAction(bagUi.tap(index, GameEngine::ins().gameState(), bagContext()));
    return false;
}

bool MenuScene::onButton(const ButtonEvent& event) {
#if STICKMON_UI_ROUND_TOUCH
    if (itemFeedback) return true;
    if (itemProgression != ItemProgression::NONE) return handleItemProgressionButton(event);
#endif
    if (auto* page = delegatedPage()) return page->onButton(event);
    if (viewMode == ViewMode::BAG)
        return handleBagAction(bagUi.button(event, GameEngine::ins().gameState(), bagContext()));
    if (viewMode != ViewMode::MENU) {
        if (!exploreContextMode && event.btn == 1 &&
            event.action == BtnAction::LONG_PRESS) {
            GameEngine::ins().completeTutorial(Game::TutorialStep::MENU_BACK);
        }
        if (event.btn == 0 && event.action == BtnAction::LONG_PRESS) {
            if (exploreContextMode) {
                resetNavigation();
                return true;
            }
            GameEngine::ins().requestScene(GameEngine::ins().homeScene());
            return true;
        }
        if (event.btn == 1 && event.action == BtnAction::LONG_PRESS) {

            popView();
            return true;
        }

        if (event.btn == 0 && event.action == BtnAction::PRESSED) {
            popView();
            return true;
        }
        return false;
    }

    return false;
}

bool MenuScene::activateMainMenu(uint8_t index) {
    AppSceneFlow::MainMenuEntry menuEntry =
        AppSceneFlow::mainMenuEntry(
            index,
            STICKMON_ENABLE_DEBUG_FEATURES != 0);
    switch (menuEntry.target) {
    case AppSceneFlow::Scene::TEAM:
        pushView(ViewMode::TEAM);
        statusUi.statusFromStorage = false;
        teamUi.teamCursor = 0;
        teamUi.teamActionCursor = 0;
        teamUi.teamActionOpen = false;
        return true;
    case AppSceneFlow::Scene::ROOM:
        pushView(ViewMode::ROOM);
        roomUi.reset();
        return true;
    case AppSceneFlow::Scene::BAG:
        pushView(ViewMode::BAG);
        bagUi.reset();
        return true;
    case AppSceneFlow::Scene::EXPLORE_AREAS: {
        const auto& state = GameEngine::ins().gameState();
        if (!state.oobeDone || state.teamCount == 0) {
            toast = Ui::Menu::HATCH_FIRST;
            toastUntil = Hal::ins().millis() + 1100;
            return true;
        }
        if (GameEngine::ins().exploreBlockedByGuest()) {
            toast = Ui::Menu::GUEST_AT_HOME;
            toastUntil = Hal::ins().millis() + 1100;
            return true;
        }
        const auto& mon = GameEngine::ins().activeMonster();
        if (mon.fainted || mon.hpCur == 0) {
            toast = Ui::Menu::FAINTED_TOAST;
            toastUntil = Hal::ins().millis() + 1100;
            return true;
        }
        GameEngine::ins().requestScene(SceneID::EXPLORE);
        return true;
    }
    case AppSceneFlow::Scene::SHOP:
        GameEngine::ins().requestScene(SceneID::SHOP);
        return true;
    case AppSceneFlow::Scene::COMPUTER:
        pushView(ViewMode::COMPUTER);
        computerUi.reset();
        return true;
    case AppSceneFlow::Scene::SETTINGS:
        GameEngine::ins().requestScene(SceneID::SETTINGS);
        return true;
#if STICKMON_ENABLE_DEBUG_FEATURES
    case AppSceneFlow::Scene::DEBUG:
        pushView(ViewMode::DEBUG);
        debugUi.reset();
        return true;
#endif
    case AppSceneFlow::Scene::HOME:
        GameEngine::ins().requestScene(GameEngine::ins().homeScene());
        return true;
    default:
        return false;
    }
}

void MenuScene::render() {
    overlayUi.renderBackground();
#if STICKMON_UI_ROUND_TOUCH
    if (itemProgression != ItemProgression::NONE) {
        renderItemProgression();
        return;
    }
#endif
    if (viewMode == ViewMode::STATUS) {
        statusUi.render();
        renderTutorial();
        return;
    }
    if (viewMode == ViewMode::MOVES) {
        movesUi.render();
        if (viewMode == ViewMode::MOVES) renderToast();
        renderTutorial();
        return;
    }
    if (viewMode == ViewMode::TEAM) {
        teamUi.renderWithToast(toast);
        renderTutorial();
        return;
    }
    if (viewMode == ViewMode::ROOM) {
        roomUi.render();
        renderToast();
        renderTutorial();
        return;
    }
    if (viewMode == ViewMode::FOOD) {
        foodUi.render();
        renderToast();
        renderTutorial();
        return;
    }
    if (viewMode == ViewMode::BAG) {
        renderBagPage();
        renderTutorial();
        return;
    }
    if (viewMode == ViewMode::COMPUTER) {
        computerUi.render();
        if (computerUi.showsNotices()) renderToast();
        renderTutorial();
        return;
    }
    if (viewMode == ViewMode::STORAGE) {
        contactsUi.render();
        renderToast();
        renderTutorial();
        return;
    }
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (viewMode == ViewMode::DEBUG) {
        debugUi.render();
        renderToast();
        renderTutorial();
        return;
    }
#endif

    mainMenuUi.render();
    renderToast();
    renderTutorial();
}

Ui::MenuOverlayFrame MenuScene::menuOverlayFrame() const {
    const auto& engine = GameEngine::ins();
    return {toast,
        viewMode == ViewMode::TEAM || viewMode == ViewMode::STATUS || viewMode == ViewMode::MOVES,
        directTouchPage(), Platform::input().touchAvailable(), exploreContextMode,
        engine.gameState().oobeDone, viewMode == ViewMode::MENU,
        engine.tutorialComplete(Game::TutorialStep::MENU_NAV),
        engine.tutorialComplete(Game::TutorialStep::MENU_BACK)};
}

void MenuScene::renderTutorial() { overlayUi.renderTutorial(menuOverlayFrame()); }

void MenuScene::renderToast() { overlayUi.renderToast(menuOverlayFrame()); }

Features::Bag::Context MenuScene::bagContext() const {
    return {battleBagMode, battleBagMode ? battleTargetTeamSlot : uint8_t(0), battleTargetName};
}

bool MenuScene::handleBagAction(const BagUi::Action& action) {
    auto& engine = GameEngine::ins();
    if (action.navigationTutorial) engine.completeTutorial(Game::TutorialStep::MENU_NAV);
    if (action.backTutorial && !exploreContextMode) engine.completeTutorial(Game::TutorialStep::MENU_BACK);
    switch (action.kind) {
#if STICKMON_UI_ROUND_TOUCH
    case BagUi::Action::Kind::SELECT_TARGET:
        pendingTeamItem = action.item;
        toast = nullptr;
        teamUi.reset();
        pushView(ViewMode::TEAM);
        break;
    case BagUi::Action::Kind::USE_DIRECT: useDirectBagItem(action.item); break;
    case BagUi::Action::Kind::UNUSABLE: pageNotice(Ui::Amoled::CANNOT_USE); break;
#endif
    case BagUi::Action::Kind::BACK: popView(); break;
    case BagUi::Action::Kind::HOME:
        if (exploreContextMode) resetNavigation();
        else engine.requestScene(engine.homeScene());
        break;
    case BagUi::Action::Kind::USE: {
        const auto outcome = Features::Bag::use(engine.gameState(), action.source, bagContext(), applyBagItem);
        toast = BagUi::notice(outcome, toastBuffer, sizeof(toastBuffer));
        toastUntil = Hal::ins().millis() + 1100;
        if (outcome.used && battleBagMode) {
            battleBagResult = outcome.battleResult;
            if (outcome.battleResult == BattleBagResult::FOOD_THROWN) battleBagFoodIndex = outcome.foodIndex;
            resetNavigation();
        } else bagUi.afterUse(engine.gameState(), bagContext());
        break;
    }
    case BagUi::Action::Kind::RECALL:
        if (movesUi.openRecall(0)) {
            foodUi.resetDescription();
            pushView(ViewMode::MOVES);
        } else {
            pageNotice(Ui::Team::MOVE_RECALL_EMPTY, 1100);
        }
        break;
    default: break;
    }
    return action.changed;
}

void MenuScene::renderBagPage() {
    bagUi.render(GameEngine::ins().gameState(), bagContext());
    bagUi.renderToast(toast);
}


const Game::GameState& MenuScene::pageState() const { return GameEngine::ins().gameState(); }
bool MenuScene::pageExploreContext() const { return exploreContextMode; }
void MenuScene::pageBack() { popView(); }
void MenuScene::pageHome() {
    if (exploreContextMode) resetNavigation();
    else GameEngine::ins().requestScene(GameEngine::ins().homeScene());
}
void MenuScene::pageNotice(const char* text, uint32_t durationMs) {
    text = overlayUi.noticeText(text);
    if (text) {
        if (text != toastBuffer) snprintf(toastBuffer, sizeof(toastBuffer), "%s", text);
        toast = toastBuffer;
    } else toast = nullptr;
    toastUntil = Hal::ins().millis() + durationMs;
}
void MenuScene::pageBackTutorial() {
    if (!exploreContextMode) GameEngine::ins().completeTutorial(Game::TutorialStep::MENU_BACK);
}
void MenuScene::pageNavigationTutorial() { GameEngine::ins().completeTutorial(Game::TutorialStep::MENU_NAV); }
void MenuScene::openTeamStatus(uint8_t slot) {
    statusUi.statusMonsterIndex = slot;
    statusUi.statusFromStorage = false;
    statusUi.statusPage = 0;
    statusUi.statusScrollKey = -1;
    statusUi.statusScroll = 0;
    pushView(ViewMode::STATUS);
}
void MenuScene::openTeamMoves(uint8_t slot) {
    movesUi.openManage(slot);
    foodUi.resetDescription();
    pushView(ViewMode::MOVES);
}
bool MenuScene::makeTeamFirst(uint8_t slot) {
    if (!GameEngine::ins().moveTeamMemberToFront(slot)) return false;
    statusUi.statusMonsterIndex = 0;
    return true;
}
bool MenuScene::leaveTeam(uint8_t slot) {
    toastUntil = Hal::ins().millis() + 1100;
    if (!GameEngine::ins().moveTeamMemberToContacts(slot)) return false;
    statusUi.statusMonsterIndex = 0;
    return true;
}
bool MenuScene::forgetPageMove(uint8_t slot, uint8_t moveSlot) {
    return GameEngine::ins().forgetTeamMemberMove(slot, moveSlot);
}
bool MenuScene::recallPageMove(uint8_t slot, Game::MoveId moveId, uint8_t replacementSlot) {
    return GameEngine::ins().recallMove(slot, moveId, replacementSlot);
}
uint8_t MenuScene::collectPageRecallable(uint8_t slot, Game::MoveId* moves, uint8_t capacity) {
    return GameEngine::ins().collectRecallableMoves(slot, moves, capacity);
}

Scene* MenuScene::delegatedPage() {
    switch (viewMode) {
    case ViewMode::MENU: return &mainMenuUi;
    case ViewMode::TEAM: return &teamUi;
    case ViewMode::STATUS: return &statusUi;
    case ViewMode::MOVES: return &movesUi;
    case ViewMode::ROOM: return &roomUi;
    case ViewMode::FOOD: return &foodUi;
    case ViewMode::COMPUTER: return &computerUi;
    case ViewMode::STORAGE: return &contactsUi;
#if STICKMON_ENABLE_DEBUG_FEATURES
    case ViewMode::DEBUG: return &debugUi;
#endif
    default: return nullptr;
    }
}

void MenuScene::openRoomFood() {
    pushView(ViewMode::FOOD);
    foodUi.enter();
}
void MenuScene::openMainMenuFood() { openRoomFood(); }
void MenuScene::openRoomShower() { GameEngine::ins().requestScene(SceneID::SHOWER); }
bool MenuScene::selectPageFood(uint8_t index) { return GameEngine::ins().selectFood(index); }

void MenuScene::openComputerSocial() { GameEngine::ins().requestScene(SceneID::SOCIAL); }
void MenuScene::openComputerContacts() {
    pushView(ViewMode::STORAGE);
    contactsUi.enter();
}
bool MenuScene::contactVisiting(uint8_t slot) const {
    return GameEngine::ins().contactIsVisiting(slot);
}
bool MenuScene::contactInviteLocked(uint8_t slot) const {
    return GameEngine::ins().contactInviteLocked(slot);
}
void MenuScene::openContactStatus(uint8_t slot) {
    const int8_t teamSlot = GameEngine::ins().teamSlotForContact(slot);
    statusUi.statusMonsterIndex = teamSlot >= 0 ? static_cast<uint8_t>(teamSlot) : slot;
    statusUi.statusFromStorage = teamSlot < 0;
    statusUi.statusPage = 0;
    statusUi.statusScrollKey = -1;
    statusUi.statusScroll = 0;
    pushView(ViewMode::STATUS);
}
Features::Contacts::InviteResult MenuScene::invitePageContact(uint8_t slot) {
    using Result = Features::Contacts::InviteResult;
    const auto result = GameEngine::ins().inviteContactToTeam(slot);
    Result semantic = Result::INVALID;
    switch (result) {
    case ContactInviteResult::JOINED: return Result::JOINED;
    case ContactInviteResult::REFUSED:
        toast = Ui::Storage::INVITE_REFUSED;
        semantic = Result::REFUSED;
        break;
    case ContactInviteResult::LOCKED:
        toast = Ui::Storage::INVITE_LOCKED;
        semantic = Result::LOCKED;
        break;
    case ContactInviteResult::ALREADY_IN_TEAM:
        toast = Ui::Storage::IN_TEAM;
        semantic = Result::ALREADY_IN_TEAM;
        break;
    case ContactInviteResult::TEAM_FULL:
        toast = Ui::Storage::TEAM_FULL_TOAST;
        semantic = Result::TEAM_FULL;
        break;
    case ContactInviteResult::INVALID: break;
    }
    toastUntil = Hal::ins().millis() + 1400;
    return semantic;
}
void MenuScene::contactJoined() {
    auto& engine = GameEngine::ins();
    engine.requestTeamMemberArrival();
    engine.requestScene(SceneID::MAIN);
}
bool MenuScene::deletePageContact(uint8_t slot) {
    if (!GameEngine::ins().deleteContact(slot)) return false;
    toast = Ui::Storage::DELETE_TOAST;
    toastUntil = Hal::ins().millis() + 1100;
    return true;
}

void MenuScene::leaveMainMenu() {
    GameEngine::ins().requestScene(GameEngine::ins().homeScene());
}
