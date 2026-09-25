#include "AmoledApp.h"
#include "HomeScreen.h"
#include "assets/PokemonSprites.h"
#include "core/RoomResource.h"
#include "game/ExploreCaveTiles.h"
#include "game/ExploreIceSlide.h"
#include "game/ExploreRouteGeometry.h"
#include "game/MonsterFactory.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/Canvas565.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

namespace AmoledV1 {

// Exercise real application transitions; only initial state and diagnostics need access.
struct AmoledAppTestAccess {
    using Phase = AmoledApp::ExpeditionDeparturePhase;
    using Scene = AppSceneFlow::Scene;

    static void tap(AmoledApp& app, DesktopPlatform& desktop, int x, int y) {
        TouchEvent event;
        event.x = x;
        event.y = y;
        event.timestampMs = desktop.millis();
        event.type = TouchEventType::DOWN;
        app.handleTouch(event);
        event.type = TouchEventType::UP;
        app.handleTouch(event);
    }

    static void checkDisplayLockArbitration(DesktopPlatform& desktop,
                                            const Game::GameState& state) {
        AmoledApp app;
        app.begin(desktop.millis());
        app.gameState = state;
        app.gameState.settings.idleTimeoutIndex = 0;
        app.sceneFlow.goHome();

        app.lockRequested = true;
        TouchEvent down;
        down.x = 10;
        down.y = 10;
        down.timestampMs = desktop.millis();
        down.type = TouchEventType::DOWN;
        app.handleTouch(down);
        assert(!app.consumeLockRequest());
        app.pointerDown = false;

        app.lockRequested = true;
        assert(app.queueExploreDeparture(0, false));
        assert(!app.consumeLockRequest());
        assert(!app.displayLockAllowed());

        desktop.advanceMs(31'000);
        app.update(desktop.millis());
        assert(!app.consumeLockRequest());
        assert(!app.displayLockAllowed());

        app.pendingExpedition = false;
        app.expeditionDeparturePhase = Phase::NONE;
        app.expeditionFade.reset();
        assert(app.displayLockAllowed());
        std::puts("display lock arbitration: transition owns the display");
    }

    static void checkMoveReplacement(DesktopPlatform& desktop,
                                     const Game::GameState& state) {
        for (int selection : {0, 1, 2}) {
            AmoledApp app;
            app.begin(desktop.millis());
            app.gameState = state;
            app.gameState.team[0].move2Id = 33;
            app.gameState.team[0].move3Id = 44;
            app.gameState.team[0].moveProficiency[1] = 70;
            app.gameState.team[0].moveProficiency[2] = 80;
            app.sceneFlow.enter(Scene::PROGRESSION);
            app.progressionMode = ProgressionViewModel::Mode::MOVE_REPLACE;
            app.progressionTeamSlot = 0;
            app.progressionOldLevel = app.gameState.team[0].level;
            app.progressionMoveId = 22;
            app.progressionOldMove2 = 33;
            app.progressionOldMove3 = 44;
            tap(app, desktop, 328, 38);
            assert(app.progressionMode == ProgressionViewModel::Mode::MOVE_REPLACE);
            assert(app.gameState.team[0].move2Id == 33);
            assert(app.gameState.team[0].move3Id == 44);
            tap(app, desktop, 160, 106 + selection * 66);
            assert(app.progressionSelectedItem == selection);
            assert(app.progressionMode == ProgressionViewModel::Mode::MOVE_REPLACE);
            assert(app.gameState.team[0].move2Id == 33);
            assert(app.gameState.team[0].move3Id == 44);
            tap(app, desktop, 328, 38);
            assert(app.gameState.team[0].move2Id == (selection == 1 ? 22 : 33));
            assert(app.gameState.team[0].move3Id == (selection == 2 ? 22 : 44));
            if (selection == 1) assert(app.gameState.team[0].moveProficiency[1] == 0);
            if (selection == 2) assert(app.gameState.team[0].moveProficiency[2] == 0);
        }
    }

#if STICKMON_ENABLE_DEBUG_FEATURES
    static void checkVisitorInvalidHostEntry(
        DesktopPlatform& desktop, const Game::GameState& state) {
        AmoledApp app;
        app.begin(desktop.millis());
        app.gameState = state;
        app.gameState.teamCount = 1;
        app.gameState.storageCount = 1;
        app.gameState.storage[0] = Game::MonsterFactory::create(172, 5);
        app.gameState.settings.idleTimeoutIndex = 4;
        app.sceneFlow.goHome();
        app.syncHomeActors(desktop.millis());
        assert(app.debugPromptContact(1));
        RoomResource& room = RoomResource::ins();
        assert(room.available());
        app.petX = static_cast<float>(room.doorwayInsideX());
        app.petY = static_cast<float>(room.doorwayInsideY());
        app.homeMainActor.x = app.petX;
        app.homeMainActor.y = app.petY;
        assert(!app.petFootprintInsideWalkArea(app.petX, app.petY));
        assert(app.debugAcceptContact());
        app.syncHomeActors(desktop.millis());
        assert(app.visitorMotion == AmoledApp::VisitorMotion::HOST_APPROACH);
        assert(app.homeCompanionActor.hidden);
        assert(app.petFootprintInsideWalkArea(app.petX, app.petY));
        bool sawHostClear = false;
        bool sawGuestEnter = false;
        for (int tick = 0;
             tick < 400 && app.visitorMotion != AmoledApp::VisitorMotion::ACTIVE;
             ++tick) {
            desktop.advanceMs(50);
            app.update(desktop.millis());
            app.markRendered();
            sawHostClear |= app.visitorHostDoorPhase == 2;
            sawGuestEnter |= app.visitorMotion ==
                AmoledApp::VisitorMotion::ENTERING;
        }
        assert(sawHostClear && sawGuestEnter);
        assert(app.visitorMotion == AmoledApp::VisitorMotion::ACTIVE);
        assert(app.petFootprintInsideWalkArea(app.petX, app.petY));
        assert(app.companionFootprintInsideWalkArea(
            app.homeCompanionActor.x, app.homeCompanionActor.y));
        assert(app.pairPhase == AmoledApp::PairPhase::APPROACH);
        const std::string& logs = desktop.logs();
        assert(logs.find("[VisitArrival] host_recover") != std::string::npos);
        assert(logs.find("[VisitArrival] host_clear_begin") != std::string::npos);
        assert(logs.find("[VisitArrival] guest_enter_end") != std::string::npos);
    }

    static void checkVisitorDiagnostics(DesktopPlatform& desktop,
                                        const Game::GameState& state) {
        AmoledApp app;
        app.begin(desktop.millis());
        app.gameState = state;
        app.gameState.teamCount = 1;
        app.gameState.storageCount = 1;
        app.gameState.storage[0] = Game::MonsterFactory::create(172, 5);
        app.gameState.settings.idleTimeoutIndex = 4;
        app.sceneFlow.goHome();
        app.syncHomeActors(desktop.millis());
        assert(app.debugPromptContact(1));
        for (int frame = 0; frame < 20; ++frame) {
            desktop.advanceMs(50);
            app.update(desktop.millis());
            app.markRendered();
        }
        assert(app.debugContactPromptFade == 255);
        assert(app.debugAcceptContact());
        app.syncHomeActors(desktop.millis());
        assert(app.visitorMotion == AmoledApp::VisitorMotion::HOST_APPROACH);
        assert(app.homeCompanionActor.hidden);
        // Firmware can service the freshly attached visitor again in the
        // same millisecond. That zero-duration tick must not reveal it.
        app.update(desktop.millis());
        app.markRendered();
        assert(app.visitorMotion == AmoledApp::VisitorMotion::HOST_APPROACH);
        assert(app.homeCompanionActor.hidden);
        int ticks = 0;
        bool sawVisitorEnter = false;
        while (app.visitorMotion != AmoledApp::VisitorMotion::ACTIVE) {
            desktop.advanceMs(50);
            app.update(desktop.millis());
            app.markRendered();
            if (app.visitorMotion == AmoledApp::VisitorMotion::ENTERING) {
                sawVisitorEnter = true;
                assert(!app.homeCompanionActor.hidden);
            }
            assert(++ticks < 300);
        }
        assert(sawVisitorEnter);
        assert(app.pairPhase == AmoledApp::PairPhase::APPROACH);
        ticks = 0;
        while (app.pairPhase == AmoledApp::PairPhase::APPROACH) {
            desktop.advanceMs(50);
            app.update(desktop.millis());
            app.markRendered();
            assert(++ticks < 120);
        }
        if (app.pairPhase != AmoledApp::PairPhase::INVITE) {
            std::fputs(desktop.logs().c_str(), stderr);
        }
        assert(app.pairPhase == AmoledApp::PairPhase::INVITE);
        const int centerDistancePx = std::abs(
            app.worldToScreenX(app.homeMainActor.x) -
            app.worldToScreenX(app.homeCompanionActor.x));
        auto idleWidth = [](uint16_t speciesId) {
            PokemonSprites::PetAnimationProfile profile{};
            if (!PokemonSprites::petAnimationProfile(speciesId, profile)) return 80;
            int widest = 0;
            for (uint16_t direction = 0; direction < 4; ++direction) {
                for (uint8_t frame = 0; frame < profile.idleFrames; ++frame) {
                    const auto kind = static_cast<PokemonSprites::SpriteKind>(
                        static_cast<uint16_t>(profile.idleBase) +
                        direction * profile.idleFrames + frame);
                    const auto* sprite = PokemonSprites::findSpeciesSprite(
                        speciesId, kind);
                    if (sprite) widest = std::max<int>(
                        widest, PokemonSprites::frameVisibleWidth(sprite) *
                                    AmoledUi::RESOURCE_SCALE);
                }
            }
            return widest > 0 ? widest : 80;
        };
        const int spriteHalfWidths = (
            idleWidth(app.gameState.team[0].speciesId) +
            idleWidth(app.gameState.team[1].speciesId)) / 2;
        assert(centerDistancePx - spriteHalfWidths >= 20);
        ticks = 0;
        int exitFrames = 0;
        while (app.debugContactActive) {
            const float previousX = app.homeCompanionActor.x;
            const float previousY = app.homeCompanionActor.y;
            desktop.advanceMs(50);
            app.update(desktop.millis());
            app.markRendered();
            if (app.visitorMotion == AmoledApp::VisitorMotion::EXITING) {
                assert(std::hypot(app.homeCompanionActor.x - previousX,
                                  app.homeCompanionActor.y - previousY) < 3.0f);
                ++exitFrames;
            }
            assert(++ticks < 900);
        }
        assert(exitFrames > 0);
        assert(app.gameState.teamCount == 1);
        assert(desktop.logs().find("[FriendDiag] visitor-exit finish kind=1") !=
               std::string::npos);
        const std::string& logs = desktop.logs();
        const size_t hostApproach = logs.find(
            "[FriendDiag] arrival host-approach kind=1");
        const size_t hostReady = logs.find(
            "[FriendDiag] arrival host-ready kind=1");
        const size_t visitorVisible = logs.find(
            "[FriendDiag] arrival visitor-visible kind=1");
        const size_t visitorEntered = logs.find(
            "[FriendDiag] arrival visitor-entered kind=1");
        const size_t talkReady = logs.find(
            "[FriendDiag] arrival talk-ready kind=1");
        assert(hostApproach < hostReady && hostReady < visitorVisible &&
               visitorVisible < visitorEntered && visitorEntered < talkReady);
        size_t begin = 0;
        while (begin < logs.size()) {
            const size_t end = logs.find('\n', begin);
            const std::string line = logs.substr(
                begin, end == std::string::npos ? end : end - begin);
            if (line.find("[FriendDiag]") != std::string::npos ||
                line.find("[HomeDiag] visitor") != std::string::npos) {
                std::puts(line.c_str());
            }
            if (end == std::string::npos) break;
            begin = end + 1;
        }
        std::puts("visitor diagnostics: prompt, talk and exit complete");
    }
#endif

    static void present(AmoledApp& app, Canvas565& canvas, bool acknowledge = true) {
        assert(app.needsRender());
        app.render(canvas);
        if (app.expeditionFade.alpha() == 255) {
            assert(app.renderRowBegin() == 0 && app.renderRowEnd() == AmoledUi::HEIGHT);
            for (int y = 0; y < AmoledUi::HEIGHT; ++y) {
                for (int x = 0; x < AmoledUi::WIDTH; ++x) {
                    assert(canvas.readPixel(x, y) == 0);
                }
            }
        }
        if (acknowledge) app.markRendered();
    }

    static void tick(AmoledApp& app, DesktopPlatform& desktop, uint32_t delay = 50) {
        desktop.advanceMs(delay);
        app.update(desktop.millis());
        assert(app.sceneFlow.current() != Scene::EXPLORE_AREAS);
    }

    static void fadeOut(AmoledApp& app, DesktopPlatform& desktop, Canvas565& canvas,
                        Scene source) {
        assert(app.expeditionFade.alpha() == 0);
        tick(app, desktop, 1200);  // No first frame submitted yet, despite elapsed time.
        assert(app.expeditionFade.alpha() == 0);
        present(app, canvas);
        int shades = 0;
        while (app.expeditionFade.alpha() != 255) {
            const int before = app.expeditionFade.alpha();
            tick(app, desktop, 500);  // Simulate a slow display/resource frame.
            assert(app.sceneFlow.current() == source);
            assert(app.expeditionFade.alpha() > before);
            assert(app.expeditionFade.alpha() - before <= 43);
            ++shades;
            if (app.expeditionFade.alpha() != 255) present(app, canvas);
            assert(shades < 20);
        }
        assert(shades >= 6);
        present(app, canvas, false);  // A failed transfer must not allow a scene switch.
        tick(app, desktop, 1000);
        assert(app.sceneFlow.current() == source);
        assert(!app.expeditionFade.complete());
        // A partial submission is not evidence that the entire screen is black.
        app.forceRenderRows(0, 100);
        app.render(canvas);
        app.markRendered();
        tick(app, desktop);
        assert(app.sceneFlow.current() == source);
        present(app, canvas);
        assert(app.expeditionFade.complete());
        tick(app, desktop);
        assert(app.sceneFlow.current() != source);
        assert(app.expeditionFade.alpha() == 255);
    }

    static void fadeIn(AmoledApp& app, DesktopPlatform& desktop, Canvas565& canvas,
                       Scene destination) {
        const float petX = app.petX;
        const float petY = app.petY;
        tick(app, desktop, 2000);  // Destination loading cannot consume its fade-in.
        assert(app.expeditionFade.alpha() == 255);
        present(app, canvas);
        int shades = 0;
        while (app.expeditionFade.active()) {
            const int before = app.expeditionFade.alpha();
            tick(app, desktop);
            assert(app.sceneFlow.current() == destination);
            assert(app.expeditionFade.alpha() <= before);
            assert(before - app.expeditionFade.alpha() <= 43);
            if (app.expeditionFade.active()) {
                assert(app.petX == petX && app.petY == petY);
                ++shades;
            }
            present(app, canvas);
            assert(shades < 20);
        }
        assert(shades >= 6);
    }

    static void checkImmediateNextFrame(DesktopPlatform& desktop, Canvas565& canvas,
                                        const Game::GameState& state) {
        AmoledApp app;
        app.begin(desktop.millis());
        app.gameState = state;
        app.gameState.settings.idleTimeoutIndex = 4;
        app.sceneFlow.enter(Scene::EXPLORE_AREAS);
        tap(app, desktop, 98, 412);
        int frames = 0;
        while (app.expeditionDeparturePhase != Phase::FADE_OUT) {
            tick(app, desktop);
            present(app, canvas);
            assert(++frames < 1000);
        }
        const uint32_t fadeStarted = desktop.millis();
        frames = 0;
        do {
            // Real boards may begin the next update in the same millisecond
            // as transfer completion. Rendering/transmission consumes the time;
            // there is no extra delay between markRendered and update.
            app.update(desktop.millis());
            assert(app.sceneFlow.current() != Scene::EXPLORE_AREAS);
            present(app, canvas, false);
            desktop.advanceMs(35);
            app.markRendered();
            assert(++frames < 100);
        } while (app.expeditionDeparturePhase != Phase::NONE);
        assert(app.sceneFlow.current() == Scene::EXPLORE_ROUTE);
        assert(desktop.millis() - fadeStarted < 2000);
        std::puts("immediate next frame: departure and both fade endpoints complete");
    }

    static void checkRouteExitWalkFrames(DesktopPlatform& desktop,
                                         const Game::GameState& state) {
        using Edge = ExploreMapGenerator::Edge;
        const float width = ExploreMapGenerator::WIDTH * ExploreRouteGeometry::TILE_SIZE;
        const float height = ExploreMapGenerator::HEIGHT * ExploreRouteGeometry::TILE_SIZE;
        for (int teamCount : {1, 2}) {
            for (Edge edge : {Edge::TOP, Edge::RIGHT, Edge::BOTTOM, Edge::LEFT}) {
                AmoledApp app;
                app.begin(desktop.millis());
                app.gameState = state;
                app.gameState.teamCount = teamCount;
                app.sceneFlow.enter(Scene::EXPLORE_ROUTE);
                app.exploreRouteMap.pathCount = 1;
                app.exploreRouteMap.paths[0].exit.edge = edge;
                app.exploreRouteMapBlockCount = 1;
                app.exploreRouteMapBlock = 0;
                app.exploreRoutePath = 0;
                app.exploreRouteWorldX = width / 2;
                app.exploreRouteWorldY = height / 2;
                app.exploreRouteFollowerWorldX = width / 2 - 12;
                app.exploreRouteFollowerWorldY = height / 2 - 12;
                app.exploreRouteBossPending = false;
                app.exploreRouteComplete = false;
                app.exploreRoutePaused = false;
                app.autonomousExpedition = false;
                uint32_t now = desktop.millis();
                app.beginExploreRouteExit(now);
                assert(app.exploreRouteExiting && app.exploreRouteMoving);
                assert(app.exploreRouteFollowerMoving == (teamCount == 2));
                app.updateExploreRoute(now);
                const uint8_t leaderFrame = app.exploreRoutePetFrame;
                const uint8_t followerFrame = app.exploreRouteFollowerFrame;
                app.updateExploreRoute(now + 45);
                assert(app.exploreRoutePetFrame == leaderFrame);
                assert(app.exploreRouteFollowerFrame == followerFrame);
                app.updateExploreRoute(now + 90);
                assert(app.exploreRoutePetFrame == static_cast<uint8_t>(leaderFrame + 1));
                assert(app.exploreRouteFollowerFrame == static_cast<uint8_t>(
                    followerFrame + (teamCount == 2 ? 1 : 0)));
                assert(!app.exploreRouteComplete);
                assert(app.exploreRouteWorldX != app.exploreRouteFromX ||
                       app.exploreRouteWorldY != app.exploreRouteFromY);
                if (teamCount == 2) {
                    assert(app.exploreRouteFollowerWorldX != app.exploreRouteFollowerFromX ||
                           app.exploreRouteFollowerWorldY != app.exploreRouteFollowerFromY);
                }
                int ticks = 0;
                now += 90;
                while (app.exploreRouteExiting) {
                    now += 90;
                    app.updateExploreRoute(now);
                    assert(++ticks < 1000);
                }
                assert(app.exploreRouteComplete);
                auto outside = [=](float x, float y) {
                    return x < 0 || x > width || y < 0 || y > height;
                };
                assert(outside(app.exploreRouteWorldX, app.exploreRouteWorldY));
                if (teamCount == 2) {
                    assert(outside(app.exploreRouteFollowerWorldX,
                                   app.exploreRouteFollowerWorldY));
                }
                assert(!app.exploreRouteMoving && !app.exploreRouteFollowerMoving);
            }
        }
        std::puts("route exit: both actors walk on all four edges before completion");
    }

    static void checkFrostLadderLinks(DesktopPlatform& desktop,
                                      const Game::GameState& state) {
        AmoledApp app;
        app.begin(desktop.millis());
        app.gameState = state;
        app.selectedExploreArea = ExploreMapGenerator::FROST_CRYSTAL_CAVE_AREA;
        app.sceneFlow.enter(Scene::EXPLORE_ROUTE);
        app.exploreRouteMapBlock = 0;
        app.exploreRouteMapBlockCount = 3;
        app.exploreRouteExpeditionSeed = 0x20260713;
        app.exploreRoutePendingEntryEdge = ExploreMapGenerator::Edge::TOP;
        app.exploreRouteBossScheduled = false;
        uint32_t now = desktop.millis() + 5000;
        assert(app.generateExploreRouteMap(now));
        app.exploreRoutePath = 0;
        const auto& path = app.exploreRouteMap.paths[0];
        for (uint8_t index = 0; index < path.pointCount; ++index) {
            const auto& point = path.points[index];
            uint16_t cell = point.y * ExploreMapGenerator::WIDTH + point.x;
            assert(app.exploreRouteMap.layers[0][cell] != 4506);
            assert(app.exploreRouteMap.layers[0][cell] !=
                   ExploreCaveTiles::FROST_BROKEN_ICE_HOLE);
        }
        assert(!path.fallsToNextLevel);
        uint16_t downCell = path.exit.point.y * ExploreMapGenerator::WIDTH +
                            path.exit.point.x;
        assert(app.exploreRouteMap.layers[1][downCell] ==
               ExploreCaveTiles::FROST_DOWNWARD_STAIRS);
        assert(app.exploreRouteMap.layers[1][
            app.exploreRouteMap.paths[1].exit.point.y * ExploreMapGenerator::WIDTH +
            app.exploreRouteMap.paths[1].exit.point.x] ==
            ExploreCaveTiles::FROST_UP_LADDER[1]);
        app.exploreRoutePath = 1;
        app.exploreRouteBossPending = false;
        assert(app.finishExploreRouteAtEnd(now + 500));
        assert(app.exploreRouteMapBlock == 1);
        app.exploreRoutePath = 1;
        assert(app.finishExploreRouteAtEnd(now + 1000));
        assert(app.exploreRouteMapBlock == 2);
        const auto& entry = app.exploreRouteMap.entry.point;
        assert(app.exploreRouteMap.layers[1][entry.y * ExploreMapGenerator::WIDTH +
                                             entry.x] ==
               ExploreCaveTiles::FROST_DOWNWARD_STAIRS);
        std::puts("frost links: down stairs and up ladder connect cave levels");
    }

    static void checkMenuPanel(Canvas565& canvas) {
        constexpr int top = (HOME_STATUS_TOP - 4 * 60) / 2;
        assert(exploreRouteMenuItemAt(340, top - 1) == -1);
        assert(exploreRouteMenuItemAt(340, top) == 0);
        assert(exploreRouteMenuItemAt(340, top + 2 * 60) == 2);
        assert(exploreRouteMenuItemAt(340, top + 4 * 60) == -1);
        assert(exploreRouteMenuItemAt(247, top + 30) == -1);
        ExploreMenuViewModel menu{};
        canvas.fillSprite(0x1234);
        renderExploreMenuScreen(canvas, menu, 0, AmoledUi::HEIGHT);
        assert(canvas.readPixel(260, top + 5) ==
               canvas.readPixel(280, top + 5));
        assert(canvas.readPixel(260, top + 12) !=
               canvas.readPixel(280, top + 12));
        assert(canvas.readPixel(300, HOME_STATUS_TOP - 1) == 0x1234);
        assert(canvas.readPixel(300, 448 - 1) == 0x1234);
        assert(canvas.readPixel(260, top + 100) != 0x1234);
    }

    static void checkMenuDismiss(DesktopPlatform& desktop,
                                 const Game::GameState& state) {
        const int blankPoints[][2] = {
            {184, 180},  // Map behind the menu.
            {340, 310},  // Below the menu but above the HUD.
            {98, 412},   // Bottom bag button must not receive the same tap.
        };
        for (const auto& point : blankPoints) {
            AmoledApp app;
            app.begin(desktop.millis());
            app.gameState = state;
            app.sceneFlow.enter(Scene::EXPLORE_ROUTE);
            app.exploreRouteMoving = false;
            app.exploreRoutePlayerWalkActive = false;
            app.pauseExploreRoute(desktop.millis());
            app.sceneFlow.openExploreMenu();
            tap(app, desktop, point[0], point[1]);
            assert(app.sceneFlow.current() == Scene::EXPLORE_ROUTE);
            assert(!app.exploreRoutePaused);
            assert(!app.exploreRouteMoving);
            assert(!app.exploreRoutePlayerWalkActive);
        }
    }

    static void checkFaintedReturn(DesktopPlatform& desktop,
                                   Canvas565& canvas,
                                   const Game::GameState& state) {
        for (int faintMask : {1, 2, 3}) {
            AmoledApp app;
            app.begin(desktop.millis());
            app.gameState = state;
            app.gameState.settings.idleTimeoutIndex = 4;
            app.syncHomeActors(desktop.millis());
            for (int slot = 0; slot < 2; ++slot) {
                if (!(faintMask & (1 << slot))) continue;
                app.gameState.team[slot].hpCur = 0;
                app.gameState.team[slot].fainted = true;
            }
            app.sceneFlow.enter(Scene::EXPLORE_ROUTE);
            app.exploreSessionActive = true;
            app.beginExploreReturn(desktop.millis());
            fadeOut(app, desktop, canvas, Scene::EXPLORE_ROUTE);
            assert(app.sceneFlow.current() == Scene::HOME);
            if (faintMask & 1) {
                assert(app.petResting);
                assert(app.petX == RoomResource::ins().bedX());
                assert(app.petY == RoomResource::ins().bedY());
                assert(app.homeMainActor.task == Home::Task::FAINTED);
            }
            if (faintMask & 2) {
                assert(app.homeCompanionActor.faintRestActive);
                assert(app.homeCompanionActor.task == Home::Task::FAINTED);
                assert(app.homeCompanionActor.x !=
                       RoomResource::ins().doorwayOutsideX() ||
                       app.homeCompanionActor.y !=
                       RoomResource::ins().doorwayOutsideY());
            }
            fadeIn(app, desktop, canvas, Scene::HOME);
            int frames = 0;
            while (app.expeditionDeparturePhase != Phase::NONE) {
                tick(app, desktop);
                present(app, canvas);
                assert(++frames < 1000);
            }
            if (faintMask & 1) {
                assert(app.petX == RoomResource::ins().bedX());
                assert(app.petResting);
            }
            if (faintMask & 2) assert(app.homeCompanionActor.faintRestActive);
        }
        std::puts("fainted return: resting positions before first home frame");
    }

    static void run(DesktopPlatform& desktop, Canvas565& canvas,
                    const Game::GameState& state) {
        checkDisplayLockArbitration(desktop, state);
        checkMenuPanel(canvas);
        checkMenuDismiss(desktop, state);
        checkFaintedReturn(desktop, canvas, state);
        checkRouteExitWalkFrames(desktop, state);
        checkFrostLadderLinks(desktop, state);
        checkImmediateNextFrame(desktop, canvas, state);
        for (int returnMode = 0; returnMode < 3; ++returnMode) {
            AmoledApp app;
            app.begin(desktop.millis());
            app.gameState = state;
            app.gameState.settings.idleTimeoutIndex = 4;
            app.sceneFlow.enter(Scene::EXPLORE_AREAS);
            app.forceFullRender();
            present(app, canvas);
            tap(app, desktop, 98, 412);  // Actual departure button and touch handler.
            assert(app.sceneFlow.current() == Scene::HOME);
            present(app, canvas);
            int walkingFrames = 0;
            while (app.expeditionDeparturePhase != Phase::FADE_OUT) {
                tick(app, desktop);
                assert(app.sceneFlow.current() == Scene::HOME);
                if (app.expeditionDeparturePhase != Phase::FADE_OUT) present(app, canvas);
                assert(++walkingFrames < 1000);
            }
            fadeOut(app, desktop, canvas, Scene::HOME);
            assert(app.sceneFlow.current() == Scene::EXPLORE_ROUTE);
            fadeIn(app, desktop, canvas, Scene::EXPLORE_ROUTE);

            app.exploreRouteMoving = false;
            app.exploreRouteBossPending = false;
            app.exploreRouteMapBlockCount = 1;
            app.exploreRouteMapBlock = 0;
            app.exploreRouteSteps = 10;
            if (returnMode == 1) {
                app.sceneFlow.openExploreMenu();
                int endY = -1;
                for (int y = 0; y < 448; ++y) {
                    if (exploreRouteMenuItemAt(340, y) == 2) { endY = y; break; }
                }
                assert(endY >= 0);
                tap(app, desktop, 340, endY);
            } else {
                app.autonomousExpedition = returnMode == 2;
                app.finishExploreRouteAtEnd(desktop.millis());
                if (returnMode == 0) tap(app, desktop, 184, 200);
            }
            assert(app.expeditionDeparturePhase == Phase::RETURN_FADE_OUT);
            assert(app.sceneFlow.current() == Scene::EXPLORE_ROUTE);
            tap(app, desktop, 98, 412);  // Repeated taps cannot cancel a return.
            app.leaveExploreRoute();   // Repeated return requests cannot settle twice.
            assert(app.expeditionDeparturePhase == Phase::RETURN_FADE_OUT);
            fadeOut(app, desktop, canvas, Scene::EXPLORE_ROUTE);
            assert(app.sceneFlow.current() == Scene::HOME);
            const auto settledBond = app.gameState.team[0].bond;
            fadeIn(app, desktop, canvas, Scene::HOME);
            assert(app.expeditionDeparturePhase == Phase::RETURN_WALK_IN);
            int enteringFrames = 0;
            while (app.expeditionDeparturePhase != Phase::NONE) {
                tick(app, desktop);
                assert(app.sceneFlow.current() == Scene::HOME);
                assert(app.expeditionFade.alpha() == 0);
                present(app, canvas);
                assert(++enteringFrames < 1000);
            }
            assert(app.gameState.team[0].bond == settledBond);
            assert(!app.exploreRouteMoving && !app.exploreRouteAutoWalk);
            assert(app.petX == app.petTargetX && app.petY == app.petTargetY);
            assert(app.homeMainActor.task == Home::Task::IDLE);
            if (app.gameState.teamCount > 1 &&
                app.homeCompanionActor.active &&
                !app.gameState.team[1].fainted &&
                app.gameState.team[1].hpCur > 0) {
                assert(app.homeCompanionActor.task == Home::Task::IDLE);
                const float dx = app.homeCompanionActor.x - app.petX;
                const float dy = app.homeCompanionActor.y - app.petY;
                assert(dx * dx + dy * dy > 16.0f);
                assert(!app.expeditionMainHidden);
                assert(!app.expeditionCompanionHidden);
            }
            std::printf("expedition return mode %d: black endpoints and direct home return verified\n",
                        returnMode);
        }
    }
};

}  // namespace AmoledV1

void checkExpeditionTransitions(DesktopPlatform& desktop, Canvas565& canvas,
                               const Game::GameState& state) {
    AmoledV1::AmoledAppTestAccess::run(desktop, canvas, state);
}

void checkProgressionReplacement(DesktopPlatform& desktop,
                                 const Game::GameState& state) {
    AmoledV1::AmoledAppTestAccess::checkMoveReplacement(desktop, state);
}

#if STICKMON_ENABLE_DEBUG_FEATURES
void checkVisitorInvalidHostEntry(DesktopPlatform& desktop,
                                  const Game::GameState& state) {
    AmoledV1::AmoledAppTestAccess::checkVisitorInvalidHostEntry(desktop, state);
}

void checkVisitorDiagnostics(DesktopPlatform& desktop,
                             const Game::GameState& state) {
    AmoledV1::AmoledAppTestAccess::checkVisitorDiagnostics(desktop, state);
}
#endif
