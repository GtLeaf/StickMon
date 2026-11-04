#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include "platform/desktop/DesktopPlatform.h"
#include "core/GameEngine.h"
#include "game/GameRandom.h"
#include "game/MonsterFactory.h"
#include "game/ItemInventory.h"
#include "hardware/Hal.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TouchUi.h"
#include "ui/common/StatusUiContent.h"
#define private public
#include "scenes/MenuScene.h"
#undef private

#if STICKMON_MAIN_MENU_UI_BASELINE
#define MAIN_MENU(scene) (scene)
#else
#define MAIN_MENU(scene) ((scene).mainMenuUi)
#endif

// Exercise the same semantic journey through each production UI using only
// scene entry points. Captures also allow comparison to the pre-refactor build.
class TouchInput final : public Platform::IInputDevice {
public:
    void update() override {}
    bool pressed(Platform::InputButton) const override { return false; }
    bool touchAvailable() const override { return STICKMON_UI_COMPACT_TOUCH; }
};

int main(int argc, char** argv) {
    assert(argc == 3);
    DesktopPlatform desktop(argv[1]);
    auto bundle = desktop.serviceBundle();
    TouchInput input;
    Platform::Services services{bundle.lifecycle, bundle.clock, bundle.logger,
        input, bundle.display, bundle.audio, bundle.imu, bundle.power,
        bundle.blobs, bundle.resources, bundle.memory, bundle.peers};
    Platform::bind(services);
    assert(desktop.begin());
    GameRandom::seed(1234);
    auto& engine = GameEngine::ins();
    assert(engine.begin());
    auto& state = engine.gameState();
    state = Game::GameState{};
    state.oobeDone = true;
    state.teamCount = 1;
    state.team[0] = Game::MonsterFactory::create(1, 5);
    state.team[0].hpCur = 1;
    state.coins = 10000;
    state.tutorialFlags = 0xff;
    for (auto& stock : state.room.food) stock = 0;
    state.bag.potion = 0;
    Game::ItemInventory::add(state, Game::ItemId::NORMAL_FOOD, 2);
    Game::ItemInventory::add(state, Game::ItemId::POTION, 2);

    auto capture = [&](MenuScene& scene, const std::string& name) {
        PixelRenderer::clear(0);
        scene.render();
        const auto fb = desktop.frameBuffer();
        FILE* file = std::fopen((std::string(argv[2]) + "/" + name + ".rgb565").c_str(), "wb");
        assert(file);
        std::fwrite(fb.pixels, sizeof(uint16_t), fb.width * fb.height, file);
        std::fclose(file);
    };
    MenuScene menu; menu.onEnter();
    menu.viewMode = MenuScene::ViewMode::TEAM;
    menu.teamUi.teamCursor = 0;
    capture(menu, "team-single");
    menu.onButton({0,BtnAction::PRESSED});
    capture(menu, "team-single-actions");
    menu.cancelTouch();
    menu.teamUi.teamActionOpen = false;
    state.teamCount = 2;
    state.team[0] = Game::MonsterFactory::create(1, 50);
    state.team[1] = Game::MonsterFactory::create(4, 50);
    state.team[1].origin = Game::Origin::BEFRIENDED;
    state.team[1].hpCur = 3;
    menu.teamUi.teamCursor = 1;
    capture(menu, "team-pair");
    menu.onButton({0,BtnAction::PRESSED});
    capture(menu, "team-partner-actions");
    state.team[1].origin = Game::Origin::VISITOR;
    capture(menu, "team-visitor-actions");
    state.team[1].origin = Game::Origin::BEFRIENDED;
    menu.exploreContextMode = true;
    capture(menu, "team-explore-actions");
    menu.teamUi.teamActionOpen = false;
    state.storageCount = 1;
    state.storage[0] = Game::MonsterFactory::create(25, 50);
    menu.viewMode = MenuScene::ViewMode::STATUS;
    for (bool storage : {false, true}) {
        menu.statusUi.statusFromStorage = storage;
        menu.statusUi.statusMonsterIndex = 0;
        for (int page = 0; page < 5; ++page) {
            menu.statusUi.statusPage = page;
            menu.statusUi.statusScroll = 0;
            const std::string name = (storage ? "storage-status-" : "team-status-") + std::to_string(page);
            capture(menu, name);
            menu.statusUi.statusScroll = 48;
            capture(menu, name + "-scrolled");
        }
    }
    menu.statusUi.statusFromStorage = false;
    menu.statusUi.statusScroll = 0;
    state.oobeDone = false;
    menu.viewMode = MenuScene::ViewMode::TEAM;
    menu.teamUi.teamCursor = 0;
    capture(menu, "team-egg");
    menu.viewMode = MenuScene::ViewMode::STATUS;
    capture(menu, "status-egg");
    state.oobeDone = true;
    menu.viewMode = MenuScene::ViewMode::MOVES;
    menu.movesUi.moveMonsterIndex = 0;
    menu.movesUi.movePageMode = MenuScene::MovePageMode::MANAGE;
    for (int row = 0; row < 4; ++row) {
        menu.movesUi.moveCursor = row;
        menu.movesUi.moveListScroll = menu.movesUi.descScroll = 0;
        capture(menu, "moves-row-" + std::to_string(row));
    }
    menu.movesUi.moveCursor = 1;
    menu.movesUi.moveForgetSlot = 1;
    menu.movesUi.moveForgetConfirmOpen = true;
    menu.movesUi.moveForgetConfirmYes = false;
    capture(menu, "moves-forget-no");
    menu.movesUi.moveForgetConfirmYes = true;
    capture(menu, "moves-forget-yes");
    menu.movesUi.moveForgetConfirmOpen = false;
    menu.movesUi.recallMoveCount = 1;
    menu.movesUi.recallMoveIds[0] = moveIdForMonster(engine.speciesFor(state.team[0]),state.team[0],false);
    menu.movesUi.movePageMode = MenuScene::MovePageMode::RECALL_SELECT;
    menu.movesUi.moveCursor = 0;
    capture(menu, "moves-recall");
    menu.movesUi.recallSelectedIndex = 0;
    menu.movesUi.movePageMode = MenuScene::MovePageMode::RECALL_REPLACE;
    capture(menu, "moves-replace");

    // Run real scene actions through both selected UIs and the same engine.
    state = Game::GameState{};
    state.oobeDone = true;
    state.tutorialFlags = 0xff;
    state.teamCount = 2;
    state.team[0] = Game::MonsterFactory::create(1, 50);
    state.team[1] = Game::MonsterFactory::create(4, 50);
    state.team[1].origin = Game::Origin::BEFRIENDED;
    state.activeSlot = 1;
    menu.onEnter();
    MAIN_MENU(menu).cursor = 0;
    auto press = [&](int button, BtnAction action = BtnAction::PRESSED) {
        assert(menu.onButton({uint8_t(button), action}));
    };
    auto chooseTeamAction = [&](MenuScene::TeamAction action) {
        assert(menu.viewMode == MenuScene::ViewMode::TEAM);
        press(0);
        assert(menu.teamUi.teamActionOpen);
        int index = 0;
        while (index < menu.teamUi.teamActionCount() && menu.teamUi.teamActionAt(index) != action) ++index;
        assert(index < menu.teamUi.teamActionCount());
        for (int i = 0; i < index; ++i) press(1);
        press(0);
    };
    press(1); // Main menu: Explore, then Team.
    press(0);
    assert(menu.viewMode == MenuScene::ViewMode::TEAM);
    press(1);
    chooseTeamAction(MenuScene::TeamAction::FIRST);
    assert(state.team[0].speciesId == 4 && state.team[1].speciesId == 1 && state.activeSlot == 0);
    assert(menu.teamUi.teamCursor == 0);
    chooseTeamAction(MenuScene::TeamAction::STATUS);
    assert(menu.viewMode == MenuScene::ViewMode::STATUS && menu.statusUi.statusMonsterIndex == 0);
    const auto beforeStatus = state;
    press(1);
    assert(menu.statusUi.statusPage == 1);
    press(1, BtnAction::LONG_PRESS);
    assert(menu.viewMode == MenuScene::ViewMode::TEAM);
    assert(std::memcmp(&state, &beforeStatus, sizeof(state)) == 0);
    chooseTeamAction(MenuScene::TeamAction::MOVES);
    assert(menu.viewMode == MenuScene::ViewMode::MOVES && menu.movesUi.moveMonsterIndex == 0);
    press(0);
    assert(!menu.movesUi.moveForgetConfirmOpen); // Basic move stays locked.
    press(1);
    const auto forgotten = menu.movesUi.touchMoveInfo(menu.movesUi.moveCursor)->id;
    const auto beforeCancel = state;
    press(0);
    assert(menu.movesUi.moveForgetConfirmOpen && !menu.movesUi.moveForgetConfirmYes);
    press(0); // Default No keeps the skill and inventory.
    assert(!menu.movesUi.moveForgetConfirmOpen);
    assert(std::memcmp(&state, &beforeCancel, sizeof(state)) == 0);
    press(0); press(1); press(0);
    assert(!menu.movesUi.moveForgetConfirmOpen);
    assert(specialMoveIdForMonster(state.team[0], 0) == 0);
    press(1, BtnAction::LONG_PRESS);
    assert(menu.viewMode == MenuScene::ViewMode::TEAM);

    assert(engine.addItem(Game::ItemId::HEART_SCALE, 1));
    assert(menu.movesUi.openRecall(0));
    menu.pushView(MenuScene::ViewMode::MOVES);
    int recalledRow = -1;
    for (int i = 0; i < menu.movesUi.recallMoveCount; ++i)
        if (menu.movesUi.recallMoveIds[i] == forgotten) recalledRow = i;
    assert(recalledRow >= 0);
    for (int i = 0; i < recalledRow; ++i) press(1);
    press(0);
    assert(menu.movesUi.movePageMode == MenuScene::MovePageMode::RECALL_REPLACE);
    press(1, BtnAction::LONG_PRESS);
    assert(menu.movesUi.movePageMode == MenuScene::MovePageMode::RECALL_SELECT);
    assert(menu.movesUi.moveCursor == recalledRow && engine.itemCount(Game::ItemId::HEART_SCALE) == 1);
    press(0); press(0);
    assert(menu.viewMode == MenuScene::ViewMode::TEAM);
    assert(specialMoveIdForMonster(state.team[0], 0) == forgotten);
    assert(engine.itemCount(Game::ItemId::HEART_SCALE) == 0);
    assert(menu.toast && std::strstr(menu.toast, findMove(forgotten)->name));
    assert(menu.toast == menu.toastBuffer); // Formatted notices outlive their caller.

    press(1);
    chooseTeamAction(MenuScene::TeamAction::LEAVE);
    assert(state.teamCount == 1 && state.team[0].speciesId == 4 && state.activeSlot == 0);
    assert(state.storageCount == 1 && state.storage[0].speciesId == 1);
    const auto idle = menu.update(Hal::ins().millis(), 0);
    assert(!idle.redraw && idle.nextUpdateDelayMs != SceneUpdateResult::NO_UPDATE); // Toast expiry wakes once.
    // Invalidated membership exits before painting a skill-page notice.
    menu.pushView(MenuScene::ViewMode::MOVES);
    menu.movesUi.moveMonsterIndex = Game::TEAM_CAP;
    PixelRenderer::clear(0);
    menu.render();
    assert(menu.viewMode == MenuScene::ViewMode::TEAM);
    const auto exited = desktop.frameBuffer();
    for (int i = 0; i < Hal::DISPLAY_W * TouchUi::CONTENT_H; ++i)
        assert(exited.pixels[i] == PixelRenderer::rgb(7, 9, 14));
#if STICKMON_UI_COMPACT_TOUCH
    menu.toast=nullptr;
    menu.movesUi.openManage(0);menu.viewMode=MenuScene::ViewMode::MOVES;
    const auto beforeRead=state;
    const int skillCount=menu.movesUi.touchMoveRowCount();
    assert(skillCount==Features::Moves::Flow::learnedCount(Features::Moves::Flow::speciesFor(state.team[0]),state.team[0]));
    assert(menu.movesUi.touchTarget(80,100)==-1 && menu.movesUi.touchTarget(80,204)==-1);
    for(int i=0;i<skillCount;++i)press(1);
    assert(menu.movesUi.moveCursor==0); // B cycles skills, with no hidden Back row.
    const MoveInfo* longest=nullptr;int lines=0;
    for(int id=1;id<1024;++id)if(const auto* move=findMove(id)){
        const int count=Ui::StatusContent::wrappedTextLineCount(move->description,191);
        if(count>lines){lines=count;longest=move;}
    }
    assert(longest && lines>1);
    menu.movesUi.movePageMode=MenuScene::MovePageMode::RECALL_SELECT;
    menu.movesUi.recallMoveCount=2;menu.movesUi.recallMoveIds[0]=longest->id;
    menu.movesUi.recallMoveIds[1]=forgotten;menu.movesUi.moveCursor=0;
    capture(menu,"moves-wrapped-longest");
    const auto still=menu.movesUi.update(desktop.millis(),0);
    assert(!still.redraw && still.nextUpdateDelayMs==SceneUpdateResult::NO_UPDATE);
    desktop.advanceMs(2000);
    const auto later=menu.movesUi.update(desktop.millis(),2);
    assert(!later.redraw && later.nextUpdateDelayMs==SceneUpdateResult::NO_UPDATE && menu.movesUi.descScroll==0);
    using Touch=Platform::IInputDevice::TouchType;
    auto gesture=[&](Touch type,int x,int y){return menu.onTouch({type,int16_t(x),int16_t(y),desktop.millis()});};
    gesture(Touch::DOWN,200,196);gesture(Touch::MOVE,200,130);
    const float maxScroll=std::max(0,lines*16-94);
    assert(menu.movesUi.descScroll==std::min(66.f,maxScroll));
    // Dragging across the divider cannot select a move or trigger recall.
    gesture(Touch::MOVE,40,126);gesture(Touch::UP,40,126);
    assert(menu.movesUi.moveCursor==0 && menu.movesUi.movePageMode==MenuScene::MovePageMode::RECALL_SELECT);
    menu.movesUi.descScroll=999;menu.movesUi.update(desktop.millis(),0);
    assert(menu.movesUi.descScroll==maxScroll);
    press(1);assert(menu.movesUi.moveCursor==1 && menu.movesUi.descScroll==0);
    assert(!menu.movesUi.onTap(2));
    menu.movesUi.moveCursor=0;
    gesture(Touch::DOWN,80,126);gesture(Touch::CANCEL,80,126);gesture(Touch::UP,80,126);
    assert(menu.movesUi.moveCursor==0);
    assert(std::memcmp(&state,&beforeRead,sizeof(state))==0);
#endif
    std::printf("team=%u first=%u contact=%u scales=%u recalled=%u\n", state.teamCount,
        state.team[0].speciesId, state.storage[0].speciesId,
        engine.itemCount(Game::ItemId::HEART_SCALE), unsigned(forgotten));
}
