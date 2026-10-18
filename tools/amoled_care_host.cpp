#include "AmoledApp.h"
#include "game/MonsterFactory.h"
#include "game/CareExperience.h"
#include "game/HomeCare.h"
#include "game/GameRandom.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/Canvas565.h"
#include <cassert>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>

namespace AmoledV1 {
struct AmoledCareTestAccess {
    static void prepareRoom(AmoledApp& app, DesktopPlatform& desktop,
                            uint8_t count = 2) {
        app.begin(desktop.millis());
        app.gameState = {};
        app.gameState.oobeDone = true;
        app.gameState.teamCount = count;
        app.gameState.gameMinutesTotal = 12 * 60;
        app.gameState.settings.speedIndex = 0;
        app.gameState.settings.idleTimeoutIndex = 4;
        for (uint8_t i = 0; i < count; ++i) {
            auto& monster = app.gameState.team[i];
            monster = Game::MonsterFactory::create(i == 0 ? 8 : 280, 20);
            monster.satiety = 100;
            monster.mood = 100;
            monster.nature = 0;
        }
        app.sceneFlow.goHome();
        app.syncHomeActors(desktop.millis());
        app.updatePetFootprint();
        if (const auto* species = findSpecies(app.gameState.team[0].speciesId)) {
            app.homeMainActor.behavior = behaviorProfileFor(*species, app.gameState.team[0]);
        }
        for (uint8_t i = 0; i < count; ++i) {
            app.homeRuntime.stop(i, desktop.millis(), 60000);
            auto* actor = app.homeRuntime.actor(i);
            actor->mind.reset(desktop.millis());
            actor->nextMindUpdateMs = desktop.millis();
        }
        app.roomAction = AmoledApp::RoomAction::NONE;
        app.nextAttentionMs = app.nextSpecialActionMs =
            app.nextPairInteractionMs = desktop.millis() + 3600000;
        app.gameClock.set(desktop.millis(), app.gameState.gameMinutesTotal);
        app.lastCareMs = app.lastPetUpdateMs = desktop.millis();
    }

    static void presentRoom(AmoledApp& app, Canvas565& canvas) {
        if (!app.needsRender()) return;
        const int left = app.renderColumnBegin(), right = app.renderColumnEnd();
        const int top = app.renderRowBegin(), bottom = app.renderRowEnd();
        app.render(canvas);
        const std::vector<uint16_t> partial(canvas.rawPixels(),
                                             canvas.rawPixels() + canvas.pixelCount());
        app.forceFullRender();
        app.render(canvas);
        if (!std::equal(partial.begin(), partial.end(), canvas.rawPixels())) {
            for (size_t i = 0; i < partial.size(); ++i) {
                if (partial[i] == canvas.rawPixels()[i]) continue;
                std::fprintf(stderr, "room damage mismatch (%zu,%zu) rect=%d,%d-%d,%d task=%u/%u action=%u\n",
                    i % AmoledUi::WIDTH, i / AmoledUi::WIDTH, left, top, right, bottom,
                    static_cast<unsigned>(app.homeMainActor.task),
                    static_cast<unsigned>(app.homeCompanionActor.task),
                    static_cast<unsigned>(app.roomAction));
                break;
            }
            assert(false);
        }
        app.markRendered();
    }

    static void assertRoomState(const AmoledApp& app) {
        assert(app.homeRuntime.validate() == Home::INVARIANT_OK);
        HomeViewModel model;
        app.fillHomePetModel(model);
        assert(model.petCenterX == app.worldToScreenX(app.homeMainActor.x));
        assert(model.petGroundY == app.worldToScreenY(app.homeMainActor.y));
        for (uint8_t i = 0; i < app.gameState.teamCount; ++i) {
            const auto* actor = app.homeRuntime.actor(i);
            assert(actor && std::isfinite(actor->x) && std::isfinite(actor->y));
            assert(std::isfinite(actor->targetX) && std::isfinite(actor->targetY));
        }
        // Exactly one visible actor can own the bowl, including while turning.
        if (app.homeMainActor.task == Home::Task::FEEDING) {
            assert(app.homeRuntime.owner(Home::Resource::BOWL) == 0);
            assert(app.homeCompanionActor.task != Home::Task::FEEDING);
        }
        if (app.homeCompanionActor.task == Home::Task::FEEDING) {
            assert(app.homeRuntime.owner(Home::Resource::BOWL) == 1);
        }
    }

