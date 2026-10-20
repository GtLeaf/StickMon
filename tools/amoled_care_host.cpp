#include "AmoledApp.h"
#include "game/MonsterFactory.h"
#include "game/CareExperience.h"
#include "game/HomeCare.h"
#include "game/GameRandom.h"
#include "core/UiStrings.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/Canvas565.h"
#include <cassert>
#include <cstdio>
#include <cstring>
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
        app.homeFlow_.reset();
        app.homeFlow_.state_.nextAttentionMs = app.homeFlow_.state_.nextSpecialActionMs =
            app.pairFlow_.state_.nextInteractionMs = desktop.millis() + 3600000;
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
                    static_cast<unsigned>(app.homeFlow_.state().action));
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

    static void checkHudMessages(DesktopPlatform& desktop, Canvas565& canvas) {
        AmoledApp app;
        prepareRoom(app, desktop, 1);
        app.updateMoodHearts(desktop.millis());
        app.forceFullRender();
        presentRoom(app, canvas);
        const std::vector<uint16_t> normal(canvas.rawPixels(),
            canvas.rawPixels() + canvas.pixelCount());
        const auto now = desktop.millis();
        app.setToast(Ui::Menu::FOOD_ADDED, now);
        assert(app.toastUntil == now + 1500);
        assert(app.renderRowBegin() == HOME_STATUS_TOP && app.renderRowEnd() == 448);
        presentRoom(app, canvas);
        assert(std::equal(normal.begin(), normal.begin() + HOME_STATUS_TOP * 368,
                          canvas.rawPixels()));
        // The hidden menu and lock cannot be activated, even if the feedback
        // expires while the finger is held down.
        app.handleTap(120, 396, now);
        app.handleTap(44, 396, now);
        assert(app.sceneFlow.current() == AppSceneFlow::Scene::HOME);
        assert(!app.consumeLockRequest());
        app.handleTouch({TouchEventType::DOWN, 120, 396, 120, 396, now});
        assert(!app.pointerDown);
        desktop.advanceMs(1500);
        app.update(desktop.millis());
        assert(!app.toast);
        app.handleTouch({TouchEventType::UP, 120, 396, 120, 396, desktop.millis()});
        assert(app.sceneFlow.current() == AppSceneFlow::Scene::HOME);
        presentRoom(app, canvas);

        app.setToast(Ui::Menu::NO_FOOD, desktop.millis());
        assert(app.toastUntil == desktop.millis() + 2000);
        presentRoom(app, canvas);
        desktop.advanceMs(500);
        app.setToast(Ui::Menu::NO_FOOD, desktop.millis());
        assert(app.toastUntil == desktop.millis() + 2000);
        assert(app.renderRowBegin() == HOME_STATUS_TOP);
        presentRoom(app, canvas);

        // A new room action replaces the message immediately without a queue.
        app.gameState.room.food[0] = 2;
        app.gameState.room.bowlCount = app.gameState.room.bowlBitesRemaining = 0;
        putServing(app, desktop);
        assert(std::strcmp(app.toast, Ui::Menu::FOOD_ADDED) == 0);
        presentRoom(app, canvas);
        const auto oldExp = app.gameState.team[0].exp;
        app.handleTap(app.worldToScreenX(app.homeMainActor.x),
                      app.worldToScreenY(app.homeMainActor.y) - 20, desktop.millis());
        assert(app.heartsUntil != 0);
        assert(app.gameState.team[0].exp >= oldExp);

        // Scene changes discard feedback in both directions, including a
        // transition rendered before the next update tick.
        app.sceneFlow.openMenu();
        app.forceFullRender();
        assert(!app.toast);
        app.setToast(Ui::Menu::NO_FOOD, desktop.millis());
        assert(app.toastUntil == desktop.millis() + 1100);
        app.sceneFlow.goHome();
        app.forceFullRender();
        assert(!app.toast);
        presentRoom(app, canvas);
        app.handleTap(120, 396, desktop.millis());
        assert(app.sceneFlow.current() == AppSceneFlow::Scene::MAIN_MENU);
        std::puts("home HUD: feedback damage, timing, replacement, room taps, hidden gestures and scene cleanup verified");
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
            assert(app.homeFlow_.beginMove(approach, 125, 125, desktop.millis()));
            app.applyHomeFlowEffects(desktop.millis());
            bool waiting = false;
            for (int frame = 0; frame < 300; ++frame) {
                desktop.advanceMs(50);
                app.updatePet(desktop.millis());
                assertRoomState(app);
                if (frame % 20 == 0) presentRoom(app, canvas);
                waiting = app.homeFlow_.state().action == AmoledApp::RoomAction::ATTENTION_WAIT ||
                          app.homeFlow_.state().action == AmoledApp::RoomAction::WINDOW_WAIT;
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
                assert(app.progressionFlow_.state().progressionMode == ProgressionViewModel::Mode::EVOLUTION);
                assert(app.progressionFlow_.state().progressionToSpeciesId == 196);
                assert(app.progressionFlow_.state().progressionEvolutionItem == Game::ItemId::COUNT);
                app.advanceProgression(desktop.millis());
                assert(monster.speciesId == 133);
                if (test.cancel) {
                    assert(app.progressionFlow_.cancelEvolution(desktop.millis()));
                    desktop.advanceMs(Game::EvolutionSequence::CANCEL_COMPLETE_MS);
                    app.handleTap(0, 0, desktop.millis());
                }
            } else {
                assert(app.progressionFlow_.state().progressionMode != ProgressionViewModel::Mode::EVOLUTION);
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

    static void checkAmbientSequence(DesktopPlatform& desktop, Canvas565& canvas,
                                     bool wrap) {
        uint16_t seen = 0;
        for (uint32_t seed = 1; seed <= 64; ++seed) {
            AmoledApp app;
            prepareRoom(app, desktop);
            auto& actor = app.homeMainActor;
            actor.x = actor.targetX = 100;
            actor.y = actor.targetY = 125;
            app.homeCompanionActor.x = app.homeCompanionActor.targetX = 155;
            app.homeCompanionActor.y = app.homeCompanionActor.targetY = 115;
            actor.behavior.activityBias = static_cast<int8_t>(seed % 3) - 1;
            app.configureHomeRuntime();
            app.updateCamera();
            presentRoom(app, canvas);
            const uint32_t base = wrap ? 0xFFFFF000U : 1000U;
            actor.nextDecisionMs = base + 60000;
            app.nextPetFrameMs = base + 140;
            GameRandom::seed(seed);
            assert(app.homeFlow_.startAutonomousAction(
                app.homeActionLayout(), app.petDirection, base));
            app.applyHomeFlowEffects(base);
            const auto selected = app.homeFlow_.state().action;
            seen |= 1U << static_cast<uint8_t>(selected);
            uint64_t hash = 1469598103934665603ULL;
            const auto add = [&](uint32_t value) {
                for (uint8_t byte = 0; byte < 4; ++byte) {
                    hash = (hash ^ (value & 255U)) * 1099511628211ULL;
                    value >>= 8;
                }
            };
            const auto number = [&](float value) {
                uint32_t bits;
                std::memcpy(&bits, &value, sizeof(bits));
                add(bits);
            };
            int frames = 0;
            do {
                add(static_cast<uint8_t>(app.homeFlow_.state().action));
                add(app.homeFlow_.state().phase);
                add(static_cast<uint8_t>(actor.task));
                add(static_cast<uint8_t>(actor.motion));
                number(actor.x); number(actor.y);
                number(actor.targetX); number(actor.targetY);
                add(actor.route.count); add(actor.route.index);
                add(static_cast<uint8_t>(app.petDirection)); add(app.petFrame);
                add(app.nextPetFrameMs - base); add(actor.nextDecisionMs - base);
                add(app.homeFlow_.state().nextSpecialActionMs - base);
                add(app.needsRender()); add(app.renderRowBegin()); add(app.renderRowEnd());
                if (frames % 13 == 0) presentRoom(app, canvas);
                if (app.homeFlow_.state().action == AmoledApp::RoomAction::NONE) break;
                ++frames;
                assert(frames <= 400);
                app.updateRoomAction(base + frames * 50U, 0.05f);
                assertRoomState(app);
            } while (true);
            presentRoom(app, canvas);
            std::printf("HomeSequence seed=%u action=%u frames=%d hash=%016llx rng=%08x\n",
                seed, static_cast<unsigned>(selected), frames,
                static_cast<unsigned long long>(hash), GameRandom::next());
        }
        for (const auto action : {AmoledApp::RoomAction::LOOK_AROUND,
                AmoledApp::RoomAction::QUIET_GAZE, AmoledApp::RoomAction::STEP_BACK,
                AmoledApp::RoomAction::CIRCLE, AmoledApp::RoomAction::DASH}) {
            assert(seen & (1U << static_cast<uint8_t>(action)));
        }
    }

    static void checkPairSequence(DesktopPlatform& desktop, Canvas565& canvas, bool wrap) {
        for (uint32_t seed = 1; seed <= 16; ++seed) for (uint8_t scenario = 0; scenario < 10; ++scenario) {
            AmoledApp app;
            prepareRoom(app, desktop);
            const uint32_t base = wrap ? 0xFFFFF000U : 1000U;
            if (scenario == 3 || scenario == 4) {
                const uint8_t slot = scenario == 3 ? 0 : 1;
                app.gameState.team[slot] = Game::MonsterFactory::create(129, 20);
                app.syncHomeActors(desktop.millis());
                app.updatePetFootprint();
            }
            for (uint8_t slot = 0; slot < 2; ++slot) {
                app.homeRuntime.stop(slot, base, 0);
                app.gameState.team[slot].mood = 95;
                app.gameState.team[slot].satiety = 100;
                app.gameState.team[slot].nature = 0;
            }
            app.homeMainActor.x = app.homeMainActor.targetX = 103;
            app.homeMainActor.y = app.homeMainActor.targetY = 125;
            app.homeCompanionActor.x = app.homeCompanionActor.targetX = 155;
            app.homeCompanionActor.y = app.homeCompanionActor.targetY = 115;
            app.configureHomeRuntime(); app.updateCamera(); presentRoom(app, canvas);
            app.nextPetFrameMs = app.nextCompanionFrameMs = base + 140;
            app.gameState.pairMoodRewardsToday = scenario == 9 ? 3 : 0;
            const bool arrival = scenario == 2 || scenario == 3 || scenario == 4 || scenario == 9;
            if (arrival) {
                app.visitorFlow_.state_.motion = AmoledApp::VisitorMotion::ACTIVE;
                app.visitArrival.begin(base);
            }
            app.requestFullRender();
            GameRandom::seed(seed);
            const bool started = app.startPairInteraction(base, scenario == 1, arrival, arrival);
            assert(started);
            uint64_t hash = 1469598103934665603ULL;
            const auto add = [&](uint32_t value) {
                for (uint8_t byte = 0; byte < 4; ++byte) {
                    hash = (hash ^ (value & 255U)) * 1099511628211ULL; value >>= 8;
                }
            };
            const auto number = [&](float value) {
                uint32_t bits; std::memcpy(&bits, &value, sizeof(bits)); add(bits);
            };
            int frames = 0;
            do {
                add(static_cast<uint8_t>(app.pairFlow_.state_.phase)); add(static_cast<uint8_t>(app.pairFlow_.state_.activity));
                add(app.pairFlow_.state_.leaderMain); add(app.pairFlow_.state_.arrivalTalk);
                add(app.pairFlow_.state_.parallelApproach); add(app.pairFlow_.state_.mainArrived); add(app.pairFlow_.state_.companionArrived);
                add(app.pairFlow_.state_.phaseUntilMs - base); add(app.pairFlow_.state_.interactionUntilMs - base);
                add(app.pairFlow_.state_.chaseLegsRemaining); add(app.pairFlow_.state_.nextInteractionMs - base);
                number(app.pairFlow_.state_.mainRenderOffsetY); number(app.pairFlow_.state_.companionRenderOffsetY);
                number(app.cameraX); number(app.cameraY);
                add(static_cast<uint8_t>(app.petDirection)); add(static_cast<uint8_t>(app.companionDirection));
                add(app.petFrame); add(app.companionFrame); add(app.nextPetFrameMs-base); add(app.nextCompanionFrameMs-base);
                add(app.heartsUntil == 0 ? 0 : app.heartsUntil-base);
                add(static_cast<uint8_t>(app.visitArrival.phase()));
                add(app.gameState.pairMoodRewardsToday);
                for (uint8_t slot = 0; slot < 2; ++slot) {
                    const auto& actor = *app.homeRuntime.actor(slot);
                    add(static_cast<uint8_t>(actor.task)); add(static_cast<uint8_t>(actor.motion));
                    number(actor.x); number(actor.y); number(actor.targetX); number(actor.targetY);
                    number(actor.velocityX); number(actor.velocityY);
                    add(actor.route.count); add(actor.route.index); add(actor.nextDecisionMs-base);
                    add(app.gameState.team[slot].exp); add(app.gameState.team[slot].mood);
                }
                add(app.needsRender()); add(app.renderRowBegin()); add(app.renderRowEnd());
                if (frames % 67 == 0) presentRoom(app, canvas);
                if (app.pairFlow_.state_.phase == AmoledApp::PairPhase::NONE) break;
                assert(++frames <= 1000);
                const uint32_t now = base + frames * 50U;
                if (frames == 5) {
                    app.requestFullRender();
                    if (scenario == 5) app.gameState.team[1].satiety = 25;
                    if (scenario == 6) app.visitorFlow_.state_.motion = AmoledApp::VisitorMotion::EXITING;
                    if (scenario == 7) app.cancelPairInteraction(now);
                    if (scenario == 8) app.homeMainActor.route.clear();
                    if (scenario == 2) app.gameState.team[0].satiety = 0;
                }
                if (app.pairFlow_.state_.phase != AmoledApp::PairPhase::NONE) app.updatePairInteraction(now, 0.05f);
                assert(app.homeRuntime.validate() == Home::INVARIANT_OK);
            } while (true);
            const auto mood = app.gameState.pairMoodRewardsToday;
            const auto experience = app.gameState.team[0].exp;
            app.finishPairInteraction(base + frames * 50U, true);
            assert(app.gameState.pairMoodRewardsToday == mood && app.gameState.team[0].exp == experience);
            presentRoom(app, canvas);
            std::printf("PairSequence seed=%u scenario=%u frames=%d hash=%016llx rng=%08x\n",seed,scenario,frames,
                static_cast<unsigned long long>(hash), GameRandom::next());
        }
    }

    static void checkVisitorSequence(DesktopPlatform& desktop, Canvas565& canvas, bool wrap) {
        for (uint32_t seed = 1; seed <= 8; ++seed) for (uint8_t scenario = 0; scenario < 8; ++scenario) {
            AmoledApp app; prepareRoom(app, desktop);
            const uint32_t base = wrap ? 0xFFFFF000U : 1000U;
            auto& guest = app.gameState.team[1];
            guest = Game::MonsterFactory::create(scenario == 1 ? 129 : 172, 10);
            guest.origin = Game::Origin::VISITOR; guest.satiety = 100; guest.nature = 0;
            app.syncHomeActors(base); app.updatePetFootprint();
            app.homeMainActor.x = app.homeMainActor.targetX = scenario == 2 ? 231 : 100;
            app.homeMainActor.y = app.homeMainActor.targetY = scenario == 2 ? 155 : 125;
            app.configureHomeRuntime(); app.updateCamera(); presentRoom(app, canvas);
            GameRandom::seed(seed);
            app.beginVisitorEntry(base);
            uint64_t hash = 1469598103934665603ULL;
            const auto add = [&](uint32_t value) {
                for (uint8_t byte=0; byte<4; ++byte) { hash=(hash^(value&255U))*1099511628211ULL; value>>=8; }
            };
            const auto number = [&](float value) {uint32_t bits; std::memcpy(&bits,&value,sizeof(bits));add(bits);};
            bool exited = false; int frames = 0; bool activeSeen = false;
            do {
                add(static_cast<uint8_t>(app.visitorFlow_.state_.motion)); add(app.visitorFlow_.state_.hostDoorPhase);
                add(app.visitorFlow_.state_.crossingDoor); add(app.visitorFlow_.state_.untilMs-base);
                add(app.visitorFlow_.state_.welcomeValid); number(app.visitorFlow_.state_.welcomeX); number(app.visitorFlow_.state_.welcomeY);
                add(app.visitorFlow_.state_.landingValid); number(app.visitorFlow_.state_.landingX); number(app.visitorFlow_.state_.landingY);
                add(static_cast<uint8_t>(app.visitArrival.phase())); add(app.gameState.teamCount);
                add(static_cast<uint8_t>(app.pairFlow_.state().phase));
                number(app.cameraX);number(app.cameraY);add(app.petFrame);add(app.companionFrame);
                add(app.nextPetFrameMs-base);add(app.nextCompanionFrameMs-base);
                add(static_cast<uint8_t>(app.petDirection));add(static_cast<uint8_t>(app.companionDirection));
                for (uint8_t slot=0;slot<2;++slot) {
                    const auto& a=slot==0?app.homeMainActor:app.homeCompanionActor;
                    number(a.x);number(a.y);number(a.targetX);number(a.targetY);
                    number(a.velocityX);number(a.velocityY);add(a.hidden);add(a.active);
                    add(static_cast<uint8_t>(a.task));add(static_cast<uint8_t>(a.motion));
                    add(a.route.count);add(a.route.index);
                }
                add(static_cast<uint32_t>(app.homeRuntime.owner(Home::Resource::DOOR)));
                add(app.needsRender());add(app.renderRowBegin());add(app.renderRowEnd());
                if(frames%79==0)presentRoom(app,canvas);
                if(exited && app.visitorFlow_.state_.motion==AmoledApp::VisitorMotion::NONE)break;
                assert(++frames<=1100);const uint32_t now=base+frames*50U;
                if(frames==1) app.updateVisitorMotion(base,0.0f);
                if(scenario==3 && frames==2)app.homeMainActor.route.clear();
                if(scenario==4 && app.visitorFlow_.state_.motion==AmoledApp::VisitorMotion::ENTERING) app.visitorFlow_.state_.untilMs=now;
                if(scenario==5 && app.visitorFlow_.state_.hostDoorPhase==2)app.visitorFlow_.state_.untilMs=now;
                if(scenario==6 && !exited && app.visitorFlow_.state_.motion==AmoledApp::VisitorMotion::ENTERING) {
                    app.beginVisitorExit(now,false);exited=true;
                }
                if(app.visitorFlow_.state_.motion==AmoledApp::VisitorMotion::ACTIVE && !exited) {
                    activeSeen=true;
                    app.cancelPairInteraction(now);
                    app.beginVisitorExit(now,false);exited=true;
                    if(scenario==7) {app.homeCompanionActor.route.clear();app.visitorFlow_.state_.crossingDoor=true;}
                }
                app.updateVisitorMotion(now,0.05f);
                assert(app.homeRuntime.validate()==Home::INVARIANT_OK);
            }while(true);
            assert(activeSeen || scenario==6);
            assert(app.gameState.teamCount==1 && app.homeRuntime.owner(Home::Resource::DOOR)<0);
            presentRoom(app,canvas);
            std::printf("VisitorSequence seed=%u scenario=%u frames=%d hash=%016llx rng=%08x\n",seed,scenario,frames,
                static_cast<unsigned long long>(hash),GameRandom::next());
        }
    }

    static void checkVisitorEffects(DesktopPlatform& desktop) {
        for (bool wrap : {false, true}) {
            const uint32_t base = wrap ? 0xFFFFF000U : 1000U;
            for (int scenario = 0; scenario < 3; ++scenario) {
                AmoledApp app; prepareRoom(app, desktop);
                auto& guest = app.gameState.team[1];
                guest = Game::MonsterFactory::create(172, 10);
                guest.origin = Game::Origin::VISITOR;
                app.syncHomeActors(base); app.updatePetFootprint();
                app.configureHomeRuntime();
                app.beginVisitorEntry(base);
                assert(app.visitorFlow_.state().motion == AmoledApp::VisitorMotion::HOST_APPROACH);
                assert(app.homeRuntime.owner(Home::Resource::DOOR) == 0);
                if (scenario == 0) {
                    // Early cancellation used to leave the host owning DOOR.
                    app.beginVisitorExit(base + 1, false);
                    assert(app.homeRuntime.owner(Home::Resource::DOOR) == 1);
                    assert(app.homeMainActor.task != Home::Task::DOOR_ACTION);
                    assert(app.homeRuntime.validate() == Home::INVARIANT_OK);
                    for (uint32_t ms = 50; ms < 25000 && app.gameState.teamCount > 1; ms += 50)
                        app.updateVisitorMotion(base + ms, 0.05f);
                    assert(app.gameState.teamCount == 1);
                    assert(app.homeRuntime.owner(Home::Resource::DOOR) < 0);
                    const auto end = app.visitorFlow_.takeEffects();
                    assert(!end.exitFinished && !end.entryFinished && !end.needLanding);
                } else if (scenario == 1) {
                    // Exploration cancels an entry and releases both door leases.
                    app.visitorFlow_.cancelEntryForDeparture();
                    assert(app.visitorFlow_.state().motion == AmoledApp::VisitorMotion::NONE);
                    assert(app.homeRuntime.owner(Home::Resource::DOOR) < 0);
                    app.markRendered();
                    app.updateVisitorMotion(base + 50, 0.05f);
                    assert(!app.needsRender());
                } else {
                    // The landing acknowledgement must be consumed only once.
                    auto layout = app.visitorLayout();
                    app.visitorFlow_.state_.hostDoorPhase = 1;
                    app.visitArrival.advance(Home::VisitArrivalEvent::HOST_READY, base);
                    app.visitorFlow_.update(layout, base + 500, 0.05f);
                    app.visitorFlow_.takeEffects();
                    // A host-clear route can precede revealing the visitor.
                    for (uint32_t ms = 550; !app.visitorFlow_.landingPending_ && ms < 20000; ms += 50) {
                        app.visitorFlow_.update(layout, base + ms, 0.05f);
                        app.visitorFlow_.takeEffects();
                        app.visitorFlow_.completeMovement(layout, base + ms);
                        app.visitorFlow_.takeEffects();
                    }
                    assert(app.visitorFlow_.landingPending_);
                    app.visitorFlow_.completeLanding(true, 100, 120);
                    const auto landed = app.visitorFlow_.takeEffects();
                    assert(landed.resetFrameMask == 2 && landed.fullRender);
                    app.visitorFlow_.completeLanding(true, 0, 0);
                    const auto duplicate = app.visitorFlow_.takeEffects();
                    assert(!duplicate.resetFrameMask && !duplicate.fullRender && !duplicate.traceCount);
                    app.visitorFlow_.cancelEntryForDeparture();
                    assert(app.homeRuntime.owner(Home::Resource::DOOR) < 0);
                }
            }
        }
        std::puts("visitor effects: early exit lease handoff, cancellation, landing acknowledgement and wrap verified");
    }

    static void checkVisitAwayEffects(DesktopPlatform& desktop) {
        using Phase = VisitAwayFlow::Phase;
        for (bool wrap : {false, true}) for (int scenario = 0; scenario < 3; ++scenario) {
            const uint32_t base = wrap ? 0xFFFFF000U : 1000U;
            AmoledApp app; prepareRoom(app, desktop);
            const auto layout = app.visitorLayout();
            auto& flow = app.visitAwayFlow_;
            const VisitAwayFlow::Door door{scenario == 0, layout.insideX, layout.insideY,
                                           layout.outsideX, layout.outsideY};
            flow.begin(door, base);
            auto effects = flow.takeEffects();
            assert(effects.resetAnimation && effects.fullRender);
            assert(!flow.takeEffects().resetAnimation);
            bool crossed = false, departed = false;
            for (uint32_t ms = 50; ms < 25000 && !departed; ms += 50) {
                const auto previous = flow.state().phase;
                assert(flow.update(true, base + ms, scenario == 2 ? 0.0f : 0.05f));
                effects = flow.takeEffects();
                assert(effects.camera && effects.actorChanged);
                flow.completeMovement(base + ms);
                effects = flow.takeEffects();
                if (flow.state().phase == Phase::CROSS_DOOR) {
                    crossed = true;
                    if (previous == Phase::WALK_TO_DOOR) {
                        assert(app.homeMainActor.x == door.insideX && app.homeMainActor.y == door.insideY);
                        assert(!app.homeMainActor.route.count);
                    }
                }
                if (effects.departed) { assert(effects.fullRender); departed = true; }
            }
            assert(crossed && departed && flow.state().phase == Phase::AWAY && app.homeMainActor.hidden);
            assert(flow.update(true, base + 25000, 0.05f));
            assert(!flow.takeEffects().departed);
            assert(!flow.update(false, base + 25001, 0.0f));
            effects = flow.takeEffects();
            assert(effects.clearRecall && effects.stopAnimation && !effects.resetAnimation);
            assert(flow.state().phase == Phase::NONE && !app.homeMainActor.hidden);
            assert(app.homeMainActor.x == door.insideX && app.homeMainActor.y == door.insideY);
            assert(app.homeRuntime.validate() == Home::INVARIANT_OK);
            // Cancellation while approaching preserves the pose and animation deadline.
            flow.begin(door, base + 26000); flow.takeEffects();
            const float x = app.homeMainActor.x, y = app.homeMainActor.y;
            app.nextPetFrameMs = base + 28000;
            flow.update(false, base + 26001, 0.0f); app.applyVisitAwayEffects(base + 26001);
            assert(app.nextPetFrameMs == base + 28000);
            assert(app.homeMainActor.x == x && app.homeMainActor.y == y);
            flow.begin(door, base + 27000); flow.takeEffects();
            flow.update(true, base + 40001, 0.0f); flow.takeEffects(); flow.completeMovement(base + 40001);
            assert(flow.state().phase == Phase::CROSS_DOOR);
            flow.takeEffects(); flow.update(false, base + 40002, 0.0f);
            assert(flow.state().phase == Phase::NONE && !app.homeMainActor.hidden);
        }
        std::puts("visit away: routed/direct/timeout departure, once-only completion, cancellation, frame clock and wrap verified");
    }

    static void checkPairEffects(DesktopPlatform& desktop) {
        const uint32_t now = 1000;
        {
            AmoledApp app; prepareRoom(app, desktop);
            app.markRendered();
            assert(!app.updatePairInteraction(desktop.millis(), 0.05f));
            assert(!app.needsRender());
            const auto empty = app.pairFlow_.takeEffects();
            assert(!empty.roomChanged && !empty.camera && !empty.finished && !empty.frameMask);
            app.schedulePairInteraction(now, true);
            assert(!app.pairFlow_.needsStart(now + 299) && app.pairFlow_.needsStart(now + 300));
        }
        // Termination must release both routes/resources and emit one completion.
        for (int reason = 0; reason < 4; ++reason) {
            AmoledApp app; prepareRoom(app, desktop);
            assert(app.startPairInteraction(now, true));
            if (reason == 0) app.gameState.team[0].majorStatus = Game::MajorStatus::SLEEP;
            if (reason == 1) app.gameState.team[1].hpCur = 0;
            if (reason == 2) app.gameState.team[0].satiety = 25;
            if (reason == 3) app.visitorFlow_.state_.motion = AmoledApp::VisitorMotion::EXITING;
            const auto exp = app.gameState.team[0].exp;
            assert(!app.updatePairInteraction(now + 50, 0.05f));
            assert(!app.homeRuntime.pairActive());
            assert(!app.homeMainActor.route.count && !app.homeCompanionActor.route.count);
            assert(app.gameState.team[0].exp == exp && app.gameState.pairMoodRewardsToday == 0);
            assert(app.heartsUntil == 0);
        }
        for (bool nearGoal : {false, true}) {
            AmoledApp app; prepareRoom(app, desktop);
            app.visitorFlow_.state_.motion = AmoledApp::VisitorMotion::ACTIVE;
            app.visitArrival.begin(now - 420);
            assert(app.visitArrival.advance(Home::VisitArrivalEvent::HOST_READY, now - 420));
            assert(app.visitArrival.advance(Home::VisitArrivalEvent::DOOR_OPENED, now));
            assert(app.visitArrival.advance(Home::VisitArrivalEvent::GUEST_ENTERING, now));
            assert(app.visitArrival.advance(Home::VisitArrivalEvent::GUEST_INSIDE, now));
            assert(app.startPairInteraction(now, false, true, true));
            app.pairFlow_.state_.phaseUntilMs = now;
            if (nearGoal) {
                app.homeMainActor.x = app.homeMainActor.targetX - 0.5f;
                app.homeMainActor.y = app.homeMainActor.targetY;
                app.homeCompanionActor.x = app.homeCompanionActor.targetX - 0.5f;
                app.homeCompanionActor.y = app.homeCompanionActor.targetY;
            }
            const bool active = app.updatePairInteraction(now, 0.0f);
            assert(active == nearGoal);
            if (!nearGoal) {
                assert(app.visitArrival.phase() == Home::VisitArrivalPhase::COMPLETE);
                assert(!app.homeRuntime.pairActive());
                continue;
            }
            assert(app.pairFlow_.state().phase == AmoledApp::PairPhase::INVITE);
            assert(app.homeMainActor.x == app.homeMainActor.targetX);
            assert(app.homeCompanionActor.x == app.homeCompanionActor.targetX);
            app.updatePairInteraction(now + 750, 0.0f);
            assert(app.visitArrival.phase() == Home::VisitArrivalPhase::TALK);
            auto& flow = app.pairFlow_;
            flow.finish(now + 800, true);
            flow.finish(now + 800, true);
            const auto done = flow.takeEffects();
            assert(done.finished && done.reward && done.stopAnimation && done.roomChanged);
            assert(app.visitArrival.phase() == Home::VisitArrivalPhase::COMPLETE);
            assert(!app.homeRuntime.pairActive());
            assert(!flow.completeMovement(app.pairLayout(), now + 800));
            const auto empty = flow.takeEffects();
            assert(!empty.finished && !empty.reward && !empty.roomChanged && !empty.frameMask);
        }
        std::puts("pair effects: idle, deadlines, health/hunger/exit cancellation, arrival timeout/tolerance and one-shot completion verified");
    }

    // Effect-contract checks apply only to the extracted owner.
    static void checkAmbientEffects(DesktopPlatform& desktop) {
        AmoledApp first, second;
        prepareRoom(first, desktop, 1);
        prepareRoom(second, desktop, 1);
        auto& flow = first.homeFlow_;
        const auto layout = first.homeActionLayout();
        float x = 0, y = 0;
        assert(flow.chooseAttentionPose(layout, x, y));
        first.homeMainActor.x = x;
        first.homeMainActor.y = y;
        constexpr uint32_t base = 0xFFFFFFF0U;
        assert(flow.startAttention(layout, first.petDirection, base));
        auto effects = flow.takeEffects();
        assert(effects.heart == HomeFlow::Heart::SHOW && !effects.save);
        assert(effects.heartsUntilMs == base + 6000U);
        assert(flow.takeEffects().heart == HomeFlow::Heart::NONE);
        assert(second.homeFlow_.state().action == AmoledApp::RoomAction::NONE);
        assert(flow.update(layout, base + 5999U, 0.05f, false).active);
        effects = flow.takeEffects();
        assert(!effects.roomChanged && !effects.save && !effects.scheduleDecision);
        assert(flow.update(layout, base + 6000U, 0.05f, false).active);
        effects = flow.takeEffects();
        assert(effects.scheduleDecision && effects.roomChanged);
        assert(flow.state().action == AmoledApp::RoomAction::NONE);
        assert(!flow.update(layout, base + 6050U, 0.05f, false).active);
        effects = flow.takeEffects();
        assert(!effects.roomChanged && !effects.save && !effects.scheduleDecision);
        assert(flow.startAttention(layout, first.petDirection, 7000));
        flow.takeEffects();
        flow.cancel(false, 7100);
        effects = flow.takeEffects();
        assert(effects.heart == HomeFlow::Heart::CLEAR && effects.scheduleDecision);
        flow.cancel(false, 7150);
        assert(!flow.takeEffects().scheduleDecision);
        // A failed exit must retain the original bowl-clearance retry.
        flow.startFeedFinish(8000);
        flow.takeEffects();
        flow.cancel(true, 8100);
        assert(flow.state().action == AmoledApp::RoomAction::FEED_FINISH);
        assert(flow.state().untilMs == 8400);
        flow.takeEffects();
        flow.cancel(false, 8500);
        assert(flow.state().action == AmoledApp::RoomAction::NONE);
        flow.takeEffects();
        // Window persistence is emitted on arrival, and consumed once.
        assert(flow.beginMove(AmoledApp::RoomAction::WINDOW_APPROACH, x, y, 9000));
        flow.takeEffects();
        flow.completeMovement(layout, Home::RouteStep::ARRIVED, false, 9050);
        effects = flow.takeEffects();
        assert(effects.save && first.gameState.team[0].lastWindowGazeAt == 12U * 60U * 60U);
        assert(flow.state().action == AmoledApp::RoomAction::WINDOW_WAIT);
        assert(!flow.takeEffects().save);
        flow.update(layout, 9100, 0.05f, false);
        effects = flow.takeEffects();
        assert(!effects.save && !effects.roomChanged);
        assert(!flow.tryAmbient(layout, first.petDirection, 9150));
        assert(flow.state().action == AmoledApp::RoomAction::WINDOW_WAIT);
        // Exercise the production survival handoff while attention is active.
        prepareRoom(first, desktop, 1);
        first.homeMainActor.x = x;
        first.homeMainActor.y = y;
        const uint32_t now = desktop.millis();
        assert(flow.startAttention(layout, first.petDirection, now));
        first.applyHomeFlowEffects(now);
        first.gameState.team[0].satiety = 10;
        first.gameState.room.bowlCount = 0;
        first.updatePet(now + 50);
        assert(flow.state().action == AmoledApp::RoomAction::NONE);
        assert(first.heartsUntil == 0);
        assert(first.homeMainActor.task != Home::Task::ROOM_ACTION);
        std::puts("HomeFlow effects: independent state, one-shot save/hearts, stationary waits, cancellation, clearance retry and clock wrap passed");
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
            app.homeFlow_.reset();
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
        assert(app.progressionFlow_.state().progressionTeamSlot == 0);
        app.advanceProgression(desktop.millis());
        assert(app.progressionFlow_.state().progressionMode == ProgressionViewModel::Mode::EVOLUTION);
        assert(app.progressionFlow_.state().progressionToSpeciesId == 2);
        for (int tries = 0; tries < 20 && app.sceneFlow.current() == AppSceneFlow::Scene::PROGRESSION; ++tries) {
            desktop.advanceMs(20000);
            app.advanceProgression(desktop.millis());
        }
        assert(app.gameState.team[0].speciesId == 2);
        assert(app.sceneFlow.current() == AppSceneFlow::Scene::HOME);
        assert(app.startPendingCareProgression(desktop.millis()));
        assert(app.progressionFlow_.state().progressionTeamSlot == 1);
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

void checkHomeHudMessages(DesktopPlatform& desktop, Canvas565& canvas) {
    AmoledV1::AmoledCareTestAccess::checkHudMessages(desktop, canvas);
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

void checkHomeAmbientSequence(DesktopPlatform& desktop, Canvas565& canvas, bool wrap) {
    AmoledV1::AmoledCareTestAccess::checkAmbientSequence(desktop, canvas, wrap);
}

void checkHomeAmbientEffects(DesktopPlatform& desktop) {
    AmoledV1::AmoledCareTestAccess::checkAmbientEffects(desktop);
}

void checkPairSequence(DesktopPlatform& desktop, Canvas565& canvas, bool wrap) {
    AmoledV1::AmoledCareTestAccess::checkPairSequence(desktop, canvas, wrap);
}

void checkPairEffects(DesktopPlatform& desktop) {
    AmoledV1::AmoledCareTestAccess::checkPairEffects(desktop);
}

void checkVisitorSequence(DesktopPlatform& desktop, Canvas565& canvas, bool wrap) {
    AmoledV1::AmoledCareTestAccess::checkVisitorSequence(desktop, canvas, wrap);
}

void checkVisitorEffects(DesktopPlatform& desktop) {
    AmoledV1::AmoledCareTestAccess::checkVisitorEffects(desktop);
}

void checkVisitAwayEffects(DesktopPlatform& desktop) {
    AmoledV1::AmoledCareTestAccess::checkVisitAwayEffects(desktop);
}
