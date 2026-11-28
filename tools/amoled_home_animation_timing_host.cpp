#include "AmoledApp.h"
#include "game/MonsterFactory.h"
#include "platform/desktop/DesktopPlatform.h"

#include <cassert>
#include <cstdio>

namespace AmoledV1 {
struct AmoledCareTestAccess {
    static void prepare(AmoledApp& app, uint16_t leader = 214, uint16_t companion = 355) {
        app.begin(1000);
        app.gameState = {};
        app.gameState.oobeDone = true;
        app.gameState.teamCount = 2;
        app.gameState.gameMinutesTotal = 12 * 60;
        for (uint8_t slot = 0; slot < 2; ++slot) {
            auto& monster = app.gameState.team[slot];
            monster = Game::MonsterFactory::create(slot ? companion : leader, 20);
            monster.satiety = monster.mood = 100;
            monster.nature = 0;
        }
        app.sceneFlow.goHome();
        app.syncHomeActors(1000);
        app.homePresenter_ = {};
        app.homeFlow_.state_.nextAttentionMs = 1000000;
        app.homeFlow_.state_.nextSpecialActionMs = 1000000;
        app.pairFlow_.state_.nextInteractionMs = 1000000;
        for (uint8_t slot = 0; slot < 2; ++slot) {
            app.homeRuntime.stop(slot, 1000, 1000000);
            app.homeRuntime.actor(slot)->nextMindUpdateMs = 1000000;
        }
        app.lastPetUpdateMs = app.lastCompanionUpdateMs = 1000;
        app.petFrame = app.companionFrame = 0;
        app.nextPetFrameMs = app.nextCompanionFrameMs = 1520;
        app.updatePet(1000);
        app.markRendered();
    }

    static void idleTurnPauseAndDirtyRows() {
        AmoledApp app;
        prepare(app);
        app.updatePet(1001);
        assert(!app.needsRender());
        app.updatePet(1399);
        assert(app.petFrame == 0 && app.companionFrame == 0);
        assert(!app.needsRender());
        app.updatePet(1400);
        assert(app.petFrame == 0 && app.companionFrame == 1 && app.needsRender());
        app.markRendered();
        app.updatePet(1500);
        assert(app.petFrame == 1 && app.companionFrame == 1 && app.needsRender());
        app.markRendered();
        app.petDirection = PokemonSprites::WalkDirection::LEFT;
        app.homeRuntime.beginTurn(0, Home::Task::IDLE, 1500, 100);
        app.updatePet(1600);
        assert(app.petFrame == 1);
        app.updatePet(1634);
        assert(app.petFrame == 2); // 30+8 source ticks, unaffected by the turn.
        app.markRendered();
        app.sceneFlow.enter(AppSceneFlow::Scene::MAIN_MENU);
        app.updatePet(1700);
        app.updatePet(10000);
        assert(app.petFrame == 2 && !app.needsRender());
        app.sceneFlow.goHome();
        app.updatePet(11000);
        assert(app.petFrame == 2);
        app.updatePet(11067);
        assert(app.petFrame == 3);
    }

    static void walkingKeepsFacingPhaseAndResetsForIdleAndSpecies() {
        AmoledApp app;
        prepare(app);
        // Exercise the app's final presentation path with the same actor task
        // and motion values consumed by ordinary and scripted movement.
        app.homeMainActor.task = Home::Task::WANDER;
        app.homeMainActor.motion = Home::MotionPhase::ROUTE;
        const auto sample = [&](uint32_t now) {
            app.updateHomeAnimationFrames(now, app.petFrame, app.companionFrame);
        };
        sample(2000);
        assert(app.petFrame == 0);
        sample(2134);
        assert(app.petFrame == 1);
        app.petDirection = PokemonSprites::WalkDirection::UP;
        sample(2334);
        assert(app.petFrame == 2);
        app.homeMainActor.task = Home::Task::IDLE;
        app.homeMainActor.motion = Home::MotionPhase::STILL;
        sample(2400);
        assert(app.petFrame == 0);
        sample(2900);
        assert(app.petFrame == 1);
        app.gameState.team[0].speciesId = 264;
        sample(3000);
        assert(app.petFrame == 0);
        sample(3999);
        assert(app.petFrame == 0); // Linoone's first Idle frame holds 60 ticks.
        sample(4000);
        assert(app.petFrame == 1);
        app.homeMainActor.hidden = true;
        sample(4100);
        sample(9000);
        app.homeMainActor.hidden = false;
        sample(10000);
        assert(app.petFrame == 1);
        sample(10167);
        assert(app.petFrame == 2);
    }

    static void faintRestPlaysTheFullSleepSequence() {
        AmoledApp app;
        prepare(app, 355, 355);
        for (auto& monster : app.gameState.team) {
            monster.fainted = true;
            monster.hpCur = 0;
        }
        app.updatePet(2000);
        assert(app.petFrame == 0 && app.companionFrame == 0);
        const uint32_t boundaries[] = {134, 267, 534, 667, 800, 934, 1200, 1334};
        for (uint8_t i = 0; i < 8; ++i) {
            app.updatePet(2000 + boundaries[i]);
            assert(app.petFrame == (i + 1) % 8);
            assert(app.companionFrame == (i + 1) % 8);
        }
        app.sceneFlow.enter(AppSceneFlow::Scene::MAIN_MENU);
        app.updatePet(4000);
        app.sceneFlow.goHome();
        app.updatePet(50000);
        assert(app.petFrame == 0 && app.companionFrame == 0);
        app.updatePet(50133);
        assert(app.petFrame == 1 && app.companionFrame == 1);
    }