    static void checkAuthoritativeRoomMotion(DesktopPlatform& desktop,
                                             Canvas565& canvas) {
        AmoledApp app;
        prepareRoom(app, desktop, 1);
        auto& actor = app.homeMainActor;
        actor.x = actor.targetX = 100;
        actor.y = actor.targetY = 125;
        app.configureHomeRuntime();
        app.updateCamera();
        app.forceFullRender();
        presentRoom(app, canvas);
        assert(app.beginPetMove(AmoledApp::PetMotion::WANDERING,
                                155, 125, desktop.millis()));
        // Scene adapters must not restore an older cached pose over a live
        // actor after movement, a save boundary, display lock or wake.
        assert(actor.x == 100 && actor.y == 125);
        assert(actor.targetX == 155 && actor.targetY == 125);
        float previousX = actor.x;
        unsigned movingFrames = 0;
        for (int frame = 0; frame < 300 && actor.x < 140; ++frame) {
            desktop.advanceMs(50);
            app.updatePet(desktop.millis());
            assertRoomState(app);
            assert(actor.x >= previousX && actor.x - previousX < 3);
            if (actor.x > previousX) ++movingFrames;
            previousX = actor.x;
            if (frame == 10) {
                const float x = actor.x, y = actor.y;
                const auto targetX = actor.targetX, targetY = actor.targetY;
                const auto task = actor.task;
                const auto waypoint = actor.route.index;
                assert(app.prepareDisplayLock(desktop.millis()));
                app.setDisplayLocked(true);
                app.setDisplayLocked(false);
                app.onWake(desktop.millis());
                assert(actor.x == x && actor.y == y && actor.task == task);
                assert(actor.targetX == targetX && actor.targetY == targetY);
                assert(actor.route.index == waypoint && !actor.route.empty());
            }
            if (frame % 10 == 0) presentRoom(app, canvas);
        }
        if (!(actor.x >= 140 && movingFrames > 20)) {
            std::fprintf(stderr,
                "room motion stopped x=%.2f y=%.2f target=%.2f,%.2f task=%u resume=%u phase=%u route=%u/%u moved=%u stopping=%u\n",
                actor.x, actor.y, actor.targetX, actor.targetY,
                static_cast<unsigned>(actor.task), static_cast<unsigned>(actor.resumeTask),
                static_cast<unsigned>(actor.motion), actor.route.index, actor.route.count,
                movingFrames, app.petStopping ? 1 : 0);
            const auto& logs = desktop.logs();
            std::fputs(logs.substr(logs.size() > 6000 ? logs.size() - 6000 : 0).c_str(), stderr);
        }
        assert(actor.x >= 140 && movingFrames > 20);
        std::puts("room authority: actor pose/target survives routing, lock/save/wake and real partial rendering");
    }

    static void putServing(AmoledApp& app, DesktopPlatform& desktop) {
        const auto stock = app.gameState.room.food[0];
        app.gameState.room.selectedFood = 0;
        const int16_t x = app.worldToScreenX(RoomResource::ins().foodX());
        const int16_t y = app.worldToScreenY(RoomResource::ins().foodY());
        app.handleTouch({TouchEventType::DOWN, x, y, x, y, desktop.millis()});
        desktop.advanceMs(16);
        app.handleTouch({TouchEventType::UP, x, y, x, y, desktop.millis()});
        assert(app.gameState.room.food[0] == stock - 1);
        assert(app.gameState.room.bowlCount == 1);
    }

    static void checkAutomaticFoodTurns(DesktopPlatform& desktop,
                                        Canvas565& canvas) {
        GameRandom::setRangeProvider(+[](uint32_t minimum, uint32_t) { return minimum; });
        AmoledApp app;
        prepareRoom(app, desktop);
        app.gameState.team[0].satiety = 25;
        app.gameState.team[1].satiety = 0;
        app.gameState.room.food[0] = 2;
        app.gameState.room.bowlCount = app.gameState.room.bowlBitesRemaining = 0;
        app.homeMainActor.x = app.homeMainActor.targetX = 100;
        app.homeMainActor.y = app.homeMainActor.targetY = 125;
        app.homeCompanionActor.x = app.homeCompanionActor.targetX = 132;
        app.homeCompanionActor.y = app.homeCompanionActor.targetY = 115;
        app.configureHomeRuntime();
        app.updateCamera();
        app.forceFullRender();
        presentRoom(app, canvas);
        putServing(app, desktop);
        bool secondAte = false;
        for (int frame = 0; frame < 1800; ++frame) {
            desktop.advanceMs(50);
            app.updatePet(desktop.millis());
            assertRoomState(app);
            assert(app.gameState.team[0].satiety == 25);
            secondAte |= app.gameState.team[1].satiety > 0;
            if (frame % 20 == 0) presentRoom(app, canvas);
            if (app.gameState.room.bowlCount == 0 &&
                !app.homeRuntime.bowlSession().active()) break;
        }
        assert(secondAte && app.gameState.team[1].satiety == 75);
        assert(app.gameState.room.bowlCount == 0);
        assert(app.homeRuntime.owner(Home::Resource::BOWL) == -1);
        assert(!app.homeRuntime.bowlSession().active());
        putServing(app, desktop);
        bool firstAte = false;
        for (int frame = 0; frame < 1800; ++frame) {
            desktop.advanceMs(50);
            app.updatePet(desktop.millis());
            assertRoomState(app);
            assert(app.gameState.team[1].satiety == 75);
            firstAte |= app.gameState.team[0].satiety > 25;
            if (frame % 20 == 0) presentRoom(app, canvas);
            if (firstAte && app.gameState.team[0].satiety >= 70 &&
                !app.homeRuntime.bowlSession().active()) break;
        }
        assert(firstAte && app.gameState.team[0].satiety == 75);
        assert(app.gameState.room.bowlCount == 1 &&
               app.gameState.room.bowlBitesRemaining == 1);
        assert(app.homeRuntime.owner(Home::Resource::BOWL) == -1);
        assert(!app.homeRuntime.bowlSession().active());
        GameRandom::setRangeProvider(nullptr);
        std::puts("room food turns: hungrier reserve eats first, clears bowl, leader takes next serving; untouched slot never consumes");
    }

