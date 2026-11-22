#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <deque>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "platform/desktop/DesktopPlatform.h"
#include "game/MonsterFactory.h"
#include "assets/PokemonExpansionSpriteProfiles.h"
#define private public
#include "hardware/Hal.h"
#include "core/GameEngine.h"
#include "scenes/MainScene.h"
#undef private
#include "sdgoods_route_animation_timing_tests.h"

// Production resources use raw DEFLATE, never checksum envelope wrappers.
extern "C" unsigned uzlib_crc32(const void*, unsigned, unsigned) { assert(false); return 0; }
extern "C" unsigned uzlib_adler32(const void*, unsigned, unsigned) { assert(false); return 0; }

using Action = PokemonAnimationTiming::Action;
using Kind = PokemonSprites::SpriteKind;

int main(int argc, char** argv) {
    assert(argc == 3);
    DesktopPlatform desktop(argv[1]);
    Platform::bind(desktop.serviceBundle());
    assert(desktop.begin());
    auto& engine = GameEngine::ins();
    assert(engine.begin());
    engine.currentScene.reset();
    auto& state = engine.gameState();
    state = Game::GameState{};
    state.oobeDone = true;
    state.tutorialFlags = 0xff;
    state.teamCount = 2;
    state.gameMinutesTotal = 420;
    engine.gameClock.set(0, state.gameMinutesTotal);
    MainScene scene;
    std::ifstream fixture(argv[2]);
    unsigned id, actionIndex, count, checks = 0, sequences = 0;
    uint32_t now = 1000;
    while (fixture >> id >> actionIndex >> count) {
        std::vector<unsigned> durations(count);
        unsigned total = 0;
        for (auto& duration : durations) { fixture >> duration; total += duration; }
        const auto action = static_cast<Action>(actionIndex);
        const auto* profile = PokemonSprites::expansionRoomProfile(id);
        assert(profile);
        state.team[0] = Game::MonsterFactory::create(id, 25);
        state.team[1] = state.team[0];
        state.team[1].origin = Game::Origin::BEFRIENDED;
        const uint16_t ids[] = {static_cast<uint16_t>(id), static_cast<uint16_t>(id)};
        assert(PokemonSprites::syncTeamCache(ids, 2));
        scene.active = findSpecies(id);
        scene.mainActor.task = action == Action::Sleep ? Home::Task::RESTING : Home::Task::IDLE;
        scene.mainActor.velocityX = 0;
        scene.mainActor.velocityY = action == Action::Walk ? 10 : 0;
        scene.pmdDirection = MainScene::PmdDirection::FRONT;
        scene.visitor.active = true;
        scene.visitor.speciesId = id;
        scene.visitor.dropOffsetY = 0;
        scene.visitor.task = action == Action::Sleep ? Home::Task::SLEEPING
            : action == Action::Walk ? Home::Task::WALK : Home::Task::IDLE;
        scene.visitor.direction = PokemonSprites::WalkDirection::DOWN;
        scene.pmdAnimationClock.reset();
        scene.visitorAnimationClock.reset();
        const auto base = action == Action::Sleep ? profile->sleepingBase
            : action == Action::Walk ? profile->walkingBase : profile->idleBase;
        const unsigned profileCount = action == Action::Sleep ? profile->sleepingFrames
            : action == Action::Walk ? profile->walkingFrames : profile->idleFrames;
        assert(profileCount == count);
        std::vector<uint32_t> samples{0};
        unsigned ticks = 0;
        for (unsigned loop = 0; loop < 2; ++loop) {
            for (auto duration : durations) {
                ticks += duration;
                const auto boundary = (ticks * 1000 + 59) / 60;
                samples.push_back(boundary - 1);
                samples.push_back(boundary);
            }
        }
        samples.push_back(86400000); // Long gaps must remain bounded work.
        std::vector<bool> seen(count, false);
        for (auto elapsed : samples) {
            const uint64_t tick = (uint64_t(elapsed) * 60) % (uint64_t(total) * 1000);
            unsigned end = 0, expected = 0;
            while (expected + 1 < count && tick >= uint64_t(end + durations[expected]) * 1000) {
                end += durations[expected++];
            }
            scene.updatePmdSpriteState(now + elapsed);
            // Route code may clear the legacy frame at a corner. The final
            // scene playback pass must recover the continuing phase.
            scene.visitor.frameIndex = 0;
            scene.updateVisitorSpriteState(now + elapsed);
            assert(scene.pmdFrame == expected);
            assert(scene.visitor.frameIndex == expected);
            assert(scene.pmdRenderOffsetX == 0 && scene.pmdRenderOffsetY == 0);
            assert(scene.visitor.renderOffsetX == 0 && scene.visitor.renderOffsetY == 0);
            const auto* wanted = PokemonSprites::findSpeciesSprite(id,
                static_cast<Kind>(static_cast<unsigned>(base) + expected));
            assert(wanted);
            assert(scene.currentMonsterFrame() == wanted);
            bool flip = false;
            assert(scene.visitorCurrentFrame(flip) == wanted);
            seen[expected] = true;
            // Every facing change preserves elapsed phase and frame.
            for (unsigned dir = 0; dir < 8; ++dir) {
                if (action == Action::Walk) {
                    constexpr float vx[] = {0,-10,-10,-10,0,10,10,10};
                    constexpr float vy[] = {10,10,0,-10,-10,-10,0,10};
                    scene.mainActor.velocityX = vx[dir];
                    scene.mainActor.velocityY = vy[dir];
                } else scene.pmdDirection = static_cast<MainScene::PmdDirection>(dir);
                scene.visitor.direction = static_cast<PokemonSprites::WalkDirection>(dir);
                scene.updatePmdSpriteState(now + elapsed);
                scene.updateVisitorSpriteState(now + elapsed);
                assert(scene.pmdFrame == expected && scene.visitor.frameIndex == expected);
                assert(scene.pmdAnimationClock.elapsedMs() == elapsed);
                assert(scene.visitorAnimationClock.elapsedMs() == elapsed);
            }
            scene.mainActor.velocityX = 0;
            scene.mainActor.velocityY = action == Action::Walk ? 10 : 0;
            scene.pmdDirection = MainScene::PmdDirection::FRONT;
            scene.visitor.direction = PokemonSprites::WalkDirection::DOWN;
            ++checks;
        }
        assert(std::all_of(seen.begin(), seen.end(), [](bool value) { return value; }));
        // Real action changes reset each actor independently.
        scene.mainActor.task = Home::Task::IDLE;
        scene.mainActor.velocityX = scene.mainActor.velocityY = 0;
        if (action == Action::Idle) scene.mainActor.task = Home::Task::RESTING;
        scene.updatePmdSpriteState(now + samples.back() + 1);
        assert(scene.pmdFrame == 0 && scene.pmdAnimationClock.elapsedMs() == 0);
        scene.visitor.task = action == Action::Idle ? Home::Task::SLEEPING : Home::Task::IDLE;
        scene.updateVisitorSpriteState(now + samples.back() + 1);
        assert(scene.visitor.frameIndex == 0 && scene.visitorAnimationClock.elapsedMs() == 0);
        ++sequences;
        now += 90000000;
    }
    assert(sequences == 3 * std::size(PokemonSprites::EXPANSION_ROOM_PROFILES));

    // Exercise the real update wrapper and photo overlay pause/resume path.
    state.team[0] = Game::MonsterFactory::create(214,25);
    state.team[1] = Game::MonsterFactory::create(264,25);
    state.team[1].origin = Game::Origin::BEFRIENDED;
    engine.clearMainSceneViewState();
    scene.onEnter();
    scene.mainActor.task = Home::Task::IDLE;
    scene.mainActor.x = scene.mainActor.targetX = 120;
    scene.mainActor.y = scene.mainActor.targetY = 90;
    scene.mainActor.nextDecisionMs = scene.mainActor.nextMindUpdateMs = 1000000;
    scene.nextAttentionMs = scene.nextSpecialActionMs = scene.nextPairInteractionMs = 1000000;
    scene.visitor.task = Home::Task::IDLE;
    scene.visitor.taskUntilMs = 1000000;
    scene.visitor.x = 160; scene.visitor.y = 110;
    scene.visitor.dropOffsetY = 0;
    scene.contactDialog = MainScene::ContactDialog::NONE;
    scene.update(100, 0);
    auto result = scene.update(266, 0);
    assert(result.nextUpdateDelayMs == 66);
    assert(scene.pmdAnimationClock.elapsedMs() == 166);
    assert(scene.visitorAnimationClock.elapsedMs() == 166);
    const auto leaderFrame = scene.pmdFrame, visitorFrame = scene.visitor.frameIndex;
    scene.homeUi.photoFrame_.active_ = true;
    scene.update(266, 0);
    scene.update(20000, 0);
    assert(scene.pmdFrame == leaderFrame && scene.visitor.frameIndex == visitorFrame);
    scene.homeUi.closeTransientOverlay();
    scene.update(20001, 0);
    assert(scene.pmdAnimationClock.elapsedMs() == 166);
    assert(scene.visitorAnimationClock.elapsedMs() == 166);
    scene.update(20167, 0);
    assert(scene.pmdAnimationClock.elapsedMs() == 332);
    assert(scene.visitorAnimationClock.elapsedMs() == 332);

    // Species changes restart even when the animation action stays idle.
    state.team[0] = Game::MonsterFactory::create(264,25);
    scene.active = findSpecies(264);
    scene.updatePmdSpriteState(20168);
    assert(scene.pmdAnimationClock.elapsedMs() == 0 && scene.pmdFrame == 0);
    scene.visitor.speciesId = 214;
    scene.updateVisitorSpriteState(20168);
    assert(scene.visitorAnimationClock.elapsedMs() == 0 && scene.visitor.frameIndex == 0);

    // Legacy Eevee keeps the existing walk cadence and turn restart.
    state.team[0] = Game::MonsterFactory::create(133,25);
    scene.active = findSpecies(133);
    scene.mainActor.task = Home::Task::IDLE;
    scene.mainActor.velocityX = 0; scene.mainActor.velocityY = 10;
    scene.pmdAction = MainScene::PmdAction::IDLE;
    scene.updatePmdSpriteState(30000);
    const auto legacyMs = PokemonMotion::behaviorForSpecies(133).moveFrameMs;
    scene.updatePmdSpriteState(30000 + legacyMs);
    assert(scene.pmdFrame == 1);
    scene.mainActor.velocityX = 10; scene.mainActor.velocityY = 0;
    scene.updatePmdSpriteState(30000 + legacyMs + 1);
    assert(scene.pmdFrame == 0);
    scene.visitor.speciesId = 133;
    scene.visitor.frameIndex = 0;
    scene.visitor.frameStartedMs = 30000;
    scene.advanceVisitorFrames(30000 + legacyMs, true);
    assert(scene.visitor.frameIndex == 1);
    checkSdgoodsRouteAnimationTiming();
    std::printf("PASS %u XML sequences, %u frame boundaries/gaps, all directions, full sleep frames, action/species reset, photo pause, 66ms cadence, legacy playback\n", sequences, checks);
}