    static void legacyIdleStillUsesItsUniformDeadline() {
        AmoledApp app;
        prepare(app, 133, 280);
        PokemonSprites::PetAnimationProfile main{}, companion{};
        assert(!PokemonAnimationTiming::hasTiming(133));
        assert(!PokemonAnimationTiming::hasTiming(280));
        assert(PokemonSprites::petAnimationProfile(133, main));
        assert(PokemonSprites::petAnimationProfile(280, companion));
        app.updatePet(1519);
        assert(app.petFrame == 0 && app.companionFrame == 0);
        app.updatePet(1520);
        assert(app.petFrame == 1 % main.idleFrames);
        assert(app.companionFrame == 1 % companion.idleFrames);
        assert(app.nextPetFrameMs == 1520 + main.idleFrameMs);
        assert(app.nextCompanionFrameMs == 1520 + companion.idleFrameMs);
    }

    static void magikarpAndGlalieUseOriginalFrames() {
        for (uint16_t species : {uint16_t(129), uint16_t(362)}) {
            AmoledApp app;
            prepare(app, species, species);
            PokemonSprites::PetAnimationProfile profile{};
            assert(PokemonSprites::petAnimationProfile(species, profile));
            assert(profile.idleFrames == (species == 129 ? 4 : 8));
            assert(profile.walkingFrames == (species == 129 ? 7 : 8));
            app.updatePet(1134);
            assert(app.petFrame == 1 && app.companionFrame == 1);
            app.homeMainActor.task = Home::Task::WANDER;
            app.homeMainActor.motion = Home::MotionPhase::ROUTE;
            app.updateHomeAnimationFrames(2000, app.petFrame, app.companionFrame);
            for (uint32_t ms = 0; ms < 800; ms += 34) {
                app.updateHomeAnimationFrames(2000 + ms, app.petFrame, app.companionFrame);
                assert(app.petFrame == PokemonAnimationTiming::frameAt(
                    species, PokemonAnimationTiming::Action::Walk, ms));
            }
        }
    }

    static void formerLegacyMotionPlaysEverySourceFrame() {
        using PokemonAnimationTiming::Action;
        for (const uint16_t species : {1, 11, 41, 93, 94, 147, 148}) {
            AmoledApp app;
            prepare(app, species, species);
            assert(PokemonAnimationTiming::hasTiming(species));
            PokemonSprites::PetAnimationProfile profile{};
            assert(PokemonSprites::petAnimationProfile(species, profile));
            uint32_t now = 1000; // prepare() initializes the first Idle clock here.
            for (const auto action : {Action::Idle, Action::Walk, Action::Sleep}) {
                const uint8_t count = action == Action::Sleep ? profile.sleepingFrames
                    : action == Action::Walk ? profile.walkingFrames : profile.idleFrames;
                bool seen[256]{};
                for (auto& monster : app.gameState.team) {
                    monster.fainted = action == Action::Sleep;
                    monster.hpCur = action == Action::Sleep ? 0 : monster.hpMax;
                }
                app.homeMainActor.task = app.homeCompanionActor.task =
                    action == Action::Sleep ? Home::Task::SLEEPING
                    : action == Action::Walk ? Home::Task::WANDER : Home::Task::IDLE;
                app.homeMainActor.motion = app.homeCompanionActor.motion =
                    action == Action::Walk ? Home::MotionPhase::ROUTE : Home::MotionPhase::STILL;
                for (uint32_t elapsed = 0; elapsed <= 10000; ++elapsed) {
                    app.petDirection = app.companionDirection =
                        static_cast<PokemonSprites::WalkDirection>((elapsed / 37) % 8);
                    app.updateHomeAnimationFrames(now + elapsed, app.petFrame, app.companionFrame);
                    const auto expected = PokemonAnimationTiming::frameAt(species, action, elapsed);
                    if (app.petFrame != expected || app.companionFrame != expected)
                        std::fprintf(stderr, "species=%u action=%u elapsed=%u expected=%u main=%u companion=%u\n",
                            species, unsigned(action), elapsed, expected, app.petFrame, app.companionFrame);
                    assert(app.petFrame == expected && app.companionFrame == expected);
                    assert(expected < count);
                    seen[expected] = true;
                }
                for (uint8_t i = 0; i < count; ++i) assert(seen[i]);
                now += 11000;
            }
        }
    }
};
}

int main(int argc, char** argv) {
    assert(argc == 2);
    DesktopPlatform desktop(argv[1]);
    Platform::bind(desktop.serviceBundle());
    assert(desktop.begin());
    using Test = AmoledV1::AmoledCareTestAccess;
    Test::idleTurnPauseAndDirtyRows();
    Test::walkingKeepsFacingPhaseAndResetsForIdleAndSpecies();
    Test::faintRestPlaysTheFullSleepSequence();
    Test::legacyIdleStillUsesItsUniformDeadline();
    Test::magikarpAndGlalieUseOriginalFrames();
    Test::formerLegacyMotionPlaysEverySourceFrame();
    std::puts("V2 home original timing: Idle/Walk/Sleep, pause, turns, damage and legacy passed");
}