    static void checkFoodExitNoStall(DesktopPlatform& desktop,
                                    Canvas565& canvas) {
        AmoledApp app;
        prepareRoom(app, desktop);
        app.gameState.team[0] = Game::MonsterFactory::create(281, 20);
        app.gameState.team[1] = Game::MonsterFactory::create(8, 29);
        app.gameState.team[0].satiety = 0;
        app.gameState.team[1].satiety = 87;
        app.syncHomeActors(desktop.millis());
        app.updatePetFootprint();
        app.homeMainActor.x = app.homeMainActor.targetX = 118;
        app.homeMainActor.y = app.homeMainActor.targetY = 119;
        app.homeCompanionActor.x = app.homeCompanionActor.targetX = 65;
        app.homeCompanionActor.y = app.homeCompanionActor.targetY = 122;
        app.homeRuntime.stop(0, desktop.millis());
        app.homeRuntime.stop(1, desktop.millis());
        app.configureHomeRuntime();
        app.gameState.room.bowlFood = 0;
        app.gameState.room.bowlCount = 1;
        app.gameState.room.bowlBitesRemaining = 3;
        app.updateCamera();
        app.forceFullRender();
        presentRoom(app, canvas);
        bool companionMoved = false;
        for (int frame = 0; frame < 1200; ++frame) {
            desktop.advanceMs(50);
            app.updatePet(desktop.millis());
            assertRoomState(app);
            if (frame % 40 == 0) presentRoom(app, canvas);
            companionMoved |= std::hypot(app.homeCompanionActor.x - 65,
                                         app.homeCompanionActor.y - 122) > 8;
            if (app.gameState.team[0].satiety >= 70 &&
                !app.homeRuntime.bowlSession().active()) break;
        }
        if (!companionMoved || app.gameState.team[0].satiety < 70) {
#if STICKMON_ENABLE_DEBUG_FEATURES
            app.debugReportHomeState();
#endif
            std::fprintf(stderr, "exit stall footprints main=%.1f,%.1f reserve=%.1f,%.1f bowl=%d,%d\n",
                app.homeMainActor.geometry.footprint.radiusX,
                app.homeMainActor.geometry.footprint.radiusY,
                app.homeCompanionActor.geometry.footprint.radiusX,
                app.homeCompanionActor.geometry.footprint.radiusY,
                RoomResource::ins().foodX(), RoomResource::ins().foodY());
            const auto& logs = desktop.logs();
            std::fputs(logs.substr(logs.size() > 5000 ? logs.size() - 5000 : 0).c_str(), stderr);
        }
        assert(companionMoved);
        assert(app.gameState.team[0].satiety >= 70);
        assert(!app.homeRuntime.bowlSession().active());
        // Repeat the reported sequence, including the actual bowl touch:
        // both hungry, companion eats first, clears the narrow gap, refill.
        prepareRoom(app, desktop);
        app.gameState.team[0] = Game::MonsterFactory::create(281, 20);
        app.gameState.team[1] = Game::MonsterFactory::create(8, 29);
        app.gameState.team[0].satiety = 25;
        app.gameState.team[1].satiety = 0;
        app.syncHomeActors(desktop.millis());
        app.updatePetFootprint();
        app.homeMainActor.x = app.homeMainActor.targetX = 118;
        app.homeMainActor.y = app.homeMainActor.targetY = 119;
        app.homeCompanionActor.x = app.homeCompanionActor.targetX = 65;
        app.homeCompanionActor.y = app.homeCompanionActor.targetY = 122;
        app.homeRuntime.stop(0, desktop.millis());
        app.homeRuntime.stop(1, desktop.millis());
        app.configureHomeRuntime();
        app.gameState.room.food[0] = 2;
        app.gameState.room.bowlCount = app.gameState.room.bowlBitesRemaining = 0;
        app.updateCamera();
        app.forceFullRender();
        presentRoom(app, canvas);
        putServing(app, desktop);
        bool sawCompanionEating = false;
        bool sawCompanionClearing = false;
        for (int frame = 0; frame < 1200; ++frame) {
            desktop.advanceMs(50);
            app.updatePet(desktop.millis());
            assertRoomState(app);
            sawCompanionEating |= app.homeCompanionActor.task == Home::Task::FEEDING;
            sawCompanionClearing |= app.homeRuntime.bowlSession().clearing();
            assert(app.gameState.team[0].satiety == 25);
            if (frame % 40 == 0) presentRoom(app, canvas);
            if (app.gameState.team[1].satiety == 75 &&
                !app.homeRuntime.bowlSession().active()) break;
        }
        assert(sawCompanionEating && sawCompanionClearing);
        assert(app.gameState.team[1].satiety == 75);
        assert(app.gameState.room.bowlCount == 0);
        assert(!app.homeRuntime.bowlSession().active());
        putServing(app, desktop);
        for (int frame = 0; frame < 1200; ++frame) {
            desktop.advanceMs(50);
            app.updatePet(desktop.millis());
            assertRoomState(app);
            assert(app.gameState.team[1].satiety == 75);
            if (frame % 40 == 0) presentRoom(app, canvas);
            if (app.gameState.team[0].satiety == 75 &&
                !app.homeRuntime.bowlSession().active()) break;
        }
        assert(app.gameState.team[0].satiety == 75);
        assert(!app.homeRuntime.bowlSession().active());
        std::puts("room food exit: narrow-gap restore and two hungry actors with sequential servings both finish without stalling");
    }

