#include "AmoledApp.h"
#include "HomeScreen.h"
#include "assets/PokemonSprites.h"
#include "core/RoomResource.h"
#include "core/UiStrings.h"
#include "game/ExploreCaveTiles.h"
#include "game/ExploreBoss.h"
#include "game/ExploreIceSlide.h"
#include "game/ExploreRouteGeometry.h"
#include "game/MonsterFactory.h"
#include "game/GameRandom.h"
#include "core/AudioManager.h"
#include "game/HatchService.h"
#include "game/ItemInventory.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/Canvas565.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace AmoledV1 {

namespace {

class HatchImuSpy final : public Platform::IImuDevice {
public:
    bool enabled = false;
    bool fail = false;
    float x = 0, y = 0, z = 1;
    size_t reads = 0, enables = 0, disables = 0;
    bool setAccelerationEnabled(bool on) override {
        if (on) ++enables; else ++disables;
        if (fail) return false;
        enabled = on;
        return true;
    }
    bool readAcceleration(float& ax, float& ay, float& az) override {
        assert(enabled);
        ++reads;
        ax = x; ay = y; az = z;
        return !fail;
    }
};

class ResetBlobStore final : public Platform::IBlobStore {
public:
    explicit ResetBlobStore(Platform::IBlobStore& backing) : backing_(backing) {}
    bool initialize() override { return backing_.initialize(); }
    size_t blobSize(const char* ns, const char* key) override {
        return backing_.blobSize(ns, key);
    }
    bool readBlob(const char* ns, const char* key, void* data, size_t size) override {
        return backing_.readBlob(ns, key, data, size);
    }
    bool writeBlob(const char* ns, const char* key, const void* data,
                   size_t size) override {
        ++writes;
        if (failSnapshots && std::strcmp(ns, "stickmon") == 0 &&
            (std::strcmp(key, "state_a") == 0 || std::strcmp(key, "state_b") == 0 ||
             std::strcmp(key, "state") == 0)) return false;
        return backing_.writeBlob(ns, key, data, size);
    }
    bool removeBlob(const char* ns, const char* key) override {
        return backing_.removeBlob(ns, key);
    }
    bool clearNamespace(const char* ns) override {
        ++clears;
        clearedNamespace = ns;
        return !failClear && backing_.clearNamespace(ns);
    }
    std::vector<uint8_t> contents(const char* ns, const char* key) {
        std::vector<uint8_t> data(blobSize(ns, key));
        if (!data.empty()) assert(readBlob(ns, key, data.data(), data.size()));
        return data;
    }

    bool failClear = false;
    bool failSnapshots = false;
    size_t writes = 0;
    size_t clears = 0;
    std::string clearedNamespace;

private:
    Platform::IBlobStore& backing_;
};

class ResetPowerSpy final : public Platform::IPowerDevice {
public:
    explicit ResetPowerSpy(Platform::IPowerDevice& backing) : backing_(backing) {}
    int batteryLevel() override { return backing_.batteryLevel(); }
    Platform::PowerCapabilities capabilities() const override {
        return backing_.capabilities();
    }
    Platform::WakeReason wakeReason() const override {
        return backing_.wakeReason();
    }
    uint32_t hardwareRandom() override { return backing_.hardwareRandom(); }
    size_t externalMemorySize() const override {
        return backing_.externalMemorySize();
    }
    size_t externalMemoryFree() const override {
        return backing_.externalMemoryFree();
    }
    void restart() override { ++restarts; }
    void enterDeepSleep(uint64_t timer, bool secondary) override {
        backing_.enterDeepSleep(timer, secondary);
    }
    size_t restarts = 0;

private:
    Platform::IPowerDevice& backing_;
};

size_t resetTestOtaCalls = 0;
bool resetTestOtaHandler() {
    ++resetTestOtaCalls;
    return true;
}

}  // namespace

// Exercise real application transitions; only initial state and diagnostics need access.
struct AmoledAppTestAccess {
    using Phase = AmoledApp::ExpeditionDeparturePhase;
    using Scene = AppSceneFlow::Scene;

    static void checkBattleAssetActions(DesktopPlatform& desktop,
                                        const Game::GameState& state) {
        for (bool wildSide : {false, true}) {
            for (Game::MoveId id : {10, 154, 163, 44, 242, 43, 45, 28}) {
                const auto* move = findMove(id);
                assert(move);
                const auto fx = GameAssets::battleFxForMove(id, move->type);
                const bool status = GameAssets::battleFxIsStatus(fx);
                for (int mode = 0; mode < 4; ++mode) {
                    if (mode == 1 && !status) continue;
                    // 0: success; 1: capped stat; 2: miss; 3: immunity.
                    GameRandom::setRangeProvider(mode == 2
                        ? +[](uint32_t, uint32_t maximum) { return maximum - 1; }
                        : +[](uint32_t minimum, uint32_t) { return minimum; });
                    AmoledApp app;
                    app.begin(desktop.millis());
                    app.gameState = state;
                    app.gameState.team[0] = Game::MonsterFactory::create(4, 30);
                    app.battleWild = Game::MonsterFactory::create(7, 30);
                    app.battlePlayerSlot = 0;
                    app.sceneFlow.enter(Scene::BATTLE);
                    auto& attacker = wildSide ? app.battleWild : app.gameState.team[0];
                    auto& defender = wildSide ? app.gameState.team[0] : app.battleWild;
                    auto& attackState = wildSide ? app.battleWildState : app.battlePlayerState;
                    auto& defendState = wildSide ? app.battlePlayerState : app.battleWildState;
                    attacker.move2Id = id;
                    if (mode == 1) {
                        for (auto& stage : defendState.statStages) stage = -6;
                    }
                    if (mode == 2) {
                        attackState.statStages[static_cast<uint8_t>(BattleStat::ACCURACY)] = -6;
                    }
                    if (mode == 3) {
                        if (status && id != 28) continue;
                        defender = Game::MonsterFactory::create(92, 30);
                        // Gastly blocks normal slashes; Levitate blocks Sand Attack.
                        if (id == 44 || id == 242) continue;
                    }
                    const uint16_t before = defender.hpCur;
                    BattleTurnController::Action action{
                        wildSide ? BattleTurnController::Side::WILD
                                 : BattleTurnController::Side::PLAYER, 0, id};
                    if (wildSide) app.performBattleWildAction(action, desktop.millis());
                    else app.performBattlePlayerAction(action, desktop.millis());
                    assert(app.battleAnimationActive);
                    assert(app.battleAnimationAttackerWild == wildSide);
                    assert(app.battleAnimationFx == fx);
                    assert(app.battleAnimationStatusEffect == (status && mode == 0));
                    assert(app.battleAnimationHit == (!status && mode == 0));
                    if (status || mode != 0) {
                        assert(defender.hpCur == before);
                        assert(!app.battleHpAnimationActive);
                        assert(app.battleImpactSfx == static_cast<uint8_t>(
                            mode == 0 ? SfxCue::UI_CONFIRM : SfxCue::UI_CANCEL));
                    } else {
                        assert(defender.hpCur < before && app.battleHpAnimationActive);
                    }
                    app.battleContinuation = AmoledApp::BattleContinuation::NONE;
                    app.battleAudioPending = false;
                    desktop.advanceMs(app.battleAnimationDurationMs + 1);
                    app.update(desktop.millis());
                    assert(!app.battleAnimationActive && !app.battleAnimationStatusEffect);
                }
            }
        }
        GameRandom::setRangeProvider(nullptr);
        std::puts("battle asset actions: both sides, success, caps, misses and immunity verified");
    }

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

