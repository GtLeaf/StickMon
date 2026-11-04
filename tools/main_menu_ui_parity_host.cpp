#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <string>
#include <vector>
#include "platform/desktop/DesktopPlatform.h"
#include "game/GameRandom.h"
#include "game/MonsterFactory.h"
#include "presentation/PixelRenderer.h"
#include "presentation/MenuGrid.h"
#define private public
#include "hardware/Hal.h"
#include "core/GameEngine.h"
#include "scenes/MenuScene.h"
#undef private
#include "presentation/TouchUi.h"

#if STICKMON_MAIN_MENU_UI_BASELINE
#define MAIN_MENU(scene) (scene)
using MainMenuPage = MenuScene;
#else
#define MAIN_MENU(scene) ((scene).mainMenuUi)
using MainMenuPage = Ui::MainMenuUiBase;
#endif
#if STICKMON_ENABLE_DEBUG_FEATURES
#if __has_include("ui/DebugUi.h")
#define DEBUG_MENU(scene) ((scene).debugUi)
using DebugPage = Ui::DebugUiBase;
#else
#define DEBUG_MENU(scene) (scene)
using DebugPage = MenuScene;
#endif
#endif
using Mode = MenuScene::ViewMode;
using Type = Platform::IInputDevice::TouchType;

class TouchInput final : public Platform::IInputDevice {
public:
    void update() override {}
    bool pressed(Platform::InputButton) const override { return false; }
    bool touchAvailable() const override { return STICKMON_UI_COMPACT_TOUCH; }
};

class PowerProbe final : public Platform::IPowerDevice {
public:
    explicit PowerProbe(Platform::IPowerDevice& device) : device(device) {}
    int level = 75;
    int batteryLevel() override { return level; }
    Platform::PowerCapabilities capabilities() const override { return device.capabilities(); }
    Platform::WakeReason wakeReason() const override { return device.wakeReason(); }
    uint32_t hardwareRandom() override { return device.hardwareRandom(); }
    size_t externalMemorySize() const override { return device.externalMemorySize(); }
    size_t externalMemoryFree() const override { return device.externalMemoryFree(); }
    void restart() override { device.restart(); }
    void enterDeepSleep(uint64_t duration, bool wake) override { device.enterDeepSleep(duration,wake); }
private:
    Platform::IPowerDevice& device;
};