    static void checkFaintRecoveryAndSleep(DesktopPlatform& desktop,
                                           Canvas565& canvas) {
        AmoledApp app;
        prepareRoom(app, desktop);
        app.gameState.gameMinutesTotal = 21 * 60;
        app.gameClock.set(desktop.millis(), app.gameState.gameMinutesTotal);
        for (auto& monster : app.gameState.team) {
            monster.fainted = 1;
            monster.hpCur = 0;
            monster.majorStatus = Game::MajorStatus::POISON;
            monster.lastSeenAt = app.gameState.gameMinutesTotal * 60;
        }
        app.updatePet(desktop.millis());
        assert(app.homeMainActor.task == Home::Task::FAINTED);
        assert(app.homeCompanionActor.task == Home::Task::FAINTED);
        app.forceFullRender();
        presentRoom(app, canvas);
        desktop.advanceMs(59 * 60000);
        app.updateClockAndCare(desktop.millis());
        assert(app.gameState.team[0].fainted && app.gameState.team[1].fainted);
        desktop.advanceMs(60000);
        app.updateClockAndCare(desktop.millis());
        for (const auto& monster : app.gameState.team) {
            assert(!monster.fainted && monster.hpCur > 0);
            assert(monster.majorStatus == Game::MajorStatus::NONE);
        }
        for (int frame = 0; frame < 1600; ++frame) {
            desktop.advanceMs(50);
            app.updatePet(desktop.millis());
            assertRoomState(app);
            if (frame % 20 == 0) presentRoom(app, canvas);
            if (app.homeMainActor.task == Home::Task::SLEEPING &&
                app.homeCompanionActor.task == Home::Task::SLEEPING) break;
        }
        assert(app.homeMainActor.task == Home::Task::SLEEPING);
        assert(app.homeCompanionActor.task == Home::Task::SLEEPING);
        const float sleepX = app.homeMainActor.x, sleepY = app.homeMainActor.y;
        app.gameClock.set(desktop.millis(), 12 * 60 + Game::GAME_MINUTES_PER_DAY);
        app.updateClockAndCare(desktop.millis());
        for (int frame = 0; frame < 800; ++frame) {
            desktop.advanceMs(50);
            app.updatePet(desktop.millis());
            assertRoomState(app);
            if (frame % 20 == 0) presentRoom(app, canvas);
            if (std::hypot(app.homeMainActor.x - sleepX,
                           app.homeMainActor.y - sleepY) > 5 &&
                app.homeCompanionActor.task != Home::Task::SLEEPING) break;
        }
        assert(std::hypot(app.homeMainActor.x - sleepX,
                          app.homeMainActor.y - sleepY) > 5);
        assert(app.homeMainActor.task != Home::Task::FAINTED &&
               app.homeMainActor.task != Home::Task::SLEEPING);
        assert(app.homeCompanionActor.task != Home::Task::FAINTED &&
               app.homeCompanionActor.task != Home::Task::SLEEPING);
        std::puts("room faint recovery: both slots rest for 60 game minutes, clear status, resume night sleep and wake by day");
    }

    static void checkSavedTurns(DesktopPlatform& desktop) {
        for (bool seekingFood : {false, true}) {
            AmoledApp app;
            prepareRoom(app, desktop, 1);
            auto& actor = app.homeMainActor;
            actor.x = actor.targetX = seekingFood ? 155 : 100;
            actor.y = actor.targetY = 125;
            app.configureHomeRuntime();
            float targetX = 155, targetY = 125;
            if (seekingFood) {
                app.gameState.team[0].satiety = 25;
                app.gameState.room.bowlFood = 0;
                app.gameState.room.bowlCount = 1;
                app.gameState.room.bowlBitesRemaining = 3;
                assert(!app.homeActorNearFood(0));
                assert(app.startMainFoodSeek(desktop.millis()));
                targetX = actor.targetX;
                targetY = actor.targetY;
            } else {
                assert(app.beginPetMove(AmoledApp::PetMotion::WANDERING,
                                        targetX, targetY, desktop.millis()));
            }
            if (actor.task != Home::Task::TURNING) {
                app.petDirection = static_cast<PokemonSprites::WalkDirection>(
                    (static_cast<unsigned>(app.petDirection) + 2) % 4);
                if (seekingFood) {
                    app.homeRuntime.stop(0, desktop.millis());
                    assert(app.startMainFoodSeek(desktop.millis()));
                    targetX = actor.targetX;
                    targetY = actor.targetY;
                } else {
                    assert(app.beginPetMove(AmoledApp::PetMotion::WANDERING,
                                            targetX, targetY, desktop.millis()));
                }
            }
            assert(actor.task == Home::Task::TURNING && !actor.route.empty());
            const auto intended = seekingFood ? Home::Task::SEEK_FOOD : Home::Task::WANDER;
            assert(actor.resumeTask == intended);
            assert(app.saveState());
            assert(app.mainViewState.aiMode == (seekingFood ? 3 : 1));
            AmoledApp restored;
            restored.begin(desktop.millis());
            const auto& next = restored.homeMainActor;
            assert(next.task == intended && !next.route.empty());
            assert(std::fabs(next.x - actor.x) < 0.01f &&
                   std::fabs(next.y - actor.y) < 0.01f);
            assert(std::fabs(next.targetX - targetX) < 0.01f &&
                   std::fabs(next.targetY - targetY) < 0.01f);
            if (seekingFood) {
                assert(restored.homeRuntime.owner(Home::Resource::BOWL) == 0);
            }
            const auto beforeX = next.x, beforeY = next.y;
            for (int frame = 0; frame < 10; ++frame) {
                desktop.advanceMs(50);
                restored.updatePet(desktop.millis());
            }
            if (std::hypot(next.x - beforeX, next.y - beforeY) <= 0.1f) {
                std::fprintf(stderr, "saved turn no move food=%u pos=%.2f,%.2f target=%.2f,%.2f task=%u phase=%u route=%u/%u bowlOwner=%d\n",
                    seekingFood ? 1 : 0, next.x, next.y, next.targetX, next.targetY,
                    static_cast<unsigned>(next.task), static_cast<unsigned>(next.motion),
                    next.route.index, next.route.count,
                    restored.homeRuntime.owner(Home::Resource::BOWL));
            }
            assert(std::hypot(next.x - beforeX, next.y - beforeY) > 0.1f);
            assertRoomState(restored);
        }
        std::puts("room saved turns: wandering and bowl-bound turns restore their intended route and continue moving");
    }

