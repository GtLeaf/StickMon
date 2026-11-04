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
#include "game/ExploreItemProgression.h"
#include "game/ExploreRouteGeometry.h"
#include "assets/PokemonSprites.h"
#include "presentation/PixelRenderer.h"
#define private public
#include "hardware/Hal.h"
#include "core/GameEngine.h"
#include "scenes/ExploreScene.h"
#undef private
#if !STICKMON_EXPLORE_UI_BASELINE
#include "ui/common/ExploreBattleUiContent.h"
#if STICKMON_UI_COMPACT_TOUCH
#include "ui/compact_touch/ExploreHudUi.h"
#include "ui/compact_touch/HudControls.h"
#endif
#endif

#if STICKMON_EXPLORE_UI_BASELINE
#define UI_STATE(scene) (scene)
#else
#define UI_STATE(scene) ((scene).ui.state)
#endif
using Phase = ExploreScene::Phase;
using Turn = ExploreScene::BattleTurnStage;
using Switch = ExploreScene::BattleSwitchStage;
using Friend = ExploreScene::FriendshipStep;
using Touch = Platform::IInputDevice::TouchType;
class Input final : public Platform::IInputDevice {
public:
    void update() override {}
    bool pressed(Platform::InputButton) const override { return false; }
    bool touchAvailable() const override { return STICKMON_UI_COMPACT_TOUCH; }
};

