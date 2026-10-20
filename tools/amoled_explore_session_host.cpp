#include "AmoledApp.h"
#include "game/GameRandom.h"
#include "game/MonsterFactory.h"
#include "game/ItemInventory.h"
#include "game/ExploreCaveTiles.h"
#include "game/ExploreRunRules.h"
#include "game/ExploreRouteGeometry.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/Canvas565.h"
#include "core/UiStrings.h"
#include "game/ShopService.h"
#include "ui/UiCommon.h"

#include <cassert>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <vector>

namespace {
uint32_t randomState, randomCalls, randomHash;
void hashValue(uint32_t& hash, uint32_t value) {
    for (int byte = 0; byte < 4; ++byte) {
        hash = (hash ^ (value & 255)) * 16777619U;
        value >>= 8;
    }
}
uint32_t deterministicRange(uint32_t low, uint32_t high) {
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    const uint32_t result = high > low ? low + randomState % (high - low) : low;
    ++randomCalls;
    hashValue(randomHash, low);
    hashValue(randomHash, high);
    hashValue(randomHash, result);
    return result;
}
}

namespace AmoledV1 {
struct AmoledAppTestAccess {
    static void fixture(ExploreSession& session, Game::GameState& game, uint8_t area = 0) {
        game = Game::GameState{};
        game.oobeDone = true;
        for (auto& reroll : game.explorePoolRerollCounts) reroll = 1;
        game.teamCount = 2;
        game.team[0] = Game::MonsterFactory::create(7, 30);
        game.team[1] = Game::MonsterFactory::create(25, 28);
        assert(session.begin(game, area));
        session.takeEffects();
        auto& route = session.state_;
        route.exploreRouteMap = {};
        route.exploreRouteMap.pathCount = 1;
        route.exploreRoutePath = 0;
        route.exploreRouteMapBlockCount = 1;
        route.exploreRouteBossScheduled = false;
        auto& path = route.exploreRouteMap.paths[0];
        path.pointCount = 16;
        for (uint8_t i = 0; i < path.pointCount; ++i) path.points[i] = {3, uint8_t(3 + i)};
    }

    static void stepAccounting() {
        ExploreSession session;
        Game::GameState game;
        fixture(session, game);
        auto& route = session.state_;
        route.exploreRouteEncounterCooldownSteps = 100;
        game.team[0].hpCur = 1;
        game.team[1].hpCur = 0;
        game.team[1].fainted = true;
        const auto maximum = game.team[0].hpMax;
        for (int i = 1; i <= 10; ++i) {
            assert(session.beginStep(game));
            assert(!session.beginStep(game));
            assert(game.stepsToday == i - 1);
            session.completeStep(game, i * 360);
            const auto effects = session.takeEffects();
            assert(effects.statusChanged == (i == 10));
            assert(!effects.startBattle && !effects.save);
            assert(route.exploreRouteSteps == i && game.stepsToday == i);
            assert(route.exploreRouteEncounterCooldownSteps == 100 - i);
            session.completeStep(game, i * 360);
            assert(route.exploreRouteSteps == i && game.stepsToday == i);
            assert(game.team[0].hpCur == (i < 10 ? 1 : 1 + ExploreRunRules::recoveryAmount(maximum)));
            assert(game.team[1].hpCur == 0 && game.team[1].fainted);
        }
        game.stepsToday = 60000;
        assert(session.beginStep(game));
        session.completeStep(game, 4000);
        assert(game.stepsToday == 60000);
    }