    static void tapMenu(AmoledApp& app, DesktopPlatform& desktop,
                        AppSceneFlow::MainMenuItem target) {
        app.menuScroll = mainMenuMaxScroll();
        if (target == AppSceneFlow::MainMenuItem::EXPLORE) app.menuScroll = 0;
        for (int y = 8; y < AmoledUi::HEIGHT; y += 8) {
            for (int x : {90, 270}) {
                const int index = mainMenuItemAt(x, y, app.menuScroll);
                if (index >= 0 && AppSceneFlow::mainMenuEntry(index,
                        STICKMON_ENABLE_DEBUG_FEATURES != 0).item == target) {
                    tap(app, desktop, x, y);
                    return;
                }
            }
        }
        assert(false && "menu item must be reachable");
    }

    static void checkHatchFrame(AmoledApp& app, Canvas565& canvas) {
        app.render(canvas);
        const std::vector<uint16_t> partial(canvas.rawPixels(),
                                          canvas.rawPixels() + canvas.pixelCount());
        app.requestFullRender();
        app.render(canvas);
        assert(std::equal(partial.begin(), partial.end(), canvas.rawPixels()));
        app.markRendered();
    }

    static void shakeHatch(AmoledApp& app, DesktopPlatform& desktop, HatchImuSpy& imu) {
        // Settle the device, then apply two samples in opposing directions.
        imu.x = 0;
        for (int i = 0; i < 60; ++i) {
            desktop.advanceMs(20); app.update(desktop.millis());
        }
        const uint16_t before = app.hatchWipeCount;
        for (int i = 0; i < 4; ++i) {
            imu.x = i % 2 ? -1.2f : 1.2f;
            desktop.advanceMs(20); app.update(desktop.millis());
        }
        assert(app.hatchWipeCount == before + 1);
        assert(app.hatchShaking && app.hatchLastShakeOffset != 0);
        imu.x = 0;
    }