int main(int argc, char** argv) {
    assert(argc == 3);
    DesktopPlatform desktop(argv[1]);
    const auto bundle = desktop.serviceBundle();
    TouchInput input;
    PowerProbe power(bundle.power);
    Platform::Services services{bundle.lifecycle, bundle.clock, bundle.logger,
        input, bundle.display, bundle.audio, bundle.imu, power, bundle.blobs,
        bundle.resources, bundle.memory, bundle.peers};
    Platform::bind(services);
    assert(desktop.begin());
    auto& engine = GameEngine::ins();
    assert(engine.begin());
    auto& state = engine.gameState();
    MenuScene menu;
    unsigned checkpoints = 0, switches = 0;
    constexpr int itemCount = MainMenuPage::ITEM_COUNT;
    static_assert(itemCount == (STICKMON_ENABLE_DEBUG_FEATURES ? 9 : 8), "Menu configuration mismatch");
    const ButtonEvent confirm{0,BtnAction::PRESSED};
    const ButtonEvent next{1,BtnAction::PRESSED};
    const ButtonEvent back{1,BtnAction::LONG_PRESS};
    auto setup = [&]() {
        engine.visitSession.stop();
        engine.contactVisit = {};
        GameRandom::seed(1234);
        state = Game::GameState{};
        state.oobeDone = true;
        state.tutorialFlags = 0xff;
        state.teamCount = 1;
        state.team[0] = Game::MonsterFactory::create(1,25);
        state.team[0].origin = Game::Origin::STARTER;
        state.coins = 1000;
        engine.prevId = SceneID::MAIN;
        engine.debugMenuReturnRequested = false;
        MainMenuPage::lastCursor = 0;
        power.level = 75;
        Hal::ins().batteryFiltered = -1;
        menu.toast = nullptr; menu.toastUntil = 0;
        menu.onEnter();
        engine.saveCoordinator.reset(desktop.millis());
    };
    auto press = [&](ButtonEvent event) { assert(menu.onButton(event)); };
    auto select = [&](int index) {
        assert(menu.viewMode == Mode::MENU);
        for (int i = 0; MAIN_MENU(menu).cursor != index; ++i) {
            assert(i < itemCount); press(next);
        }
    };
    auto open = [&](int index) { select(index); press(confirm); };
    auto settle = [&]() {
        for (int i = 0; i < 100; ++i) {
            desktop.advanceMs(20); menu.update(desktop.millis(),.02f);
        }
    };
    auto capture = [&](const std::string& name) {
        PixelRenderer::clear(0); menu.render();
        const auto fb = desktop.frameBuffer();
        FILE* file = std::fopen((std::string(argv[2]) + "/" + name + ".rgb565").c_str(),"wb");
        assert(file);
        assert(std::fwrite(fb.pixels,sizeof(uint16_t),fb.width*fb.height,file) == size_t(fb.width)*fb.height);
        std::fclose(file);
    };
    auto touch = [&](Scene& scene,Type type,int x,int y) {
        desktop.advanceMs(10);
        return scene.onTouch({type,int16_t(x),int16_t(y),desktop.millis()});
    };
    auto tap = [&](Scene& scene,int x,int y) {
        touch(scene,Type::DOWN,x,y); return touch(scene,Type::UP,x,y);
    };
    auto center = [&](MenuScene& scene,int index) {
#if STICKMON_UI_COMPACT_TOUCH
        const auto rect = MenuGrid::mainLayout(Hal::DISPLAY_W,Hal::DISPLAY_H)
            .cell(index,MAIN_MENU(scene).menuGridScroll);
        return std::array<int,2>{{rect.x+rect.width/2,rect.y+rect.height/2}};
#else
        assert(MAIN_MENU(scene).animCursor == index);
        return std::array<int,2>{{120,TouchUi::CONTENT_H/2}};
#endif
    };
    auto tapSelected = [&](MenuScene& scene,int index) {
        const auto point = center(scene,index); return tap(scene,point[0],point[1]);
    };
    auto sameState = [&](const Game::GameState& before) {
        assert(std::memcmp(&state,&before,sizeof(state)) == 0);
    };
    auto isNotice = [&](const char* text) {
        assert(menu.toast && std::strcmp(menu.toast,text) == 0);
        assert(menu.toastUntil == desktop.millis()+1100);
        assert(menu.viewMode == Mode::MENU);
    };

    // Visual state injection is limited to fixtures generated from frozen code.
    setup();
    for (int index = 0; index < itemCount; ++index) {
        select(index); settle(); capture("selected-"+std::to_string(index));
    }
    setup();
    for (int value : {-1,0,20,21,50,100}) {
        power.level = value; Hal::ins().batteryFiltered = -1;
        capture("battery-"+std::to_string(value));
    }
    setup();
#if STICKMON_UI_COMPACT_TOUCH
    for (int index = 0; index < itemCount; ++index) {
        const auto point = center(menu,index);
        touch(menu,Type::DOWN,point[0],point[1]);
        capture("pressed-"+std::to_string(index)); menu.cancelTouch();
    }
    for (int offset : {-25,20,100}) {
        MAIN_MENU(menu).menuGridScroll = offset;
        capture("scroll-"+std::to_string(offset));
    }
#else
    for (float position : {.25f,.49f,.5f,.75f,3.25f,6.5f,7.75f}) {
        MAIN_MENU(menu).animCursor = position;
        capture("animation-"+std::to_string(int(position*100)));
    }
#endif
    setup(); state.tutorialFlags = 0;
    capture("tutorial-menu");
    menu.exploreContextMode = true; capture("tutorial-explore-suppressed");
    menu.exploreContextMode = false; state.oobeDone = false; capture("tutorial-hatch-suppressed");
    setup(); open(1); state.tutorialFlags = Game::tutorialMask(Game::TutorialStep::MENU_NAV);
    capture("tutorial-back");
    setup();
    for (const char* notice : {Ui::Menu::HATCH_FIRST,Ui::Menu::GUEST_AT_HOME,Ui::Menu::FAINTED_TOAST}) {
        menu.toast = notice;
        capture("notice-"+std::to_string(checkpoints++));
    }
    checkpoints = 0;

    setup(); const auto beforeWrap = state;
    for (int i = 1; i <= itemCount; ++i) {
        press(next); assert(MAIN_MENU(menu).cursor == i%itemCount);
    }
#if !STICKMON_UI_COMPACT_TOUCH
    assert(MAIN_MENU(menu).animCursor == 0);
#endif
    sameState(beforeWrap); assert(!engine.saveCoordinator.dirty());
    ++checkpoints;

    setup(); const auto beforeInvalid = state;
    assert(!menu.onButton({2,BtnAction::PRESSED}));
    assert(!menu.onButton({0,BtnAction::DOWN}));
    assert(!menu.onButton({1,BtnAction::UP}));
    assert(!menu.onTap(128) && !menu.onTap(256+1));
    assert(menu.touchTarget(-1,-1) == -1 && menu.touchTarget(Hal::DISPLAY_W,10) == -1);
    sameState(beforeInvalid);
    ++checkpoints;

    setup(); select(5); menu.onExit();
    assert(MainMenuPage::lastCursor == 5);
    engine.prevId = SceneID::SETTINGS; menu.onEnter();
    assert(MAIN_MENU(menu).cursor == 5);
#if !STICKMON_UI_COMPACT_TOUCH
    assert(MAIN_MENU(menu).animCursor == 5);
#endif
    menu.onExit(); engine.prevId = SceneID::MAIN; menu.onEnter();
    assert(MAIN_MENU(menu).cursor == 0);
    state.oobeDone = false; engine.prevId = SceneID::HATCH; menu.onEnter();
    assert(MAIN_MENU(menu).cursor == 0);
    ++checkpoints;

    setup(); const auto beforeChildren = state;
    for (int index : {1,2,3,5}) {
        open(index);
        const Mode expected = index==1 ? Mode::TEAM : index==2 ? Mode::ROOM : index==3 ? Mode::BAG : Mode::COMPUTER;
        assert(menu.viewMode == expected && menu.navDepth == 1);
        assert(!menu.onTap(index));
        press(back); assert(menu.viewMode == Mode::MENU && menu.navDepth == 0);
        assert(MAIN_MENU(menu).cursor == index); sameState(beforeChildren);
    }
    ++checkpoints;

    setup(); state.tutorialFlags = 0;
    press(next); assert(engine.tutorialComplete(Game::TutorialStep::MENU_NAV));
    assert(state.tutorialFlags == Game::tutorialMask(Game::TutorialStep::MENU_NAV));
    assert(engine.saveCoordinator.dirty());
    ++checkpoints;

    setup(); state.tutorialFlags = 0; select(1);
    state.tutorialFlags = 0; engine.saveCoordinator.reset(desktop.millis());
    press(confirm); assert(menu.viewMode == Mode::TEAM);
    assert(!engine.tutorialComplete(Game::TutorialStep::MENU_NAV));
    press(back); assert(!engine.tutorialComplete(Game::TutorialStep::MENU_NAV));
    ++checkpoints;

    setup(); select(1); settle(); state.tutorialFlags = 0;
    assert(tapSelected(menu,1) && menu.viewMode == Mode::TEAM);
    assert(engine.tutorialComplete(Game::TutorialStep::MENU_NAV));
    assert(!touch(menu,Type::UP,120,TouchUi::CONTENT_H/2));
    press(back); assert(menu.viewMode == Mode::MENU);
    ++checkpoints;

    setup(); state.tutorialFlags = 0; menu.openExploreTeamView();
    assert(menu.exploreContextMode && menu.viewMode == Mode::TEAM);
    press(back); assert(menu.exploreViewClosed());
    press(next); assert(!engine.tutorialComplete(Game::TutorialStep::MENU_NAV));
    settle(); assert(tapSelected(menu,1));
    assert(menu.viewMode == Mode::TEAM && engine.tutorialComplete(Game::TutorialStep::MENU_NAV));
    press({0,BtnAction::LONG_PRESS}); assert(menu.exploreViewClosed() && menu.navDepth == 0);
    menu.openExploreBagView(); assert(menu.viewMode == Mode::BAG && !menu.exploreViewClosed());
    press(back); assert(menu.exploreViewClosed());
    ++checkpoints;

    setup(); state.oobeDone = false; const auto beforeHatch = state;
    press(confirm); isNotice(Ui::Menu::HATCH_FIRST); sameState(beforeHatch);
    ++checkpoints;
    setup(); state.teamCount = 0; const auto beforeEmpty = state;
    press(confirm); isNotice(Ui::Menu::HATCH_FIRST); sameState(beforeEmpty);
    ++checkpoints;
    setup(); engine.contactVisit.active = true; const auto beforeGuest = state;
    press(confirm); isNotice(Ui::Menu::GUEST_AT_HOME); sameState(beforeGuest);
    ++checkpoints;
    setup();
    engine.visitSession.state_ = Communication::VisitSessionService::State::ACTIVE;
    engine.visitSession.localIsHost_ = true;
    const auto beforeLinkedGuest = state;
    press(confirm); isNotice(Ui::Menu::GUEST_AT_HOME); sameState(beforeLinkedGuest);
    engine.visitSession.state_ = Communication::VisitSessionService::State::IDLE;
    ++checkpoints;
    setup(); state.team[0].fainted = true; const auto beforeFainted = state;
    press(confirm); isNotice(Ui::Menu::FAINTED_TOAST); sameState(beforeFainted);
    ++checkpoints;
    setup(); state.team[0].hpCur = 0; const auto beforeZeroHp = state;
    press(confirm); isNotice(Ui::Menu::FAINTED_TOAST); sameState(beforeZeroHp);
    ++checkpoints;

    setup(); state.oobeDone = false;
    press(confirm); isNotice(Ui::Menu::HATCH_FIRST);
    auto demand = menu.update(desktop.millis(),.02f);
    assert(!demand.redraw && demand.nextUpdateDelayMs == 1100);
    desktop.advanceMs(1099); demand = menu.update(desktop.millis(),.02f);
    assert(!demand.redraw && demand.nextUpdateDelayMs == 1 && menu.toast);
    desktop.advanceMs(1); demand = menu.update(desktop.millis(),.02f);
    assert(demand.redraw && !menu.toast);
    ++checkpoints;

    setup(); settle();
    demand = menu.update(desktop.millis(),.02f);
    assert(!demand.redraw);
#if STICKMON_UI_COMPACT_TOUCH
    assert(demand.nextUpdateDelayMs == SceneUpdateResult::NO_UPDATE);
    const auto grid = MenuGrid::mainLayout(Hal::DISPLAY_W,Hal::DISPLAY_H);
    assert(grid.columns == 3 && grid.cell(0).width == 88 && grid.cell(0).height == 72);
    assert(grid.maxScroll(itemCount) == 0);
    assert(menu.touchTarget(100,42) == -1);
    const auto empty = grid.cell(8);
#if !STICKMON_ENABLE_DEBUG_FEATURES
    assert(menu.touchTarget(empty.x+20,empty.y+20) == -1);
#else
    assert(menu.touchTarget(empty.x+20,empty.y+20) == 8);
#endif
    MAIN_MENU(menu).menuGridScroll = -25;
    demand = menu.update(desktop.millis(),.02f);
    assert(MAIN_MENU(menu).menuGridScroll == 0 && !demand.redraw);
    MAIN_MENU(menu).menuGridScroll = 100;
    demand = menu.update(desktop.millis(),.02f);
    assert(MAIN_MENU(menu).menuGridScroll == 0 && !demand.redraw);
#else
    assert(demand.nextUpdateDelayMs == 30000);
    press(next); demand = menu.update(desktop.millis(),.02f);
    assert(demand.redraw && demand.nextUpdateDelayMs == 66 && MAIN_MENU(menu).animCursor == .25f);
    settle(); assert(MAIN_MENU(menu).animCursor == 1);
    power.level = 0; desktop.advanceMs(30000);
    demand = menu.update(desktop.millis(),.02f);
    assert(demand.redraw && demand.nextUpdateDelayMs == 30000 && MAIN_MENU(menu).lastBatteryLevel < 75);
    demand = menu.update(desktop.millis(),.02f);
    assert(!demand.redraw && demand.nextUpdateDelayMs == 30000);
#endif
    ++checkpoints;

    setup(); select(1); settle(); state.tutorialFlags = 0;
    const auto point = center(menu,1); const auto beforeCanceledTouch = state;
    touch(menu,Type::DOWN,point[0],point[1]);
#if STICKMON_UI_COMPACT_TOUCH
    assert(MAIN_MENU(menu).gridTouch.pressed() == 1);
#endif
    menu.cancelTouch(); assert(!touch(menu,Type::UP,point[0],point[1]));
    touch(menu,Type::DOWN,point[0],point[1]); touch(menu,Type::CANCEL,point[0],point[1]);
    assert(!touch(menu,Type::UP,point[0],point[1]));
    sameState(beforeCanceledTouch); assert(menu.viewMode == Mode::MENU);
    ++checkpoints;

    setup(); select(1); settle(); const auto dragPoint = center(menu,1);
    touch(menu,Type::DOWN,dragPoint[0],dragPoint[1]);
    touch(menu,Type::MOVE,dragPoint[0]+24,dragPoint[1]);
    touch(menu,Type::UP,dragPoint[0],dragPoint[1]);
    assert(menu.viewMode == Mode::MENU);
    touch(menu,Type::DOWN,dragPoint[0],dragPoint[1]);
    touch(menu,Type::MOVE,dragPoint[0],dragPoint[1]+32);
    touch(menu,Type::UP,dragPoint[0],dragPoint[1]);
    assert(menu.viewMode == Mode::MENU);
    assert(tapSelected(menu,1)); assert(menu.viewMode == Mode::TEAM);
    assert(!menu.onTap(2));
    assert(!touch(menu,Type::UP,dragPoint[0],dragPoint[1]));
    assert(!menu.teamUi.teamActionOpen);
    press(back); assert(menu.viewMode == Mode::MENU);
    ++checkpoints;

    setup(); select(1); settle(); const auto stalePoint = center(menu,1);
    touch(menu,Type::DOWN,stalePoint[0],stalePoint[1]);
    menu.openExploreBagView();
    assert(!touch(menu,Type::UP,stalePoint[0],stalePoint[1]));
    assert(menu.viewMode == Mode::BAG && menu.bagUi.cursor == 0);
#if STICKMON_UI_COMPACT_TOUCH
    assert(!menu.bagUi.detailOpen);
#endif
    press(back); assert(menu.exploreViewClosed());
    assert(!touch(menu,Type::UP,stalePoint[0],stalePoint[1]));
    assert(menu.viewMode == Mode::MENU);
    ++checkpoints;

    auto ownedMenu = [&]() {
        setup();
        engine.requestScene(SceneID::MAIN,false); engine.requestScene(SceneID::MENU,false);
        assert(engine.currentId == SceneID::MENU);
        return static_cast<MenuScene*>(engine.currentScene.get());
    };
    auto ownedSelect = [&](MenuScene* owned,int index) {
        for (int i = 0; MAIN_MENU(*owned).cursor != index; ++i) {
            assert(i < itemCount); assert(owned->onButton(next));
        }
        for (int i = 0; i < 100; ++i) {
            desktop.advanceMs(20); owned->update(desktop.millis(),.02f);
        }
    };
    // Each final activation below synchronously destroys the owned MenuScene.
    // No reads from the old scene are allowed after that public entry returns.
    for (int index : {0,4,6,itemCount-1}) {
        for (bool useTouch : {false,true}) {
            MenuScene* owned = ownedMenu(); ownedSelect(owned,index);
            const auto expected = index==0 ? SceneID::EXPLORE : index==4 ? SceneID::SHOP : index==6 ? SceneID::SETTINGS : SceneID::MAIN;
            if (useTouch) assert(tapSelected(*owned,index));
            else assert(owned->onButton(confirm));
            assert(engine.currentId == expected);
            assert(MainMenuPage::lastCursor == index);
            ++switches;
        }
    }
    ++checkpoints;

    for (unsigned button : {0,1}) {
        MenuScene* owned = ownedMenu();
        assert(owned->onButton({uint8_t(button),BtnAction::LONG_PRESS}));
        assert(engine.currentId == SceneID::MAIN); ++switches;
    }
#if STICKMON_UI_COMPACT_TOUCH
    {
        MenuScene* owned = ownedMenu();
        assert(owned->onTap(230));
        assert(engine.currentId == SceneID::MAIN);
    }
#else
    setup(); assert(!menu.onTap(230));
#endif
    ++checkpoints;

    for (int index : {4,6}) {
        MenuScene* owned = ownedMenu(); ownedSelect(owned,index);
        assert(owned->onButton(confirm));
        assert(engine.currentId == (index==4 ? SceneID::SHOP : SceneID::SETTINGS));
        assert(engine.currentScene->onButton(back));
        assert(engine.currentId == SceneID::MENU);
        assert(MAIN_MENU(*static_cast<MenuScene*>(engine.currentScene.get())).cursor == index);
        ++switches;
    }
    engine.requestScene(SceneID::MAIN,false); engine.requestScene(SceneID::MENU,false);
    assert(MAIN_MENU(*static_cast<MenuScene*>(engine.currentScene.get())).cursor == 0);
    ++checkpoints;

    setup();
#if STICKMON_ENABLE_DEBUG_FEATURES
    open(7); assert(menu.viewMode == Mode::DEBUG && menu.navDepth == 1);
    assert(DEBUG_MENU(menu).debugCategory == DebugPage::DebugCategory::ROOT && DEBUG_MENU(menu).debugCursor == 0);
    press(back); assert(menu.viewMode == Mode::MENU && MAIN_MENU(menu).cursor == 7);
#else
    assert(AppSceneFlow::mainMenuEntry(7,false).target == AppSceneFlow::Scene::HOME);
#endif
    ++checkpoints;

    setup();
#if STICKMON_ENABLE_DEBUG_FEATURES
    engine.requestScene(SceneID::MAIN,false); engine.endDebugBattle();
    assert(engine.currentId == SceneID::MENU);
    const auto* returned = static_cast<MenuScene*>(engine.currentScene.get());
    assert(returned->viewMode == Mode::DEBUG && returned->navDepth == 1);
    assert(DEBUG_MENU(*returned).debugCategory == DebugPage::DebugCategory::BATTLE);
    assert(DEBUG_MENU(*returned).debugCursor == DebugPage::DEBUG_BATTLE_RANDOM_INDEX);
    assert(!engine.consumeDebugMenuReturnRequest());
    assert(engine.currentScene->onButton(back));
    assert(DEBUG_MENU(*static_cast<MenuScene*>(engine.currentScene.get())).debugCategory == DebugPage::DebugCategory::ROOT);
    assert(engine.currentScene->onButton(back));
    assert(static_cast<MenuScene*>(engine.currentScene.get())->viewMode == Mode::MENU);
#else
    assert(!engine.consumeDebugMenuReturnRequest());
#endif
    ++checkpoints;
    std::printf("main-menu-checkpoints=%u items=%d switches=%u\n",checkpoints,itemCount,switches);
    engine.currentScene.reset();
}