    static void checkYieldTurnAndStationaryActions(DesktopPlatform& desktop,
                                                    Canvas565& canvas) {
        {
            AmoledApp app;
            prepareRoom(app, desktop);
            app.homeMainActor.x = app.homeMainActor.targetX = 100;
            app.homeMainActor.y = app.homeMainActor.targetY = 125;
            app.homeCompanionActor.x = app.homeCompanionActor.targetX = 132;
            app.homeCompanionActor.y = app.homeCompanionActor.targetY = 115;
            app.petDirection = PokemonSprites::WalkDirection::DOWN;
            app.configureHomeRuntime();
            assert(app.tryYieldHomeBlocker(1, 75, 125, desktop.millis()));
            auto& actor = app.homeMainActor;
            assert(actor.task == Home::Task::TURNING);
            assert(actor.resumeTask == Home::Task::YIELDING && !actor.route.empty());
            const float beforeX = actor.x, beforeY = actor.y;
            bool sawYield = false;
            for (int frame = 0; frame < 30; ++frame) {
                desktop.advanceMs(50);
                app.updatePet(desktop.millis());
                sawYield |= actor.task == Home::Task::YIELDING;
                assertRoomState(app);
            }
            assert(sawYield && std::hypot(actor.x - beforeX, actor.y - beforeY) > 1);
        }
        for (auto approach : {AmoledApp::RoomAction::ATTENTION_APPROACH,
                              AmoledApp::RoomAction::WINDOW_APPROACH}) {
            AmoledApp app;
            prepareRoom(app, desktop, 1);
            auto& actor = app.homeMainActor;
            actor.x = actor.targetX = 100;
            actor.y = actor.targetY = 125;
            app.configureHomeRuntime();
            app.updateCamera();
            app.forceFullRender();
            presentRoom(app, canvas);
            assert(app.beginRoomActionMove(approach, 125, 125, desktop.millis()));
            bool waiting = false;
            for (int frame = 0; frame < 300; ++frame) {
                desktop.advanceMs(50);
                app.updatePet(desktop.millis());
                assertRoomState(app);
                if (frame % 20 == 0) presentRoom(app, canvas);
                waiting = app.roomAction == AmoledApp::RoomAction::ATTENTION_WAIT ||
                          app.roomAction == AmoledApp::RoomAction::WINDOW_WAIT;
                if (waiting) break;
            }
            assert(waiting && actor.motion == Home::MotionPhase::STILL);
            const float x = actor.x, y = actor.y;
            for (int frame = 0; frame < 6; ++frame) {
                desktop.advanceMs(50);
                app.updatePet(desktop.millis());
                HomeViewModel model;
                app.fillHomePetModel(model);
                assert(model.petAction == HomeViewModel::PetVisualAction::IDLE);
                assert(actor.x == x && actor.y == y);
                presentRoom(app, canvas);
            }
        }
        std::puts("room presentation: yielding completes its turn and attention/window waits stop walking after arrival");
    }

    static void checkEeveeEvolution(DesktopPlatform& desktop) {
        struct Case {
            uint32_t minute;
            uint8_t affection;
            bool evolves;
            bool cancel;
        };
        const Case cases[] = {
            {6U * 60U, 220, true, false},
            {17U * 60U + 59U, 255, true, false},
            {18U * 60U, 220, false, false},
            {12U * 60U, 219, false, false},
            {12U * 60U, 220, true, true},
        };
        for (const auto& test : cases) {
            AmoledApp app;
            app.begin(desktop.millis());
            app.gameState = {};
            app.gameState.oobeDone = true;
            app.gameState.teamCount = 1;
            app.gameState.gameMinutesTotal = test.minute;
            auto& monster = app.gameState.team[0];
            monster = Game::MonsterFactory::create(133, 19);
            monster.ivPacked = 123456;
            monster.affection = test.affection;
            monster.move2Id = monster.move3Id = 0;
            monster.exp = minimumExpForLevel(findSpecies(133)->growthRate, 20) - 2;
            app.sceneFlow.goHome();
            app.syncHomeActors(desktop.millis());

            // Being eligible alone must not evolve Eevee: a level is required.
            assert(app.applyHomeCareExperience(0, 1) == 1);
            assert(!app.startPendingCareProgression(desktop.millis()));
            assert(monster.speciesId == 133 && monster.level == 19);
            assert(app.applyHomeCareExperience(0, 1) == 1);
            assert(app.startPendingCareProgression(desktop.millis()));
            app.advanceProgression(desktop.millis());
            assert(monster.speciesId == 133 && monster.level == 20);
            if (test.evolves) {
                assert(app.progressionMode == ProgressionViewModel::Mode::EVOLUTION);
                assert(app.progressionToSpeciesId == 196);
                assert(app.progressionEvolutionItem == Game::ItemId::COUNT);
                app.advanceProgression(desktop.millis());
                assert(monster.speciesId == 133);
                if (test.cancel) {
                    assert(app.progressionEvolution.beginCancellation(desktop.millis()));
                    desktop.advanceMs(Game::EvolutionSequence::CANCEL_COMPLETE_MS);
                    app.handleTap(0, 0, desktop.millis());
                }
            } else {
                assert(app.progressionMode != ProgressionViewModel::Mode::EVOLUTION);
            }
            for (int tries = 0; tries < 20 &&
                 app.sceneFlow.current() == AppSceneFlow::Scene::PROGRESSION; ++tries) {
                desktop.advanceMs(20000);
                app.advanceProgression(desktop.millis());
            }
            assert(app.sceneFlow.current() == AppSceneFlow::Scene::HOME);
            assert(monster.speciesId == (test.evolves && !test.cancel ? 196 : 133));
            assert(monster.ivPacked == 123456 && monster.affection == test.affection);
            assert(!app.startPendingCareProgression(desktop.millis()));
        }
    }