    static void checkHatchFlow(DesktopPlatform& desktop, Canvas565& canvas) {
        auto& original = desktop.serviceBundle();
        ResetBlobStore blobs(original.blobs);
        HatchImuSpy imu;
        Platform::Services guarded{
            original.lifecycle, original.clock, original.logger, original.input,
            original.display, original.audio, imu, original.power,
            blobs, original.resources, original.memory, original.peers};
        Platform::bind(guarded);
        assert(blobs.clearNamespace("stickmon"));
        AmoledApp app;
        app.begin(desktop.millis());
        assert(!app.startupStateLoaded() && app.sceneFlow.current() == Scene::HATCH);
        assert(!app.gameState.oobeDone && app.gameState.teamCount == 0);
        assert(imu.enabled && imu.enables == 1);
        assert(app.gameState.coins == 0 && app.gameState.room.food[0] == 0);
        assert(!app.displayLockAllowed() && !app.prepareDisplayLock(desktop.millis()));
        app.render(canvas);
        const std::vector<uint16_t> initial(canvas.rawPixels(),
                                          canvas.rawPixels() + canvas.pixelCount());
        HatchViewModel egg;
        egg.gameMinutesOfDay = app.gameState.gameMinutesTotal % Game::GAME_MINUTES_PER_DAY;
        canvas.fillSprite(0xF81F);
        renderHatchScreen(canvas, egg);
        assert(std::equal(initial.begin(), initial.end(), canvas.rawPixels()));
        app.markRendered();
        desktop.advanceMs(40000);
        app.update(desktop.millis());
        assert(app.sceneFlow.current() == Scene::HATCH && app.hatchElapsedSeconds == 40);
        assert(app.renderRowBegin() >= HOME_STATUS_TOP || app.renderRowBegin() == 0);
        assert(app.renderRowEnd() == 448);
        checkHatchFrame(app, canvas);
        for (int i = 0; i < 10; ++i) tap(app, desktop, 184, 200);
        assert(app.hatchHeartVisible && app.hatchShaking);
        assert(app.hatchPokeCount == 10 && app.hatchWipeCount == 0);
        assert(app.renderRowBegin() == HOME_ROOM_TOP && app.renderRowEnd() == AmoledV1::HOME_STATUS_TOP);
        checkHatchFrame(app, canvas);
        tap(app, desktop, 272, 396);
        assert(app.hatchPokeCount == 10 && app.hatchWipeCount == 0);
        for (int i = 0; i < 3; ++i) shakeHatch(app, desktop, imu);
        assert(!app.hatchHeartVisible);
        checkHatchFrame(app, canvas);
        const uint16_t interactionsBeforeFeedback = app.hatchPokeCount + app.hatchWipeCount;
        desktop.advanceMs(900); app.update(desktop.millis());
        assert(app.hatchShaking && app.hatchLastShakeOffset != 0);
        assert(app.hatchPokeCount + app.hatchWipeCount == interactionsBeforeFeedback);
        checkHatchFrame(app, canvas);
        desktop.advanceMs(60); app.update(desktop.millis());
        assert(!app.hatchShaking && app.hatchLastShakeOffset == 0);
        checkHatchFrame(app, canvas);
        desktop.advanceMs(20); app.update(desktop.millis());
        assert(!app.needsRender());
        assert(app.saveState());
        const float pausedElapsed = app.hatchElapsedSeconds;
        assert(app.hatchPokeCount == 10 && app.hatchWipeCount == 3);
        Game::HatchProgress progress;
        assert(app.saveManager.loadHatchProgress(progress));
        assert(progress.elapsedSeconds == static_cast<uint16_t>(pausedElapsed) && progress.pokeCount == 10 && progress.wipeCount == 3);
        tap(app, desktop, 120, 396);
        assert(app.sceneFlow.current() == Scene::MAIN_MENU);
        assert(!imu.enabled);
        const size_t pausedReads = imu.reads;
        const size_t pausedDisables = imu.disables;
        tapMenu(app, desktop, AppSceneFlow::MainMenuItem::EXPLORE);
        assert(app.sceneFlow.current() == Scene::MAIN_MENU);
        assert(app.toast && std::strcmp(app.toast, Ui::Menu::HATCH_FIRST) == 0);
        tapMenu(app, desktop, AppSceneFlow::MainMenuItem::SETTINGS);
        assert(app.sceneFlow.current() == Scene::SETTINGS);
        app.gameState.settings.brightness = 96;
        assert(app.saveState());
        desktop.advanceMs(300000);
        app.update(desktop.millis());
        assert(app.hatchElapsedSeconds == pausedElapsed && !app.consumeLockRequest() && imu.reads == pausedReads);
        assert(imu.disables == pausedDisables);
        tap(app, desktop, 40, 38);
        assert(app.sceneFlow.current() == Scene::MAIN_MENU);
        tapMenu(app, desktop, AppSceneFlow::MainMenuItem::COMPUTER);
        assert(app.sceneFlow.current() == Scene::COMPUTER);
        tap(app, desktop, 180, 110);
        assert(app.sceneFlow.current() == Scene::COMPUTER);
        assert(app.toast && std::strcmp(app.toast, Ui::Menu::HATCH_FIRST) == 0);
        tap(app, desktop, 40, 38);
        tapMenu(app, desktop, AppSceneFlow::MainMenuItem::BACK);
        assert(app.sceneFlow.current() == Scene::HATCH);
        app.update(desktop.millis());
        assert(app.hatchElapsedSeconds == pausedElapsed && imu.enabled);

        AmoledApp resumed;
        resumed.begin(desktop.millis());
        assert(resumed.startupStateLoaded() && resumed.sceneFlow.current() == Scene::HATCH);
        assert(resumed.hatchElapsedSeconds == progress.elapsedSeconds && resumed.hatchPokeCount == 10);
        assert(resumed.hatchWipeCount == 3 && resumed.gameState.settings.brightness == 96);
        desktop.advanceMs((179 - progress.elapsedSeconds) * 1000);
        resumed.update(desktop.millis());
        assert(!resumed.gameState.oobeDone && resumed.hatchLastRemainingSecond == 1);
        desktop.advanceMs(1000);
        resumed.update(desktop.millis());
        assert(resumed.sceneFlow.current() == Scene::HOME && resumed.gameState.oobeDone);
        assert(!imu.enabled);
        const size_t finishedReads = imu.reads;
        desktop.advanceMs(1000); resumed.update(desktop.millis());
        assert(imu.reads == finishedReads);
        assert(resumed.gameState.teamCount == 1 && resumed.gameState.team[0].speciesId == 4);
        assert(resumed.gameState.team[0].origin == Game::Origin::HATCHED);
        assert(resumed.gameState.team[0].metArea == Game::MET_AREA_HATCHED);
        assert(resumed.gameState.team[0].metAt == resumed.gameState.gameMinutesTotal * 60);
        assert(resumed.gameState.team[0].level == 5 && resumed.gameState.coins == 1000);
        assert(blobs.blobSize("stickmon", "hatch") == 0);
        resumed.gameState.coins = 1234;
        assert(resumed.saveState());
        const auto originalIv = resumed.gameState.team[0].ivPacked;
        AmoledApp existing;
        existing.begin(desktop.millis());
        assert(existing.sceneFlow.current() == Scene::HOME && existing.gameState.oobeDone);
        assert(existing.gameState.coins == 1234 && existing.gameState.team[0].speciesId == 4);
        assert(existing.gameState.team[0].ivPacked == originalIv && !imu.enabled);

        for (uint8_t speed = 0; speed < 4; ++speed) {
            assert(blobs.clearNamespace("stickmon"));
            AmoledApp accelerated;
            accelerated.begin(desktop.millis());
            accelerated.gameState.settings.speedIndex = speed;
            if (speed == 0) {
                for (int i = 0; i < 10; ++i) shakeHatch(accelerated, desktop, imu);
            }
            const uint32_t duration = std::ceil((180 - accelerated.hatchElapsedSeconds) * 1000 / (1U << speed));
            desktop.advanceMs(duration - 2);
            accelerated.update(desktop.millis());
            assert(!accelerated.gameState.oobeDone && accelerated.gameState.teamCount == 0);
            desktop.advanceMs(2);
            accelerated.update(desktop.millis());
            assert(accelerated.sceneFlow.current() == Scene::HOME);
            assert(accelerated.gameState.team[0].speciesId == (speed == 0 ? 7 : 1));
        }

        assert(blobs.clearNamespace("stickmon"));
        AmoledApp retry;
        retry.begin(desktop.millis());
        for (int i = 0; i < 10; ++i) tap(retry, desktop, 184, 200);
        blobs.failSnapshots = true;
        desktop.advanceMs(180000);
        retry.update(desktop.millis());
        assert(retry.sceneFlow.current() == Scene::HATCH && !retry.gameState.oobeDone);
        assert(retry.gameState.teamCount == 0 && retry.hatchResult);
        assert(retry.hatchLastRemainingSecond == 0 && retry.hatchLastProgress == 100 && !imu.enabled);
        assert(retry.toast && std::strcmp(retry.toast, Ui::Hatch::SAVE_FAILED) == 0);
        assert(retry.saveManager.loadHatchProgress(progress) && progress.elapsedSeconds == 180);
        const auto candidateIv = retry.hatchResult->team[0].ivPacked;
        tap(retry, desktop, 272, 396);
        assert(retry.hatchWipeCount == 0);
        blobs.failSnapshots = false;
        desktop.advanceMs(1000);
        retry.update(desktop.millis());
        assert(retry.sceneFlow.current() == Scene::HOME && retry.gameState.team[0].speciesId == 4);
        assert(retry.gameState.team[0].ivPacked == candidateIv);
        assert(blobs.blobSize("stickmon", "hatch") == 0);

        for (bool future : {false, true}) {
            assert(blobs.clearNamespace("stickmon"));
            const uint8_t record[] = {0x4d, 0x53, 0x56, 0x31,
                static_cast<uint8_t>(future ? 99 : 4), 0, 0, 0, 0xaa, 0xbb, 0xcc, 0xdd};
            assert(blobs.writeBlob("stickmon", "state", record, sizeof(record)));
            const auto before = blobs.contents("stickmon", "state");
            AmoledApp protectedApp;
            const size_t writes = blobs.writes;
            protectedApp.begin(desktop.millis());
            assert(protectedApp.saveWritesBlocked && !protectedApp.saveState());
            desktop.advanceMs(180000);
            protectedApp.update(desktop.millis());
            assert(!protectedApp.gameState.oobeDone && protectedApp.sceneFlow.current() == Scene::HATCH);
            assert(blobs.writes == writes && blobs.contents("stickmon", "state") == before);
        }
        // Failed hardware reads do not block touching or completing the egg.
        assert(blobs.clearNamespace("stickmon"));
        imu.fail = true;
        AmoledApp noImu;
        noImu.begin(desktop.millis());
        tap(noImu, desktop, 184, 200);
        assert(noImu.hatchPokeCount == 1 && noImu.hatchHeartVisible);
        desktop.advanceMs(180000); noImu.update(desktop.millis());
        assert(noImu.gameState.oobeDone);
        imu.fail = false;
        desktop.advanceMs(1000); noImu.update(desktop.millis());
        assert(!imu.enabled);
        Platform::bind(original);
        std::puts("hatch: first frame, interaction, menu pause, resume, speed, commit retry and existing saves verified");
    }

