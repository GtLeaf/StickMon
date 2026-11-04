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
        engine.currentId = engine.prevId = SceneID::MAIN;
        engine.sceneFade = decltype(engine.sceneFade)::NONE;
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
        scene.homeUi.toast=nullptr; scene.homeUi.toastUntil=0;
        power.level=75; Hal::ins().batteryFiltered=-1;
        scene.updateCamera();
        engine.saveCoordinator.reset(desktop.millis());
        ProgressionUi::resetEvolutionAnimation();
        return scene;
    };

    // Run the real sleep -> seek -> feed -> sleep journey, not just intent rules.
    for (int scenario = 0; scenario < 4; ++scenario) {
        auto& scene = setup(1, true);
        state.gameMinutesTotal = 60;
        engine.gameClock.set(desktop.millis(), state.gameMinutesTotal);
        state.team[0].satiety = 100;
        state.team[1].satiety = scenario == 2 ? 60 : 10;
        if (scenario == 3) state.team[1].majorStatus = Game::MajorStatus::SLEEP;
        state.room.bowlCount = scenario == 1 ? 0 : 5;
        state.room.bowlBitesRemaining = scenario == 1 ? 0 : 3;
        const auto start = desktop.millis();
        scene.enterResting(start);
        assert(scene.pickVisitorSleepSpot(scene.visitor.sleepX, scene.visitor.sleepY));
        auto& v = scene.visitor;
        v.sleepSpotValid = true;
        v.x = v.targetX = v.sleepX;
        v.y = v.targetY = v.sleepY;
        scene.homeCoordinator.transition(1, Home::Task::SLEEPING, start, 0, true);
        v.foodWakeRetryAfterMs = 0;
        scene.nextAttentionMs = scene.nextSpecialActionMs = scene.nextPairInteractionMs = start + 1000000;
        bool ate = false;
        for (unsigned elapsed = 0; elapsed < 150000; elapsed += 66) {
            desktop.advanceMs(66);
            scene.update(desktop.millis(), 0.066f);
            ate |= v.task == Home::Task::FEEDING;
            if (scenario != 0) assert(v.task == Home::Task::SLEEPING);
        }
        assert(ate == (scenario == 0));
        assert(state.team[1].satiety == (scenario == 0 || scenario == 2 ? 60 : 10));
        assert(v.task == Home::Task::SLEEPING);
        assert(std::hypot(v.x - v.sleepX, v.y - v.sleepY) < 0.01f);
        if (scenario == 0) assert(state.room.bowlCount < 5 || state.room.bowlBitesRemaining < 3);
    }
    puts("PASS hungry sleeper eats; empty bowl, ordinary hunger and status sleep stay asleep");

    {
        auto& scene = setup(1, true);
        state.team[0].satiety = 100;
        state.team[1].satiety = 10;
        state.room.bowlCount = 5;
        state.room.bowlBitesRemaining = 3;
        scene.enterResting(desktop.millis());
        assert(scene.pickVisitorSleepSpot(scene.visitor.x, scene.visitor.y));
        scene.homeCoordinator.stop(1, desktop.millis());
        assert(scene.startVisitorFoodSeek(desktop.millis()));
        assert(scene.visitor.task == Home::Task::SEEK_FOOD);
        scene.updateVisitorFoodSeek(desktop.millis(), 0.0f);
        assert(scene.visitor.task == Home::Task::SEEK_FOOD);
        assert(scene.visitor.blockedSinceMs == 0);
        const auto blockedStart = desktop.millis();
        // Inject continuous movement failure while the real route planner can
        // still succeed. Planning must not reset or instantly expire the timer.
        scene.handleVisitorMoveBlocked(blockedStart, false);
        for (unsigned elapsed = 0; elapsed < 1600; elapsed += 66) {
            assert(scene.visitor.task == Home::Task::SEEK_FOOD);
            assert(scene.visitor.blockedSinceMs == blockedStart);
            desktop.advanceMs(66);
            scene.handleVisitorMoveBlocked(desktop.millis(), false);
        }
        assert(scene.visitor.task == Home::Task::IDLE);
        assert(scene.homeCoordinator.owner(Home::Resource::BOWL) == -1);
    }
    puts("PASS paused frame and continuous-blockage watchdog across replanning");

    auto driveDeparture = [&](MainScene& scene, bool expectCrossing) {
        const auto start = desktop.millis();
        const bool paired = scene.doorDepartureHasVisitor;
        while (scene.doorTransition != MainScene::DoorTransitionMode::EXIT_FADE &&
               desktop.millis() - start < 26000) {
            const float ax = scene.mainActor.x, ay = scene.mainActor.y;
            const float vx = scene.visitor.x, vy = scene.visitor.y;
            desktop.advanceMs(66);
            scene.update(desktop.millis(), 0.066f);
            // 42 px/s is the fastest departure motion: 2.772 px per frame.
            assert(std::hypot(scene.mainActor.x - ax, scene.mainActor.y - ay) <= 2.78f);
            assert(std::hypot(scene.visitor.x - vx, scene.visitor.y - vy) <= 2.78f);
            if (scene.doorMainHidden) {
                assert(std::hypot(scene.mainActor.x - scene.doorOutsideX,
                                  scene.mainActor.y - scene.doorOutsideY) < 0.01f);
            }
            if (paired && scene.doorVisitorHidden) {
                assert(std::hypot(scene.visitor.x - scene.doorOutsideX,
                                  scene.visitor.y - scene.visitorDoorOutsideY) < 0.01f);
            }
        }
        assert(scene.doorTransition == MainScene::DoorTransitionMode::EXIT_FADE);
        assert(engine.sceneFadeActive());
        if (expectCrossing) {
            assert(scene.doorMainHidden);
            assert(!paired || scene.doorVisitorHidden);
        }
    };
    for (unsigned scenario = 0; scenario < 64; ++scenario) {
        const bool paired = scenario >= 8;
        auto& scene = setup(1, paired);
        state.team[0].satiety = state.team[1].satiety = 100;
        GameRandom::seed(1000 + scenario);
        assert(scene.randomActorCenterWalkPoint(scene.mainGeometry(), scene.mainActor.x, scene.mainActor.y));
        if (paired) {
            bool valid = false;
            for (int trial = 0; trial < 200; ++trial) {
                assert(scene.randomActorCenterWalkPoint(scene.visitorGeometry(), scene.visitor.x, scene.visitor.y));
                if (scene.actorTargetsKeepSpacing(scene.mainActor.x, scene.mainActor.y,
                        scene.mainGeometry(), scene.visitor.x, scene.visitor.y, scene.visitorGeometry())) {
                    valid = true;
                    break;
                }
            }
            assert(valid);
        }
        if (scenario == 0 || scenario == 8) scene.enterResting(desktop.millis());
        const float ax = scene.mainActor.x, ay = scene.mainActor.y;
        const float vx = scene.visitor.x, vy = scene.visitor.y;
        const auto before = state;
        assert(engine.beginExploreDeparture(0));
        scene.beginDoorTransition(desktop.millis());
        assert(scene.mainActor.x == ax && scene.mainActor.y == ay);
        assert(scene.visitor.x == vx && scene.visitor.y == vy);
        driveDeparture(scene, true);
        assert(std::memcmp(&before, &state, sizeof(state)) == 0);
    }
    puts("PASS 64 solo/paired departure poses: continuous movement, both cross, gameplay state unchanged");

    // If the requested first actor cannot cross, the peer may leave first.
    {
        auto& scene = setup(1, true);
        state.team[0].satiety = state.team[1].satiety = 100;
        scene.mainActor.x = 120; scene.mainActor.y = 100;
        scene.visitor.x = 82; scene.visitor.y = 104;
        assert(engine.beginExploreDeparture(0));
        scene.beginDoorTransition(desktop.millis());
        scene.doorFirstActor = MainScene::DoorActor::MAIN;
        assert(scene.planFirstDoorExit(desktop.millis()));
        assert(scene.doorFirstActor == MainScene::DoorActor::VISITOR);
        assert(scene.mainActor.x == 120 && scene.mainActor.y == 100);
        assert(scene.visitor.x == 82 && scene.visitor.y == 104);
        driveDeparture(scene, true);
    }
    puts("PASS blocked first actor yields departure order without moving either actor");

    // A stale route must replan instead of teleporting when a peer moves onto it.
    {
        auto& scene = setup(1, true);
        state.team[0].satiety = state.team[1].satiety = 100;
        scene.mainActor.x = 180; scene.mainActor.y = 100;
        scene.visitor.x = 160; scene.visitor.y = 60;
        assert(engine.beginExploreDeparture(0));
        scene.beginDoorTransition(desktop.millis());
        scene.doorFirstActor = MainScene::DoorActor::MAIN;
        assert(scene.planDoorExitRoute(MainScene::DoorActor::MAIN, desktop.millis()));
        scene.visitor.x = 140; scene.visitor.y = 100;
        driveDeparture(scene, true);
    }
    puts("PASS changed peer position replans the departure route");

    // A completely invalid start tests bounded failure without a visible snap.
    {
        auto& scene = setup(1, false);
        state.team[0].satiety = 100;
        scene.mainActor.x = -100; scene.mainActor.y = -100;
        assert(engine.beginExploreDeparture(0));
        scene.beginDoorTransition(desktop.millis());
        assert(scene.mainActor.route.empty());
        desktop.advanceMs(66);
        scene.update(desktop.millis(), 0.066f);
        assert(scene.doorTransition == MainScene::DoorTransitionMode::EXIT_ROUTE);
        driveDeparture(scene, false);
        assert(scene.mainActor.x == -100 && scene.mainActor.y == -100);
        assert(!scene.doorMainHidden);
    }
    puts("PASS failed route never counts as arrival and fades without changing coordinates");
    return 0;
}
