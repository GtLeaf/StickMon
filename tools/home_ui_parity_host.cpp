#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "platform/desktop/DesktopPlatform.h"
#include "game/GameRandom.h"
#include "game/MonsterFactory.h"
#include "presentation/PixelRenderer.h"
#include "core/RoomRenderer.h"
#include "core/UiStrings.h"
#define private public
#include "hardware/Hal.h"
#include "core/GameEngine.h"
#include "scenes/MainScene.h"
#undef private
#include "presentation/TouchUi.h"

#if STICKMON_HOME_UI_BASELINE
#define HOME(scene) (scene)
#define CAMERA_X(scene) ((scene).cameraX)
#define CAMERA_Y(scene) ((scene).cameraY)
#else
#define HOME(scene) ((scene).homeUi)
#define CAMERA_X(scene) ((scene).homeUi.camera.x)
#define CAMERA_Y(scene) ((scene).homeUi.camera.y)
#endif

using Type = Platform::IInputDevice::TouchType;
class TouchInput final : public Platform::IInputDevice {
public:
    void update() override {}
    bool pressed(Platform::InputButton) const override { return false; }
    bool touchAvailable() const override { return STICKMON_UI_COMPACT_TOUCH; }
};
class PowerProbe final : public Platform::IPowerDevice {
public:
    int level = 75;
    int batteryLevel() override { return level; }
    Platform::WakeReason wakeReason() const override { return Platform::WakeReason::NORMAL; }
    uint32_t hardwareRandom() override { return 1234; }
    size_t externalMemorySize() const override { return 0; }
    size_t externalMemoryFree() const override { return 0; }
    void restart() override {}
    void enterDeepSleep(uint64_t, bool) override {}
};