int main(int argc,char** argv) {
    assert(argc==3 || argc==4);
    DesktopPlatform desktop(argv[1]);
    const auto bundle=desktop.serviceBundle(); Input input;
    Platform::Services services{bundle.lifecycle,bundle.clock,bundle.logger,input,
        bundle.display,bundle.audio,bundle.imu,bundle.power,bundle.blobs,
        bundle.resources,bundle.memory,bundle.peers};
    Platform::bind(services); assert(desktop.begin());
    auto& engine=GameEngine::ins(); assert(engine.begin());
    auto& state=engine.gameState();
    std::unique_ptr<ExploreScene> scene;
    unsigned checks=0;
    const ButtonEvent a{0,BtnAction::PRESSED},b{1,BtnAction::PRESSED};
    const ButtonEvent longA{0,BtnAction::LONG_PRESS},longB{1,BtnAction::LONG_PRESS};
    auto setup=[&](uint16_t species=1,bool pair=true,int unlocked=5) -> ExploreScene& {
        scene.reset(); engine.currentScene.reset();
        engine.currentId=SceneID::MAIN; engine.prevId=SceneID::MAIN;
        engine.visitSession.stop(); engine.contactVisit={};
        engine.exploreTravel=ExploreTravelPhase::NONE;
        engine.sceneFade=GameEngine::SceneFadePhase::NONE;
        engine.evolutionEventHead=engine.evolutionEventCount=0;
        engine.moveReplacementEventHead=engine.moveReplacementEventCount=0;
        engine.debugBattleRequested=false; engine.debugMenuReturnRequested=false;
        engine.debugShowBattleDrawBounds=false;
        engine.clearMainSceneViewState();
        GameRandom::seed(1234);
        state=Game::GameState{}; state.oobeDone=true; state.tutorialFlags=0xff;
        state.teamCount=pair?2:1;
        state.team[0]=Game::MonsterFactory::create(species,35);
        state.team[1]=Game::MonsterFactory::create(4,30);
        state.team[1].origin=Game::Origin::BEFRIENDED;
        for (int i=0;i<unlocked;++i) state.explorePoolRerollCounts[i]=1;
        state.gameMinutesTotal=420;
        engine.gameClock.set(desktop.millis(),state.gameMinutesTotal);
        uint16_t speciesIds[]={species,4};
        assert(PokemonSprites::syncTeamCache(speciesIds,pair?2:1));
        scene=std::make_unique<ExploreScene>(); scene->onEnter();
        scene->areaPreviewLoadPending=false;
        scene->clearBattleLogs();
        engine.saveCoordinator.reset(desktop.millis());
        ProgressionUi::resetEvolutionAnimation();
        return *scene;
    };
    auto walking=[&](int area=0,uint16_t species=1,bool pair=true) -> ExploreScene& {
        auto& s=setup(species,pair);
        s.activeArea=ExploreScene::Area(area); UI_STATE(s).areaCursor=area;
        s.resetWalk();
        assert(s.phase==Phase::WALKING && s.generatedMap.paths[0].pointCount>1);
        s.routeMoving=s.routeFollowerMoving=s.autoWalkActive=false;
        s.areaPreviewLoadPending=false;
        return s;
    };
    auto encounter=[&](uint16_t wildSpecies=25,uint16_t playerSpecies=1,bool pair=true) -> ExploreScene& {
        auto& s=walking(0,playerSpecies,pair);
        s.beginEncounter(*findSpecies(wildSpecies),15);
        s.clearBattleLogs(); s.phase=Phase::ENCOUNTER;
        s.playerBattleState={}; s.wildBattleState={}; s.battlePlayerSlot=0;
        s.wildHp=s.wildHpMax=s.wildRuntime.hpMax;
        s.defeatAwaitInput=false;
        return s;
    };
    auto preview=[&](ExploreScene& s) {
        PokemonSprites::preloadDynamicSpecies(s.areaPreloadSpeciesIds,s.areaPreloadSpeciesCount,255);
        s.refreshAreaPreviewFrames(); s.areaPreviewLoadPending=false;
    };
    auto capture=[&](const std::string& name) {
        PixelRenderer::clear(0); scene->render();
        const auto fb=desktop.frameBuffer();
        FILE* file=std::fopen((std::string(argv[2])+"/"+name+".rgb565").c_str(),"wb");
        assert(file);
        assert(std::fwrite(fb.pixels,sizeof(uint16_t),fb.width*fb.height,file)==size_t(fb.width)*fb.height);
        std::fclose(file);
    };
    auto advance=[&](uint32_t ms) { desktop.advanceMs(ms);return scene->update(desktop.millis(),ms*.001f); };
    auto sameState=[&](const Game::GameState& before) { assert(std::memcmp(&state,&before,sizeof(state))==0); };
    auto ownedDeparture=[&]() {
        setup(); scene.reset(); engine.requestScene(SceneID::EXPLORE);
        assert(engine.currentId==SceneID::EXPLORE);
        assert(engine.currentScene->onButton(a));
        assert(engine.currentId==SceneID::MAIN && engine.exploreTravel==ExploreTravelPhase::DEPARTING);
    };
    auto ownedDebugFlee=[&]() {
#if STICKMON_ENABLE_DEBUG_FEATURES
        setup(25,false); scene.reset(); engine.beginDebugBattle();
        assert(engine.currentId==SceneID::EXPLORE);
        auto* s=static_cast<ExploreScene*>(engine.currentScene.get());
        s->clearBattleLogs();
        s->wild=findSpecies(10); s->wildRuntime=Game::MonsterFactory::create(10,1);
        s->wildHp=s->wildHpMax=s->wildRuntime.hpMax;
        UI_STATE(*s).battleCursor=3;
        assert(s->onButton(a) && s->fleeExitPending);
        desktop.advanceMs(1100);
        s->update(desktop.millis(),1.1f);
        assert(engine.currentId==SceneID::MENU);
#endif
    };
    if (argc==4) {
        if (std::strcmp(argv[3],"departure-uaf")==0) ownedDeparture();
        else if (std::strcmp(argv[3],"debug-flee-uaf")==0) ownedDebugFlee();
        else assert(false);
        engine.currentScene.reset(); scene.reset(); return 0;
    }

    // Frozen production rendering, including animation boundaries and overlays.
    for (int unlocked : {0,2,5}) {
        auto& s=setup(1,true,unlocked);
        const int count=ExploreItemProgression::visibleAreaCount(state);
        for (int index=0;index<=count;++index) {
            preview(s);
            capture("select-unlocked-"+std::to_string(unlocked)+"-item-"+std::to_string(index));
            assert(s.onButton(b));
        }
    }
    {
        auto& s=setup(); preview(s);
        for (float at : {.25f,.49f,.5f,.75f,2.5f,5.75f}) {
            UI_STATE(s).areaAnimCursor=at; capture("select-lerp-"+std::to_string(int(at*100)));
        }
        UI_STATE(s).areaAnimCursor=0;
        for (unsigned elapsed : {0u,2299u,2300u,2301u,2425u,2550u,2799u,2800u,5100u}) {
            s.areaPreviewStartedAt=desktop.millis()-elapsed;
            capture("preview-cycle-"+std::to_string(elapsed));
        }
        for (int rarity=0;rarity<3;++rarity) {
            for (int i=0;i<s.areaPreviewPool.count;++i)
                s.areaPreviewPool.entries[i].rarity=i>=rarity?ExplorePool::Rarity::COMMON:ExplorePool::Rarity::RARE;
            capture("preview-rare-"+std::to_string(rarity));
        }
        s.areaPreviewPool.count=0; capture("preview-empty");
    }
    for (int area=0;area<6;++area) {
        auto& s=walking(area);
        const auto& path=s.generatedMap.paths[0];
        for (int pose=0;pose<3;++pose) {
            const int index=pose*(path.pointCount-1)/2;
            s.routeIndex=index;
            s.routeWorldX=path.points[index].x*26.f+13;
            s.routeWorldY=path.points[index].y*26.f+13;
            s.routeFollowerWorldX=s.routeWorldX+4;
            s.routeFollowerWorldY=s.routeWorldY+(pose==1?12:-12);
            capture("walking-area-"+std::to_string(area)+"-pose-"+std::to_string(pose));
        }
        s.expeditionBossSpeciesId=143; s.routeBossPending=true;
        s.routeBossIndex=std::min<int>(path.pointCount-1,s.routeIndex);
        s.routePickupAvailable=true; s.routePickupIndex=s.routeBossIndex;
        capture("walking-boss-pickup-area-"+std::to_string(area));
        state.gameMinutesTotal=1320;capture("walking-night-area-"+std::to_string(area));
        s.routePickupAvailable=false;s.routeBossPending=false;
        state.team[0].hpCur=0;state.team[0].fainted=true;
        capture("walking-reserve-leader-area-"+std::to_string(area));
    }
    // Outdoor paired-cell roads use their centerline for horizontal travel;
    // keep explicit left/right captures to guard the authored foot anchor.
    {
        auto& s=walking(0,1,false);
        const auto& path=s.generatedMap.paths[0];
        int horizontalIndex=-1;
        for (int i=1;i<path.pointCount;++i) {
            if (path.points[i-1].y==path.points[i].y) { horizontalIndex=i; break; }
        }
        assert(horizontalIndex>=1);
        const auto point=ExploreRouteGeometry::pathPoint(path, static_cast<uint8_t>(horizontalIndex));
        assert(point.y==path.points[horizontalIndex].y*26.f+26.f);
        s.routeIndex=static_cast<uint8_t>(horizontalIndex);
        s.routeWorldX=point.x;s.routeWorldY=point.y;
        s.routeWalkDirection=static_cast<uint8_t>(PokemonSprites::WalkDirection::LEFT);
        capture("walking-horizontal-left");
        s.routeWalkDirection=static_cast<uint8_t>(PokemonSprites::WalkDirection::RIGHT);
        capture("walking-horizontal-right");
    }
    for (uint16_t species : {1,92,147}) {
        auto& s=walking(2,species);
        s.routeMoving=s.routeFollowerMoving=true;
        s.routeMoveDurationMs=s.routeLeaderMoveDurationMs=s.routeFollowerMoveDurationMs=600;
        s.routeIndex=3;
        for (unsigned elapsed : {0u,100u,120u,121u,220u,400u,599u}) {
            s.routeMoveStarted=desktop.millis()-elapsed;
            s.routeWalkDirection=(elapsed/100)%4;
            s.routeFollowerWalkDirection=(s.routeWalkDirection+1)%4;
            capture("walking-animation-"+std::to_string(species)+"-"+std::to_string(elapsed));
        }
        s.exploreMenuOpen=true;s.exploreMenuOpenedAt=desktop.millis();
        for (int cursor=0;cursor<4;++cursor) {
            UI_STATE(s).exploreMenuCursor=cursor;
            capture("walking-side-menu-"+std::to_string(species)+"-"+std::to_string(cursor));
        }
        s.exploreMenuOpen=false;s.phase=Phase::EXITING;capture("walking-exiting-"+std::to_string(species));
    }
    for (uint16_t species : {1,92,143,147}) {
        auto& s=encounter(species);
        for (int command=0;command<4;++command) {
            UI_STATE(s).battleCursor=command;
            capture("battle-species-"+std::to_string(species)+"-command-"+std::to_string(command));
        }
        for (int hp : {0,20,21,50,51,100}) {
            s.wildHpMax=state.team[0].hpMax=100;s.wildHp=hp;state.team[0].hpCur=100-hp;
            capture("battle-hp-"+std::to_string(species)+"-"+std::to_string(hp));
        }
    }
    for (int status=0;status<=int(Game::MajorStatus::FREEZE);++status) {
        auto& s=encounter();
        state.team[0].majorStatus=s.wildRuntime.majorStatus=Game::MajorStatus(status);
        for (int frame=0;frame<3;++frame) {
            capture("battle-status-"+std::to_string(status)+"-frame-"+std::to_string(frame));desktop.advanceMs(180);
        }
    }
    for (int condition=0;condition<3;++condition) {
        auto& s=encounter();
        if (condition==0) s.playerBattleState.confusionTurns=s.wildBattleState.confusionTurns=3;
        if (condition==1) s.playerBattleState.bindTurns=s.wildBattleState.bindTurns=3;
        if (condition==2) s.playerBattleState.yawnTurns=s.wildBattleState.yawnTurns=2;
        for (int frame=0;frame<3;++frame) {
            capture("battle-volatile-"+std::to_string(condition)+"-frame-"+std::to_string(frame));desktop.advanceMs(180);
        }
    }
    for (bool wildAttacker : {false,true}) for (bool selfHit : {false,true}) {
        auto& s=encounter();s.battleTurnStage=Turn::ANIMATING;
        s.battleActionAttackerWild=wildAttacker;s.battleActionSelfHit=selfHit;
        s.battleHpFrom=100;s.battleHpTo=10;s.wildHpMax=state.team[0].hpMax=100;
        s.wildHp=state.team[0].hpCur=10;
        for (unsigned elapsed : {0u,259u,260u,300u,420u,539u,540u,750u,959u,960u}) {
            s.battleActionStarted=desktop.millis()-elapsed;
            capture("battle-hit-"+std::to_string(wildAttacker)+"-self-"+std::to_string(selfHit)+"-"+std::to_string(elapsed));
        }
    }
    {
        auto& s=encounter();s.wildHp=0;s.battleResultPending=true;
        s.expAnimationFrom=state.team[0].exp;s.expAnimationTo=s.expAnimationFrom+1000;
        s.expAnimationPending=true;capture("battle-exp-pending");
        s.expAnimationPending=false;s.expAnimationActive=s.battleExpVisible=true;
        state.team[0].exp=s.expAnimationTo;
        for (unsigned elapsed : {0u,450u,900u}) {
            s.expAnimationStarted=desktop.millis()-elapsed;
            capture("battle-exp-"+std::to_string(elapsed));
        }
    }
    for (int stage=1;stage<3;++stage) {
        auto& s=encounter();s.battleSwitchStage=Switch(stage);
        for (unsigned elapsed : {0u,180u,359u,360u}) {
            s.battleSwitchStarted=desktop.millis()-elapsed;
            capture("battle-switch-"+std::to_string(stage)+"-"+std::to_string(elapsed));
        }
    }
    for (int food=0;food<Game::ROOM_FOOD_COUNT;++food) {
        auto& s=encounter();s.foodThrowActive=true;s.foodThrowIndex=food;
        for (unsigned elapsed : {0u,175u,350u,525u,700u}) {
            s.foodThrowStarted=desktop.millis()-elapsed;
            capture("battle-food-"+std::to_string(food)+"-"+std::to_string(elapsed));
        }
    }
    {
        auto& s=encounter();
        for (int rows=0;rows<3;++rows) {
            s.battleLogActive=rows>0;s.battleLogVisibleCount=rows;
            std::snprintf(s.battleLogVisible[0],64,"%s",Ui::Explore::BATTLE_WIN);
            std::snprintf(s.battleLogVisible[1],64,"%s",Ui::Explore::FLEE_SUCCESS);
            capture("battle-log-rows-"+std::to_string(rows));
        }
        s.clearBattleLogs();s.defeatAwaitInput=true;capture("battle-defeat");
        s.defeatAwaitInput=false;s.battleTurnStage=Turn::WAIT_ACTION_START;capture("battle-wait-command-suppressed");
    }
    for (int step=0;step<6;++step) {
        auto& s=encounter();s.phase=Phase::FRIENDSHIP;s.friendshipStep=Friend(step);
        s.battleLogVisibleCount=2;
        std::snprintf(s.battleLogVisible[0],64,"%s",Ui::Explore::FRIEND_CONTACT_QUESTION);
        std::snprintf(s.battleLogVisible[1],64,"%s",Ui::Explore::FRIEND_TEAM_QUESTION);
        for (bool yes : {false,true}) {
            UI_STATE(s).friendshipConfirmYes=yes;
            capture("friendship-step-"+std::to_string(step)+"-yes-"+std::to_string(yes));
        }
    }
    for (int kind=0;kind<=int(ExploreSpecial::Kind::MEW_EVENT);++kind) {
        auto& s=walking(4);s.phase=Phase::SPECIAL_PROMPT;
        s.expeditionSpecialKind=ExploreSpecial::Kind(kind);
        s.expeditionBossSpeciesId=ExploreSpecial::configFor(s.expeditionSpecialKind).speciesId;
        for (bool yes : {false,true}) {
            UI_STATE(s).specialChallengeYes=yes;
            capture("special-kind-"+std::to_string(kind)+"-yes-"+std::to_string(yes));
        }
    }
    {
        auto& s=walking();s.phase=Phase::PICKUP;s.resultMessage=Ui::Bag::USED_POTION;capture("pickup-message");
        s.resultMessage=nullptr;capture("pickup-empty");
        s.phase=Phase::RESULT;s.resultMessage=Ui::Explore::BATTLE_WIN;capture("result-message");
        s.resultMessage=nullptr;s.wild=findSpecies(25);capture("result-wild-fallback");
        s.wild=nullptr;capture("result-empty");
        s.phase=Phase::ENDING;
        for (int bond : {0,1,5}) { s.adventureBondGain=bond;capture("ending-bond-"+std::to_string(bond)); }
        s.phase=Phase::WALKING;state.tutorialFlags=0;capture("tutorial-walk");
        state.tutorialFlags=Game::tutorialMask(Game::TutorialStep::EXPLORE_WALK);capture("tutorial-menu");
        s.exploreMenuOpen=true;capture("tutorial-side-menu-suppressed");
    }
    {
        auto& s=encounter();state.pendingLevelUp=true;state.pendingLevelUpLevel=36;
        s.phase=Phase::LEVEL_UP;capture("progression-level");
        state.pendingMoveLearn=true;state.pendingMoveId=73;s.phase=Phase::LEARN_MOVE;
        for (int stage=0;stage<2;++stage) for (int slot=0;slot<2;++slot) {
            s.moveLearnState.stage=ProgressionUi::MoveLearnStage(stage);
            s.moveLearnState.promptCursor=slot;s.moveLearnState.replacementSlot=slot+1;
            s.moveLearnState.confirmOpen=false;
            capture("progression-learn-"+std::to_string(stage)+"-"+std::to_string(slot));
            s.moveLearnState.confirmOpen=true;s.moveLearnState.confirmYes=slot;
            capture("progression-learn-confirm-"+std::to_string(stage)+"-"+std::to_string(slot));
        }
        engine.queueMoveReplacementEvent(0,33,73);s.phase=Phase::MOVE_REPLACED;capture("progression-replaced");
        engine.queueEvolutionEvent(0,1,2,35);s.phase=Phase::EVOLUTION;
        for (int frame=0;frame<4;++frame) { capture("progression-evolution-"+std::to_string(frame));desktop.advanceMs(800); }
        s.phase=Phase::EVOLUTION_CANCELLED;s.progressionCancelledSpeciesId=1;
        ProgressionUi::beginEvolutionCancellation(1,2,desktop.millis());capture("progression-cancel-start");
        desktop.advanceMs(2000);capture("progression-cancel-end");
    }
#if STICKMON_ENABLE_DEBUG_FEATURES
    encounter();engine.debugShowBattleDrawBounds=true;capture("debug-battle-bounds");
#endif

    // Real operation journeys. Their semantic output is identical in both families.
    setup();const auto invalid=state;
    assert(!scene->onButton({2,BtnAction::PRESSED}));assert(!scene->onButton({0,BtnAction::DOWN}));
    assert(!scene->onButton({1,BtnAction::UP}));sameState(invalid);++checks;

    setup(1,true,0);assert(UI_STATE(*scene).areaCursor==0);
    assert(scene->onButton(b) && UI_STATE(*scene).areaCursor==1);
    assert(scene->onButton(b) && UI_STATE(*scene).areaCursor==6);
    assert(scene->onButton(b) && UI_STATE(*scene).areaCursor==0 && UI_STATE(*scene).areaAnimCursor==0);++checks;

    setup(1,true,0);scene->onButton(b);const auto locked=state;
    assert(scene->onButton(a) && engine.exploreTravel==ExploreTravelPhase::NONE);sameState(locked);++checks;

    setup();scene->onButton(b);const float beforeLerp=UI_STATE(*scene).areaAnimCursor;
    { const auto d=advance(20);assert(d.redraw && d.nextUpdateDelayMs==66);assert(UI_STATE(*scene).areaAnimCursor>beforeLerp); }++checks;

    setup();preview(*scene);scene->areaPreviewStartedAt=desktop.millis();UI_STATE(*scene).areaPreviewVisualCycle=0;
    { const auto d=advance(100);assert(!d.redraw && d.nextUpdateDelayMs==2201); }++checks;

    scene->areaPreviewStartedAt=desktop.millis()-2400;
    { const auto d=scene->update(desktop.millis(),0);assert(d.redraw && d.nextUpdateDelayMs==66); }++checks;

    setup();UI_STATE(*scene).areaCursor=6;UI_STATE(*scene).areaAnimCursor=6;scene->areaPreviewPool.count=0;
    { const auto d=advance(20);assert(!d.redraw && d.nextUpdateDelayMs==SceneUpdateResult::NO_UPDATE); }++checks;

    walking();state.tutorialFlags=0;assert(scene->onButton(a));
    assert(scene->autoWalkActive && scene->routeMoving && engine.tutorialComplete(Game::TutorialStep::EXPLORE_WALK));++checks;

    walking();state.tutorialFlags=0;assert(scene->onButton(b));
    assert(scene->exploreMenuOpen && UI_STATE(*scene).exploreMenuCursor==0 && engine.tutorialComplete(Game::TutorialStep::EXPLORE_MENU));
    for (int i=0;i<4;++i) assert(scene->onButton(b));assert(UI_STATE(*scene).exploreMenuCursor==0);++checks;

    walking();scene->beginAutoWalk();scene->onButton(b);
    {const float x=scene->routeWorldX,y=scene->routeWorldY;const auto started=scene->routeMoveStarted;
    const auto d=advance(500);assert(d.redraw && d.nextUpdateDelayMs==66);assert(scene->routeWorldX==x && scene->routeWorldY==y);
    UI_STATE(*scene).exploreMenuCursor=3;assert(scene->onButton(a));assert(!scene->exploreMenuOpen && scene->routeMoveStarted==started+500);}++checks;

    walking();scene->onButton(b);assert(scene->onButton(longA) && scene->exploreMenuOpen);
    assert(!scene->onButton({0,BtnAction::DOWN}));assert(scene->onButton(a) && scene->exploreSubViewOpen);
    assert(scene->exploreSubView.viewMode==MenuScene::ViewMode::TEAM);++checks;

    {const auto demand=advance(20);assert(demand.nextUpdateDelayMs==SceneUpdateResult::NO_UPDATE);
    assert(scene->onButton(longB) && !scene->exploreSubViewOpen);}++checks;

    walking();scene->onButton(b);scene->onButton(b);assert(scene->onButton(a) && scene->exploreSubViewOpen);
    assert(scene->exploreSubView.viewMode==MenuScene::ViewMode::BAG);assert(scene->onButton(longB) && !scene->exploreSubViewOpen);++checks;

    walking();scene->iceSliding=true;const auto ice=state;
    assert(scene->onButton(a) && scene->onButton(b) && scene->onButton(longA));
    assert(!scene->exploreMenuOpen && !scene->autoWalkActive);sameState(ice);++checks;

    walking();scene->phase=Phase::PICKUP;assert(scene->onButton(b) && scene->phase==Phase::PICKUP);
    assert(scene->onButton(longA) && scene->phase==Phase::PICKUP);assert(scene->onButton(a) && scene->phase==Phase::WALKING);++checks;

    walking();scene->phase=Phase::EXITING;const auto exiting=state;
    assert(scene->onButton(a) && scene->onButton(b) && scene->onButton(longA));sameState(exiting);++checks;

    walking();scene->phase=Phase::SPECIAL_PROMPT;UI_STATE(*scene).specialChallengeYes=true;scene->routeBossPending=true;
    assert(scene->onButton(longA) && UI_STATE(*scene).specialChallengeYes);
    assert(scene->onButton(b) && !UI_STATE(*scene).specialChallengeYes);
    assert(scene->onButton(a) && scene->phase==Phase::WALKING && !scene->routeBossPending);++checks;

    encounter();for(int i=1;i<=4;++i) {assert(scene->onButton(b));assert(UI_STATE(*scene).battleCursor==i%4);}
    assert(scene->onButton(longA) && scene->phase==Phase::ENCOUNTER);++checks;

    encounter();scene->enqueueBattleLog(Ui::Explore::BATTLE_WIN);const auto busy=state;
    assert(scene->onButton(b) && UI_STATE(*scene).battleCursor==0);assert(scene->onButton(a));sameState(busy);
    assert(scene->battleLogBusy());++checks;

    encounter();scene->enqueueBattleLog("first");scene->enqueueBattleLog("second");scene->enqueueBattleLog("third");
    assert(scene->battleLogVisibleCount==1 && scene->battleLogCount==2);advance(1000);advance(1000);
    assert(scene->battleLogVisibleCount==2 && std::strcmp(scene->battleLogVisible[0],"second")==0 && std::strcmp(scene->battleLogVisible[1],"third")==0);
    advance(1000);assert(scene->battleLogVisibleCount==0 && !scene->battleLogBusy());++checks;

    encounter();scene->phase=Phase::FRIENDSHIP;scene->friendshipStep=Friend::CONTACT_CONFIRM;
    UI_STATE(*scene).friendshipConfirmYes=true;scene->battleLogVisibleCount=2;
    assert(scene->onButton(longA) && UI_STATE(*scene).friendshipConfirmYes);
    assert(scene->onButton(b) && !UI_STATE(*scene).friendshipConfirmYes);
    assert(scene->onButton(a) && scene->phase==Phase::WALKING && state.storageCount==0);++checks;

    encounter();scene->phase=Phase::FRIENDSHIP;scene->friendshipStep=Friend::CONTACT_INTRO;
    scene->battleLogVisibleCount=2;scene->battleLogActive=true;scene->battleLogUntil=desktop.millis()+100;
    advance(100);assert(scene->friendshipStep==Friend::CONTACT_CONFIRM && scene->battleLogVisibleCount==2 && UI_STATE(*scene).friendshipConfirmYes);++checks;

    encounter();UI_STATE(*scene).battleCursor=1;assert(scene->onButton(a) && scene->exploreSubViewOpen);
    assert(scene->exploreSubView.viewMode==MenuScene::ViewMode::BAG);assert(scene->onButton(longB) && !scene->exploreSubViewOpen);++checks;

    encounter();UI_STATE(*scene).battleCursor=2;assert(scene->onButton(a));
    assert(scene->battleSwitchStage==Switch::RETREATING && scene->pendingBattleSwitchSlot==1);
    advance(360);assert(scene->battlePlayerSlot==1 && scene->battleSwitchStage==Switch::ENTERING);
    advance(360);assert(scene->battleSwitchStage==Switch::NONE && scene->battleLogBusy());++checks;

    encounter(25,1,false);UI_STATE(*scene).battleCursor=2;assert(scene->onButton(a));
    assert(scene->battleSwitchStage==Switch::NONE && scene->battleLogActive);
    assert(std::strcmp(scene->battleLogVisible[0],Ui::Explore::NO_SWITCH_TARGET)==0);++checks;

    encounter();state.tutorialFlags=0;assert(scene->onButton(a));
    assert(engine.tutorialComplete(Game::TutorialStep::BATTLE_ACTION));
    assert(scene->battleTurnStage!=Turn::IDLE || scene->battleLogBusy());++checks;

    encounter();const auto foodStock=state.room.food[0];scene->throwFood(0);
    assert(scene->foodThrowActive && scene->battleLogBusy() && state.room.food[0]==foodStock);
    advance(699);assert(scene->foodThrowActive);advance(1);assert(!scene->foodThrowActive);++checks;

    encounter();scene->phase=Phase::LEVEL_UP;state.pendingLevelUp=true;state.pendingLevelUpLevel=36;
    assert(scene->onButton(longA) && state.pendingLevelUp);assert(scene->onButton(b) && state.pendingLevelUp);
    assert(scene->onButton(a) && !state.pendingLevelUp);++checks;

    encounter();engine.queueMoveReplacementEvent(0,33,73);scene->phase=Phase::MOVE_REPLACED;
    assert(scene->onButton(b) && engine.hasPendingMoveReplacement());assert(scene->onButton(a) && !engine.hasPendingMoveReplacement());++checks;

    auto shownExp=[&](uint32_t now) {
#if STICKMON_EXPLORE_UI_BASELINE
        return scene->battleExpForRender(now);
#else
        return Ui::ExploreBattleUiContent::battleExpForRender(scene->exploreBattleFrame(),now);
#endif
    };
    auto shownHp=[&](bool wild,uint16_t hp,uint32_t now) {
#if STICKMON_EXPLORE_UI_BASELINE
        return scene->battleHpForRender(wild,hp,now);
#else
        return Ui::ExploreBattleUiContent::battleHpForRender(scene->exploreBattleFrame(),wild,hp,now);
#endif
    };
    auto hitShake=[&](bool wild,uint32_t now) {
#if STICKMON_EXPLORE_UI_BASELINE
        return scene->battleHitShakeX(wild,now);
#else
        return Ui::ExploreBattleUiContent::battleHitShakeX(scene->exploreBattleFrame(),wild,now);
#endif
    };
    encounter();scene->prepareExpAnimation(100,1000);assert(shownExp(desktop.millis())==100);
    scene->startExpAnimation(desktop.millis());const auto started=desktop.millis();
    assert(shownExp(started+450)==550);advance(900);assert(!scene->expAnimationActive);++checks;

    encounter();scene->battleTurnStage=Turn::ANIMATING;scene->battleHpFrom=100;scene->battleHpTo=10;
    scene->battleActionAttackerWild=false;scene->battleActionStarted=desktop.millis();
    assert(shownHp(true,10,desktop.millis()+540)==100);
    assert(shownHp(true,10,desktop.millis()+750)==55);
    assert(shownHp(true,10,desktop.millis()+960)==10);
    assert(hitShake(true,desktop.millis()+260)==-3);++checks;

    encounter();{const auto d=advance(20);assert(!d.redraw && d.nextUpdateDelayMs==SceneUpdateResult::NO_UPDATE);}
    state.team[0].majorStatus=Game::MajorStatus::POISON;
    {const auto d=advance(20);assert(d.redraw && d.nextUpdateDelayMs==66);}++checks;

    walking();scene->steps=20;scene->beginExploreEnding();const auto bond=state.team[0].bond;
    assert(scene->adventureBondSettled && scene->phase==Phase::ENDING);scene->beginExploreEnding();
    assert(state.team[0].bond==bond);engine.currentId=SceneID::EXPLORE;assert(scene->onButton(a));
    assert(engine.exploreTravel==ExploreTravelPhase::RETURNING && engine.sceneFadeActive());++checks;

    setup();const auto beforeTouch=state;
    for(int x : {10,100,Hal::DISPLAY_W-10}) {
        assert(scene->touchTarget(x,20)==-1);
#if !STICKMON_UI_COMPACT_TOUCH || STICKMON_EXPLORE_UI_BASELINE
        assert(!scene->onTap(0));
#endif
        assert(!scene->onTouch({Touch::DOWN,int16_t(x),20,desktop.millis()}));
        scene->cancelTouch();assert(!scene->onTouch({Touch::UP,int16_t(x),20,desktop.millis()+20}));
    }sameState(beforeTouch);++checks;

    walking();const auto mapSeed=scene->generatedMap.seed;const auto beforeRender=state;
    scene->render();scene->render();sameState(beforeRender);assert(scene->generatedMap.seed==mapSeed);
    scene->onExit();++checks;

    setup();UI_STATE(*scene).areaCursor=6;assert(scene->onButton(a));assert(engine.currentId==SceneID::MENU);++checks;

#if !STICKMON_EXPLORE_UI_BASELINE
#if STICKMON_UI_COMPACT_TOUCH
    auto touch=[&](Touch type,int x,int y,uint32_t elapsed=20) {
        desktop.advanceMs(elapsed);
        return scene->onTouch({type,int16_t(x),int16_t(y),desktop.millis()});
    };
    auto tap=[&](int x,int y){touch(Touch::DOWN,x,y);return touch(Touch::UP,x,y);};
    auto swipe=[&](int fromY,int toY) {
        touch(Touch::DOWN,35,fromY);touch(Touch::MOVE,35,toY);
        return touch(Touch::UP,35,toY);
    };
    auto settle=[&](){for(int i=0;i<12;++i)advance(66);};
    auto pixels=[&](const char* name){
        PixelRenderer::clear(0);scene->render();
        const auto fb=desktop.frameBuffer();
        std::vector<uint16_t> result(fb.pixels,fb.pixels+fb.width*fb.height);
        FILE* file=std::fopen((std::string(argv[2])+"/"+name+".trace565").c_str(),"wb");
        assert(file && std::fwrite(result.data(),sizeof(uint16_t),result.size(),file)==result.size());
        std::fclose(file);return result;
    };
    setup();scene->areaPreviewPool.count=0;const auto beforeSwipe=state;
    const auto beforeDrag=pixels("drag-start");
    touch(Touch::DOWN,35,170);assert(touch(Touch::MOVE,35,158));
    assert(std::fabs(UI_STATE(*scene).areaAnimCursor-12.0f/34.0f)<0.00001f);
    assert(UI_STATE(*scene).areaCursor==0);
    const auto duringDrag=pixels("drag-up12");
    const auto gold=PixelRenderer::rgb(255,216,72);
    for(int y=102;y<114;++y)for(int x=5;x<8;++x){
        assert(beforeDrag[(y+12)*Hal::DISPLAY_W+x]==gold);
        assert(duringDrag[y*Hal::DISPLAY_W+x]==gold);
    }
    for(int y=0;y<Hal::DISPLAY_H;++y)for(int x=70;x<Hal::DISPLAY_W;++x)
        assert(beforeDrag[y*Hal::DISPLAY_W+x]==duringDrag[y*Hal::DISPLAY_W+x]);
    // Update never eases an active drag back, and a stationary hold can park.
    advance(66);const auto held=advance(66);
    assert(!held.redraw && held.nextUpdateDelayMs==SceneUpdateResult::NO_UPDATE);
    assert(std::fabs(UI_STATE(*scene).areaAnimCursor-12.0f/34.0f)<0.00001f);
    desktop.advanceMs(1500);assert(touch(Touch::MOVE,35,146));
    assert(std::fabs(UI_STATE(*scene).areaAnimCursor-24.0f/34.0f)<0.00001f);
    assert(UI_STATE(*scene).areaCursor==0);
    assert(touch(Touch::UP,35,146) && UI_STATE(*scene).areaCursor==1);
    assert(std::fabs(UI_STATE(*scene).areaAnimCursor-24.0f/34.0f)<0.00001f);
    advance(66);assert(UI_STATE(*scene).areaAnimCursor>24.0f/34.0f);
    settle();assert(UI_STATE(*scene).areaAnimCursor==1);
    pixels("drag-snapped");sameState(beforeSwipe);
    assert(engine.exploreTravel==ExploreTravelPhase::NONE && !engine.sceneFadeActive());
    assert(!touch(Touch::UP,35,146) && UI_STATE(*scene).areaCursor==1);
    // Short drags snap back without becoming taps; reverse motion follows immediately.
    setup();assert(swipe(170,158) && UI_STATE(*scene).areaCursor==0);
    settle();assert(UI_STATE(*scene).areaAnimCursor==0 && !engine.sceneFadeActive());
    touch(Touch::DOWN,35,170);touch(Touch::MOVE,35,100);
    assert(std::fabs(UI_STATE(*scene).areaAnimCursor-70.0f/34.0f)<0.00001f);
    touch(Touch::MOVE,35,146);
    assert(std::fabs(UI_STATE(*scene).areaAnimCursor-24.0f/34.0f)<0.00001f);
    assert(touch(Touch::UP,35,146) && UI_STATE(*scene).areaCursor==1);
    // A new touch catches the current fractional position without jumping.
    advance(66);const float caught=UI_STATE(*scene).areaAnimCursor;
    assert(caught<1);touch(Touch::DOWN,35,170);advance(66);
    assert(UI_STATE(*scene).areaAnimCursor==caught);
    touch(Touch::MOVE,35,158);
    assert(std::fabs(UI_STATE(*scene).areaAnimCursor-caught-12.0f/34.0f)<0.00001f);
    scene->cancelTouch();assert(UI_STATE(*scene).areaAnimCursor==caught);
    // A timed-out tap or a cancelled touch must resume an interrupted snap animation.
    for(auto end:{Touch::UP,Touch::CANCEL}){
        setup();UI_STATE(*scene).areaCursor=1;UI_STATE(*scene).areaAnimCursor=.5f;
        touch(Touch::DOWN,35,137);advance(66);
        assert(UI_STATE(*scene).areaAnimCursor==.5f);
        assert(touch(end,35,137,1001));settle();
        assert(UI_STATE(*scene).areaCursor==1 && UI_STATE(*scene).areaAnimCursor==1);
        assert(!engine.sceneFadeActive());
    }
    // Long drags cross several rows, clamp at each end, and never execute Back.
    setup();const auto beforeLongSwipe=state;
    assert(swipe(210,40) && UI_STATE(*scene).areaCursor==5);settle();
    assert(swipe(170,100) && UI_STATE(*scene).areaCursor==6);settle();
    assert(swipe(170,100) && UI_STATE(*scene).areaCursor==6);
    assert(engine.currentId==SceneID::MAIN && !engine.sceneFadeActive());
    assert(swipe(40,210) && UI_STATE(*scene).areaCursor==1);settle();
    assert(swipe(40,210) && UI_STATE(*scene).areaCursor==0);settle();
    assert(swipe(80,150) && UI_STATE(*scene).areaCursor==0);sameState(beforeLongSwipe);
    // A drag may start between rows; the right preview and cross-column gestures are isolated.
    setup();assert(swipe(137,100) && UI_STATE(*scene).areaCursor==1);
    setup();touch(Touch::DOWN,100,170);touch(Touch::MOVE,35,100);touch(Touch::UP,35,100);
    assert(UI_STATE(*scene).areaCursor==0 && UI_STATE(*scene).areaAnimCursor==0);
    touch(Touch::DOWN,35,170);touch(Touch::MOVE,35,140);touch(Touch::MOVE,75,120);
    touch(Touch::UP,35,100);
    assert(UI_STATE(*scene).areaCursor==0 && UI_STATE(*scene).areaAnimCursor==0);
    touch(Touch::DOWN,35,170);touch(Touch::MOVE,60,170);touch(Touch::UP,35,100);
    assert(UI_STATE(*scene).areaCursor==0);
    touch(Touch::DOWN,35,170);touch(Touch::MOVE,35,100);touch(Touch::MOVE,35,170);
    touch(Touch::UP,35,170);assert(UI_STATE(*scene).areaCursor==0 && !engine.sceneFadeActive());
    // Cancel, physical input, exit and phase changes restore the anchor without committing.
    for(int cancel=0;cancel<5;++cancel){
        setup();touch(Touch::DOWN,35,170);touch(Touch::MOVE,35,100);
        if(cancel==0)scene->cancelTouch();
        if(cancel==1)touch(Touch::CANCEL,35,100);
        if(cancel==2)scene->onButton(b);
        if(cancel==3)scene->onExit();
        if(cancel==4)scene->phase=Phase::WALKING;
        touch(Touch::UP,35,100);
        assert(UI_STATE(*scene).areaCursor==(cancel==2?1:0));
        assert(UI_STATE(*scene).areaAnimCursor==0);
        assert(engine.exploreTravel==ExploreTravelPhase::NONE);
    }
    // Only the unlocked area, its visible locked neighbour, and Back can be selected.
    setup(1,true,0);const auto beforeLockedSwipe=state;
    assert(swipe(170,136) && UI_STATE(*scene).areaCursor==1);settle();
    assert(swipe(170,100) && UI_STATE(*scene).areaCursor==6);settle();
    assert(swipe(170,100) && UI_STATE(*scene).areaCursor==6);
    assert(swipe(80,114) && UI_STATE(*scene).areaCursor==1);
    assert(engine.exploreTravel==ExploreTravelPhase::NONE);sameState(beforeLockedSwipe);
    setup();const auto beforeSelection=state;
    assert(scene->touchTarget(35,120)==0 && scene->touchTarget(35,154)==1);
    for(int x:{-1,70,100,295,296})assert(scene->touchTarget(x,154)==-1);
    for(int y:{-1,137,138,239,240})assert(scene->touchTarget(35,y)==-1);
    assert(!scene->onTap(-1) && !scene->onTap(7));sameState(beforeSelection);
    assert(!touch(Touch::DOWN,35,154) && UI_STATE(*scene).areaCursor==0);
    assert(touch(Touch::UP,35,154) && UI_STATE(*scene).areaCursor==1);
    assert(engine.exploreTravel==ExploreTravelPhase::NONE && !engine.sceneFadeActive());
    sameState(beforeSelection);
    assert(!touch(Touch::UP,35,154));sameState(beforeSelection);
    // Hit testing follows the same rounded interpolation as the rendered list.
    UI_STATE(*scene).areaAnimCursor=.5f;
    assert(scene->touchTarget(35,137)==1 && scene->touchTarget(35,120)==-1);
    UI_STATE(*scene).areaAnimCursor=1;
    touch(Touch::DOWN,35,154);touch(Touch::MOVE,60,154);
    assert(!touch(Touch::UP,35,154) && UI_STATE(*scene).areaCursor==1);
    touch(Touch::DOWN,35,154);touch(Touch::CANCEL,35,154);assert(!touch(Touch::UP,35,154));
    touch(Touch::DOWN,35,154);scene->cancelTouch();assert(!touch(Touch::UP,35,154));
    touch(Touch::DOWN,35,154);assert(!touch(Touch::UP,35,154,1001));
    touch(Touch::DOWN,35,154);assert(touch(Touch::UP,35,166));
    settle();assert(UI_STATE(*scene).areaAnimCursor==1);
    assert(UI_STATE(*scene).areaCursor==1);sameState(beforeSelection);
    touch(Touch::DOWN,35,154);scene->onButton(b);assert(UI_STATE(*scene).areaCursor==2);
    assert(!touch(Touch::UP,35,154) && UI_STATE(*scene).areaCursor==2);
    touch(Touch::DOWN,35,120);scene->onExit();assert(!touch(Touch::UP,35,120));
    sameState(beforeSelection);
    // Every region is reachable, including the visible locked region; previewing never departs.
    for(int index=0;index<6;++index){
        setup();UI_STATE(*scene).areaCursor=index==0?1:0;UI_STATE(*scene).areaAnimCursor=index;
        assert(scene->touchTarget(35,120)==index && tap(35,120));
        assert(UI_STATE(*scene).areaCursor==index && engine.exploreTravel==ExploreTravelPhase::NONE);
        preview(*scene);assert(scene->areaPreviewPool.count>0);
    }
    setup(1,true,0);const auto beforeLocked=state;
    assert(tap(35,154) && UI_STATE(*scene).areaCursor==1);
    UI_STATE(*scene).areaAnimCursor=1;assert(tap(35,120));
    assert(engine.exploreTravel==ExploreTravelPhase::NONE);sameState(beforeLocked);
    assert(!scene->onTap(2));
    // Phase changes and embedded menus cannot inherit an unfinished area tap.
    setup();touch(Touch::DOWN,35,120);scene->phase=Phase::WALKING;
    assert(!touch(Touch::UP,35,120) && engine.exploreTravel==ExploreTravelPhase::NONE);
    assert(!scene->autoWalkActive && !scene->routeMoving);
    for(auto phase:{Phase::ENCOUNTER,Phase::SPECIAL_PROMPT,Phase::ENDING}){
        scene->phase=phase;assert(scene->touchTarget(35,120)==-1 && !scene->onTap(0));
    }
    scene->phase=Phase::SELECT;scene->exploreSubViewOpen=true;
    assert(scene->touchTarget(35,120)==-1 && !scene->onTap(0));
    // Map taps reuse auto walking, without accepting drags, stale modal taps or blocked routes.
    walking();assert(!scene->autoWalkActive);assert(tap(150,120));
    assert(scene->autoWalkActive && scene->routeMoving);
    const auto routeStarted=scene->routeMoveStarted;
    assert(tap(150,120) && scene->routeMoveStarted==routeStarted);
    for(int cancel=0;cancel<7;++cancel){
        walking();touch(Touch::DOWN,150,120);
        if(cancel==0)scene->cancelTouch();
        if(cancel==1)touch(Touch::CANCEL,150,120);
        if(cancel==2)touch(Touch::MOVE,170,120);
        if(cancel==3)scene->onButton({0,BtnAction::DOWN});
        if(cancel==4)scene->phase=Phase::PICKUP;
        if(cancel==5)scene->exploreMenuOpen=true;
        assert(!touch(Touch::UP,150,120,cancel==6?1001:20));
        assert(!scene->autoWalkActive && !scene->routeMoving);
    }
    for(bool slide:{false,true}){
        walking();scene->iceSliding=slide;scene->pendingFrostFall=!slide;
        assert(scene->touchTarget(150,120)==-1 && !tap(150,120));
        assert(!scene->autoWalkActive);
    }
    // The room-aligned button opens the route menu only on a valid release.
    const auto menuButton=Ui::HudControls::menuBounds();
    assert(menuButton.x==236 && menuButton.y==192 && menuButton.width==36 && menuButton.height==32);
    const int menuX=menuButton.x+menuButton.width/2,menuY=menuButton.y+menuButton.height/2;
    walking();UI_STATE(*scene).exploreMenuCursor=3;
    assert(!touch(Touch::DOWN,menuX,menuY) && !scene->exploreMenuOpen);
    assert(touch(Touch::UP,menuX,menuY) && scene->exploreMenuOpen);
    assert(UI_STATE(*scene).exploreMenuCursor==0 && !scene->autoWalkActive && !scene->routeMoving);
    assert(!touch(Touch::UP,menuX,menuY) && !scene->exploreSubViewOpen);
    for(int cancel=0;cancel<8;++cancel){
        walking();touch(Touch::DOWN,menuX,menuY);
        if(cancel==0)scene->cancelTouch();
        if(cancel==1)touch(Touch::CANCEL,menuX,menuY);
        if(cancel==2)touch(Touch::MOVE,menuX-20,menuY);
        if(cancel==3)scene->onButton({0,BtnAction::DOWN});
        if(cancel==4)scene->phase=Phase::PICKUP;
        if(cancel==5)scene->iceSliding=true;
        if(cancel==6)scene->pendingFrostFall=true;
        assert(!touch(Touch::UP,menuX,menuY,cancel==7?1001:20));
        assert(!scene->exploreMenuOpen && !scene->autoWalkActive);
    }
    const auto menuPanel=Ui::ExploreHudUi::menuBounds();
    assert(menuPanel.height<180 && menuPanel.x>=28 && menuPanel.y>=34);
    assert(menuPanel.y+menuPanel.height<=menuButton.y);
    auto menuTap=[&](int index){const auto row=Ui::ExploreHudUi::menuItem(index);
        return tap(row.x+row.width/2,row.y+row.height/2);};
    // Every item keeps the existing navigation; outside taps and row gaps block walking.
    walking();assert(tap(menuX,menuY));
    assert(!tap(150,120) && !tap(265,15) && !tap(menuX,menuY) && !scene->autoWalkActive);
    const auto firstRow=Ui::ExploreHudUi::menuItem(0);
    assert(!tap(firstRow.x+10,firstRow.y+firstRow.height+1));
    assert(menuTap(0) && scene->exploreSubViewOpen);
    assert(tap(48,24) && !scene->exploreSubViewOpen && scene->exploreMenuOpen);
    assert(menuTap(3) && !scene->exploreMenuOpen && !scene->autoWalkActive && !scene->routeMoving);
    const auto returnedSteps=scene->steps;
    const auto returnRow=Ui::ExploreHudUi::menuItem(3);
    assert(!touch(Touch::UP,returnRow.x+10,returnRow.y+10) && scene->steps==returnedSteps);
    assert(tap(menuX,menuY) && menuTap(1) && scene->exploreSubViewOpen);
    assert(tap(48,24) && !scene->exploreSubViewOpen && scene->exploreMenuOpen);
    assert(menuTap(3) && tap(150,120) && scene->routeMoving);
    const auto movingStarted=scene->routeMoveStarted;
    assert(tap(menuX,menuY));const auto openedAt=scene->exploreMenuOpenedAt;
    assert(menuTap(3));
    assert(scene->routeMoveStarted==movingStarted+desktop.millis()-openedAt);
    assert(!scene->autoWalkActive && scene->routePausedForTouch);
    const auto pausedX=scene->routeWorldX,pausedY=scene->routeWorldY;
    const auto pausedFollowerX=scene->routeFollowerWorldX,pausedFollowerY=scene->routeFollowerWorldY;
    const auto pausedSteps=scene->steps;
    const auto pausedStarted=scene->routeMoveStarted;
    advance(2000);
    assert(scene->routeWorldX==pausedX && scene->routeWorldY==pausedY && scene->steps==pausedSteps);
    assert(scene->routeFollowerWorldX==pausedFollowerX && scene->routeFollowerWorldY==pausedFollowerY);
    assert(!scene->exploreRouteFrame().routeMoving && !scene->exploreRouteFrame().routeFollowerMoving);
    // Reopening and returning again must not double-count menu pause time.
    assert(tap(menuX,menuY) && menuTap(3));
    const auto totalPaused=desktop.millis()-scene->routePausedAt;
    assert(tap(150,120) && scene->autoWalkActive && !scene->routePausedForTouch);
    assert(scene->steps==pausedSteps && scene->routeMoveStarted==pausedStarted+totalPaused+40);
    advance(80);
    assert(scene->routeWorldX!=pausedX || scene->routeWorldY!=pausedY);
    // Returning keeps an interrupted single step frozen until the next map tap.
    walking();scene->walk();const auto singleStep=scene->steps;
    assert(!scene->autoWalkActive && tap(menuX,menuY) && menuTap(3));
    assert(!scene->autoWalkActive && scene->routePausedForTouch && scene->steps==singleStep);
    const auto singleX=scene->routeWorldX,singleY=scene->routeWorldY;
    advance(2000);
    assert(scene->routeWorldX==singleX && scene->routeWorldY==singleY && scene->steps==singleStep);
    touch(Touch::DOWN,150,120);touch(Touch::CANCEL,150,120);
    assert(!touch(Touch::UP,150,120) && scene->routePausedForTouch && !scene->autoWalkActive);
    assert(tap(150,120) && scene->autoWalkActive && scene->routeMoving && scene->steps==singleStep);
    advance(80);
    assert(scene->routeWorldX!=singleX || scene->routeWorldY!=singleY);
    for(int cancel=0;cancel<4;++cancel){
        walking();assert(tap(menuX,menuY));
        touch(Touch::DOWN,returnRow.x+10,returnRow.y+10);
        if(cancel==0)scene->cancelTouch();
        if(cancel==1)touch(Touch::MOVE,returnRow.x-20,returnRow.y+10);
        if(cancel==2)touch(Touch::CANCEL,returnRow.x+10,returnRow.y+10);
        assert(!touch(Touch::UP,returnRow.x+10,returnRow.y+10,cancel==3?1001:20));
        assert(scene->exploreMenuOpen && !scene->autoWalkActive && !scene->routeMoving);
    }
    walking();assert(tap(menuX,menuY) && menuTap(2));
    assert(engine.exploreTravel==ExploreTravelPhase::RETURNING);

    // Exercise actual prepared/committed actions and all four shared AMOLED FX assets.
    const Ui::ExploreBattleLayout fxLayout(296,240,28,true,10,true);
    auto renderedBattle=[&](const Ui::ExploreBattleFrame& frame){
        PixelRenderer::clear(0);Ui::ExploreBattleUiContent::drawEncounter(frame,fxLayout);
        const auto fb=desktop.frameBuffer();
        return std::vector<uint16_t>(fb.pixels,fb.pixels+fb.width*fb.height);
    };
    static constexpr const char* fxNames[]={"hit","fire","water","electric","scratch","bite","leer","growl","sand"};
    static constexpr Game::MoveId fxMoves[]={33,52,55,84,10,44,43,45,28};
    for(int fx=0;fx<9;++fx){
        const auto kind=static_cast<GameAssets::BattleFx>(fx);
        assert(GameAssets::battleFxForMove(fxMoves[fx],findMove(fxMoves[fx])->type)==kind);
        for(uint8_t f=0;f<4;++f){
            PixelRenderer::clear(0xf81f);
            assert(GameAssets::drawCentered(GameAssets::battleFxKind(kind,f),148,100));
            const auto fb=desktop.frameBuffer();
            assert(std::count(fb.pixels,fb.pixels+fb.width*fb.height,uint16_t(0xf81f))<fb.width*fb.height);
        }
        for(bool wildAttacker:{false,true}){
            encounter(4,1);
            auto& attacker=wildAttacker?scene->wildRuntime:state.team[0];
            attacker.move2Id=fxMoves[fx];
            scene->battleTurnPlan.count=1;
            scene->battleTurnPlan.actions[0]={wildAttacker?BattleTurnController::Side::WILD:
                BattleTurnController::Side::PLAYER,0,fxMoves[fx]};
            scene->battleActionIndex=0;scene->beginBattleAction();
            auto frame=scene->exploreBattleFrame();
            assert(frame.battleMoveId==fxMoves[fx]);
            const bool status=GameAssets::battleFxIsStatus(kind);
            assert(status?frame.battleDefenderStatChanged:frame.battleTurnAnimating);
            const auto actionState=state;
            for(unsigned elapsed:{80U,160U,240U,320U}){
                scene->battleActionStarted=desktop.millis()-elapsed;
                capture(std::string("battle-fx-")+fxNames[fx]+(wildAttacker?"-wild-":"-player-")+std::to_string(elapsed));
                frame=scene->exploreBattleFrame();
                const auto shown=renderedBattle(frame);
                auto noFx=frame;noFx.battleHpTo=noFx.battleHpFrom;
                noFx.battleDefenderStatChanged=false;
                assert(shown!=renderedBattle(noFx));
                assert(shown==renderedBattle(frame));sameState(actionState);
            }
            frame.nowMs=frame.battleActionStarted+480;
            auto expired=frame;expired.battleTurnAnimating=false;expired.battleDefenderStatChanged=false;
            assert(renderedBattle(frame)==renderedBattle(expired));
            assert(!tap(118,214) && !scene->exploreSubViewOpen);
            scene->battleActionStarted=desktop.millis();
            scene->updateBattleTurn(desktop.millis()+960);
            assert(scene->battlePendingAction.committed);
            const auto committed=state;scene->applyBattleDamage();sameState(committed);
        }
    }
    // A capped stat, a miss, a failed move or an immune target must not produce status particles.
    for(int blocked=0;blocked<4;++blocked){
        encounter(4,1);state.team[0].move2Id=43;
        if(blocked==0)scene->wildBattleState.statStages[static_cast<int>(BattleStat::DEFENSE)]=-6;
        scene->battleTurnPlan.count=1;
        scene->battleTurnPlan.actions[0]={BattleTurnController::Side::PLAYER,0,43};
        scene->beginBattleAction();
        if(blocked!=0)assert(scene->exploreBattleFrame().battleDefenderStatChanged);
        if(blocked==1)scene->battlePendingAction.damage.missed=true;
        if(blocked==2)scene->battlePendingAction.damage.failed=true;
        if(blocked==3)scene->battlePendingAction.damage.effectiveness=0;
        const auto frame=scene->exploreBattleFrame();
        assert(!frame.battleDefenderStatChanged);
    }

    // HUD values follow real team slots, including a fainted first member.
    walking(0,1,false);auto hud=scene->exploreRouteFrame();
    assert(hud.teamHudCount==1 && hud.teamHud[0].hpCur==state.team[0].hpCur);
    capture("hud-single");
    walking();state.team[0].hpMax=100;state.team[0].hpCur=50;
    state.team[0].majorStatus=Game::MajorStatus::POISON;
    state.team[1].hpMax=100;state.team[1].hpCur=20;
    state.team[1].majorStatus=Game::MajorStatus::PARALYSIS;
    hud=scene->exploreRouteFrame();assert(hud.teamHudCount==2);
    assert(hud.teamHud[0].hpCur==50 && hud.teamHud[1].hpCur==20);
    assert(hud.teamHud[0].status==Game::MajorStatus::POISON && hud.teamHud[1].status==Game::MajorStatus::PARALYSIS);
    capture("hud-pair-status");
    state.team[0].hpCur=0;state.team[0].fainted=true;
    state.team[0].majorStatus=Game::MajorStatus::NONE;
    state.team[1].hpCur=60;state.team[1].hpMax=0;
    hud=scene->exploreRouteFrame();assert(hud.teamHud[0].fainted && hud.teamHud[0].hpCur==0);
    assert(hud.teamHud[1].hpMax==0);capture("hud-fainted-zero-max");
    for(auto status:{Game::MajorStatus::NONE,Game::MajorStatus::POISON,Game::MajorStatus::TOXIC,
        Game::MajorStatus::PARALYSIS,Game::MajorStatus::SLEEP,Game::MajorStatus::BURN,Game::MajorStatus::FREEZE}){
        walking();state.team[0].majorStatus=status;state.team[0].hpMax=100;
        state.team[0].hpCur=101;state.team[1].hpMax=100;state.team[1].hpCur=51;
        capture("hud-status-"+std::to_string(static_cast<int>(status)));
    }
    walking();assert(tap(menuX,menuY));capture("hud-touch-menu");
    // Centered modals block the map; each choice resolves once on release.
    const Ui::ExploreRouteLayout routeLayout{296,240,true};
    const Ui::ExploreBattleLayout battleLayout{296,240,28,true,10};
    assert(battleLayout.footerY==194 && battleLayout.footerH==46);
    assert(battleLayout.playerGroundY==battleLayout.footerY);
    assert(battleLayout.playerNameY==161 && battleLayout.expBarY==180);
    for(int command=0;command<4;++command){
        const auto target=battleLayout.commandTarget(command);
        assert(target.y==194 && target.height==46);
        encounter();
        assert(scene->touchTarget(target.x+target.width/2,193)==-1);
        assert(scene->touchTarget(target.x+target.width/2,194)>=0);
        assert(tap(target.x+target.width/2,199));
    }
    for(const auto panel:{routeLayout.specialPrompt(),routeLayout.pickupPrompt(),
                         routeLayout.endPrompt(),routeLayout.resultPrompt(),battleLayout.friendshipPrompt()}){
        assert(std::abs(panel.x*2+panel.width-296)<=1);
        assert(std::abs(panel.y*2+panel.height-240)<=1);
    }
    for(bool yes:{false,true}){
        walking();scene->phase=Phase::SPECIAL_PROMPT;scene->routeBossPending=true;
        scene->currentMapBlock=scene->mapBlockCount-1;
        scene->expeditionBossSpeciesId=143;scene->expeditionBossLevel=15;
        assert(!tap(10,120) && scene->phase==Phase::SPECIAL_PROMPT && !scene->autoWalkActive);
        const auto panel=routeLayout.specialPrompt();
        assert(tap(panel.x+(yes?60:145),panel.y+68));
        assert(scene->phase==(yes?Phase::ENCOUNTER:Phase::WALKING) && !scene->routeBossPending);
        assert(!touch(Touch::UP,panel.x+60,panel.y+68));
    }
    walking();scene->phase=Phase::PICKUP;
    assert(!tap(10,120) && scene->phase==Phase::PICKUP);
    assert(tap(148,120) && scene->phase==Phase::WALKING);
    for(bool yes:{false,true}){
        encounter();scene->phase=Phase::FRIENDSHIP;scene->friendshipStep=Friend::CONTACT_CONFIRM;
        const auto panel=battleLayout.friendshipPrompt();
        assert(!tap(20,120) && scene->friendshipStep==Friend::CONTACT_CONFIRM);
        assert(tap(panel.x+30,panel.y+(yes?15:43)));
        assert(yes?scene->friendshipStep==Friend::CONTACT_ACQUIRED:scene->phase!=Phase::FRIENDSHIP);
    }
    // Four gathered command targets invoke the same existing actions as A.
    for(int command=0;command<4;++command){
        encounter(10,25);const auto cell=battleLayout.commandTarget(command);
        assert(scene->touchTarget(18,214)==-1 && scene->touchTarget(280,214)==-1);
        assert(scene->touchTarget(cell.x-1,214)==-1);
        assert(!tap(150,120) && !scene->battleLogBusy());
        assert(tap(cell.x+cell.width/2,214));
        if(command==0)assert(scene->battleTurnStage!=Turn::IDLE);
        if(command==1)assert(scene->exploreSubViewOpen && scene->exploreSubView.battleBagMode);
        if(command==2)assert(scene->battleSwitchStage==Switch::RETREATING && scene->pendingBattleSwitchSlot==1);
        if(command==3)assert(scene->fleeExitPending);
        const auto after=state;
        assert(!touch(Touch::UP,cell.x+cell.width/2,214));sameState(after);
    }
    // Logs, HP/EXP, throws, switches and defeat transitions never activate hidden commands.
    for(int busy=0;busy<10;++busy){
        encounter();
        if(busy==0)scene->enqueueBattleLog("busy");
        if(busy==1)scene->battleTurnStage=Turn::ANIMATING;
        if(busy==2)scene->battleSwitchStage=Switch::ENTERING;
        if(busy==3)scene->foodThrowActive=true;
        if(busy==4)scene->expAnimationPending=true;
        if(busy==5)scene->expAnimationActive=true;
        if(busy==6)scene->battleResultPending=true;
        if(busy==7)scene->fleeExitPending=true;
        if(busy==8)scene->battleLogVisibleCount=1;
        if(busy==9)scene->battleLogActive=true;
        const auto before=state;
        assert(scene->touchTarget(118,214)==-1 && !tap(118,214));
        assert(!scene->exploreSubViewOpen);sameState(before);
    }
    for(int cancel=0;cancel<6;++cancel){
        encounter();touch(Touch::DOWN,118,214);
        if(cancel==0)scene->phase=Phase::RESULT;
        if(cancel==1)scene->battleTurnStage=Turn::ANIMATING;
        if(cancel==2)scene->defeatAwaitInput=true;
        if(cancel==3)touch(Touch::MOVE,138,214);
        if(cancel==4)scene->cancelTouch();
        assert(!touch(Touch::UP,118,214,cancel==5?1001:20));
        assert(!scene->exploreSubViewOpen);
    }
    // Enter via the actual walking menu: the embedded scene owns raw gestures.
    walking();scene->onButton(b);scene->onButton(a);
    auto& submenu=scene->exploreSubView;
    const auto beforeSkills=state;
    assert(tap(80,85) && submenu.teamUi.teamActionOpen);
    assert(tap(210,140) && submenu.viewMode==MenuScene::ViewMode::MOVES);
    assert(scene->exploreSubViewOpen && submenu.movesUi.moveCursor==0);
    assert(tap(40,126) && submenu.movesUi.moveCursor==1);
    assert(tap(40,126) && submenu.movesUi.moveForgetConfirmOpen);
    assert(!submenu.movesUi.moveForgetConfirmYes);
    tap(40,76);assert(submenu.movesUi.moveForgetConfirmOpen); // Modal blocks list.
    assert(tap(193,132) && !submenu.movesUi.moveForgetConfirmOpen);
    sameState(beforeSkills);
    // Cancellation must reach child gesture trackers (lock/input change/exit).
    for(int cancel=0;cancel<4;++cancel){
        touch(Touch::DOWN,40,76);
        if(cancel==0)scene->cancelTouch();
        if(cancel==1)touch(Touch::CANCEL,40,76);
        if(cancel==2)scene->onButton({0,BtnAction::DOWN});
        if(cancel==3)scene->onExit();
        touch(Touch::UP,40,76);
        assert(submenu.movesUi.moveCursor==1 && !submenu.movesUi.moveForgetConfirmOpen);
    }
    touch(Touch::DOWN,200,196);touch(Touch::MOVE,40,126);touch(Touch::UP,40,126);
    assert(submenu.movesUi.moveCursor==1 && !submenu.movesUi.moveForgetConfirmOpen);
    sameState(beforeSkills);
    assert(tap(40,126) && submenu.movesUi.moveForgetConfirmOpen);
    assert(tap(123,132) && !submenu.movesUi.moveForgetConfirmOpen);
    assert(specialMoveIdForMonster(state.team[0],0)==0);
    assert(tap(48,24) && submenu.viewMode==MenuScene::ViewMode::TEAM);
    assert(tap(80,85) && tap(210,100) && submenu.viewMode==MenuScene::ViewMode::STATUS);
    touch(Touch::DOWN,240,130);touch(Touch::MOVE,100,130);touch(Touch::UP,100,130);
    assert(submenu.statusUi.statusPage==1);
    advance(400); // Finish page transition before using its header.
    assert(tap(48,24) && submenu.viewMode==MenuScene::ViewMode::TEAM);
    touch(Touch::DOWN,80,85);scene->onButton(longB);
    assert(!scene->exploreSubViewOpen);
    touch(Touch::UP,80,85);assert(!scene->exploreSubViewOpen);
    scene->onButton(a);assert(scene->exploreSubViewOpen && !submenu.teamUi.teamActionOpen);
    assert(tap(48,24) && !scene->exploreSubViewOpen && scene->exploreMenuOpen);
    // Walking bag stays open after use; battle results close and resolve once.
    auto openPotion=[&](){
        auto& bag=scene->exploreSubView.bagUi;
        const auto grid=bag.grid(state,scene->exploreSubView.battleBagMode);
        Features::Bag::Row rows[Features::Bag::ITEM_COUNT];
        const int count=Features::Bag::collect(state,scene->exploreSubView.battleBagMode,rows,Features::Bag::ITEM_COUNT);
        int index=0;while(index<count && Features::Bag::itemId(rows[index].source)!=Game::ItemId::POTION)++index;
        assert(index<count);
        bag.gridScroll=grid.reveal(index,bag.gridScroll);
        const auto cell=grid.targetCell(index,bag.gridScroll);
        assert(tap(cell.x+cell.width/2,cell.y+cell.height/2) && bag.detailOpen);
        const auto action=ItemTouchUi::action();
        assert(tap(action.x+action.width/2,action.y+action.height/2) && bag.confirmOpen);
    };
    walking();state.bag.potion=2;state.team[0].hpCur=1;
    scene->onButton(b);scene->onButton(b);scene->onButton(a);openPotion();
    assert(tap(130,90) && scene->exploreSubViewOpen && state.bag.potion==1 && state.team[0].hpCur>1);
    assert(tap(48,24) && !scene->exploreSubView.bagUi.detailOpen);
    assert(tap(48,24) && !scene->exploreSubViewOpen);
    encounter();state.bag.potion=2;state.team[0].hpCur=1;
    UI_STATE(*scene).battleCursor=1;scene->onButton(a);openPotion();
    assert(tap(130,90) && !scene->exploreSubViewOpen && state.bag.potion==1);
    assert(scene->battleLogCount+scene->battleLogVisibleCount>0 && scene->battleTurnStage!=Turn::IDLE);
    const auto afterPotion=state;const auto logCount=scene->battleLogCount;
    touch(Touch::UP,130,90);sameState(afterPotion);assert(scene->battleLogCount==logCount);
    encounter();state.room.food[0]=2;
    UI_STATE(*scene).battleCursor=1;scene->onButton(a);
    auto& foodBag=scene->exploreSubView.bagUi;
    const auto foodGrid=foodBag.grid(state,true);
    Features::Bag::Row foodRows[Features::Bag::ITEM_COUNT];
    const int foodCount=Features::Bag::collect(state,true,foodRows,Features::Bag::ITEM_COUNT);
    int foodRow=0;while(foodRow<foodCount && foodRows[foodRow].source!=Features::Bag::SOURCE_FOOD_BASE)++foodRow;
    assert(foodRow<foodCount);
    foodBag.gridScroll=foodGrid.reveal(foodRow,0);
    const auto foodCell=foodGrid.targetCell(foodRow,foodBag.gridScroll);
    assert(tap(foodCell.x+foodCell.width/2,foodCell.y+foodCell.height/2));
    const auto useFood=ItemTouchUi::action();
    assert(tap(useFood.x+useFood.width/2,useFood.y+useFood.height/2));
    assert(tap(130,90) && !scene->exploreSubViewOpen && scene->foodThrowActive && state.room.food[0]==1);
    const auto afterFood=state;touch(Touch::UP,130,90);sameState(afterFood);
    // Exercise synchronous destruction through the real engine for depart and Back.
    setup();scene.reset();engine.requestScene(SceneID::EXPLORE);
    assert(!engine.currentScene->onTouch({Touch::DOWN,35,120,desktop.millis()}));
    assert(engine.currentScene->onTouch({Touch::UP,35,120,desktop.millis()+20}));
    assert(engine.currentId==SceneID::MAIN && engine.exploreTravel==ExploreTravelPhase::DEPARTING);
    setup(1,true,0);scene.reset();engine.requestScene(SceneID::EXPLORE);
    auto* owned=static_cast<ExploreScene*>(engine.currentScene.get());
    UI_STATE(*owned).areaAnimCursor=1; // Back is the third displayed row when only one area is unlocked.
    assert(owned->touchTarget(35,154)==6);
    assert(!owned->onTouch({Touch::DOWN,35,154,desktop.millis()}));
    assert(owned->onTouch({Touch::UP,35,154,desktop.millis()+20}));
    assert(engine.currentId==SceneID::MENU);
    // End requests its existing fade; debug return/defeat can destroy the scene immediately.
    setup();scene.reset();engine.requestScene(SceneID::EXPLORE);
    owned=static_cast<ExploreScene*>(engine.currentScene.get());owned->phase=Phase::ENDING;
    owned->onTouch({Touch::DOWN,148,120,desktop.millis()});
    assert(owned->onTouch({Touch::UP,148,120,desktop.millis()+20}));
    assert(engine.exploreTravel==ExploreTravelPhase::RETURNING && engine.sceneFadeActive());
#if STICKMON_ENABLE_DEBUG_FEATURES
    for(bool result:{false,true}){
        setup();scene.reset();engine.beginDebugBattle();
        owned=static_cast<ExploreScene*>(engine.currentScene.get());owned->clearBattleLogs();
        if(result)owned->phase=Phase::RESULT;
        else owned->defeatAwaitInput=true;
        owned->onTouch({Touch::DOWN,148,120,desktop.millis()});
        assert(owned->onTouch({Touch::UP,148,120,desktop.millis()+20}));
        assert(engine.currentId==SceneID::MENU);
    }
#endif
#endif
    ownedDeparture();ownedDebugFlee();
#endif
    engine.currentScene.reset();scene.reset();
    std::printf("explore-checkpoints=%u\n",checks);
}