    static void checkWakeToFoodFacing(DesktopPlatform& desktop) {
        for (uint8_t slot : {0, 1}) {
            AmoledApp app;
            app.begin(desktop.millis());
            app.gameState = {};
            app.gameState.oobeDone = true;
            app.gameState.teamCount = 2;
            app.gameState.gameMinutesTotal = 23 * 60;
            app.gameState.settings.speedIndex = 0;
            app.gameState.settings.idleTimeoutIndex = 4;
            for (int i = 0; i < 2; ++i) {
                app.gameState.team[i] = Game::MonsterFactory::create(i == 0 ? 8 : 280, 20);
                app.gameState.team[i].satiety = i == slot ? 0 : 100;
            }
            app.sceneFlow.goHome();
            const uint32_t start = desktop.millis();
            app.syncHomeActors(start);
            app.homeRuntime.releaseAll(0);
            app.homeRuntime.releaseAll(1);
            if (slot == 0) {
                app.homeMainActor.x = app.homeMainActor.targetX = RoomResource::ins().bedX();
                app.homeMainActor.y = app.homeMainActor.targetY = RoomResource::ins().bedY();
                app.startSleepAnimation(start, 800);
            }
            app.homeRuntime.transition(slot, Home::Task::SLEEPING, start, 0, true);
            app.gameState.room.bowlFood = 0;
            app.gameState.room.bowlCount = 1;
            app.gameState.room.bowlBitesRemaining = 3;
            bool eating = false;
            for (int frame = 0; frame < 1000 && !eating; ++frame) {
                desktop.advanceMs(50);
                app.updatePet(desktop.millis());
                eating = slot == 0 ? app.petMotion() == AmoledApp::PetMotion::EATING
                    : app.homeCompanionActor.task == Home::Task::FEEDING;
            }
            assert(eating);
            assert(app.homeMainActor.task != Home::Task::SLEEPING || slot == 1);
            const float x = slot == 0 ? app.homeMainActor.x : app.homeCompanionActor.x;
            const float y = slot == 0 ? app.homeMainActor.y : app.homeCompanionActor.y;
            const float dx = RoomResource::ins().foodX() - x;
            const float dy = RoomResource::ins().foodY() - y;
            const auto expected = std::fabs(dx) >= std::fabs(dy)
                ? (dx >= 0 ? PokemonSprites::WalkDirection::RIGHT : PokemonSprites::WalkDirection::LEFT)
                : (dy >= 0 ? PokemonSprites::WalkDirection::DOWN : PokemonSprites::WalkDirection::UP);
            assert((slot == 0 ? app.petDirection : app.companionDirection) == expected);
        }
        // Arriving from the opposite side must turn at the feeding boundary,
        // regardless of the direction of the final route segment.
        AmoledApp app;
        app.begin(desktop.millis());
        app.gameState = {};
        app.gameState.oobeDone = true;
        app.gameState.teamCount = 1;
        app.gameState.team[0] = Game::MonsterFactory::create(8, 20);
        app.gameState.team[0].satiety = 0;
        app.sceneFlow.goHome();
        app.homeMainActor.faintRestActive = false;
        app.homeMainActor.x = app.homeMainActor.targetX = RoomResource::ins().foodX() + 12;
        app.homeMainActor.y = app.homeMainActor.targetY = RoomResource::ins().foodY() + 4;
        app.syncHomeActors(desktop.millis());
        app.homeRuntime.releaseAll(0);
        assert(app.homeRuntime.acquire(Home::Resource::BOWL, 0, Home::Task::SEEK_FOOD, desktop.millis()));
        app.homeRuntime.transition(0, Home::Task::SEEK_FOOD, desktop.millis(), 0, true);
        app.gameState.room.bowlFood = 0;
        app.gameState.room.bowlCount = 1;
        app.gameState.room.bowlBitesRemaining = 3;
        app.petDirection = PokemonSprites::WalkDirection::RIGHT;
        app.finishPetMove(desktop.millis());
        for (int frame = 0; frame < 20 && app.petMotion() != AmoledApp::PetMotion::EATING; ++frame) {
            desktop.advanceMs(100);
            app.updatePet(desktop.millis());
        }
        assert(app.petMotion() == AmoledApp::PetMotion::EATING);
        assert(app.petDirection == PokemonSprites::WalkDirection::LEFT);
        std::puts("wake-to-food: both actors face the bowl after waking and routed arrival turns before eating");
    }

    static void bite(AmoledApp& app, DesktopPlatform& desktop, uint8_t slot) {
        const uint32_t now = desktop.millis();
        app.gameState.room.bowlFood = 0;
        app.gameState.room.bowlCount = 1;
        app.gameState.room.bowlBitesRemaining = 3;
        app.gameState.team[slot].satiety = 25;
        app.homeRuntime.releaseAll(0);
        app.homeRuntime.releaseAll(1);
        assert(app.homeRuntime.acquire(Home::Resource::BOWL, slot, Home::Task::FEEDING, now));
        app.homeRuntime.transition(slot, Home::Task::FEEDING, now, 10000, true);
        if (slot == 0) {
            app.nextFeedBiteMs = now;
            app.updatePet(now);
        } else {
            app.companionFoodBiteMs = now;
            app.updateCompanion(now);
        }
    }