    static void pickupAndBoss() {
        for (bool automatic : {false, true}) {
            ExploreSession session;
            Game::GameState game;
            fixture(session, game);
            auto& route = session.state_;
            route.exploreRouteEncounterCooldownSteps = 100;
            route.exploreRoutePickupAvailable = true;
            route.exploreRoutePickupIndex = 1;
            route.exploreRoutePickupItem = 1;  // Coin.
            if (automatic) session.setAutoWalk(true); else session.walkByPlayer();
            const auto coins = game.coins;
            assert(session.beginStep(game));
            session.completeStep(game, 360);
            const auto pickup = session.takeEffects();
            assert(pickup.notice == ExploreSession::Notice::COINS && pickup.save);
            assert(game.coins == coins + pickup.coins);
            assert(pickup.startStep == automatic);
            assert(route.exploreRouteAutoWalk == automatic && !route.exploreRoutePlayerWalkActive);
            session.resolvePickup(game);
            assert(game.coins == coins + pickup.coins && !session.takeEffects().save);

            route.exploreRouteBossPending = true;
            route.exploreRouteBossIndex = 3;
            route.exploreRouteBossSpeciesId = 7;
            route.exploreRouteBossLevel = 20;
            if (!automatic) session.walkByPlayer();
            assert(session.beginStep(game));
            session.completeStep(game, 720);
            const auto beforeBoss = session.takeEffects();
            assert(beforeBoss.startStep == automatic);
            assert(!route.exploreRoutePlayerWalkActive);
            assert(session.beginStep(game));
            session.completeStep(game, 1080);
            const auto battle = session.takeEffects();
            assert(battle.startBattle && battle.encounter.boss);
            assert(battle.encounter.wild.speciesId == 7 && battle.encounter.wild.level == 20);
            assert(!route.exploreRouteBossPending && !route.exploreRouteAutoWalk);
            assert(!session.beginStep(game));
            assert(!session.beginEncounter(game, 0, true, 7, 20));
            assert(!session.takeEffects().startBattle);
            session.battleClosed();
            assert(session.beginStep(game));
        }
        ExploreSession session;
        Game::GameState game;
        fixture(session, game);
        auto& route = session.state_;
        route.exploreRoutePickupAvailable = true;
        route.exploreRoutePickupItem = 2;  // Potion.
        game.bag.potion = 99;
        session.resolvePickup(game);
        const auto full = session.takeEffects();
        assert(full.notice == ExploreSession::Notice::BAG_FULL && full.save);
        assert(!route.exploreRoutePickupAvailable && game.bag.potion == 99);
        for (auto& monster : game.team) { monster.hpCur = 0; monster.fainted = true; }
        route.exploreRouteBossPending = true;
        route.exploreRouteBossSpeciesId = 7;
        route.exploreRouteBossLevel = 20;
        assert(!session.finishAtEnd(game));
        assert(route.exploreRouteBossPending && !route.exploreRouteComplete);
        assert(session.takeEffects().notice == ExploreSession::Notice::PET_REST);
    }

    static void pickupToast(DesktopPlatform& desktop) {
        AmoledApp app;
        app.begin(desktop.millis());
        fixture(app.exploreSession_, app.gameState);
        assert(app.startExploreRoute(desktop.millis()));
        app.exploreSession_.state_.exploreRoutePaused = true;
        std::vector<uint16_t> pixels(AmoledUi::WIDTH * AmoledUi::HEIGHT);
        Canvas565 canvas;
        canvas.attach({pixels.data(), AmoledUi::WIDTH, AmoledUi::HEIGHT, false});
        const auto present = [&]() {
            assert(app.needsRender());
            app.render(canvas);
            app.markRendered();
        };
        present();

        // Text must survive the caller reusing its formatting buffer.
        char transient[] = "Pickup notification";
        app.setToast(transient, desktop.millis());
        std::memset(transient, 'X', sizeof(transient) - 1);
        assert(std::strcmp(app.exploreRouteModel().toast, "Pickup notification") == 0);
        app.toast = nullptr;

        for (int scenario = 0; scenario < 3; ++scenario) {
            auto& route = app.exploreSession_.state_;
            route.exploreRoutePickupAvailable = true;
            route.exploreRoutePickupItem = scenario == 0 ? 1 : 2;
            app.gameState.bag.potion = scenario == 2 ? 99 : 0;
            const auto coins = app.gameState.coins;
            app.exploreSession_.resolvePickup(app.gameState);
            app.applyExploreEffects(desktop.millis());
            char expected[64];
            if (scenario == 0) {
                std::snprintf(expected, sizeof(expected), Ui::Explore::PICKUP_COIN_FMT,
                    static_cast<unsigned long>(app.gameState.coins - coins));
            } else if (scenario == 1) {
                std::snprintf(expected, sizeof(expected), Ui::Explore::PICKUP_FMT,
                    Game::ShopService::shortName(Game::ItemId::POTION));
            } else {
                std::snprintf(expected, sizeof(expected), "%s", Ui::Amoled::BAG_FULL);
            }
            assert(app.toast && std::strcmp(app.exploreRouteModel().toast, expected) == 0);
            assert(app.dirtyRowBegin <= UiCommon::TOAST_TOP &&
                   app.dirtyRowEnd >= UiCommon::TOAST_BOTTOM);
            present();
            const auto shown = pixels;
            app.forceFullRender();
            present();
            assert(pixels == shown);  // Dirty redraw includes the entire notification.

            const auto until = app.toastUntil;
            desktop.advanceMs(until - desktop.millis() - 1);
            app.update(desktop.millis());
            assert(app.toast && std::strcmp(app.toast, expected) == 0);
            desktop.advanceMs(1);
            app.update(desktop.millis());
            assert(!app.toast);
            assert(app.dirtyRowBegin <= UiCommon::TOAST_TOP &&
                   app.dirtyRowEnd >= UiCommon::TOAST_BOTTOM);
            present();
            bool changed = false;
            for (int y = UiCommon::TOAST_TOP; y < UiCommon::TOAST_BOTTOM; ++y)
                for (int x = 0; x < AmoledUi::WIDTH; ++x)
                    changed |= pixels[y * AmoledUi::WIDTH + x] != shown[y * AmoledUi::WIDTH + x];
            assert(changed);  // Notification was visible, then removed.
            const auto expired = pixels;
            app.forceFullRender();
            present();
            assert(pixels == expired);  // No notification residue remains.
        }
    }