    static void checkSettingsReset(DesktopPlatform& desktop) {
        auto& original = desktop.serviceBundle();
        ResetBlobStore blobs(original.blobs);
        ResetPowerSpy power(original.power);
        Platform::Services guarded{
            original.lifecycle, original.clock, original.logger, original.input,
            original.display, original.audio, original.imu, power,
            blobs, original.resources, original.memory, original.peers};
        Platform::bind(guarded);
        AmoledApp app;
        app.begin(desktop.millis());
        app.gameState.coins = 9876;
        app.gameState.gameMinutesTotal = 5432;
        app.gameState.team[0] = Game::MonsterFactory::create(4, 25);
        app.gameState.storageCount = 1;
        app.gameState.storage[0] = Game::MonsterFactory::create(25, 15);
        app.encounterHistory.add(150);
        app.encounterHistoryDirty = true;
        assert(app.saveState());
        assert(app.saveState());  // Populate both snapshot slots and legacy mirror.
        Game::HatchProgress hatch;
        hatch.elapsedSeconds = 91;
        hatch.pokeCount = 7;
        assert(app.saveManager.saveHatchProgress(hatch));
        const uint8_t marker[] = {1, 3, 5, 7};
        assert(blobs.writeBlob("wifi", "ssid", marker, sizeof(marker)));
        assert(blobs.writeBlob("claw", "coze_token", marker, sizeof(marker)));
        // Also clear unrecognized game keys, rather than only current save keys.
        assert(blobs.writeBlob("stickmon", "future_key", marker, sizeof(marker)));
        static constexpr const char* GAME_KEYS[] = {
            "state_a", "state_b", "state", "encounters", "hatch", "future_key",
        };
        std::vector<std::vector<uint8_t>> before;
        for (const char* key : GAME_KEYS) {
            before.push_back(blobs.contents("stickmon", key));
            assert(!before.back().empty());
        }
        const auto wifiBefore = blobs.contents("wifi", "ssid");
        const auto clawBefore = blobs.contents("claw", "coze_token");
        auto assertSavesUnchanged = [&]() {
            for (size_t i = 0; i < before.size(); ++i) {
                assert(blobs.contents("stickmon", GAME_KEYS[i]) == before[i]);
            }
            assert(blobs.contents("wifi", "ssid") == wifiBefore);
            assert(blobs.contents("claw", "coze_token") == clawBefore);
            assert(power.restarts == 0);
            assert(!app.gameResetPending);
        };
        app.sceneFlow.enter(Scene::SETTINGS);
        app.markRendered();
        tap(app, desktop, 200, 368);  // V1 has no action below reset.
        assert(!app.settingsResetConfirm);
        assertSavesUnchanged();
        size_t writesBefore = blobs.writes;
        tap(app, desktop, 200, 320);
        assert(app.settingsResetConfirm);
        assert(blobs.clears == 0 && blobs.writes == writesBefore);
        assert(app.needsRender());
        assert(app.renderRowBegin() == 0 && app.renderRowEnd() == AmoledUi::HEIGHT);
        assertSavesUnchanged();
        tap(app, desktop, 184, 300);  // Body text and blank area are inert.
        assert(app.settingsResetConfirm);
        tap(app, desktop, 106, 390);  // Cancel returns to the settings list.
        assert(!app.settingsResetConfirm && app.sceneFlow.current() == Scene::SETTINGS);
        assert(blobs.clears == 0 && blobs.writes == writesBefore);
        assertSavesUnchanged();
        tap(app, desktop, 200, 320);
        tap(app, desktop, 40, 38);  // Header back also only dismisses confirmation.
        assert(!app.settingsResetConfirm && app.sceneFlow.current() == Scene::SETTINGS);
        assertSavesUnchanged();

        app.visitSession.startSearch();
        assert(app.visitSession.busy());
        tap(app, desktop, 200, 320);
        assert(!app.settingsResetConfirm);
        assert(blobs.clears == 0);
        assertSavesUnchanged();
        app.visitSession.stop();

        app.setSystemUpdateHandler(resetTestOtaHandler);
        resetTestOtaCalls = 0;
        tap(app, desktop, 200, 320);  // Adding OTA does not replace the reset row.
        assert(app.settingsResetConfirm && resetTestOtaCalls == 0);
        tap(app, desktop, 106, 390);
        tap(app, desktop, 200, 368);
#if STICKMON_ENABLE_DEBUG_FEATURES
        assert(resetTestOtaCalls == 1 && !app.settingsResetConfirm);
#else
        assert(resetTestOtaCalls == 0 && !app.settingsResetConfirm);
        assertSavesUnchanged();
#endif
        assert(blobs.clears == 0 && power.restarts == 0);
        // OTA legitimately saves before taking over; use its saved state below.
        for (size_t i = 0; i < before.size(); ++i) {
            before[i] = blobs.contents("stickmon", GAME_KEYS[i]);
        }
        writesBefore = blobs.writes;
        tap(app, desktop, 200, 320);
        assert(app.settingsResetConfirm);
        TouchEvent event;
        event.timestampMs = desktop.millis();
        event.y = 390;
        event.type = TouchEventType::UP;
        event.x = 262;
        app.handleTouch(event);  // An orphan release cannot confirm deletion.
        assert(blobs.clears == 0);
        event.type = TouchEventType::DOWN;
        event.x = 333;
        app.handleTouch(event);
        event.type = TouchEventType::MOVE;
        event.x = 330;
        app.handleTouch(event);
        event.type = TouchEventType::UP;
        app.handleTouch(event);  // A short move from outside the button is inert.
        assert(blobs.clears == 0 && app.settingsResetConfirm);
        event.type = TouchEventType::DOWN;
        app.handleTouch(event);
        event.type = TouchEventType::UP;
        event.x = 333;
        app.handleTouch(event);  // Releasing outside is inert too.
        assert(blobs.clears == 0);
        assert(blobs.writes == writesBefore);
        assertSavesUnchanged();

        app.visitSession.startSearch();
        assert(app.visitSession.busy());
        tap(app, desktop, 262, 390);  // A session starting on the dialog blocks reset.
        assert(blobs.clears == 0 && app.settingsResetConfirm);
        assertSavesUnchanged();
        app.visitSession.stop();
        blobs.failClear = true;
        tap(app, desktop, 262, 390);
        assert(blobs.clears == 1 && app.settingsResetConfirm && app.storageReady);
        assert(app.toast && std::strcmp(app.toast, Ui::Settings::RESET_FAILED) == 0);
        assert(app.gameState.coins == 9876 && app.gameState.storageCount == 1);
        assert(blobs.writes == writesBefore);
        assertSavesUnchanged();

        blobs.failClear = false;
        tap(app, desktop, 262, 390);
        assert(blobs.clears == 2 && blobs.clearedNamespace == "stickmon");
        assert(app.gameResetPending && !app.storageReady && power.restarts == 1);
        assert(!app.displayLockAllowed());
        assert(!app.visitSession.busy());
        for (const char* key : GAME_KEYS) assert(blobs.blobSize("stickmon", key) == 0);
        assert(blobs.contents("wifi", "ssid") == wifiBefore);
        assert(blobs.contents("claw", "coze_token") == clawBefore);
        const size_t writesAfterReset = blobs.writes;
        const uint32_t oldMinutes = app.gameState.gameMinutesTotal;
        const uint32_t oldCoins = app.gameState.coins;
        desktop.advanceMs(120000);
        app.update(desktop.millis());  // The old app cannot recreate its erased save.
        assert(!app.saveState());
        tap(app, desktop, 262, 390);
        assert(app.gameState.gameMinutesTotal == oldMinutes && app.gameState.coins == oldCoins);
        assert(blobs.writes == writesAfterReset && power.restarts == 1);
        assert(blobs.clears == 2);
        for (const char* key : GAME_KEYS) assert(blobs.blobSize("stickmon", key) == 0);

        AmoledApp fresh;
        fresh.begin(desktop.millis());  // Simulate restart using the same NVS contents.
        assert(!fresh.startupStateLoaded());
        assert(!fresh.gameResetPending && fresh.storageReady);
        assert(fresh.gameState.gameMinutesTotal == 7 * 60);
        assert(fresh.sceneFlow.current() == Scene::HATCH);
        assert(fresh.gameState.coins == 0 && !fresh.gameState.oobeDone);
        assert(fresh.gameState.teamCount == 0 && fresh.gameState.storageCount == 0);
        assert(fresh.hatchElapsedSeconds == 0 && fresh.hatchPokeCount == 0 && fresh.hatchWipeCount == 0);
        desktop.advanceMs(180000);
        fresh.update(desktop.millis());
        assert(fresh.sceneFlow.current() == Scene::HOME && fresh.gameState.oobeDone);
        assert(fresh.gameState.coins == 1000 && fresh.gameState.teamCount == 1);
        const Game::GameState defaults;
        assert(fresh.gameState.team[0].speciesId == defaults.team[0].speciesId);
        assert(fresh.gameState.team[0].level == defaults.team[0].level);
        assert(fresh.gameState.team[0].hpCur > 0);
        assert(!fresh.encounterHistory.contains(150));
        assert(blobs.blobSize("stickmon", "hatch") == 0);
        assert(blobs.blobSize("stickmon", "future_key") == 0);
        fresh.gameState.coins = 1234;
        assert(fresh.saveState());
        Game::GameState restored;
        MainSceneViewState restoredView;
        assert(fresh.saveManager.load(restored, restoredView));
        assert(restored.coins == 1234 && restored.storageCount == 0);
        assert(blobs.contents("wifi", "ssid") == wifiBefore);
        assert(blobs.contents("claw", "coze_token") == clawBefore);
        Platform::bind(original);
        std::puts("settings reset: confirmation, storage failure, namespace isolation and restart verified");
    }