    static void checkRoomAvoidance(DesktopPlatform& desktop) {
        using Task = Home::Task;
        const auto prepare = [&](AmoledApp& app) {
            app.begin(desktop.millis());
            app.gameState = {};
            app.gameState.oobeDone = true;
            app.gameState.teamCount = 2;
            app.gameState.gameMinutesTotal = 12 * 60;
            for (auto& mon : app.gameState.team) {
                mon = Game::MonsterFactory::create(7, 20);
                mon.satiety = 100;
            }
            app.sceneFlow.goHome();
            app.syncHomeActors(desktop.millis());
            app.homeMainActor.x = app.homeMainActor.targetX = 100;
            app.homeMainActor.y = app.homeMainActor.targetY = 125;
            app.homeCompanionActor.x = app.homeCompanionActor.targetX = 132;
            app.homeCompanionActor.y = app.homeCompanionActor.targetY = 115;
            app.roomAction = AmoledApp::RoomAction::NONE;
            app.homeRuntime.stop(0, desktop.millis(), 60000);
            app.homeRuntime.stop(1, desktop.millis(), 60000);
            app.configureHomeRuntime();
        };
        {
            AmoledApp app;
            prepare(app);
            float x = 0, y = 0;
            assert(app.chooseFoodApproachTarget(132, 115, x, y));
            assert(app.homeRuntime.targetAvailable(0, x, y));
            for (int pick = 0; pick < 40; ++pick) {
                if (app.chooseWanderTarget(x, y)) {
                    assert(app.homeRuntime.targetAvailable(0, x, y));
                }
                if (app.chooseCompanionTarget(x, y)) {
                    assert(app.homeRuntime.targetAvailable(1, x, y));
                }
            }
        }
        for (uint8_t waiting : {0, 1}) {
            AmoledApp app;
            prepare(app);
            auto& blocker = waiting == 0 ? app.homeCompanionActor : app.homeMainActor;
            assert(app.tryYieldHomeBlocker(waiting, waiting == 0 ? 160 : 75,
                                          waiting == 0 ? 110 : 125, desktop.millis()));
            assert((blocker.task == Task::YIELDING ||
                    (blocker.task == Task::TURNING && blocker.resumeTask == Task::YIELDING)) &&
                   !blocker.route.empty());
            assert(app.homeRuntime.targetAvailable(waiting, waiting == 0 ? 160 : 75,
                                                    waiting == 0 ? 110 : 125));
            assert(!app.homeRuntime.overlapsOther(waiting == 0 ? 1 : 0,
                                                  blocker.targetX, blocker.targetY));
        }
        for (auto task : {Task::SLEEPING, Task::FAINTED, Task::FEEDING}) {
            AmoledApp app;
            prepare(app);
            app.homeRuntime.transition(1, task, desktop.millis(), 0, true);
            assert(!app.tryYieldHomeBlocker(0, 160, 110, desktop.millis()));
            assert(app.homeCompanionActor.task == task);
            assert(app.homeCompanionActor.x == 132 && app.homeCompanionActor.y == 115);
        }
        AmoledApp app;
        prepare(app);
        app.gameState.gameMinutesTotal = 23 * 60;
        app.homeMainActor.x = app.homeMainActor.targetX = 120;
        app.homeMainActor.y = app.homeMainActor.targetY = 120;
        app.homeCompanionActor.x = 80;
        app.homeCompanionActor.y = 120;
        app.homeRuntime.transition(0, Task::SLEEPING, desktop.millis(), 0, true);
        assert(!app.beginCompanionMove(Task::SEEK_SLEEP, 120, 120, desktop.millis()));
        assert(app.beginCompanionMove(Task::SEEK_SLEEP, 160, 120, desktop.millis()));
        assert(app.homeCompanionActor.route.count >= 2);
        for (int frame = 0; frame < 800 && app.homeCompanionActor.task != Task::SLEEPING; ++frame) {
            desktop.advanceMs(50);
            app.updateCompanion(desktop.millis());
            assert(!app.homeRuntime.overlapsOther(1, app.homeCompanionActor.x,
                                                   app.homeCompanionActor.y));
        }
        assert(app.homeCompanionActor.task == Task::SLEEPING);
        assert(app.homeMainActor.x == 120 && app.homeMainActor.y == 120);
        std::puts("room avoidance: either idle actor yields; protected poses retained; reserve walks around sleeping leader before sleeping");
    }

    static void checkBothRecover(DesktopPlatform& desktop) {
        for (uint8_t speed : {0, 2}) {
            AmoledApp app;
            app.begin(desktop.millis());
            app.gameState = {};
            app.gameState.oobeDone = true;
            app.gameState.teamCount = 2;
            app.gameState.settings.speedIndex = speed;
            app.sceneFlow.goHome();
            for (auto& mon : app.gameState.team) {
                mon = Game::MonsterFactory::create(7, 20);
                mon.hpMax = 200;
                mon.hpCur = 20;
                mon.satiety = 80;
            }
            app.lastCareMs = desktop.millis();
            desktop.advanceMs(3 * 60000);
            app.updateClockAndCare(desktop.millis());
            const uint16_t expected = 20 + 3 * (speed == 0 ? 1 : 4) * 2;
            assert(app.gameState.team[0].hpCur == expected);
            assert(app.gameState.team[1].hpCur == expected);
            // Empty hunger slows either slot, independent of team order.
            app.gameState.team[1].satiety = 0;
            desktop.advanceMs(60000);
            app.updateClockAndCare(desktop.millis());
            assert(app.gameState.team[1].hpCur == expected + (speed == 0 ? 1 : 4));
        }
        std::puts("room recovery: both slots heal equally, delayed minutes preserved, empty hunger slows either slot");
    }