    static void encountersAndItems() {
        ExploreSession session;
        Game::GameState game;
        fixture(session, game);
        auto& route = session.state_;
        assert(Game::ItemInventory::add(game, Game::ItemId::MAX_REPEL));
        assert(session.useItem(game, Game::ItemId::MAX_REPEL));
        assert(Game::ItemInventory::add(game, Game::ItemId::MAX_REPEL));
        assert(!session.useItem(game, Game::ItemId::MAX_REPEL));
        assert(Game::ItemInventory::count(game, Game::ItemId::MAX_REPEL) == 1);
        GameRandom::setRangeProvider(+[](uint32_t low, uint32_t) { return low; });
        assert(session.beginStep(game));
        session.completeStep(game, 360);
        assert(!session.takeEffects().startBattle && route.exploreItemEffects.repelStepsRemaining() == 99);
        route.exploreRouteGuaranteedEncounterPending = true;
        route.exploreRouteGuaranteedEncounterIndex = 2;
        assert(session.beginStep(game));
        session.completeStep(game, 720);
        assert(session.takeEffects().startBattle);  // Guaranteed bypasses repel.
        assert(route.exploreRouteMapEncounterCount == 1 && route.exploreRouteEncounterCooldownSteps == 5);
        session.battleClosed();
        assert(Game::ItemInventory::add(game, Game::ItemId::HONEY));
        assert(session.useItem(game, Game::ItemId::HONEY));
        assert(Game::ItemInventory::add(game, Game::ItemId::HONEY));
        assert(!session.useItem(game, Game::ItemId::HONEY));
        assert(Game::ItemInventory::count(game, Game::ItemId::HONEY) == 1);
        assert(session.beginStep(game));
        session.completeStep(game, 1080);
        assert(session.takeEffects().startBattle);  // Honey bypasses cooldown and repel.
        assert(route.exploreRouteMapEncounterCount == 2 && !route.exploreItemEffects.honeyEncounterPending());
        session.battleClosed();
        route.exploreItemEffects.reset();
        for (int i = 0; i < 6; ++i) {
            assert(session.beginStep(game));
            session.completeStep(game, 1440 + i * 360);
            assert(!session.takeEffects().startBattle);  // Per-map encounter cap.
        }
        assert(route.exploreRouteEncounterCooldownSteps == 0);
        GameRandom::setRangeProvider(nullptr);
    }