    static void checkLockFocusPixels(DesktopPlatform& desktop, Canvas565& canvas,
                                     const Game::GameState& state) {
        AmoledApp app;
        app.begin(desktop.millis());
        app.gameState = state;
        app.sceneFlow.goHome();
        for (uint16_t species : {1, 4, 7, 6, 143, 92}) {
            app.gameState.team[0] = Game::MonsterFactory::create(species, 5);
            app.syncHomeActors(desktop.millis());
            PokemonSprites::PetAnimationProfile profile;
            assert(PokemonSprites::petAnimationProfile(species, profile));
            for (auto direction : {PokemonSprites::WalkDirection::DOWN,
                                   PokemonSprites::WalkDirection::LEFT,
                                   PokemonSprites::WalkDirection::UP,
                                   PokemonSprites::WalkDirection::RIGHT}) {
                for (int pose = 0; pose < 3; ++pose) {
                    for (int hop : {0, -14}) {
                        app.petDirection = direction;
                        app.petFrame = 0;
                        app.petResting = pose == 1;
                        app.petMotion = pose == 2 ? AmoledApp::PetMotion::WANDERING
                                                  : AmoledApp::PetMotion::IDLE;
                        app.pairMainRenderOffsetY = hop / 2.0f;
                        HomeViewModel model;
                        app.fillHomePetModel(model);
                        bool flip = false;
                        auto kind = profile.sleepingBase;
                        if (pose == 0) {
                            const int directionIndex = direction == PokemonSprites::WalkDirection::LEFT ? 2
                                : direction == PokemonSprites::WalkDirection::UP ? 4
                                : direction == PokemonSprites::WalkDirection::RIGHT
                                    ? (profile.mirrorRightDirections ? 2 : 6) : 0;
                            flip = direction == PokemonSprites::WalkDirection::RIGHT &&
                                   profile.mirrorRightDirections;
                            kind = static_cast<PokemonSprites::SpriteKind>(
                                static_cast<int>(profile.idleBase) + directionIndex * profile.idleFrames);
                        } else if (pose == 2) {
                            PokemonSprites::WalkingAnimation walking;
                            assert(PokemonSprites::walkingAnimation(species, direction, walking));
                            kind = walking.base;
                            flip = walking.flipX;
                        }
                        const auto* frame = PokemonSprites::findSpeciesSprite(species, kind);
                        assert(frame);
                        canvas.fillSprite(0);
                        assert(PokemonSprites::drawFrameSilhouette(frame, 0, 0, 0xFFFF, flip));
                        int left = 368, top = 448, right = -1, bottom = -1;
                        for (int py = 0; py < frame->height; ++py) {
                            for (int px = 0; px < frame->width; ++px) {
                                if (canvas.readPixel(px, py) != 0xFFFF) continue;
                                left = std::min(left, px); top = std::min(top, py);
                                right = std::max(right, px); bottom = std::max(bottom, py);
                            }
                        }
                        assert(right >= left && bottom >= top);
                        const int frameLeft = model.petCenterX - frame->width;
                        const int frameTop = model.petGroundY + hop - frame->height -
                            PokemonSprites::frameGroundOffsetY(frame) * 2;
                        const AmoledUi::Rect expected{frameLeft + left * 2, frameTop + top * 2,
                                                     (right - left + 1) * 2, (bottom - top + 1) * 2};
                        const auto bounds = homePetVisibleBounds(model);
                        assert(bounds.x == expected.x && bounds.y == expected.y);
                        assert(bounds.width == expected.width && bounds.height == expected.height);
                        int16_t x = 0, y = 0;
                        assert(app.lockFocusPoint(x, y));
                        assert(x == expected.x + expected.width / 2);
                        assert(y == expected.y + expected.height / 2);
                        assert(app.lockTapHitsPet(x, y));
                        assert(!app.lockTapHitsPet(expected.x - 13, y));
                        if (pose == 0 && hop == 0 && direction == PokemonSprites::WalkDirection::DOWN) {
                            std::printf("lock focus species %u: old center y=%d, visible center y=%d, offset=%d px\n",
                                        species, model.petGroundY - 64, y, y - (model.petGroundY - 64));
                        }
                    }
                }
            }
        }
        std::puts("lock focus: 144 poses match decoded sprite pixels, including mirror, sleep and hops");
    }