    static void run(DesktopPlatform& desktop, Canvas565& canvas) {
        checkBothRecover(desktop);
        checkRoomAvoidance(desktop);
        desktop.advanceMs(1000);
        AmoledApp app;
        app.begin(desktop.millis());
        app.gameState = {};
        app.gameState.oobeDone = true;
        app.gameState.teamCount = 2;
        for (int i = 0; i < 2; ++i) {
            app.gameState.team[i] = Game::MonsterFactory::create(i == 0 ? 1 : 7, 20);
            app.gameState.team[i].ivPacked = 123 + i;
        }
        app.sceneFlow.goHome();
        app.syncHomeActors(desktop.millis());
        Game::CareExperience::prepareDay(app.gameState);
        const uint32_t firstExp = app.gameState.team[0].exp;
        const uint32_t secondExp = app.gameState.team[1].exp;
        bite(app, desktop, 0);
        assert(app.gameState.team[0].exp == firstExp + 13);
        assert(app.gameState.team[1].exp == secondExp);
        bite(app, desktop, 1);
        assert(app.gameState.team[1].exp == secondExp + 13);

        // Both pets may level in one interaction: show both progressions in
        // order, and use the same evolution/move-learning flow as battle XP.
        for (int i = 0; i < 2; ++i) {
            auto& monster = app.gameState.team[i];
            monster = Game::MonsterFactory::create(i == 0 ? 1 : 7, 15);
            monster.ivPacked = 123 + i;
            monster.exp = minimumExpForLevel(findSpecies(monster.speciesId)->growthRate, 16) - 1;
            monster.move2Id = monster.move3Id = 0;
        }
        app.gameState.careRewardDay = UINT32_MAX;
        Game::CareExperience::prepareDay(app.gameState);
        app.syncHomeActors(desktop.millis());
        for (int i = 0; i < 2; ++i) {
            const auto result = Game::HomeCare::petMonster(app.gameState, i, 1);
            assert(result.careExp == 8);
            assert(app.applyHomeCareExperience(i, result.careExp) == 8);
            assert(app.gameState.team[i].level == 16);
        }
        assert(app.startPendingCareProgression(desktop.millis()));
        assert(app.progressionTeamSlot == 0);
        app.advanceProgression(desktop.millis());
        assert(app.progressionMode == ProgressionViewModel::Mode::EVOLUTION);
        assert(app.progressionToSpeciesId == 2);
        for (int tries = 0; tries < 20 && app.sceneFlow.current() == AppSceneFlow::Scene::PROGRESSION; ++tries) {
            desktop.advanceMs(20000);
            app.advanceProgression(desktop.millis());
        }
        assert(app.gameState.team[0].speciesId == 2);
        assert(app.sceneFlow.current() == AppSceneFlow::Scene::HOME);
        assert(app.startPendingCareProgression(desktop.millis()));
        assert(app.progressionTeamSlot == 1);
        for (int tries = 0; tries < 20 && app.sceneFlow.current() == AppSceneFlow::Scene::PROGRESSION; ++tries) {
            desktop.advanceMs(20000);
            app.advanceProgression(desktop.millis());
        }
        assert(app.gameState.team[1].speciesId == 8);
        assert(!app.startPendingCareProgression(desktop.millis()));

        // A care level-up crossing a learnset entry fills a free special slot.
        app.gameState.team[0] = Game::MonsterFactory::create(1, 8);
        app.gameState.team[0].ivPacked = 789;
        app.gameState.team[0].move2Id = app.gameState.team[0].move3Id = 0;
        app.gameState.team[0].exp = minimumExpForLevel(findSpecies(1)->growthRate, 9) - 1;
        const auto learnedPet = Game::HomeCare::petMonster(app.gameState, 0, 1);
        app.applyHomeCareExperience(0, learnedPet.careExp);
        assert(app.startPendingCareProgression(desktop.millis()));
        app.advanceProgression(desktop.millis());
        assert(app.gameState.team[0].move2Id != 0);
        assert(app.sceneFlow.current() == AppSceneFlow::Scene::HOME);

        // Restart resumes pending care progression without awarding XP twice.
        auto& monster = app.gameState.team[0];
        monster.exp = minimumExpForLevel(findSpecies(monster.speciesId)->growthRate, 10) - 1;
        const auto pet = Game::HomeCare::petMonster(app.gameState, 0, 2);
        app.applyHomeCareExperience(0, pet.careExp);
        const uint32_t savedExp = monster.exp;
        assert(app.saveState());
        AmoledApp rebooted;
        rebooted.begin(desktop.millis());
        rebooted.sceneFlow.goHome();
        assert(rebooted.gameState.team[0].exp == savedExp);
        assert(rebooted.startPendingCareProgression(desktop.millis()));
        assert(rebooted.gameState.team[0].exp == savedExp);
        rebooted.forceFullRender();
        rebooted.render(canvas);
        checkEeveeEvolution(desktop);
        checkWakeToFoodFacing(desktop);
        std::puts("home care: both actual feeding paths, progression queue, evolution, moves and reboot verified");
    }
};
}

void checkHomeCareRewards(DesktopPlatform& desktop, Canvas565& canvas) {
    AmoledV1::AmoledCareTestAccess::run(desktop, canvas);
}

void checkHomeRoomAuthority(DesktopPlatform& desktop, Canvas565& canvas) {
    AmoledV1::AmoledCareTestAccess::checkAuthoritativeRoomMotion(desktop, canvas);
}

void checkHomeAutomaticFoodTurns(DesktopPlatform& desktop, Canvas565& canvas) {
    AmoledV1::AmoledCareTestAccess::checkAutomaticFoodTurns(desktop, canvas);
}

void checkHomeFoodExitNoStall(DesktopPlatform& desktop, Canvas565& canvas) {
    AmoledV1::AmoledCareTestAccess::checkFoodExitNoStall(desktop, canvas);
}

void checkHomeFaintRecoveryAndSleep(DesktopPlatform& desktop, Canvas565& canvas) {
    AmoledV1::AmoledCareTestAccess::checkFaintRecoveryAndSleep(desktop, canvas);
}

void checkHomeSavedTurns(DesktopPlatform& desktop) {
    AmoledV1::AmoledCareTestAccess::checkSavedTurns(desktop);
}

void checkHomeYieldAndActions(DesktopPlatform& desktop, Canvas565& canvas) {
    AmoledV1::AmoledCareTestAccess::checkYieldTurnAndStationaryActions(desktop, canvas);
}