    static void iceAndPause(DesktopPlatform& desktop) {
        ExploreSession session;
        Game::GameState game;
        fixture(session, game, ExploreMapGenerator::FROST_CRYSTAL_CAVE_AREA);
        auto& route = session.state_;
        auto& map = route.exploreRouteMap;
        const auto first = map.paths[0].points[1];
        const auto second = map.paths[0].points[2];
        map.layers[0][first.y * ExploreMapGenerator::WIDTH + first.x] = 4504;
        map.layers[0][second.y * ExploreMapGenerator::WIDTH + second.x] = 4504;
        route.exploreRouteGuaranteedEncounterPending = true;
        route.exploreRouteGuaranteedEncounterIndex = 1;
        for (int i = 1; i <= 2; ++i) {
            assert(session.beginStep(game));
            session.completeStep(game, i * 360);
            const auto effects = session.takeEffects();
            assert(effects.startStep && !effects.startBattle);
        }
        assert(session.beginStep(game));
        session.completeStep(game, 1080);
        assert(session.takeEffects().startBattle);  // Resolve at the first non-ice point.

        fixture(session, game, ExploreMapGenerator::FROST_CRYSTAL_CAVE_AREA);
        route.exploreRouteMapBlockCount = 2;
        route.exploreRouteEncounterCooldownSteps = 100;
        map.layers[0][first.y * ExploreMapGenerator::WIDTH + first.x] = 4506;
        assert(session.beginStep(game));
        session.completeStep(game, 100);
        assert(session.takeEffects().invalidateWorld);
        assert(map.layers[0][first.y * ExploreMapGenerator::WIDTH + first.x] == ExploreCaveTiles::FROST_BROKEN_ICE_HOLE);
        assert(!session.beginStep(game));
        session.pause(200);
        session.pause(500);  // Idempotent, retains the original anchor.
        assert(session.resume(1200) == 1000);
        assert(session.resume(1300) == 0);
        session.updatePendingFall(game, 1339);
        assert(route.exploreRouteMapBlock == 0);
        session.updatePendingFall(game, 1340);
        assert(route.exploreRouteMapBlock == 1 && !route.exploreRoutePendingFrostFall);
        assert(session.takeEffects().mapChanged);

        AmoledApp app;
        app.begin(desktop.millis());
        fixture(app.exploreSession_, app.gameState);
        auto& moving = app.exploreSession_.state_;
        moving.exploreRouteEncounterCooldownSteps = 100;
        app.sceneFlow.enterExploreRoute();
        app.resetExploreMapPresentation(0);
        const uint32_t start = UINT32_MAX - 127;
        assert(app.beginExploreRouteStep(start));
        app.updateExploreRoute(start + 120);
        const float y = app.explorePresenter_.state_.exploreRouteWorldY;
        app.pauseExploreRoute(start + 120);
        app.resumeExploreRoute(start + 50120);
        app.updateExploreRoute(start + 50120);
        assert(std::fabs(app.explorePresenter_.state_.exploreRouteWorldY - y) < 0.001f);
        app.updateExploreRoute(start + 50359);
        assert(moving.exploreRouteMoving && moving.exploreRouteSteps == 0);
        app.updateExploreRoute(start + 50360);
        assert(!moving.exploreRouteMoving && moving.exploreRouteSteps == 1);
        const uint32_t exitStart = start + 60000;
        app.beginExploreRouteExit(exitStart);
        app.updateExploreRoute(exitStart + 100);
        const auto exitX = app.explorePresenter_.state_.exploreRouteWorldX, exitY = app.explorePresenter_.state_.exploreRouteWorldY;
        app.pauseExploreRoute(exitStart + 100);
        app.resumeExploreRoute(exitStart + 10100);
        app.updateExploreRoute(exitStart + 10100);
        assert(std::fabs(app.explorePresenter_.state_.exploreRouteWorldX - exitX) < 0.001f);
        assert(std::fabs(app.explorePresenter_.state_.exploreRouteWorldY - exitY) < 0.001f && moving.exploreRouteExiting);
        app.updateExploreRoute(exitStart + 10000 + app.explorePresenter_.state_.exploreRouteExitDurationMs);
        assert(moving.exploreRouteComplete && !moving.exploreRouteExiting);
    }