    static void checkLockRoom(DesktopPlatform& desktop,
                              const Game::GameState& state) {
        AmoledApp app;
        app.begin(desktop.millis());
        app.gameState = state;
        for (Scene scene : {Scene::MAIN_MENU, Scene::SETTINGS, Scene::BAG,
                            Scene::TEAM, Scene::EXPLORE_AREAS}) {
            app.sceneFlow.enter(scene);
            app.itemConfirmOpen = true;
            app.settingsResetConfirm = true;
            assert(app.prepareDisplayLock(desktop.millis()));
            assert(app.sceneFlow.current() == Scene::HOME);
            assert(!app.itemConfirmOpen && !app.settingsResetConfirm);
            assert(app.needsRender());
            int16_t x = 0, y = 0;
            assert(app.lockFocusPoint(x, y));
            assert(app.lockTapHitsPet(x, y));
            assert(!app.lockTapHitsPet(x + 76, y));
            assert(!app.lockTapHitsPet(x, y + 74));
            assert(!app.lockTapHitsPet(0, 0));
            app.homeMainActor.hidden = true;
            assert(!app.lockFocusPoint(x, y));
            assert(!app.lockTapHitsPet(x, y));
            app.homeMainActor.hidden = false;
        }
        app.visitDeparture = AmoledApp::VisitDeparture::AWAY;
        assert(!app.prepareDisplayLock(desktop.millis()));
        app.visitDeparture = AmoledApp::VisitDeparture::NONE;
        app.gameState.teamCount = 0;
        assert(!app.prepareDisplayLock(desktop.millis()));
        std::puts("lock room: menus return home and invisible pets cannot lock/unlock");
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

    static void checkExploreMotionDamage(DesktopPlatform& desktop, Canvas565& canvas,
                                         const Game::GameState& state) {
        using Scene = AppSceneFlow::Scene;
        for (uint16_t species : {8, 92, 143}) {
            AmoledApp app;
            app.begin(desktop.millis());
            app.gameState = state;
            app.gameState.teamCount = 2;
            app.gameState.team[0].speciesId = species;
            app.gameState.team[1].speciesId = 280;
            app.gameState.team[0].fainted = app.gameState.team[1].fainted = false;
            app.gameState.team[0].hpCur = app.gameState.team[1].hpCur = 10;
            app.selectedExploreArea = ExploreMapGenerator::TALL_GRASS_PARK_AREA;
            assert(ExploreMapGenerator::generate(9137, ExploreMapGenerator::Edge::LEFT,
                app.selectedExploreArea, app.exploreRouteMap));
            app.sceneFlow.enter(Scene::EXPLORE_ROUTE);
            app.exploreRoutePaused = false;
            app.exploreRouteComplete = false;
            app.exploreRouteBossPending = false;
            app.exploreRoutePickupAvailable = false;
            app.exploreRouteAutoWalk = false;
            app.exploreRoutePlayerWalkActive = false;
            app.exploreRouteExiting = false;
            app.autonomousExpedition = false;
            for (uint8_t direction = 0; direction < 4; ++direction) {
                uint32_t now = desktop.millis();
                app.exploreRouteDirection = direction;
                app.exploreRouteFollowerDirection = direction;
                app.exploreRouteMoving = app.exploreRouteFollowerMoving = true;
                app.exploreRouteWorldX = app.exploreRouteFromX = 70;
                app.exploreRouteWorldY = app.exploreRouteFromY = 72;
                app.exploreRouteFollowerWorldX = app.exploreRouteFollowerFromX = 54;
                app.exploreRouteFollowerWorldY = app.exploreRouteFollowerFromY = 56;
                const float dx = direction == 2 ? -16 : direction == 3 ? 16 : 0;
                const float dy = direction == 0 ? 16 : direction == 1 ? -16 : 0;
                app.exploreRouteTargetX = 70 + dx;
                app.exploreRouteTargetY = 72 + dy;
                app.exploreRouteFollowerTargetX = 54 + dx;
                app.exploreRouteFollowerTargetY = 56 + dy;
                app.exploreRouteMoveStartedMs = now;
                app.nextExploreRouteFrameMs = now + 90;
                app.nextExploreRouteMapFrameMs = now + 10000;
                app.updateExploreRouteCamera();
                app.forceFullRender();
                app.render(canvas);
                app.markRendered();
                const uint8_t frame = app.exploreRoutePetFrame;
                for (int tick = 1; tick <= 12; ++tick) {
                    const std::vector<uint16_t> before(canvas.rawPixels(),
                        canvas.rawPixels() + AmoledUi::WIDTH * AmoledUi::HEIGHT);
                    desktop.advanceMs(20);
                    app.updateExploreRoute(desktop.millis());
                    assert(app.needsRender());
                    if (tick < 5) assert(app.exploreRoutePetFrame == frame);
                    const int left = app.renderColumnBegin(), right = app.renderColumnEnd();
                    const int top = app.renderRowBegin(), bottom = app.renderRowEnd();
                    assert(right - left < AmoledUi::WIDTH);
                    assert(bottom - top < 240);
                    app.render(canvas);
                    const std::vector<uint16_t> partial(canvas.rawPixels(),
                        canvas.rawPixels() + AmoledUi::WIDTH * AmoledUi::HEIGHT);
                    for (int y = 0; y < AmoledUi::HEIGHT; ++y) {
                        for (int x = 0; x < AmoledUi::WIDTH; ++x) {
                            const int index = y * AmoledUi::WIDTH + x;
                            if (x < left || x >= right || y < top || y >= bottom)
                                assert(partial[index] == before[index]);
                        }
                    }
                    app.markRendered();
                    app.forceFullRender();
                    app.render(canvas);
                    assert(std::equal(partial.begin(), partial.end(), canvas.rawPixels()));
                    app.markRendered();
                }
                // Stopping the follower changes its frame even without a position change.
                app.exploreRouteMoving = app.exploreRouteFollowerMoving = false;
                app.requestExploreRouteDynamicRender();
                app.render(canvas);
                const std::vector<uint16_t> stopped(canvas.rawPixels(),
                    canvas.rawPixels() + AmoledUi::WIDTH * AmoledUi::HEIGHT);
                app.markRendered();
                app.forceFullRender();
                app.render(canvas);
                assert(std::equal(stopped.begin(), stopped.end(), canvas.rawPixels()));
                app.markRendered();
                app.exploreRoutePaused = true;
                desktop.advanceMs(20);
                app.updateExploreRoute(desktop.millis());
                assert(!app.needsRender());
                app.exploreRoutePaused = false;
            }
            // A changed viewport restores the entire map, including both actors.
            app.exploreRouteFromX = app.exploreRouteWorldX = 180;
            app.exploreRouteTargetX = 196;
            app.exploreRouteFromY = app.exploreRouteWorldY = 140;
            app.exploreRouteTargetY = 140;
            app.exploreRouteMoving = true;
            app.exploreRouteMoveStartedMs = desktop.millis();
            desktop.advanceMs(20);
            app.updateExploreRoute(desktop.millis());
            assert(app.renderColumnBegin() == 0 && app.renderColumnEnd() == AmoledUi::WIDTH);
            assert(app.renderRowBegin() == 0 && app.renderRowEnd() == AmoledV1::HOME_STATUS_TOP);
        }
        std::puts("explore motion: 20ms position refresh, all directions, floating bodies, shadows, follower stop, grass clips and scrolling verified");
    }

    static void checkParkRouteSelection(DesktopPlatform& desktop,
                                       const Game::GameState& state) {
        AmoledApp app;
        app.begin(desktop.millis());
        app.gameState = state;
        app.selectedExploreArea = ExploreMapGenerator::TALL_GRASS_PARK_AREA;
        app.exploreRouteMapBlock = 0;
        app.exploreRouteMapBlockCount = 1;
        for (bool withBoss : {false, true}) {
            uint8_t selected[4] = {};
            for (uint32_t seed = 1; seed <= 96; ++seed) {
                for (auto edge : {ExploreMapGenerator::Edge::TOP, ExploreMapGenerator::Edge::RIGHT,
                                  ExploreMapGenerator::Edge::BOTTOM, ExploreMapGenerator::Edge::LEFT}) {
                    app.exploreRouteExpeditionSeed = seed;
                    app.exploreRoutePendingEntryEdge = edge;
                    app.exploreRouteBossScheduled = withBoss;
                    assert(app.generateExploreRouteMap(desktop.millis()));
                    assert(app.exploreRouteMap.entry.edge == edge);
                    const uint8_t count = app.exploreRouteMap.pathCount;
                    assert(count >= 1 && count <= 3 && app.exploreRoutePath < count);
                    selected[count] |= 1U << app.exploreRoutePath;
                    const auto& path = app.exploreRouteMap.paths[app.exploreRoutePath];
                    auto start = ExploreRouteGeometry::pathPoint(path, 0);
                    assert(app.exploreRouteWorldX == start.x && app.exploreRouteWorldY == start.y);
                    if (withBoss) {
                        assert(ExploreBoss::canPlaceOnPath(path.pointCount));
                        assert(app.exploreRouteBossPending);
                        assert(app.exploreRouteBossIndex > 0 &&
                               app.exploreRouteBossIndex < path.pointCount - 1);
                    }
                }
            }
            assert(selected[1] == 1 && selected[2] == 3);
            assert((selected[3] & 4) != 0);  // The third exit must be selectable, including boss runs.
            if (!withBoss) assert(selected[3] == 7);
        }
        // Follow the third route to the next map with its actual exit direction.
        app.exploreRouteBossScheduled = false;
        app.exploreRouteMapBlockCount = 2;
        app.exploreRouteMapBlock = 0;
        for (uint32_t seed = 1; ; ++seed) {
            assert(seed < 100);
            app.exploreRouteExpeditionSeed = seed;
            assert(app.generateExploreRouteMap(desktop.millis()));
            if (app.exploreRouteMap.pathCount == 3) break;
        }
        app.exploreRoutePath = 2;
        auto exit = app.exploreRouteMap.paths[2].exit.edge;
        app.finishExploreRouteAtEnd(desktop.millis());
        assert(app.exploreRouteMapBlock == 1);
        assert(app.exploreRouteMap.entry.edge == ExploreMapGenerator::opposite(exit));
        std::puts("park routes: 1-3 exits, third-route selection, boss placement and map links verified");
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

    static void checkExploreSaveBoundaries(DesktopPlatform& desktop,
                                          const Game::GameState& state) {
        auto& original = desktop.serviceBundle();
        ResetBlobStore blobs(original.blobs);
        Platform::Services guarded{
            original.lifecycle, original.clock, original.logger, original.input,
            original.display, original.audio, original.imu, original.power,
            blobs, original.resources, original.memory, original.peers};
        Platform::bind(guarded);
        AmoledApp app;
        app.begin(desktop.millis());
        app.gameState = state;
        app.gameState.settings.idleTimeoutIndex = 4;
        app.gameState.stepsToday = 0;
        app.gameState.team[0].hpCur = app.gameState.team[0].hpMax - 5;
        app.gameState.team[0].bond = Game::Bond::MAX_VALUE;
        app.gameState.team[1].bond = Game::Bond::MAX_VALUE;
        app.sceneFlow.enter(Scene::EXPLORE_ROUTE);
        app.exploreSessionActive = true;
        app.exploreRouteMap = ExploreMapGenerator::Map{};
        app.exploreRouteMap.pathCount = 1;
        app.exploreRoutePath = 0;
        app.exploreRouteMapBlockCount = 1;
        auto& path = app.exploreRouteMap.paths[0];
        path.pointCount = 16;
        for (uint8_t index = 0; index < path.pointCount; ++index) {
            path.points[index] = {3, static_cast<uint8_t>(3 + index)};
        }
        app.exploreRouteEncounterCooldownSteps = 100;
        const uint16_t hpBefore = app.gameState.team[0].hpCur;
        const size_t beforeWalk = blobs.writes;
        for (int step = 0; step < 10; ++step) {
            assert(app.beginExploreRouteStep(desktop.millis()));
            desktop.advanceMs(1000);
            app.updateExploreRoute(desktop.millis());
            assert(!app.exploreRouteMoving);
            assert(app.sceneFlow.current() == Scene::EXPLORE_ROUTE);
        }
        assert(app.gameState.stepsToday == 10);
        assert(app.gameState.team[0].hpCur > hpBefore);
        assert(blobs.writes == beforeWalk);  // Includes the tenth-step recovery.
        desktop.advanceMs(6UL * 60UL * 1000UL);
        app.updateClockAndCare(desktop.millis());
        assert(blobs.writes == beforeWalk);  // Periodic care saves wait for exit.

        Game::GameState loaded;
        MainSceneViewState view;
        const auto load = [&]() { assert(app.saveManager.load(loaded, view)); };
        // Pickup IDs are local to AmoledApp.cpp: coin=1, potion=2.
        for (uint8_t pickup : {1, 2}) {
            const auto coins = app.gameState.coins;
            const auto potions = Game::ItemInventory::count(
                app.gameState, Game::ItemId::POTION);
            app.exploreRoutePickupAvailable = true;
            app.exploreRoutePickupItem = pickup;
            const auto writes = blobs.writes;
            app.resolveExploreRoutePickup(desktop.millis());
            assert(blobs.writes > writes);
            load();
            assert(loaded.stepsToday == 10);
            assert(loaded.team[0].hpCur == app.gameState.team[0].hpCur);
            if (pickup == 1) assert(loaded.coins > coins);
            else assert(Game::ItemInventory::count(loaded, Game::ItemId::POTION) ==
                        potions + 1);
        }

        // A battle closes after all rewards and boss progress have settled.
        app.sceneFlow.enter(Scene::BATTLE);
        app.battlePhase = BattleViewModel::Phase::VICTORY;
        app.battleWild = Game::MonsterFactory::create(7, 5);
        app.battleRewardCoins = 123;
        app.battleRewardExp = 0;  // Keep this case independent of level animations.
        app.battleIsBoss = true;
        app.battleSpecialKind = ExploreSpecial::Kind::FIRST_LATIAS;
        const auto coins = app.gameState.coins;
        const auto writes = blobs.writes;
        std::snprintf(app.battleMessage, sizeof(app.battleMessage), "Victory");
        app.pushBattleLog(desktop.millis());
        app.finishBattleVictory(desktop.millis());
        assert(app.gameState.coins == coins + 123);
        assert(blobs.writes == writes);  // Reward logs still own the battle screen.
        app.clearBattleLog();
        app.finishBattleAfterFriendship(desktop.millis());
        assert(app.sceneFlow.current() == Scene::EXPLORE_ROUTE);
        load();
        assert(loaded.coins == coins + 123);
        assert(loaded.specialBossDefeatedMask & ExploreSpecial::LATIAS_DEFEATED);
        assert(loaded.explorePoolRerollCounts[0] == app.gameState.explorePoolRerollCounts[0]);

        // Autonomous victories share the exit save and resume automatic walking.
        app.sceneFlow.enter(Scene::BATTLE);
        app.autonomousExpedition = true;
        app.gameState.coins += 17;
        app.finishBattleAfterFriendship(desktop.millis());
        assert(app.exploreRouteAutoWalk);
        load();
        assert(loaded.coins == coins + 140);
        app.autonomousExpedition = false;

        // A level-up leaves battle for progression first; save at that handoff
        // and after the final progression choice, before resuming the map.
        app.sceneFlow.enter(Scene::BATTLE);
        const uint8_t oldLevel = app.gameState.team[0].level++;
        app.queueBattleProgression(0, oldLevel, true);
        app.finishBattleAfterFriendship(desktop.millis());
        assert(app.sceneFlow.current() == Scene::PROGRESSION);
        load();
        assert(loaded.team[0].level == oldLevel + 1);
        app.gameState.pendingLevelUp = true;
        app.completeProgression(desktop.millis());
        assert(app.sceneFlow.current() == Scene::EXPLORE_ROUTE);
        load();
        assert(!loaded.pendingLevelUp);

        // Escape uses the same exit, preserving damage even without a reward.
        app.sceneFlow.enter(Scene::BATTLE);
        --app.gameState.team[0].hpCur;
        app.closeBattle(desktop.millis());
        load();
        assert(loaded.team[0].hpCur == app.gameState.team[0].hpCur);

        // No recovery, status or bond changes: steps alone must still be saved.
        ++app.gameState.stepsToday;
        app.completeExploreReturn();
        assert(app.sceneFlow.current() == Scene::HOME);
        load();
        assert(loaded.stepsToday == 11);

        app.sceneFlow.enter(Scene::BATTLE);
        app.battlePhase = BattleViewModel::Phase::DEFEAT;
        app.finishBattleDefeat(desktop.millis());
        load();
        assert(loaded.team[0].hpCur == 0 && loaded.team[0].fainted);

        // Locking a menu must save at the actual lock boundary, too.
        app.sceneFlow.enter(Scene::SETTINGS);
        ++app.gameState.stepsToday;
        assert(app.prepareDisplayLock(desktop.millis()));
        load();
        assert(loaded.stepsToday == 12);
        const auto beforeHomeCare = blobs.writes;
        desktop.advanceMs(6UL * 60UL * 1000UL);
        app.updateClockAndCare(desktop.millis());
        assert(blobs.writes > beforeHomeCare);  // Home periodic saves still run.
        Platform::bind(original);
        std::puts("explore saves: no step writes, pickup, battle reward/flee/defeat, return and lock verified");
    }

    static void checkExploreReserveEncounters(DesktopPlatform& desktop,
                                              const Game::GameState& state) {
        AmoledApp app;
        app.begin(desktop.millis());
        app.gameState = state;
        app.sceneFlow.enter(Scene::EXPLORE_ROUTE);
        app.selectedExploreArea = 0;
        app.exploreRouteMapBlock = 0;
        app.exploreRouteMapBlockCount = 1;
        app.exploreRoutePath = 0;
        app.exploreRouteMap = ExploreMapGenerator::Map{};
        app.exploreRouteMap.pathCount = 1;
        auto& path = app.exploreRouteMap.paths[0];
        path.pointCount = 4;
        for (uint8_t index = 0; index < path.pointCount; ++index) {
            path.points[index] = {3, static_cast<uint8_t>(3 + index)};
        }

        // Faint the original leader in battle, then finish the automatic switch
        // and return to the route without changing the permanent team order.
        assert(app.beginExploreEncounter(desktop.millis()));
        assert(app.battlePlayerSlot == 0);
        app.gameState.team[0].hpCur = 0;
        assert(app.resolveBattleFaint(desktop.millis()));
        assert(app.gameState.team[0].fainted);
        desktop.advanceMs(360);
        app.updateBattleSwitch(desktop.millis());
        desktop.advanceMs(360);
        app.updateBattleSwitch(desktop.millis());
        assert(app.battlePlayerSlot == 1);
        assert(app.battleSwitchStage == AmoledApp::BattleSwitchStage::NONE);
        app.closeBattle(desktop.millis());

        app.exploreRouteIndex = 1;
        app.exploreRouteGuaranteedEncounterIndex = 1;
        app.exploreRouteGuaranteedEncounterPending = true;
        app.exploreRouteMapEncounterCount = 0;
        app.resolveExploreStepEvent(desktop.millis(), false, false);
        assert(app.sceneFlow.current() == Scene::BATTLE);
        assert(app.battlePlayerSlot == 1 && !app.battleIsBoss);
        assert(app.exploreRouteMapEncounterCount == 1);
        assert(!app.exploreRouteGuaranteedEncounterPending);
        assert(app.toast == nullptr);
        app.closeBattle(desktop.millis());

        app.exploreRouteBossPending = true;
        app.exploreRouteBossIndex = 2;
        app.exploreRouteIndex = 2;
        app.exploreRouteBossSpeciesId = 7;
        app.exploreRouteBossLevel = 12;
        app.exploreRouteBossExperiencePercent = 150;
        app.resolveExploreStepEvent(desktop.millis(), true, true);
        assert(app.sceneFlow.current() == Scene::BATTLE);
        assert(app.battlePlayerSlot == 1 && app.battleIsBoss);
        assert(app.battleWild.speciesId == 7 && app.battleWild.level == 12);
        assert(app.battleExperiencePercent == 150);
        assert(!app.exploreRouteBossPending);
        assert(app.gameState.team[0].speciesId == state.team[0].speciesId);
        assert(app.gameState.team[1].speciesId == state.team[1].speciesId);
        assert(app.gameState.activeSlot == state.activeSlot);
        app.closeBattle(desktop.millis());

        // The route-end retry must use the same healthy reserve selection.
        app.exploreRouteBossPending = true;
        assert(app.finishExploreRouteAtEnd(desktop.millis()));
        assert(app.battlePlayerSlot == 1 && app.battleIsBoss);
        assert(!app.exploreRouteBossPending);
        app.closeBattle(desktop.millis());

        // Zero HP and an explicit fainted flag both prevent entry. A healthy
        // first member remains preferred; absent members are never selected.
        for (bool boss : {false, true}) {
            for (int healthCase = 0; healthCase < 7; ++healthCase) {
                app.gameState = state;
                if (healthCase == 1 || healthCase == 3 || healthCase == 5) {
                    app.gameState.team[0].hpCur = 0;
                }
                if (healthCase == 2 || healthCase == 4) {
                    app.gameState.team[0].fainted = true;
                }
                if (healthCase == 3) app.gameState.team[1].fainted = true;
                if (healthCase == 4) app.gameState.team[1].hpCur = 0;
                if (healthCase == 5) app.gameState.teamCount = 1;
                if (healthCase == 6) app.gameState.teamCount = 0;
                const bool expectedEntry = healthCase < 3;
                assert(app.beginExploreEncounter(desktop.millis(), boss, 7, 12) ==
                       expectedEntry);
                if (expectedEntry) {
                    assert(app.battlePlayerSlot == (healthCase == 0 ? 0 : 1));
                    app.closeBattle(desktop.millis());
                } else {
                    assert(app.sceneFlow.current() == Scene::EXPLORE_ROUTE);
                    assert(app.toast && std::strcmp(app.toast, Ui::Menu::PET_REST) == 0);
                }
            }
        }
        std::puts("explore reserve: faint switch, next encounter, boss and empty-team guards verified");
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
        checkLockRoom(desktop, state);
        checkDisplayLockArbitration(desktop, state);
        checkMenuPanel(canvas);
        checkMenuDismiss(desktop, state);
        checkFaintedReturn(desktop, canvas, state);
        checkRouteExitWalkFrames(desktop, state);
        checkFrostLadderLinks(desktop, state);
        checkImmediateNextFrame(desktop, canvas, state);
        for (int returnMode = 0; returnMode < 4; ++returnMode) {
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
            if (returnMode == 3) {
                app.sceneFlow.openMenu();
                assert(!app.prepareDisplayLock(desktop.millis()));
                assert(!app.prepareDisplayLock(desktop.millis()));
            } else if (returnMode == 1) {
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
            if (returnMode == 3) {
                assert(app.prepareDisplayLock(desktop.millis()));
                int16_t x = 0, y = 0;
                assert(app.lockFocusPoint(x, y));
                assert(app.lockTapHitsPet(x, y));
            }
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

void checkExploreReserveEncounters(DesktopPlatform& desktop,
                                   const Game::GameState& state) {
    AmoledV1::AmoledAppTestAccess::checkExploreReserveEncounters(desktop, state);
}

void checkExploreSaveBoundaries(DesktopPlatform& desktop,
                                const Game::GameState& state) {
    AmoledV1::AmoledAppTestAccess::checkExploreSaveBoundaries(desktop, state);
}

void checkProgressionReplacement(DesktopPlatform& desktop,
                                 const Game::GameState& state) {
    AmoledV1::AmoledAppTestAccess::checkMoveReplacement(desktop, state);
}

void checkSettingsReset(DesktopPlatform& desktop) {
    AmoledV1::AmoledAppTestAccess::checkSettingsReset(desktop);
}

void checkBattleAssetActions(DesktopPlatform& desktop, const Game::GameState& state) {
    AmoledV1::AmoledAppTestAccess::checkBattleAssetActions(desktop, state);
}


void checkHatchFlow(DesktopPlatform& desktop, Canvas565& canvas) {
    AmoledV1::AmoledAppTestAccess::checkHatchFlow(desktop, canvas);
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

void checkLockFocusPixels(DesktopPlatform& desktop, Canvas565& canvas,
                         const Game::GameState& state) {
    AmoledV1::AmoledAppTestAccess::checkLockFocusPixels(desktop, canvas, state);
}

void checkExploreMotionDamage(DesktopPlatform& desktop, Canvas565& canvas,
                             const Game::GameState& state) {
    AmoledV1::AmoledAppTestAccess::checkExploreMotionDamage(desktop, canvas, state);
}

void checkExploreParkRoutes(DesktopPlatform& desktop, const Game::GameState& state) {
    AmoledV1::AmoledAppTestAccess::checkParkRouteSelection(desktop, state);
}