int main(int argc, char** argv) {
    assert(argc == 3);
    DesktopPlatform desktop(argv[1]);
    const auto bundle = desktop.serviceBundle();
    TouchInput input;
    PowerProbe power;
    Platform::Services services{bundle.lifecycle,bundle.clock,bundle.logger,input,
        bundle.display,bundle.audio,bundle.imu,power,bundle.blobs,bundle.resources,
        bundle.memory,bundle.peers};
    Platform::bind(services);
    assert(desktop.begin());
    auto& engine = GameEngine::ins();
    assert(engine.begin());
    auto& state = engine.gameState();
    std::unique_ptr<MainScene> room;
    unsigned checkpoints = 0;
    const ButtonEvent pet{0,BtnAction::PRESSED},feed{1,BtnAction::PRESSED};
    auto clock = [&](bool night) {
        state.gameMinutesTotal = night ? 1320 : 420;
        engine.gameClock.set(desktop.millis(),state.gameMinutesTotal);
    };
    auto setup = [&](uint16_t species=1,bool pair=false) -> MainScene& {
        room.reset(); engine.currentScene.reset();
        engine.visitSession.stop(); engine.contactVisit = {};
        engine.exploreTravel = ExploreTravelPhase::NONE;
        engine.clearMainSceneViewState();
        engine.evolutionEventHead=engine.evolutionEventCount=0;
        engine.moveReplacementEventHead=engine.moveReplacementEventCount=0;
        engine.idleActive = false;
        GameRandom::seed(1234);
        state = Game::GameState{};
        state.oobeDone = true; state.tutorialFlags=0xff;
        state.teamCount = pair ? 2 : 1;
        state.team[0] = Game::MonsterFactory::create(species,25);
        state.team[0].origin = Game::Origin::STARTER;
        state.team[1] = Game::MonsterFactory::create(4,25);
        state.team[1].origin = Game::Origin::BEFRIENDED;
        state.room.food[0]=5; state.room.food[1]=5;
        uint16_t ids[] = {species,4};
        assert(PokemonSprites::syncTeamCache(ids,pair?2:1));
        engine.prevId = SceneID::MAIN;
        engine.debugLightSource=0; engine.debugShowWalkBoundary=false;
        clock(false);
        room = std::make_unique<MainScene>();
        auto& scene = *room;
        scene.onEnter();
        scene.mainActor.x=120; scene.mainActor.y=90;
        scene.mainActor.task=Home::Task::IDLE;
        scene.mainActor.nextDecisionMs=desktop.millis()+60000;
        scene.mainActor.nextMindUpdateMs=desktop.millis()+60000;
        scene.mainActor.targetX=120; scene.mainActor.targetY=90;
        scene.pmdAction=MainScene::PmdAction::IDLE;
        scene.pmdDirection=MainScene::PmdDirection::FRONT;
        scene.pmdFrame=0; scene.pmdRenderOffsetX=scene.pmdRenderOffsetY=0;
        scene.contactGuestMotion=MainScene::ContactGuestMotion::NONE;
        scene.doorTransition=MainScene::DoorTransitionMode::NONE;
        scene.doorMainHidden=scene.doorVisitorHidden=false;
        scene.visitor.active=pair; scene.visitor.speciesId=4;
        scene.visitor.x=155; scene.visitor.y=105; scene.visitor.dropOffsetY=0;
        scene.visitor.task=Home::Task::IDLE;
        scene.visitor.direction=PokemonSprites::WalkDirection::DOWN;
        scene.visitor.frameIndex=0;
        scene.visitor.renderOffsetX=scene.visitor.renderOffsetY=0;
        scene.pairInteraction=MainScene::PairInteraction::NONE;
        scene.pairInteractionPhase=MainScene::PairInteractionPhase::NONE;
        scene.roomAction=MainScene::RoomAction::NONE;
        scene.nextAttentionMs=scene.nextSpecialActionMs=scene.nextPairInteractionMs=desktop.millis()+60000;
        HOME(scene).toast=nullptr; HOME(scene).toastUntil=0;
        power.level=75; Hal::ins().batteryFiltered=-1;
        scene.updateCamera();
        engine.saveCoordinator.reset(desktop.millis());
        ProgressionUi::resetEvolutionAnimation();
        return scene;
    };
    auto capture = [&](const std::string& name,bool hudOnly=false) {
        PixelRenderer::clear(0);
        if (hudOnly) {
#if STICKMON_HOME_UI_BASELINE
            room->drawHud(); room->drawToast();
#else
            room->homeUi.drawLayer(Ui::HomeLayer::HUD);
            room->homeUi.drawLayer(Ui::HomeLayer::TOAST);
#endif
        } else room->render();
        const auto fb=desktop.frameBuffer();
        FILE* file=std::fopen((std::string(argv[2])+"/"+name+".rgb565").c_str(),"wb");
        assert(file);
        assert(std::fwrite(fb.pixels,sizeof(uint16_t),fb.width*fb.height,file)==size_t(fb.width)*fb.height);
        std::fclose(file);
    };
    auto notice = [&](const char* text) {
        assert(HOME(*room).toast && std::strcmp(HOME(*room).toast,text)==0);
        assert(HOME(*room).toastUntil==desktop.millis()+1200);
    };
    auto touch = [&](Scene& scene,Type type,int x,int y) {
        desktop.advanceMs(10);
        return scene.onTouch({type,int16_t(x),int16_t(y),desktop.millis()});
    };
    auto sameState = [&](const Game::GameState& before) {
        assert(std::memcmp(&state,&before,sizeof(state))==0);
    };

    // Every expected framebuffer is generated by frozen pre-extraction code.
    for (bool night : {false,true}) {
        for (bool pair : {false,true}) {
            setup(1,pair); clock(night);
            capture(std::string(night?"night":"day")+(pair?"-two":"-one"));
        }
        for (uint16_t species : {1,4,92,147}) {
            auto& scene=setup(species,true); clock(night);
            scene.visitor.speciesId=species;
            for (bool flip : {false,true}) {
                scene.pmdDirection=flip?MainScene::PmdDirection::RIGHT:MainScene::PmdDirection::LEFT;
                scene.visitor.direction=flip?PokemonSprites::WalkDirection::RIGHT:PokemonSprites::WalkDirection::LEFT;
                for (int pose : {0,6}) {
                    scene.pmdRenderOffsetX=scene.visitor.renderOffsetX=3;
                    scene.pmdRenderOffsetY=scene.visitor.renderOffsetY=pose;
                    capture(std::string(night?"night":"day")+"-species-"+std::to_string(species)+
                        "-flip-"+std::to_string(flip)+"-pose-"+std::to_string(pose));
                }
            }
        }
        for (int light=0;light<7;++light) {
            setup(); clock(night); engine.debugLightSource=light;
            capture(std::string(night?"night":"day")+"-light-"+std::to_string(light));
        }
    }
    {
        auto& scene=setup(1,true);
        for (int offset : {0,12,28}) {
            scene.visitor.dropOffsetY=offset; capture("visitor-drop-"+std::to_string(offset));
        }
        scene.visitor.dropOffsetY=0;
        for (int mask=1;mask<4;++mask) {
            scene.doorMainHidden=mask&1; scene.doorVisitorHidden=mask&2;
            scene.updateCamera(); capture("hidden-"+std::to_string(mask));
        }
        scene.doorMainHidden=scene.doorVisitorHidden=false;
        for (int mask=1;mask<4;++mask) {
            scene.pmdAction=mask&1?MainScene::PmdAction::SLEEPING:MainScene::PmdAction::IDLE;
            scene.visitor.task=mask&2?Home::Task::RESTING:Home::Task::IDLE;
            capture("sleep-"+std::to_string(mask));
        }
    }
    {
        auto& scene=setup(1,true);
        int index=0;
        for (const auto& point : std::array<std::array<float,2>,7>{{{{-10,5}},{{20,35}},{{120,80}},{{220,140}},{{250,170}},{{120,20}},{{120,150}}}}) {
            scene.mainActor.x=point[0]; scene.mainActor.y=point[1]; scene.updateCamera();
            capture("camera-"+std::to_string(index++));
        }
        scene.mainActor.x=120; scene.mainActor.y=100;
        for (float y : {85.f,100.f,115.f}) {
            scene.visitor.x=122; scene.visitor.y=y; scene.updateCamera();
            capture("depth-"+std::to_string(int(y)));
        }
        scene.pairInteraction=MainScene::PairInteraction::TALK;
        scene.pairInteractionPhase=MainScene::PairInteractionPhase::CONVERSATION;
        scene.pairPhaseStartedMs=desktop.millis(); scene.pairPhaseUntilMs=desktop.millis()+2000;
        for (unsigned elapsed : {0u,60u,120u,250u}) {
            desktop.advanceMs(elapsed); scene.updateCamera(); capture("pair-hop-"+std::to_string(elapsed));
        }
    }
    for (uint8_t food=0;food<Game::ROOM_FOOD_COUNT;++food) {
        setup(); state.room.bowlFood=food; state.room.bowlCount=1;
        for (int bites=0;bites<=3;++bites) {
            state.room.bowlBitesRemaining=bites;
            capture("food-"+std::to_string(food)+"-bites-"+std::to_string(bites));
        }
    }
    for (int hp : {0,20,21,50,51,100}) {
        setup(1,true);
        for (int i=0;i<2;++i) { state.team[i].hpMax=100; state.team[i].hpCur=i?100-hp:hp; state.team[i].satiety=i?100-hp:hp; }
        capture("hud-hp-hunger-"+std::to_string(hp));
    }
    for (int battery : {-1,0,10,20,21,50,100}) {
        setup(); power.level=battery; capture("battery-"+std::to_string(battery));
    }
    {
        auto& scene=setup();
        HOME(scene).hungerAnimFrom=20; HOME(scene).hungerAnimTo=90;
        HOME(scene).hungerAnimStartedMs=desktop.millis(); HOME(scene).hungerAnimUntilMs=desktop.millis()+1000;
        for (int step=0;step<3;++step) { capture("hunger-tween-"+std::to_string(step)); desktop.advanceMs(500); }
        for (int hearts=0;hearts<3;++hearts) {
            scene.heartEffect=MainScene::HeartEffect(hearts); scene.heartEffectUntilMs=desktop.millis()+2000;
            capture("hearts-"+std::to_string(hearts));
        }
        scene.roomAction=MainScene::RoomAction::ATTENTION_WAIT;
        desktop.advanceMs(260); capture("hearts-pulse");
        scene.roomAction=MainScene::RoomAction::PET_HAPPY;
        scene.roomActionStartedMs=desktop.millis(); scene.roomActionUntilMs=desktop.millis()+1000;
        desktop.advanceMs(80); capture("action-hop");
    }
    for (int index=0;index<7;++index) {
        setup(); const char* texts[]={Ui::Menu::PET_TOAST,Ui::Menu::PET_LIMIT,Ui::Menu::PET_REST,
            Ui::Menu::FOOD_ADDED,Ui::Menu::FOOD_FULL,Ui::Menu::FOOD_MIXED,Ui::Menu::NO_FOOD};
        HOME(*room).toast=texts[index]; capture("toast-"+std::to_string(index));
    }
    {
        auto& scene=setup(); state.tutorialFlags=0; capture("tutorial-feed");
        state.tutorialFlags=Game::tutorialMask(Game::TutorialStep::ROOM_FEED); capture("tutorial-pet");
        state.tutorialFlags|=Game::tutorialMask(Game::TutorialStep::ROOM_PET); capture("tutorial-menu");
        scene.roomAction=MainScene::RoomAction::LOOK_AROUND; capture("tutorial-action-suppressed");
        scene.roomAction=MainScene::RoomAction::NONE; engine.idleActive=true; capture("tutorial-idle-suppressed");
        engine.idleActive=false; state.oobeDone=false; capture("tutorial-hatch-suppressed");
    }
    for (int dialog=0;dialog<=int(MainScene::ContactDialog::BYE_VISIT);++dialog) {
        auto& scene=setup(1,true); scene.contactDialog=MainScene::ContactDialog(dialog);
        for (bool yes : {false,true}) {
            scene.contactDialogYes=yes; capture("contact-"+std::to_string(dialog)+"-yes-"+std::to_string(yes));
        }
    }
    setup().visitDeparture=MainScene::VisitDeparture::AWAY; capture("contact-away");
    {
        auto& scene=setup(); state.pendingLevelUp=true; state.pendingLevelUpLevel=26;
        scene.progressionModal=MainScene::ProgressionModal::LEVEL_UP; capture("progression-level");
        state.pendingMoveLearn=true; state.pendingMoveId=73; state.pendingMoveSlot=0;
        scene.progressionModal=MainScene::ProgressionModal::LEARN_MOVE;
        for (int stage=0;stage<2;++stage) for (int cursor=0;cursor<2;++cursor) {
            scene.progressionMoveLearn.stage=ProgressionUi::MoveLearnStage(stage);
            scene.progressionMoveLearn.promptCursor=cursor;
            scene.progressionMoveLearn.replacementSlot=cursor+1;
            scene.progressionMoveLearn.confirmOpen=false;
            capture("progression-learn-"+std::to_string(stage)+"-"+std::to_string(cursor));
            scene.progressionMoveLearn.confirmOpen=true;
            scene.progressionMoveLearn.confirmYes=cursor;
            capture("progression-learn-confirm-"+std::to_string(stage)+"-"+std::to_string(cursor));
        }
        engine.queueMoveReplacementEvent(0,33,73);
        scene.progressionModal=MainScene::ProgressionModal::MOVE_REPLACED; capture("progression-replaced");
        engine.queueEvolutionEvent(0,1,2,25); scene.progressionModal=MainScene::ProgressionModal::EVOLUTION;
        for (int step=0;step<4;++step) { capture("progression-evolution-"+std::to_string(step)); desktop.advanceMs(800); }
        scene.progressionModal=MainScene::ProgressionModal::EVOLUTION_CANCELLED;
        scene.progressionCancelledSpeciesId=1;
        ProgressionUi::beginEvolutionCancellation(1,2,desktop.millis());
        capture("progression-cancel-start"); desktop.advanceMs(2000); capture("progression-cancel-end");
    }
#if STICKMON_ENABLE_DEBUG_FEATURES
    {
        auto& scene=setup(); engine.debugShowWalkBoundary=true;
        scene.mainActor.route.count=2; scene.mainActor.route.index=0;
        scene.mainActor.route.x[0]=100; scene.mainActor.route.y[0]=100;
        scene.mainActor.route.x[1]=150; scene.mainActor.route.y[1]=120;
        scene.mainActor.targetX=150; scene.mainActor.targetY=120;
        capture("debug-boundary-route");
    }
#endif

    // Exercise HUD/toast placement for room resources smaller than the panel.
    // Only presentation layers are drawn while resource geometry is injected.
    for (bool pair : {false,true}) {
        setup(1,pair);
        auto& resource=RoomResource::ins();
        const uint16_t originalWidth=resource.width_,originalHeight=resource.height_;
        for (int margin : {23,24,30,31,33,34,50}) {
            resource.width_=180;
            resource.height_=Hal::DISPLAY_H-2*margin;
            assert(RoomRenderer::viewport().y==margin);
            power.level=margin%3==0?10:(margin%3==1?-1:75);
            Hal::ins().batteryFiltered=-1;
            HOME(*room).toast=Ui::Menu::FOOD_ADDED;
            capture("small-room-margin-"+std::to_string(margin)+(pair?"-two":"-one"),true);
        }
        resource.width_=originalWidth; resource.height_=originalHeight;
    }

    // Real input journeys must retain shared game-state, save and timing effects.
    setup(); const auto beforeInvalid=state;
    assert(!room->onButton({2,BtnAction::PRESSED}));
    assert(!room->onButton({0,BtnAction::DOWN})); assert(!room->onButton({1,BtnAction::UP}));
    assert(!room->onTap(-1) && !room->onTap(3)); sameState(beforeInvalid); ++checkpoints;

    setup(); state.tutorialFlags=0; const auto oldMood=state.team[0].mood;
    assert(room->onButton(pet)); notice(Ui::Menu::PET_TOAST);
    assert(state.team[0].petCountToday==1 && state.team[0].mood==oldMood+5);
    assert(engine.tutorialComplete(Game::TutorialStep::ROOM_PET)); assert(engine.saveCoordinator.dirty()); ++checkpoints;

    setup(); state.tutorialFlags=0; state.team[0].petCountToday=4;
    assert(room->onButton(pet)); notice(Ui::Menu::PET_LIMIT);
    assert(state.team[0].petCountToday==4 && !engine.tutorialComplete(Game::TutorialStep::ROOM_PET)); ++checkpoints;

    setup(); state.tutorialFlags=0; state.team[0].fainted=true; state.team[0].hpCur=0;
    assert(room->onButton(pet)); notice(Ui::Menu::PET_REST);
    assert(state.team[0].petCountToday==0 && !engine.tutorialComplete(Game::TutorialStep::ROOM_PET)); ++checkpoints;

    setup(); state.tutorialFlags=0; const auto oldFood=state.room.food[0];
    assert(room->onButton(feed)); notice(Ui::Menu::FOOD_ADDED);
    assert(state.room.food[0]==oldFood-1 && state.room.bowlCount==1 && state.room.bowlBitesRemaining==3);
    assert(engine.tutorialComplete(Game::TutorialStep::ROOM_FEED)); ++checkpoints;

    setup(); state.tutorialFlags=0; state.room.bowlCount=3; state.room.bowlBitesRemaining=3;
    const auto fullFood=state.room.food[0]; assert(room->onButton(feed)); notice(Ui::Menu::FOOD_FULL);
    assert(state.room.food[0]==fullFood && !engine.tutorialComplete(Game::TutorialStep::ROOM_FEED)); ++checkpoints;

    setup(); state.tutorialFlags=0; state.room.bowlCount=1; state.room.bowlFood=1; state.room.bowlBitesRemaining=1;
    const auto mixedFood=state.room.food[0]; assert(room->onButton(feed)); notice(Ui::Menu::FOOD_MIXED);
    assert(state.room.food[0]==mixedFood && !engine.tutorialComplete(Game::TutorialStep::ROOM_FEED)); ++checkpoints;

    setup(); state.tutorialFlags=0; state.room.food[0]=0; state.room.food[1]=0;
    assert(room->onButton(feed)); notice(Ui::Menu::NO_FOOD);
    assert(state.room.bowlCount==0 && !engine.tutorialComplete(Game::TutorialStep::ROOM_FEED)); ++checkpoints;

    setup(); assert(!room->onButton({1,BtnAction::LONG_PRESS}));
    assert(room->onTap(0)); notice(Ui::Menu::PET_TOAST);
    assert(room->onTap(1)); notice(Ui::Menu::FOOD_ADDED); ++checkpoints;

    setup(); room->mainActor.x=100; room->mainActor.y=85; room->updateCamera();
    {
        const int x=room->worldToScreenX(100),y=room->worldToScreenY(85);
        assert(room->touchTarget(x,y)==0);
        assert(!touch(*room,Type::DOWN,x,y)); assert(touch(*room,Type::UP,x,y));
        assert(state.team[0].petCountToday==1);
        assert(!touch(*room,Type::UP,x,y)); assert(state.team[0].petCountToday==1);
    } ++checkpoints;

    setup(); {
        const int x=room->worldToScreenX(RoomResource::ins().foodX()),y=room->worldToScreenY(RoomResource::ins().foodY());
        assert(room->touchTarget(x,y)==1);
        touch(*room,Type::DOWN,x,y); assert(touch(*room,Type::UP,x,y));
        assert(state.room.bowlCount==1);
    } ++checkpoints;

    setup(); {
        const int x=room->worldToScreenX(120),y=room->worldToScreenY(90);
        touch(*room,Type::DOWN,x,y); touch(*room,Type::MOVE,x+35,y);
        assert(!touch(*room,Type::UP,x,y)); assert(state.team[0].petCountToday==0);
        touch(*room,Type::DOWN,x,y); room->cancelTouch(); assert(!touch(*room,Type::UP,x,y));
        assert(state.team[0].petCountToday==0);
    } ++checkpoints;

    setup(); {
        const auto view=RoomRenderer::viewport();
        assert(room->touchTarget(view.x-1,view.y+20)==-1);
        assert(room->touchTarget(view.x+view.width,view.y+20)==-1);
        assert(room->touchTarget(view.x+20,view.y-1)==-1);
        assert(room->touchTarget(view.x+20,view.y+view.height)==-1);
        room->doorMainHidden=true;
        assert(room->touchTarget(room->worldToScreenX(120),room->worldToScreenY(90))==-1);
    } ++checkpoints;

    setup(); room->doorTransition=MainScene::DoorTransitionMode::EXIT_ROUTE;
    {
        const auto before=state;
        assert(room->onButton(pet) && room->onButton(feed));
        assert(room->touchTarget(room->worldToScreenX(120),room->worldToScreenY(90))==-1);
        sameState(before);
    } ++checkpoints;

    setup(); room->contactGuestMotion=MainScene::ContactGuestMotion::HOST_TO_MEET;
    {
        const auto before=state; assert(room->onButton(pet) && room->onButton(feed)); sameState(before);
        assert(room->touchTarget(room->worldToScreenX(120),room->worldToScreenY(90))==-1);
    } ++checkpoints;

    setup(); room->contactDialog=MainScene::ContactDialog::KNOCK;
    assert(room->contactDialogYes && room->onButton(feed) && !room->contactDialogYes);
    assert(room->onButton({0,BtnAction::LONG_PRESS}) && !room->contactDialogYes);
    assert(room->onButton(pet) && room->contactDialog==MainScene::ContactDialog::NONE && room->contactDialogYes);
    assert(state.team[0].petCountToday==0); ++checkpoints;

    setup(); room->contactDialog=MainScene::ContactDialog::HAPPY_RETURN;
    assert(room->onButton(pet) && room->contactDialog==MainScene::ContactDialog::BYE_RETURN);
    assert(room->onButton(feed) && room->contactDialog==MainScene::ContactDialog::BYE_RETURN);
    assert(room->touchTarget(room->worldToScreenX(120),room->worldToScreenY(90))==-1); ++checkpoints;

    setup(); state.pendingLevelUp=true; state.pendingLevelUpLevel=26;
    room->progressionModal=MainScene::ProgressionModal::LEVEL_UP;
    assert(room->onButton(feed) && state.pendingLevelUp);
    assert(room->onButton({0,BtnAction::LONG_PRESS}) && state.pendingLevelUp);
    assert(room->touchTarget(room->worldToScreenX(120),room->worldToScreenY(90))==-1);
    assert(room->onButton(pet) && !state.pendingLevelUp && room->progressionModal==MainScene::ProgressionModal::NONE);
    assert(state.team[0].petCountToday==0); ++checkpoints;

    setup(); HOME(*room).toast=Ui::Menu::FOOD_ADDED; HOME(*room).toastUntil=desktop.millis()+100;
    desktop.advanceMs(100); {
        const auto demand=room->update(desktop.millis(),0);
        assert(demand.redraw && demand.nextUpdateDelayMs==66 && HOME(*room).toast==nullptr);
    } ++checkpoints;

    setup(); HOME(*room).toast=Ui::Menu::FOOD_ADDED; HOME(*room).toastUntil=desktop.millis()+100;
    room->progressionModal=MainScene::ProgressionModal::LEVEL_UP; state.pendingLevelUp=true;
    desktop.advanceMs(200); {
        const auto demand=room->update(desktop.millis(),0);
        assert(demand.redraw && demand.nextUpdateDelayMs==66 && HOME(*room).toast!=nullptr);
    } ++checkpoints;

    setup(); room->mainActor.x=100; room->mainActor.y=85;
    room->mainActor.targetX=100; room->mainActor.targetY=85;
    room->onBeforeSave(); {
        const auto saved=engine.mainSceneViewState();
        assert(saved.valid && saved.monsterX==100 && saved.monsterY==85);
        room->onExit(); room=std::make_unique<MainScene>(); room->onEnter();
        assert(room->mainActor.x==100 && room->mainActor.y==85);
    } ++checkpoints;

    setup(); room->mainActor.x=100; room->mainActor.y=85; room->onBeforeSave();
    room->doorTransition=MainScene::DoorTransitionMode::EXIT_ROUTE;
    room->mainActor.x=200; room->mainActor.y=120; room->onBeforeSave();
    assert(engine.mainSceneViewState().monsterX==100 && engine.mainSceneViewState().monsterY==85); ++checkpoints;

    // Owned scene destruction is exercised during input dispatch (ASan/UBSan).
    setup(); room.reset(); engine.requestScene(SceneID::MAIN);
    state.tutorialFlags=0;
    assert(engine.currentScene->onButton({0,BtnAction::LONG_PRESS}));
    assert(engine.currentId==SceneID::MENU && engine.tutorialComplete(Game::TutorialStep::OPEN_MENU)); ++checkpoints;

    setup(); room.reset(); engine.requestScene(SceneID::MAIN);
#if STICKMON_UI_COMPACT_TOUCH
    {
        const int x=254,y=208;
        assert(engine.currentScene->touchTarget(234,218)==-1);
        assert(engine.currentScene->touchTarget(235,y)==-1);
        assert(engine.currentScene->touchTarget(272,y)==-1);
        assert(engine.currentScene->touchTarget(271,218)==2);
        assert(engine.currentScene->touchTarget(x,y)==2);
        touch(*engine.currentScene,Type::DOWN,x,y);
        engine.currentScene->cancelTouch();
        assert(!touch(*engine.currentScene,Type::UP,x,y));
        assert(engine.currentId==SceneID::MAIN);
        touch(*engine.currentScene,Type::DOWN,x,y);
        assert(touch(*engine.currentScene,Type::UP,x,y));
    }
#else
    assert(engine.currentScene->onTap(2));
#endif
    assert(engine.currentId==SceneID::MENU); ++checkpoints;
#if !STICKMON_HOME_UI_BASELINE
    // Regression: accepting an invitation synchronously destroys this scene.
    setup(1,true); room.reset(); engine.requestScene(SceneID::MAIN);
    engine.contactVisit.active=true; engine.contactVisit.kind=ContactVisitKind::EXPLORE;
    engine.contactVisit.storageSlot=0;
    state.team[1].origin=Game::Origin::VISITOR;
    state.storageCount=1; state.storage[0]=state.team[1];
    auto* invitation=static_cast<MainScene*>(engine.currentScene.get());
    invitation->contactGuestMotion=MainScene::ContactGuestMotion::NONE;
    invitation->contactDialog=MainScene::ContactDialog::EXPLORE_INVITE;
    invitation->contactDialogYes=true;
    assert(invitation->onButton(pet));
    assert(engine.currentId==SceneID::EXPLORE && engine.contactVisit.exploring);
#endif
    engine.currentScene.reset(); room.reset();
    std::printf("home-checkpoints=%u\n",checkpoints);
}