    static void presenterContract() {
        ExploreSession session;
        Game::GameState game;
        fixture(session, game);
        auto& route = session.state_;
        route.exploreRouteBossPending = false;
        ExplorePresenter presenter, other;
        presenter.resetMap(route, game, 0, 0, 100);
        presenter.startRun(100);
        unsigned char before[sizeof(game)];
        std::memcpy(before, &game, sizeof(game));

        const auto model = presenter.model(route, game, 0, 123, 42, "test");
        assert(model.map == &route.exploreRouteMap && model.state == &game);
        assert(model.grass == &presenter.state().exploreGrass);
        assert(model.animationNowMs == 123 && model.fadeAlpha == 42);
        assert(model.toast && !std::strcmp(model.toast, "test"));
        assert(!presenter.takeDamage().valid && !other.takeDamage().valid);

        // Building a model cannot acknowledge a draw. The first damage still
        // restores the whole map band, then a recorded draw permits actor clips.
        presenter.requestDynamicDamage(model);
        auto damage = presenter.takeDamage();
        assert(damage.valid && damage.bounds.x == 0 && damage.bounds.y == 0);
        assert(damage.bounds.width == AmoledUi::WIDTH && damage.bounds.height == HOME_STATUS_TOP);
        assert(!presenter.takeDamage().valid);
        presenter.recordDrawn(model);
        presenter.requestDynamicDamage(model);
        damage = presenter.takeDamage();
        assert(damage.valid && damage.bounds.height < HOME_STATUS_TOP);
        assert(!other.takeDamage().valid);

        // Viewport/map mismatch must restore the background before actors.
        auto movedCamera = model;
        movedCamera.cameraY += 1;
        presenter.recordDrawn(movedCamera);
        presenter.requestDynamicDamage(model);
        damage = presenter.takeDamage();
        assert(damage.valid && damage.bounds.height == HOME_STATUS_TOP);
        presenter.recordDrawn(model);
        auto changedMap = route.exploreRouteMap;
        changedMap.seed += 1;
        auto changedModel = model;
        changedModel.map = &changedMap;
        presenter.recordDrawn(changedModel);
        presenter.requestDynamicDamage(model);
        assert(presenter.takeDamage().bounds.height == HOME_STATUS_TOP);

        // A static or paused route cannot schedule motion repaint or mutate
        // gameplay. Map resets invalidate the previous draw even for one seed.
        assert(presenter.updateMotion(route, game, 0, 10000) == ExplorePresenter::Motion::NONE);
        presenter.updateEcology(route, game, 0, false, 10000);
        assert(!presenter.takeDamage().valid);
        assert(std::memcmp(before, &game, sizeof(game)) == 0);
        presenter.resetMap(route, game, 0, 0, 10000);
        presenter.requestDynamicDamage(presenter.model(route, game, 0, 10000));
        assert(presenter.takeDamage().bounds.height == HOME_STATUS_TOP);
        assert(!other.takeDamage().valid);
    }

    static void returnSettlement() {
        ExploreSession session, other;
        Game::GameState game, otherGame;
        fixture(session, game);
        fixture(other, otherGame);
        auto& route = session.state_;
        route.exploreRouteSteps = 40;
        game.team[0].bond = 10;
        game.team[0].majorStatus = Game::MajorStatus::POISON;
        game.team[0].majorStatusTurns = 3;
        game.team[1].origin = Game::Origin::VISITOR;
        game.team[1].bond = 20;
        route.exploreItemEffects.activateHoney();
        route.exploreItemEffects.activateMaxRepel();
        session.settleReturn(game);
        assert(game.team[0].bond == 13 && game.team[1].bond == 20);
        assert(game.team[0].majorStatus == Game::MajorStatus::NONE && game.team[0].majorStatusTurns == 0);
        assert(!route.exploreItemEffects.honeyEncounterPending() && route.exploreItemEffects.repelStepsRemaining() == 0);
        assert(session.takeEffects().save && !session.takeEffects().save);
        session.settleReturn(game);
        session.finishReturn(game);
        assert(!session.takeEffects().save && game.team[0].bond == 13);
        assert(other.state().exploreRouteSteps == 0 && other.state().exploreRoutePool.count > 0);
        assert(!session.state().exploreRoutePool.count);
        assert(&session.state().exploreRouteMap != &other.state().exploreRouteMap);
        // Starting a new run resets the settlement latch.
        assert(session.begin(game, 0));
        session.takeEffects();
        session.state_.exploreRouteSteps = 1;
        session.settleReturn(game);
        assert(game.team[0].bond == 14 && session.takeEffects().save);
    }

    static void battleReturnClock(DesktopPlatform& desktop) {
        AmoledApp app;
        app.begin(desktop.millis());
        fixture(app.exploreSession_, app.gameState);
        app.sceneFlow.enterExploreRoute();
        const uint32_t started = desktop.millis();
        app.explorePresenter_.state_.nextExploreRouteMapFrameMs = started + 280;
        assert(app.beginExploreEncounter(started, true, 7, 20));
        assert(app.exploreSession_.state().exploreRoutePaused);
        // Growth and other battle-owned pages retain this same pause anchor.
        app.sceneFlow.enter(AppSceneFlow::Scene::PROGRESSION);
        desktop.advanceMs(60000);
        app.closeBattle(desktop.millis());
        assert(!app.exploreSession_.state().exploreRoutePaused);
        assert(app.explorePresenter_.state_.nextExploreRouteMapFrameMs == desktop.millis() + 280);
        assert(app.exploreSession_.state().exploreRouteSteps == 0);
        assert(app.sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_ROUTE);
    }

    static void menuReturn(DesktopPlatform& desktop) {
        AmoledApp app;
        app.begin(desktop.millis());
        fixture(app.exploreSession_, app.gameState);
        app.exploreSessionActive = true;
        app.exploreSession_.state_.exploreRouteSteps = 10;
        app.gameState.team[0].bond = 10;
        const auto oldBond = app.gameState.team[0].bond;
        app.sceneFlow.enterExploreRoute();
        app.pauseExploreRoute(desktop.millis());
        app.sceneFlow.openMenu();
        app.menuScroll = 400;
        bool hit = false;
        for (int y = 60; y < 448 && !hit; ++y) {
            for (int x = 0; x < 368; ++x) {
                int item = mainMenuItemAt(x, y, app.menuScroll);
                if (item >= 0 && AppSceneFlow::mainMenuEntry(item, true).target == AppSceneFlow::Scene::HOME) {
                    app.handleTap(x, y, desktop.millis());
                    hit = true;
                    break;
                }
            }
        }
        assert(hit && app.expeditionDeparturePhase == AmoledApp::ExpeditionDeparturePhase::RETURN_FADE_OUT);
        assert(!app.exploreSession_.state().exploreRouteMoving);
        for (int i = 0; i < 1000 && app.expeditionDeparturePhase != AmoledApp::ExpeditionDeparturePhase::NONE; ++i) {
            desktop.advanceMs(100);
            app.updateExploreDeparture(desktop.millis());
            if (app.needsRender()) app.markRendered();
        }
        assert(app.sceneFlow.current() == AppSceneFlow::Scene::HOME && !app.exploreSessionActive);
        assert(app.gameState.team[0].bond == oldBond + 1);
        Game::GameState saved;
        MainSceneViewState savedView;
        assert(app.saveManager.load(saved, savedView));
        assert(saved.team[0].bond == app.gameState.team[0].bond);
        app.completeExploreReturn();
        assert(app.gameState.team[0].bond == oldBond + 1);
    }

    static void deterministicRoutes(DesktopPlatform& desktop) {
        using Scene = AppSceneFlow::Scene;
        for (uint8_t area = 0; area < Game::EXPLORE_AREA_COUNT; ++area) {
            for (uint32_t seed : {1U, 17U, 391U, 20260713U, 0x12345678U, 0xCAFEBABEU}) {
                GameRandom::setRangeProvider(nullptr);
                AmoledApp app;
                app.begin(desktop.millis());
                app.gameState = Game::GameState{};
                app.gameState.oobeDone = true;
                for (auto& rerolls : app.gameState.explorePoolRerollCounts) rerolls = 1;
                app.gameState.gameMinutesTotal = 7 * 60;
                app.gameState.teamCount = 2;
                app.gameState.team[0] = Game::MonsterFactory::create(7, 30);
                app.gameState.team[1] = Game::MonsterFactory::create(25, 28);
                app.gameState.stepsToday = 5100;
                app.selectedExploreArea = area;
                randomState = seed;
                randomCalls = 0;
                randomHash = 2166136261U;
                GameRandom::setRangeProvider(deterministicRange);
                assert(app.startExploreRoute(desktop.millis()));
                uint32_t hash = 2166136261U;
                uint32_t maps = 0, battles = 0;
                int previousBlock = -1;
                for (int tick = 0; tick < 5000 && !app.exploreSession_.state_.exploreRouteComplete; ++tick) {
                    if (app.exploreSession_.state_.exploreRouteMapBlock != previousBlock) {
                        previousBlock = app.exploreSession_.state_.exploreRouteMapBlock;
                        ++maps;
                        hashValue(hash, ExploreMapGenerator::fingerprint(app.exploreSession_.state_.exploreRouteMap));
                        hashValue(hash, app.exploreSession_.state_.exploreRoutePath);
                        hashValue(hash, app.exploreSession_.state_.exploreRoutePickupIndex);
                        hashValue(hash, app.exploreSession_.state_.exploreRoutePickupItem);
                        hashValue(hash, app.exploreSession_.state_.exploreRouteGuaranteedEncounterIndex);
                        hashValue(hash, app.exploreSession_.state_.exploreRouteBossIndex);
                    }
                    if (app.sceneFlow.current() == Scene::BATTLE) {
                        ++battles;
                        hashValue(hash, app.battleSession_.state().battleWild.speciesId);
                        hashValue(hash, app.battleSession_.state().battleWild.level);
                        hashValue(hash, app.battleSession_.state().battleWild.ivPacked);
                        app.closeBattle(desktop.millis());
                    } else if (!app.exploreSession_.state_.exploreRouteMoving && !app.exploreSession_.state_.exploreRoutePendingFrostFall) {
                        app.beginExploreRouteStep(desktop.millis());
                    } else {
                        desktop.advanceMs(480);
                        app.updateExploreRoute(desktop.millis());
                    }
                }
                assert(app.exploreSession_.state_.exploreRouteComplete);
                hashValue(hash, app.gameState.stepsToday);
                hashValue(hash, app.gameState.coins);
                hashValue(hash, app.exploreSession_.state_.exploreRouteSteps);
                for (uint8_t item = 0; item < static_cast<uint8_t>(Game::ItemId::COUNT); ++item)
                    hashValue(hash, Game::ItemInventory::count(app.gameState, static_cast<Game::ItemId>(item)));
                std::printf("route area=%u seed=%u maps=%u battles=%u steps=%u state=%08x rng=%u/%08x\n",
                    area, seed, maps, battles, app.exploreSession_.state_.exploreRouteSteps,
                    hash, randomCalls, randomHash);
            }
        }
        GameRandom::setRangeProvider(nullptr);
    }
};
}

int main(int argc, char** argv) {
    assert(argc == 3);
    DesktopPlatform desktop(argv[1]);
    Platform::bind(desktop.serviceBundle());
    desktop.begin();
    assert(Platform::resources().mount());
    using Probe = AmoledV1::AmoledAppTestAccess;
    if (std::strcmp(argv[2], "routes") == 0) Probe::deterministicRoutes(desktop);
    else if (std::strcmp(argv[2], "steps") == 0) Probe::stepAccounting();
    else if (std::strcmp(argv[2], "pickup-boss") == 0) Probe::pickupAndBoss();
    else if (std::strcmp(argv[2], "pickup-toast") == 0) Probe::pickupToast(desktop);
    else if (std::strcmp(argv[2], "encounters") == 0) Probe::encountersAndItems();
    else if (std::strcmp(argv[2], "pause-ice") == 0) Probe::iceAndPause(desktop);
    else if (std::strcmp(argv[2], "settlement") == 0) Probe::returnSettlement();
    else if (std::strcmp(argv[2], "presenter") == 0) Probe::presenterContract();
    else if (std::strcmp(argv[2], "menu-return") == 0) Probe::menuReturn(desktop);
    else if (std::strcmp(argv[2], "battle-clock") == 0) Probe::battleReturnClock(desktop);
    else assert(false && "unknown scenario");
}
