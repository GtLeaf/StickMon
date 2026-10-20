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

size_t sharedBattleDraws = 0;
uint32_t sharedBattleMinimum(uint32_t minimum, uint32_t) {
    ++sharedBattleDraws;
    return minimum;
}

}  // namespace

// Exercise real application transitions; only initial state and diagnostics need access.
struct AmoledAppTestAccess {
    using Phase = AmoledApp::ExpeditionDeparturePhase;
    using Scene = AppSceneFlow::Scene;

    static bool finishRoute(AmoledApp& app, uint32_t nowMs) {
        const bool advanced = app.exploreSession_.finishAtEnd(app.gameState);
        const bool autoReturn = app.exploreSession_.state().exploreRouteComplete && app.autonomousExpedition;
        app.applyExploreEffects(nowMs);
        return advanced || autoReturn;
    }
    static void pickupRoute(AmoledApp& app, uint32_t nowMs) {
        app.exploreSession_.resolvePickup(app.gameState);
        app.applyExploreEffects(nowMs);
    }
    static void resolveRoute(AmoledApp& app, uint32_t nowMs, bool blocked, bool repel) {
        app.exploreSession_.resolveStepEvent(app.gameState, blocked, repel);
        app.applyExploreEffects(nowMs);
    }

    static void checkNoEffectBattleLogs(DesktopPlatform& desktop,
                                        const Game::GameState& state) {
        for (bool wildSide : {false, true}) {
            // Splash, a capped stat drop, an immune damaging move, and a
            // successful stat drop exercise the distinct result sources.
            for (int scenario = 0; scenario < 4; ++scenario) {
                GameRandom::setRangeProvider(+[](uint32_t minimum, uint32_t) { return minimum; });
                AmoledApp app;
                app.begin(desktop.millis());
                app.gameState = state;
                app.gameState.teamCount = 1;
                app.gameState.team[0] = Game::MonsterFactory::create(wildSide ? 7 : 129, 10);
                app.battleSession_.state_.battleWild = Game::MonsterFactory::create(wildSide ? 129 : 7, 10);
                app.battleSession_.state_.battlePlayerSlot = 0;
                app.sceneFlow.enter(Scene::BATTLE);
                app.battleSession_.state_.battlePlayerState = {};
                app.battleSession_.state_.battleWildState = {};
                auto& attacker = wildSide ? app.battleSession_.state_.battleWild : app.gameState.team[0];
                auto& defender = wildSide ? app.gameState.team[0] : app.battleSession_.state_.battleWild;
                auto& defendState = wildSide ? app.battleSession_.state_.battlePlayerState : app.battleSession_.state_.battleWildState;
                const Game::MoveId move = scenario == 0 ? 150 : scenario == 2 ? 10 : 45;
                attacker.move2Id = move;
                if (scenario == 1) {
                    defendState.statStages[static_cast<uint8_t>(BattleStat::ATTACK)] = -6;
                }
                if (scenario == 2) defender = Game::MonsterFactory::create(92, 10);
                const auto attackerHp = attacker.hpCur;
                const auto defenderHp = defender.hpCur;
                app.battleSession_.clearLog();
                BattleTurnController::Action action{
                    wildSide ? BattleTurnController::Side::WILD
                             : BattleTurnController::Side::PLAYER, 0, move};
                if (wildSide) app.performBattleWildAction(action, desktop.millis());
                else app.performBattlePlayerAction(action, desktop.millis());
                assert(app.battleSession_.timeline_.state_.battleLogVisibleCount == 1);
                assert(std::strstr(app.battleSession_.timeline_.state_.battleLogLines[0], findMove(move)->name));
                app.battleSession_.timeline_.state_.battleAttackLogHeld = false;
                int noEffect = 0, resultLines = 0;
                while (app.battleSession_.timeline_.state_.battleLogQueueCount > 0) {
                    desktop.advanceMs(5000);
                    assert(app.battleSession_.serviceLog(desktop.millis()));
                    const auto* line = app.battleSession_.timeline_.state_.battleLogLines[app.battleSession_.timeline_.state_.battleLogVisibleCount - 1];
                    if (std::strcmp(line, Ui::Explore::NO_EFFECT) == 0) ++noEffect;
                    ++resultLines;
                }
                assert(resultLines == 1);
                assert(noEffect == (scenario == 3 ? 0 : 1));
                assert(attacker.hpCur == attackerHp && defender.hpCur == defenderHp);
                if (scenario == 3) {
                    assert(defendState.statStages[static_cast<uint8_t>(BattleStat::ATTACK)] == -1);
                }
            }
        }
        GameRandom::setRangeProvider(nullptr);
        std::puts("battle no-effect logs: Splash, capped status and immunity report once for both attackers; successful status retained");
    }

    static void checkBattleItemsAndBlocks(DesktopPlatform& desktop,
                                          const Game::GameState& state) {
        for (bool wildSide : {false, true}) {
            AmoledApp app;
            app.begin(desktop.millis());
            app.gameState = state;
            app.gameState.team[0] = Game::MonsterFactory::create(7, 20);
            app.battleSession_.state_.battleWild = Game::MonsterFactory::create(4, 20);
            app.battleSession_.state_.battlePlayerSlot = 0;
            app.sceneFlow.enter(Scene::BATTLE);
            auto& attacker = wildSide ? app.battleSession_.state_.battleWild : app.gameState.team[0];
            auto& attackState = wildSide ? app.battleSession_.state_.battleWildState : app.battleSession_.state_.battlePlayerState;
            attackState = {};
            attackState.confusionTurns = 3;
            const auto hp = attacker.hpCur;
            GameRandom::setRangeProvider(+[](uint32_t minimum, uint32_t) { return minimum; });
            app.battleSession_.clearLog();
            BattleTurnController::Action action{
                wildSide ? BattleTurnController::Side::WILD : BattleTurnController::Side::PLAYER,
                0, 33};
            if (wildSide) app.performBattleWildAction(action, desktop.millis());
            else app.performBattlePlayerAction(action, desktop.millis());
            char expected[128];
            std::snprintf(expected, sizeof(expected), Ui::Explore::CONFUSION_HIT_FMT,
                          findSpecies(attacker.speciesId)->name);
            assert(std::strcmp(app.battleSession_.timeline_.state_.battleLogLines[0], expected) == 0);
            assert(attacker.hpCur < hp && app.battleSession_.state_.battleTurnDamaged[wildSide ? 1 : 0]);
            assert(app.battleSession_.timeline_.state_.battleHpAnimationActive && app.battleSession_.timeline_.state_.battleHpAnimationWild == wildSide);
            const auto started = app.battleSession_.timeline_.state_.battleHpAnimationStartedMs;
            const auto before = app.battleHpPercentForRender(wildSide, attacker.hpCur,
                                                            attacker.hpMax, started);
            const auto middle = app.battleHpPercentForRender(wildSide, attacker.hpCur,
                                                            attacker.hpMax, started + 210);
            const auto after = app.battleHpPercentForRender(wildSide, attacker.hpCur,
                                                           attacker.hpMax, started + 420);
            assert(before == hp * 100 / attacker.hpMax);
            assert(before > middle && middle > after);
            assert(after == attacker.hpCur * 100 / attacker.hpMax);
            desktop.advanceMs(210);
            app.update(desktop.millis());
            assert(app.battleSession_.timeline_.state_.battleHpAnimationActive);
            assert(app.battleSession_.state_.battleContinuation == AmoledApp::BattleContinuation::ADVANCE_TURN);
            desktop.advanceMs(210);
            app.update(desktop.millis());
            assert(!app.battleSession_.timeline_.state_.battleHpAnimationActive);
        }
        AmoledApp app;
        app.begin(desktop.millis());
        app.gameState = state;
        app.gameState.oobeDone = true;
        app.battleSession_.state_.battleWild = Game::MonsterFactory::create(4, 10);
        app.battleSession_.state_.battlePlayerSlot = 0;
        app.sceneFlow.enter(Scene::BATTLE);
        app.battleSession_.clearLog();
        app.performBattleBag(desktop.millis());
        const auto item = Game::ItemId::SWEET_FOOD;
        Game::ItemInventory::add(app.gameState, item);
        const auto stock = Game::ItemInventory::count(app.gameState, item);
        const auto hp = app.battleSession_.state_.battleWild.hpCur;
        app.performBattleBagItem(item, desktop.millis());
        assert(app.sceneFlow.current() == Scene::BATTLE);
        assert(app.battleSession_.timeline_.state_.battleThrownItem == item && app.battleSession_.timeline_.state_.battleLogQueueCount == 1);
        assert(Game::ItemInventory::count(app.gameState, item) == stock - 1);
        const auto continuation = app.battleSession_.state_.battleContinuation;
        desktop.advanceMs(400);
        app.update(desktop.millis());
        assert(app.battleSession_.timeline_.state_.battleThrownItem == item && app.battleSession_.timeline_.state_.battleItemThrowFrame > 0);
        assert(app.battleSession_.state_.battleContinuation == continuation && app.battleSession_.state_.battleWild.hpCur == hp);
        app.handleTap(80, 400, desktop.millis());
        app.performBattleBagItem(item, desktop.millis());
        assert(Game::ItemInventory::count(app.gameState, item) == stock - 1);
        desktop.advanceMs(410);
        app.update(desktop.millis());
        assert(app.battleSession_.timeline_.state_.battleThrownItem == Game::ItemId::COUNT);
        assert(app.battleSession_.state_.battleContinuation == continuation);
        GameRandom::setRangeProvider(nullptr);

        // Sell the last item in the persistent detail panel, then ignore taps.
        app.sceneFlow.enter(Scene::SHOP);
        app.battleSession_.state_.battleContinuation = AmoledApp::BattleContinuation::NONE;
        const auto potions = Game::ItemInventory::count(app.gameState, Game::ItemId::POTION);
        if (potions) Game::ItemInventory::remove(app.gameState, Game::ItemId::POTION, potions);
        Game::ItemInventory::add(app.gameState, Game::ItemId::POTION);
        app.inventoryController_.selectCategory(Game::ShopService::Category::SELL);
        for (uint8_t i = 0; i < app.inventoryController_.itemCount(app.gameState, Scene::SHOP); ++i) {
            if (app.inventoryController_.itemAt(app.gameState, Scene::SHOP, i) == Game::ItemId::POTION) {
                app.inventoryController_.selectItem(app.gameState, Scene::SHOP, i);
                break;
            }
        }
        app.handleTap(100, 390, desktop.millis());
        assert(Game::ItemInventory::count(app.gameState, app.inventoryController_.state().pendingItem) == 0);
        assert(app.inventoryController_.state().itemConfirmOpen && app.toast == nullptr);
        const auto coins = app.gameState.coins;
        app.handleTap(100, 390, desktop.millis());
        assert(app.gameState.coins == coins && app.toast == nullptr);
        std::puts("battle food: arc blocks input/turn and consumes once; confusion names self-hit on both sides; sold-out action ignored");
    }

    static void checkBattleDamageLogs(DesktopPlatform& desktop,
                                      const Game::GameState& state) {
        for (bool wildSide : {false, true}) {
            // Normal, critical, super, critical+super, resisted,
            // critical+resisted, miss, immune, and zero-damage status.
            for (int scenario = 0; scenario < 9; ++scenario) {
                const bool critical = scenario == 1 || scenario == 3 || scenario == 5;
                GameRandom::setRangeProvider(scenario == 6
                    ? +[](uint32_t, uint32_t maximum) { return maximum - 1; }
                    : critical ? +[](uint32_t minimum, uint32_t) { return minimum; }
                    : +[](uint32_t minimum, uint32_t maximum) {
                        return std::min(minimum + 1, maximum - 1);
                    });
                AmoledApp app;
                app.begin(desktop.millis());
                app.gameState = state;
                app.gameState.teamCount = 1;
                app.gameState.team[0] = Game::MonsterFactory::create(7, 20);
                app.battleSession_.state_.battleWild = Game::MonsterFactory::create(7, 20);
                app.battleSession_.state_.battlePlayerSlot = 0;
                app.sceneFlow.enter(Scene::BATTLE);
                app.battleSession_.state_.battlePlayerState = {};
                app.battleSession_.state_.battleWildState = {};
                auto& attacker = wildSide ? app.battleSession_.state_.battleWild : app.gameState.team[0];
                auto& defender = wildSide ? app.gameState.team[0] : app.battleSession_.state_.battleWild;
                auto& attackState = wildSide ? app.battleSession_.state_.battleWildState : app.battleSession_.state_.battlePlayerState;
                const bool super = scenario == 2 || scenario == 3;
                const bool resisted = scenario == 4 || scenario == 5;
                const Game::MoveId move = super || resisted ? 55 : scenario == 8 ? 45 : 10;
                attacker.move2Id = move;
                if (super) defender = Game::MonsterFactory::create(4, 20);
                if (scenario == 7) defender = Game::MonsterFactory::create(92, 20);
                if (scenario == 6) {
                    attackState.statStages[static_cast<uint8_t>(BattleStat::ACCURACY)] = -6;
                }
                app.battleSession_.clearLog();
                BattleTurnController::Action action{
                    wildSide ? BattleTurnController::Side::WILD
                             : BattleTurnController::Side::PLAYER, 0, move};
                if (wildSide) app.performBattleWildAction(action, desktop.millis());
                else app.performBattlePlayerAction(action, desktop.millis());
                assert(app.battleSession_.timeline_.state_.battleLogVisibleCount == 1);
                assert(std::strstr(app.battleSession_.timeline_.state_.battleLogLines[0], findMove(move)->name));
                desktop.advanceMs(app.battleSession_.timeline_.state_.battleAnimationDurationMs + 1);
                // Results must remain queued while the attack animation holds the log.
                assert(!app.battleSession_.serviceLog(desktop.millis()));
                app.battleSession_.timeline_.state_.battleAttackLogHeld = false;
                app.battleSession_.timeline_.state_.battleAnimationActive = false;
                std::vector<std::string> lines;
                while (app.battleSession_.timeline_.state_.battleLogQueueCount > 0) {
                    desktop.advanceMs(5000);
                    assert(app.battleSession_.serviceLog(desktop.millis()));
                    lines.emplace_back(app.battleSession_.timeline_.state_.battleLogLines[app.battleSession_.timeline_.state_.battleLogVisibleCount - 1]);
                }
                if (scenario < 6) {
                    char expected[64];
                    std::snprintf(expected, sizeof(expected),
                        wildSide ? Ui::Explore::WILD_DAMAGE_FMT : Ui::Explore::DAMAGE_FMT,
                        app.battleSession_.timeline_.state_.battleAnimationDamage);
                    assert(lines[0] == expected);
                    size_t next = 1;
                    if (critical) assert(lines.at(next++) == Ui::Explore::CRITICAL_HIT);
                    if (super) assert(lines.at(next++) == Ui::Explore::SUPER_EFFECTIVE);
                    if (resisted) assert(lines.at(next++) == Ui::Explore::NOT_VERY_EFFECTIVE);
                    assert(lines.size() == next);
                } else {
                    assert(lines.size() == 1);
                    if (scenario == 6) assert(lines[0] == Ui::Explore::MOVE_MISSED);
                    if (scenario == 7) assert(lines[0] == Ui::Explore::NO_EFFECT);
                    for (const auto* message : {Ui::Explore::CRITICAL_HIT,
                            Ui::Explore::SUPER_EFFECTIVE, Ui::Explore::NOT_VERY_EFFECTIVE}) {
                        assert(lines[0] != message);
                    }
                }
            }
        }
        GameRandom::setRangeProvider(nullptr);
        std::puts("battle damage logs: both sides report critical/effectiveness in order after the animation; misses, immunity and status do not misreport");
    }

    static void checkBattleAssetActions(DesktopPlatform& desktop,
                                        const Game::GameState& state) {
        checkBattleDamageLogs(desktop, state);
        checkNoEffectBattleLogs(desktop, state);
        checkBattleItemsAndBlocks(desktop, state);
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
                    app.battleSession_.state_.battleWild = Game::MonsterFactory::create(7, 30);
                    app.battleSession_.state_.battlePlayerSlot = 0;
                    app.sceneFlow.enter(Scene::BATTLE);
                    auto& attacker = wildSide ? app.battleSession_.state_.battleWild : app.gameState.team[0];
                    auto& defender = wildSide ? app.gameState.team[0] : app.battleSession_.state_.battleWild;
                    auto& attackState = wildSide ? app.battleSession_.state_.battleWildState : app.battleSession_.state_.battlePlayerState;
                    auto& defendState = wildSide ? app.battleSession_.state_.battlePlayerState : app.battleSession_.state_.battleWildState;
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
                    assert(app.battleSession_.timeline_.state_.battleAnimationActive);
                    assert(app.battleSession_.timeline_.state_.battleAnimationAttackerWild == wildSide);
                    assert(app.battleSession_.timeline_.state_.battleAnimationFx == fx);
                    assert(app.battleSession_.timeline_.state_.battleAnimationStatusEffect == (status && mode == 0));
                    assert(app.battleSession_.timeline_.state_.battleAnimationHit == (!status && mode == 0));
                    if (status || mode != 0) {
                        assert(defender.hpCur == before);
                        assert(!app.battleSession_.timeline_.state_.battleHpAnimationActive);
                        assert(app.battleSession_.timeline_.state_.battleImpactSfx == static_cast<uint8_t>(
                            mode == 0 ? SfxCue::UI_CONFIRM : SfxCue::UI_CANCEL));
                    } else {
                        assert(defender.hpCur < before && app.battleSession_.timeline_.state_.battleHpAnimationActive);
                    }
                    app.battleSession_.state_.battleContinuation = AmoledApp::BattleContinuation::NONE;
                    app.battleSession_.timeline_.state_.battleAudioPending = false;
                    desktop.advanceMs(app.battleSession_.timeline_.state_.battleAnimationDurationMs + 1);
                    app.update(desktop.millis());
                    assert(!app.battleSession_.timeline_.state_.battleAnimationActive && !app.battleSession_.timeline_.state_.battleAnimationStatusEffect);
                }
            }
        }
        GameRandom::setRangeProvider(nullptr);
        std::puts("battle asset actions: both sides, success, caps, misses and immunity verified");
    }

    static void checkSharedBattleExecution(DesktopPlatform& desktop,
                                           const Game::GameState& state) {
        // Exercise both actual App adapters, not a second service-only caller.
        for (bool wildSide : {false, true}) {
            for (int scenario = 0; scenario < 7; ++scenario) {
                GameRandom::setRangeProvider(+[](uint32_t minimum, uint32_t) { return minimum; });
                AmoledApp app;
                app.begin(desktop.millis());
                app.gameState = state;
                app.gameState.teamCount = 1;
                app.gameState.team[0] = Game::MonsterFactory::create(4, 35);
                app.battleSession_.state_.battleWild = Game::MonsterFactory::create(7, 35);
                app.battleSession_.state_.battlePlayerSlot = 0;
                app.sceneFlow.enter(Scene::BATTLE);
                app.battleSession_.state_.battlePlayerState = {};
                app.battleSession_.state_.battleWildState = {};
                auto& attacker = wildSide ? app.battleSession_.state_.battleWild : app.gameState.team[0];
                auto& defender = wildSide ? app.gameState.team[0] : app.battleSession_.state_.battleWild;
                auto& attackState = wildSide ? app.battleSession_.state_.battleWildState : app.battleSession_.state_.battlePlayerState;
                auto& defendState = wildSide ? app.battleSession_.state_.battlePlayerState : app.battleSession_.state_.battleWildState;
                Game::MoveId move = 55;
                if (scenario == 0) {
                    defender = Game::MonsterFactory::create(134, 35);
                    defender.hpCur = 1;
                } else if (scenario == 1) {
                    defender = Game::MonsterFactory::create(25, 35);
                    defender.hpCur = defender.hpMax = 500;
                    move = 424;
                } else if (scenario == 2) {
                    move = 256;
                    attacker.hpCur = 1;
                    attackState.stockpileCount = 3;
                } else if (scenario == 3) {
                    move = 37;
                    attackState.lockedMoveId = move;
                    attackState.lockedSpecialSlot = 0;
                    attackState.lockedTurns = 1;
                } else if (scenario == 4) {
                    move = 130;
                } else if (scenario == 5) {
                    attacker.majorStatus = Game::MajorStatus::SLEEP;
                    attacker.majorStatusTurns = 1;
                } else {
                    move = 10;
                    defender = Game::MonsterFactory::create(92, 35);
                }
                attacker.move2Id = move;
                const auto proficiency = attacker.moveProficiency[1];
                const auto attackerHp = attacker.hpCur;
                app.battleSession_.clearLog();
                const auto side = wildSide ? BattleTurnController::Side::WILD
                                           : BattleTurnController::Side::PLAYER;
                app.battleSession_.state_.battleTurnPlan.count = 2;
                app.battleSession_.state_.battleTurnPlan.actions[0] = {side, 0, move};
                app.battleSession_.state_.battleTurnPlan.actions[1] = {
                    wildSide ? BattleTurnController::Side::PLAYER
                             : BattleTurnController::Side::WILD,
                    BattleSystem::SPECIAL_SLOT_NONE, 33};
                app.battleSession_.state_.battleTurnActionIndex = 0;
                const BattleTurnController::Action action{side, 0, move};
                if (wildSide) app.performBattleWildAction(action, desktop.millis());
                else app.performBattlePlayerAction(action, desktop.millis());
                if (scenario == 0) {
                    assert(defender.hpCur == 1 + defender.hpMax / 4);
                    assert(app.battleSession_.timeline_.state_.battleHpAnimationActive && !app.battleSession_.timeline_.state_.battleAnimationHit);
                } else if (scenario == 1) {
                    assert(attacker.majorStatus == Game::MajorStatus::PARALYSIS);
                    assert(defender.majorStatus == Game::MajorStatus::BURN && defendState.flinched);
                } else if (scenario == 2) {
                    assert(attacker.hpCur == attacker.hpMax && attackState.stockpileCount == 0);
                    assert(app.battleSession_.timeline_.state_.battleHpAnimationActive);
                } else if (scenario == 3) {
                    assert(attackState.lockedMoveId == 0 && attackState.confusionTurns > 0);
                } else if (scenario == 4) {
                    assert(attackState.chargingMoveId == 130);
                    assert(attackState.statStages[static_cast<uint8_t>(BattleStat::DEFENSE)] == 1);
                    assert(!app.battleSession_.timeline_.state_.battleAnimationActive);
                } else if (scenario == 5) {
                    assert(attacker.majorStatus == Game::MajorStatus::NONE);
                } else {
                    assert(defender.hpCur == defender.hpMax && !app.battleSession_.timeline_.state_.battleAnimationHit);
                }
                assert(attacker.moveProficiency[1] == proficiency +
                    ((!wildSide && scenario != 0 && scenario != 4 && scenario != 6) ? 1 : 0));

                // Polling the animation and draining its queue cannot reapply rewards.
                app.battleSession_.state_.battleContinuation = AmoledApp::BattleContinuation::NONE;
                app.battleSession_.timeline_.state_.battleAudioPending = false;
                app.battleSession_.timeline_.state_.battleAttackLogHeld = false;
                app.battleSession_.timeline_.state_.battleAnimationActive = false;
                std::vector<std::string> lines;
                for (uint8_t i = 0; i < app.battleSession_.timeline_.state_.battleLogVisibleCount; ++i)
                    lines.emplace_back(app.battleSession_.timeline_.state_.battleLogLines[i]);
                while (app.battleSession_.timeline_.state_.battleLogQueueCount > 0) {
                    desktop.advanceMs(5000);
                    assert(app.battleSession_.serviceLog(desktop.millis()));
                    lines.emplace_back(app.battleSession_.timeline_.state_.battleLogLines[app.battleSession_.timeline_.state_.battleLogVisibleCount - 1]);
                }
                const auto count = [&](const std::string& expected) {
                    return std::count(lines.begin(), lines.end(), expected);
                };
                char expected[96];
                const Species& a = *findSpecies(attacker.speciesId);
                const Species& d = *findSpecies(defender.speciesId);
                if (scenario == 0) {
                    assert(count(Ui::Explore::NO_EFFECT) == 1);
                    std::snprintf(expected, sizeof(expected), Ui::Explore::ABSORB_HEAL_FMT,
                                  d.name, defender.hpMax / 4);
                    assert(count(expected) == 1);
                } else if (scenario == 1) {
                    std::snprintf(expected, sizeof(expected), Ui::Explore::STATUS_APPLIED_FMT,
                                  a.name, Ui::Status::STATUS_PARALYSIS);
                    assert(count(expected) == 1);
                    std::snprintf(expected, sizeof(expected), Ui::Explore::ABILITY_ACTIVATED_FMT,
                                  d.name, abilityName(AbilityId::STATIC));
                    assert(count(expected) == 1);
                } else if (scenario == 2) {
                    std::snprintf(expected, sizeof(expected), Ui::Explore::HEALED_FMT,
                                  a.name, attacker.hpMax - attackerHp);
                    assert(count(expected) == 1);
                } else if (scenario == 3) {
                    std::snprintf(expected, sizeof(expected), Ui::Explore::RAMPAGE_CONFUSED_FMT, a.name);
                    assert(count(expected) == 1);
                } else if (scenario == 4) {
                    std::snprintf(expected, sizeof(expected), Ui::Explore::STAT_ROSE_FMT,
                                  a.name, Ui::Explore::STAT_NAMES[static_cast<uint8_t>(BattleStat::DEFENSE)]);
                    assert(count(expected) == 1);
                } else if (scenario == 5) {
                    std::snprintf(expected, sizeof(expected), Ui::Explore::WOKE_UP_FMT, a.name);
                    assert(count(expected) == 1);
                } else assert(count(Ui::Explore::NO_EFFECT) == 1);
                const auto finalAttackerHp = attacker.hpCur, finalDefenderHp = defender.hpCur;
                const auto finalProficiency = attacker.moveProficiency[1];
                for (int repeat = 0; repeat < 3; ++repeat) {
                    desktop.advanceMs(5000);
                    app.update(desktop.millis());
                    assert(attacker.hpCur == finalAttackerHp && defender.hpCur == finalDefenderHp);
                    assert(attacker.moveProficiency[1] == finalProficiency);
                }
            }
        }
        GameRandom::setRangeProvider(nullptr);
        std::puts("shared battle App adapters: absorb, contact, stockpile, rampage, charge, wake and immunity apply/log once on both sides");

        for (int effect = 0; effect < 4; ++effect) {
            AmoledApp app;
            app.begin(desktop.millis());
            app.gameState = state;
            app.gameState.teamCount = 1;
            app.gameState.team[0] = Game::MonsterFactory::create(4, 35);
            app.battleSession_.state_.battleWild = Game::MonsterFactory::create(7, 35);
            app.battleSession_.state_.battlePlayerSlot = 0;
            app.sceneFlow.enter(Scene::BATTLE);
            auto& player = app.gameState.team[0];
            player.hpCur = player.hpMax = 500;
            if (effect == 3) player.hpCur = 1;
            player.move2Id = 76;
            app.battleSession_.state_.battlePlayerState = {};
            app.battleSession_.state_.battleWildState = {};
            BattleSystem::beginChargingMove(app.battleSession_.state_.battlePlayerState, 76, 0);
            if (effect == 0 || effect == 3) player.majorStatus = Game::MajorStatus::POISON;
            if (effect == 1) app.battleSession_.state_.battlePlayerState.bindTurns = 1;
            if (effect == 2) app.battleSession_.state_.battlePlayerState.yawnTurns = 1;
            app.battleSession_.state_.battleTurnPlan.count = 1;
            app.battleSession_.state_.battleTurnPlan.actions[0] = {BattleTurnController::Side::PLAYER, 0, 76};
            app.battleSession_.state_.battleTurnActionIndex = 0;
            app.battleSession_.clearLog();
            GameRandom::setRangeProvider(sharedBattleMinimum);
            sharedBattleDraws = 0;
            app.advanceBattleTurn(desktop.millis());
            assert(app.battleSession_.state_.battleContinuation == AmoledApp::BattleContinuation::FINISH_END_TURN);
            assert(!app.battleSession_.timeline_.state_.battleAnimationActive && app.battleSession_.logBusy());
            assert(app.battleSession_.state_.battlePhase == BattleViewModel::Phase::ACTION);
            const auto drawsAfterEnd = sharedBattleDraws;
            const auto hpAfterEnd = player.hpCur;
            for (int frame = 0; frame < 5; ++frame) {
                desktop.advanceMs(10);
                app.update(desktop.millis());
                assert(!app.battleSession_.timeline_.state_.battleAnimationActive && app.battleSession_.state_.battleTurnPlan.count == 0);
                assert(sharedBattleDraws == drawsAfterEnd && player.hpCur == hpAfterEnd);
                assert(app.battleSession_.state_.battlePhase == BattleViewModel::Phase::ACTION);
            }
            while (app.battleSession_.timeline_.state_.battleLogQueueCount > 0) {
                desktop.advanceMs(5000);
                assert(app.battleSession_.serviceLog(desktop.millis()));
            }
            // The last visible line also receives its full reading time.
            assert(app.battleSession_.logBusy());
            desktop.advanceMs(10);
            app.update(desktop.millis());
            assert(sharedBattleDraws == drawsAfterEnd && app.battleSession_.state_.battleTurnPlan.count == 0);
            desktop.advanceMs(5000);
            app.update(desktop.millis());
            if (effect == 3) {
                assert(app.battleSession_.state_.battlePhase == BattleViewModel::Phase::DEFEAT);
                assert(app.battleSession_.state_.battleTurnPlan.count == 0 && sharedBattleDraws == drawsAfterEnd);
            } else assert(app.battleSession_.state_.battleTurnPlan.count == 2);
            assert(app.battleSession_.state_.battleContinuation != AmoledApp::BattleContinuation::FINISH_END_TURN);
            GameRandom::setRangeProvider(nullptr);
        }
        std::puts("shared battle end turn: poison, bind and yawn logs finish before charge release or faint resolution");
    }

#if STICKMON_ENABLE_DEBUG_FEATURES
    static void checkBattleDiagnosticsSaveIsolation(DesktopPlatform& desktop,
                                                    const Game::GameState& state) {
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
        app.gameState = state;
        app.gameState.oobeDone = true;
        app.gameState.coins = 43210;
        app.gameState.team[0] = Game::MonsterFactory::create(1, 23);
        app.sceneFlow.enter(Scene::HOME);
        assert(app.saveState());
        blobs.failSnapshots = true;
        assert(!app.debugBattleScenario(1, desktop.millis()));
        assert(!app.debugBattleScenarioActive && app.gameState.team[0].speciesId == 1);
        blobs.failSnapshots = false;
        assert(app.debugBattleScenario(1, desktop.millis()));
        assert(app.debugBattleScenarioActive);
        const size_t writesAfterBackup = blobs.writes;
        const auto stateA = blobs.contents("stickmon", "state_a");
        const auto stateB = blobs.contents("stickmon", "state_b");
        const auto legacy = blobs.contents("stickmon", "state");
        auto assertSavedPlayer = [&]() {
            assert(blobs.writes == writesAfterBackup);
            assert(blobs.contents("stickmon", "state_a") == stateA);
            assert(blobs.contents("stickmon", "state_b") == stateB);
            assert(blobs.contents("stickmon", "state") == legacy);
            Game::GameState loaded;
            MainSceneViewState view;
            assert(app.saveManager.load(loaded, view));
            assert(loaded.coins == 43210 && loaded.team[0].speciesId == 1);
            assert(loaded.team[0].level == 23 && loaded.teamCount == state.teamCount);
        };
        assertSavedPlayer();
        for (uint8_t scenario = 2; scenario <= 12; ++scenario) {
            assert(app.debugBattleScenario(scenario, desktop.millis()));
            if (scenario == 9) {
                assert(app.gameState.team[0].speciesId == 129);
                assert(app.battleSession_.state_.battleTurnPlan.actions[0].moveId == 150);
                assert(app.battleSession_.state_.battleTurnPlan.actions[0].specialSlot == BattleSystem::SPECIAL_SLOT_NONE);
                assert(app.battleSession_.timeline_.state_.battleAnimationDamage == 0 && !app.battleSession_.timeline_.state_.battleAnimationHit);
                assert(app.battleSession_.state_.battleWild.hpCur == app.battleSession_.state_.battleWild.hpMax);
                // Inspect the actual executed move line: a planned move alone
                // would miss an invalid-special-slot fallback to Scratch.
                assert(app.battleSession_.timeline_.state_.battleLogVisibleCount == 1);
                assert(std::strstr(app.battleSession_.timeline_.state_.battleLogLines[0], findMove(150)->name));
                assert(!std::strstr(app.battleSession_.timeline_.state_.battleLogLines[0], findMove(10)->name));
                app.battleSession_.timeline_.state_.battleAttackLogHeld = false;
                app.battleSession_.timeline_.state_.battleAnimationActive = false;
                app.battleSession_.state_.battleContinuation = AmoledApp::BattleContinuation::NONE;
                unsigned noEffectCount = 0;
                while (app.battleSession_.timeline_.state_.battleLogQueueCount > 0) {
                    desktop.advanceMs(5000);
                    assert(app.battleSession_.serviceLog(desktop.millis()));
                    if (std::strcmp(app.battleSession_.timeline_.state_.battleLogLines[app.battleSession_.timeline_.state_.battleLogVisibleCount - 1],
                                    Ui::Explore::NO_EFFECT) == 0) ++noEffectCount;
                }
                assert(noEffectCount == 1);
                assert(app.battleSession_.state_.battleWild.hpCur == app.battleSession_.state_.battleWild.hpMax);
            }
            desktop.advanceMs(50);
            app.update(desktop.millis());
            assert(!app.saveState());
            assertSavedPlayer();
        }
        // Closing a normal encounter also reaches saveState; it stays isolated.
        app.battleSession_.clearLog();
        app.battleSession_.state_.battleContinuation = AmoledApp::BattleContinuation::NONE;
        app.closeBattle(desktop.millis());
        assertSavedPlayer();
        assert(app.debugBattleScenario(0, desktop.millis()));
        assert(power.restarts == 1);
        assertSavedPlayer();
        Platform::bind(original);
        std::puts("battle diagnostics: actual Splash is move 150, zero damage, one no-effect log; failed backup and all 12 fixtures preserve original snapshots");
    }
#endif

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

    static void checkMedicineFlow(DesktopPlatform& desktop, Canvas565& canvas,
                                  const Game::GameState& state) {
        const auto tapTarget = [&](AmoledApp& app, uint8_t slot) {
            for (int y = 0; y < 448; ++y) {
                if (teamMemberAt(100, y, app.gameState.teamCount) == slot) {
                    app.handleTap(100, y + 20, desktop.millis());
                    return;
                }
            }
            assert(false && "team member must be reachable");
        };
        const auto tapBack = [&](AmoledApp& app) {
            for (int y = 0; y < 80; ++y) {
                for (int x = 0; x < 368; ++x) {
                    if (teamBackAt(x, y)) {
                        app.handleTap(x, y, desktop.millis());
                        return;
                    }
                }
            }
            assert(false && "team back must be reachable");
        };
        const auto confirmMedicine = [&](AmoledApp& app, Game::ItemId item) {
            for (uint8_t i = 0; i < app.inventoryController_.itemCount(app.gameState, Scene::BAG); ++i) {
                if (app.inventoryController_.itemAt(app.gameState, Scene::BAG, i) == item) {
                    app.inventoryController_.selectItem(app.gameState, Scene::BAG, i);
                    break;
                }
            }
            for (int y = 0; y < 448; ++y) {
                if (itemConfirmChoiceAt(100, y) == 0) {
                    app.handleTap(100, y, desktop.millis());
                    return;
                }
            }
            assert(false && "medicine confirm must be reachable");
        };

        for (bool battle : {false, true}) {
            for (uint8_t target : {0, 1}) {
                for (Game::ItemId item : {Game::ItemId::POTION, Game::ItemId::REVIVE,
                                          Game::ItemId::FULL_HEAL}) {
                    AmoledApp app;
                    app.begin(desktop.millis());
                    app.gameState = state;
                    app.gameState.teamCount = 2;
                    app.gameState.team[0] = Game::MonsterFactory::create(7, 20);
                    app.gameState.team[1] = Game::MonsterFactory::create(4, 20);
                    app.gameState.activeSlot = 1;
                    app.battleSession_.state_.battlePlayerSlot = 1;
                    app.battleSession_.state_.battleWild = Game::MonsterFactory::create(1, 20);
                    app.battleSession_.state_.battlePhase = BattleViewModel::Phase::ACTION;
                    app.battleSession_.clearLog();
                    auto& monster = app.gameState.team[target];
                    monster.hpCur = item == Game::ItemId::REVIVE ? 0 : 5;
                    monster.fainted = item == Game::ItemId::REVIVE;
                    if (item == Game::ItemId::FULL_HEAL) {
                        monster.majorStatus = Game::MajorStatus::POISON;
                    }
                    const auto oldHp = monster.hpCur;
                    const auto otherHp = app.gameState.team[1 - target].hpCur;
                    Game::ItemInventory::add(app.gameState, item, 2);
                    const auto stock = Game::ItemInventory::count(app.gameState, item);
                    app.sceneFlow.enter(battle ? Scene::BATTLE : Scene::HOME);
                    if (battle) app.performBattleBag(desktop.millis());
                    else app.openItemScene(Scene::BAG);
                    confirmMedicine(app, item);
                    assert(app.sceneFlow.current() == Scene::TEAM && app.inventoryController_.state().selectingItemTarget);
                    assert(Game::ItemInventory::count(app.gameState, item) == stock);

                    // Cancelling target selection costs neither an item nor a turn.
                    tapBack(app);
                    assert(app.sceneFlow.current() == Scene::BAG && !app.inventoryController_.state().selectingItemTarget);
                    assert(Game::ItemInventory::count(app.gameState, item) == stock);
                    assert(app.battleSession_.state_.battleContinuation == AmoledApp::BattleContinuation::NONE);
                    confirmMedicine(app, item);
                    tapTarget(app, target);
                    const auto newHp = monster.hpCur;
                    assert(app.inventoryController_.state().teamItemFeedbackActive && app.inventoryController_.state().teamItemTargetSlot == target);
                    assert(app.sceneFlow.current() == Scene::TEAM);
                    assert(Game::ItemInventory::count(app.gameState, item) == stock - 1);
                    assert(app.gameState.team[1 - target].hpCur == otherHp);
                    assert(app.battleSession_.state_.battlePlayerSlot == 1 && app.gameState.activeSlot == 1);
                    assert(app.gameState.team[0].speciesId == 7 && app.gameState.team[1].speciesId == 4);
                    assert(!monster.fainted && monster.majorStatus == Game::MajorStatus::NONE);
                    if (item == Game::ItemId::POTION) assert(newHp == oldHp + 20);
                    if (item == Game::ItemId::REVIVE) assert(newHp == monster.hpMax / 2);
                    if (item == Game::ItemId::FULL_HEAL) assert(newHp == oldHp);
                    assert(app.teamItemHpForRender(desktop.millis()) == oldHp);
                    checkHatchFrame(app, canvas);
                    const std::vector<uint16_t> first(canvas.rawPixels(),
                                                      canvas.rawPixels() + canvas.pixelCount());
                    // Repeated target and back taps during feedback must be ignored.
                    tapTarget(app, target);
                    tapBack(app);
                    assert(app.sceneFlow.current() == Scene::TEAM);
                    assert(Game::ItemInventory::count(app.gameState, item) == stock - 1);
                    desktop.advanceMs(210);
                    app.update(desktop.millis());
                    assert(app.teamItemHpForRender(desktop.millis()) == oldHp + (newHp - oldHp) / 2);
                    checkHatchFrame(app, canvas);
                    if (newHp != oldHp) {
                        assert(!std::equal(first.begin(), first.end(), canvas.rawPixels()));
                    }
                    desktop.advanceMs(210);
                    app.update(desktop.millis());
                    assert(app.teamItemHpForRender(desktop.millis()) == newHp);
                    assert(app.sceneFlow.current() == Scene::TEAM && app.inventoryController_.state().teamItemFeedbackActive);
                    checkHatchFrame(app, canvas);
                    desktop.advanceMs(199);
                    app.update(desktop.millis());
                    assert(app.sceneFlow.current() == Scene::TEAM);
                    desktop.advanceMs(1);
                    app.update(desktop.millis());
                    assert(!app.inventoryController_.state().teamItemFeedbackActive && !app.inventoryController_.state().selectingItemTarget);
                    assert(app.sceneFlow.current() == (battle ? Scene::BATTLE : Scene::BAG));
                    assert(!app.inventoryController_.state().battleBagMode && !app.battleSession_.timeline_.state_.battleHpAnimationActive);
                    assert(app.battleSession_.state_.battleContinuation == (battle ? AmoledApp::BattleContinuation::WILD_TURN
                                                             : AmoledApp::BattleContinuation::NONE));
                    assert(Game::ItemInventory::count(app.gameState, item) == stock - 1);
                    checkHatchFrame(app, canvas);
                    if (!battle) {
                        app.closeItemScene();
                        assert(app.sceneFlow.current() == Scene::HOME);
                    }
                }
            }
        }
        // A full-health or fainted target rejects a potion without consuming it.
        AmoledApp app;
        app.begin(desktop.millis());
        app.gameState = state;
        app.battleSession_.state_.battlePlayerSlot = 0;
        app.gameState.team[0].hpCur = app.gameState.team[0].hpMax;
        app.gameState.team[1].hpCur = 0;
        app.gameState.team[1].fainted = true;
        Game::ItemInventory::add(app.gameState, Game::ItemId::POTION);
        const auto stock = Game::ItemInventory::count(app.gameState, Game::ItemId::POTION);
        app.sceneFlow.enter(Scene::BATTLE);
        app.performBattleBag(desktop.millis());
        confirmMedicine(app, Game::ItemId::POTION);
        for (uint8_t slot : {0, 1}) {
            tapTarget(app, slot);
            assert(app.sceneFlow.current() == Scene::TEAM && !app.inventoryController_.state().teamItemFeedbackActive);
            assert(Game::ItemInventory::count(app.gameState, Game::ItemId::POTION) == stock);
        }
        tapBack(app);
        app.closeItemScene();
        assert(app.sceneFlow.current() == Scene::BATTLE && !app.inventoryController_.state().battleBagMode);
        assert(app.battleSession_.state_.battleContinuation == AmoledApp::BattleContinuation::NONE);
        std::puts("medicine: target selection, active/reserve healing, revive/status feedback, animated partial frames, return timing and cancellation verified");
    }

    static void shakeHatch(AmoledApp& app, DesktopPlatform& desktop, HatchImuSpy& imu) {
        // Settle the device, then apply two samples in opposing directions.
        imu.x = 0;
        for (int i = 0; i < 60; ++i) {
            desktop.advanceMs(20); app.update(desktop.millis());
        }
        const uint16_t before = app.hatchController_.state().wipeCount;
        for (int i = 0; i < 4; ++i) {
            imu.x = i % 2 ? -1.2f : 1.2f;
            desktop.advanceMs(20); app.update(desktop.millis());
        }
        assert(app.hatchController_.state().wipeCount == before + 1);
        assert(app.hatchController_.state().shaking && app.hatchController_.state().lastShakeOffset != 0);
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
        // Cancelled, dragged and orphaned releases cannot count as egg taps.
        const auto sendEgg = [&](TouchEventType type, int x, int y) {
            TouchEvent event{};
            event.type = type; event.x = x; event.y = y;
            event.timestampMs = desktop.millis();
            app.handleTouch(event);
        };
        sendEgg(TouchEventType::UP, 184, 200);
        sendEgg(TouchEventType::DOWN, 184, 200);
        app.cancelTouch();
        sendEgg(TouchEventType::UP, 184, 200);
        sendEgg(TouchEventType::DOWN, 184, 200);
        sendEgg(TouchEventType::MOVE, 260, 200);
        sendEgg(TouchEventType::UP, 184, 200);
        sendEgg(TouchEventType::DOWN, 120, 396);
        sendEgg(TouchEventType::UP, 184, 200);
        assert(app.hatchController_.state().pokeCount == 0);
        assert(app.sceneFlow.current() == Scene::HATCH);
        assert(!app.hatchController_.state().heartVisible);
        desktop.advanceMs(40000);
        app.update(desktop.millis());
        assert(app.sceneFlow.current() == Scene::HATCH && app.hatchController_.state().elapsedSeconds == 40);
        assert(app.renderRowBegin() >= HOME_STATUS_TOP || app.renderRowBegin() == 0);
        assert(app.renderRowEnd() == 448);
        checkHatchFrame(app, canvas);
        for (int i = 0; i < 10; ++i) tap(app, desktop, 184, 200);
        assert(app.hatchController_.state().heartVisible && app.hatchController_.state().shaking);
        assert(app.hatchController_.state().pokeCount == 10 && app.hatchController_.state().wipeCount == 0);
        assert(app.renderRowBegin() == HOME_ROOM_TOP && app.renderRowEnd() == AmoledV1::HOME_STATUS_TOP);
        checkHatchFrame(app, canvas);
        tap(app, desktop, 272, 396);
        assert(app.hatchController_.state().pokeCount == 10 && app.hatchController_.state().wipeCount == 0);
        for (int i = 0; i < 3; ++i) shakeHatch(app, desktop, imu);
        assert(!app.hatchController_.state().heartVisible);
        checkHatchFrame(app, canvas);
        const uint16_t interactionsBeforeFeedback = app.hatchController_.state().pokeCount + app.hatchController_.state().wipeCount;
        desktop.advanceMs(900); app.update(desktop.millis());
        assert(app.hatchController_.state().shaking && app.hatchController_.state().lastShakeOffset != 0);
        assert(app.hatchController_.state().pokeCount + app.hatchController_.state().wipeCount == interactionsBeforeFeedback);
        checkHatchFrame(app, canvas);
        desktop.advanceMs(60); app.update(desktop.millis());
        assert(!app.hatchController_.state().shaking && app.hatchController_.state().lastShakeOffset == 0);
        checkHatchFrame(app, canvas);
        desktop.advanceMs(20); app.update(desktop.millis());
        assert(!app.needsRender());
        assert(app.saveState());
        const float pausedElapsed = app.hatchController_.state().elapsedSeconds;
        assert(app.hatchController_.state().pokeCount == 10 && app.hatchController_.state().wipeCount == 3);
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
        assert(app.hatchController_.state().elapsedSeconds == pausedElapsed && !app.consumeLockRequest() && imu.reads == pausedReads);
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
        assert(app.hatchController_.state().elapsedSeconds == pausedElapsed && imu.enabled);

        AmoledApp resumed;
        resumed.begin(desktop.millis());
        assert(resumed.startupStateLoaded() && resumed.sceneFlow.current() == Scene::HATCH);
        assert(resumed.hatchController_.state().elapsedSeconds == progress.elapsedSeconds && resumed.hatchController_.state().pokeCount == 10);
        assert(resumed.hatchController_.state().wipeCount == 3 && resumed.gameState.settings.brightness == 96);
        desktop.advanceMs((179 - progress.elapsedSeconds) * 1000);
        resumed.update(desktop.millis());
        assert(!resumed.gameState.oobeDone && resumed.hatchController_.state().lastRemainingSecond == 1);
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
            const uint32_t duration = std::ceil((180 - accelerated.hatchController_.state().elapsedSeconds) * 1000 / (1U << speed));
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
        assert(retry.hatchController_.state().lastRemainingSecond == 0 && Game::HatchService::progressPercent(retry.hatchController_.state().elapsedSeconds, retry.gameState.hatchSeconds) == 100 && !imu.enabled);
        assert(retry.toast && std::strcmp(retry.toast, Ui::Hatch::SAVE_FAILED) == 0);
        assert(retry.saveManager.loadHatchProgress(progress) && progress.elapsedSeconds == 180);
        const auto candidateIv = retry.hatchResult->team[0].ivPacked;
        tap(retry, desktop, 272, 396);
        assert(retry.hatchController_.state().wipeCount == 0);
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
        assert(noImu.hatchController_.state().pokeCount == 1 && noImu.hatchController_.state().heartVisible);
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
        tap(app, desktop, 200, 414);  // V1 has no action below reset.
        assert(!app.settingsController.resetConfirm());
        assertSavesUnchanged();
        size_t writesBefore = blobs.writes;
        tap(app, desktop, 200, 354);
        assert(app.settingsController.resetConfirm());
        assert(blobs.clears == 0 && blobs.writes == writesBefore);
        assert(app.needsRender());
        assert(app.renderRowBegin() == 0 && app.renderRowEnd() == AmoledUi::HEIGHT);
        assertSavesUnchanged();
        tap(app, desktop, 184, 300);  // Body text and blank area are inert.
        assert(app.settingsController.resetConfirm());
        tap(app, desktop, 106, 390);  // Cancel returns to the settings list.
        assert(!app.settingsController.resetConfirm() && app.sceneFlow.current() == Scene::SETTINGS);
        assert(blobs.clears == 0 && blobs.writes == writesBefore);
        assertSavesUnchanged();
        tap(app, desktop, 200, 354);
        tap(app, desktop, 40, 38);  // Header back also only dismisses confirmation.
        assert(!app.settingsController.resetConfirm() && app.sceneFlow.current() == Scene::SETTINGS);
        assertSavesUnchanged();

        app.visitSession.startSearch();
        assert(app.visitSession.busy());
        tap(app, desktop, 200, 354);
        assert(!app.settingsController.resetConfirm());
        assert(blobs.clears == 0);
        assertSavesUnchanged();
        app.visitSession.stop();

        app.setSystemUpdateHandler(resetTestOtaHandler);
        resetTestOtaCalls = 0;
        tap(app, desktop, 200, 354);  // Adding OTA does not replace the reset row.
        assert(app.settingsController.resetConfirm() && resetTestOtaCalls == 0);
        tap(app, desktop, 106, 390);
        tap(app, desktop, 200, 414);
#if STICKMON_ENABLE_DEBUG_FEATURES
        assert(resetTestOtaCalls == 1 && !app.settingsController.resetConfirm());
#else
        assert(resetTestOtaCalls == 0 && !app.settingsController.resetConfirm());
        assertSavesUnchanged();
#endif
        assert(blobs.clears == 0 && power.restarts == 0);
        // OTA legitimately saves before taking over; use its saved state below.
        for (size_t i = 0; i < before.size(); ++i) {
            before[i] = blobs.contents("stickmon", GAME_KEYS[i]);
        }
        writesBefore = blobs.writes;
        tap(app, desktop, 200, 354);
        assert(app.settingsController.resetConfirm());
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
        assert(blobs.clears == 0 && app.settingsController.resetConfirm());
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
        assert(blobs.clears == 0 && app.settingsController.resetConfirm());
        assertSavesUnchanged();
        app.visitSession.stop();
        blobs.failClear = true;
        tap(app, desktop, 262, 390);
        assert(blobs.clears == 1 && app.settingsController.resetConfirm() && app.storageReady);
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
        assert(fresh.hatchController_.state().elapsedSeconds == 0 && fresh.hatchController_.state().pokeCount == 0 && fresh.hatchController_.state().wipeCount == 0);
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
                        app.homeMainActor.faintRestActive = false;
                        app.homeRuntime.transition(0,
                            pose == 1 ? Home::Task::SLEEPING
                                : pose == 2 ? Home::Task::WANDER : Home::Task::IDLE,
                            desktop.millis(), 0, true);
                        app.petStopping = false;
                        app.pairFlow_.state_.mainRenderOffsetY = hop / 2.0f;
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
                        auto focusBounds = expected;
                        if (pose == 1) {
                            for (uint8_t i = 1; i < profile.sleepingFrames; ++i) {
                                model.petFrame = i;
                                const auto other = homePetVisibleBounds(model);
                                const int right = std::max(focusBounds.x + focusBounds.width, other.x + other.width);
                                const int bottom = std::max(focusBounds.y + focusBounds.height, other.y + other.height);
                                focusBounds.x = std::min(focusBounds.x, other.x);
                                focusBounds.y = std::min(focusBounds.y, other.y);
                                focusBounds.width = right - focusBounds.x;
                                focusBounds.height = bottom - focusBounds.y;
                                app.petFrame = i;
                                int16_t nextX = 0, nextY = 0;
                                assert(app.lockFocusPoint(nextX, nextY));
                                assert(nextX == x && nextY == y);
                            }
                        }
                        assert(x == focusBounds.x + focusBounds.width / 2);
                        assert(y == focusBounds.y + focusBounds.height / 2);
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
            app.inventoryController_.selectCategory(Game::ShopService::Category::DAILY);
            app.inventoryController_.selectItem(app.gameState, Scene::SHOP, 0);
            app.settingsController.showResetConfirmation();
            assert(app.prepareDisplayLock(desktop.millis()));
            assert(app.sceneFlow.current() == Scene::HOME);
            assert(!app.inventoryController_.state().itemConfirmOpen && !app.settingsController.resetConfirm());
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
        app.visitAwayFlow_.state_.phase = AmoledApp::VisitDeparture::AWAY;
        assert(!app.prepareDisplayLock(desktop.millis()));
        app.visitAwayFlow_.state_.phase = AmoledApp::VisitDeparture::NONE;
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
            const auto* species = findSpecies(1);
            uint8_t learnLevel = 0;
            for (uint16_t i = 0; i < learnsetEntryCountForSpecies(*species); ++i) {
                const auto* entry = learnsetEntryForSpecies(*species, i);
                if (entry && entry->level > 1 && entry->level < 16 &&
                    entry->moveId != 33 && entry->moveId != 44 &&
                    canLearnAsSpecialMove(*species, entry->moveId)) {
                    learnLevel = entry->level;
                    break;
                }
            }
            assert(learnLevel > 1);
            app.gameState.team[0].speciesId = 1;
            app.gameState.team[0].level = learnLevel;
            app.openProgressionScene(Scene::HOME, 0, learnLevel - 1, desktop.millis());
            app.advanceProgression(desktop.millis());
            const auto learnedMove = app.progressionFlow_.state().progressionMoveId;
            assert(learnedMove != 0);
            tap(app, desktop, 328, 38);
            assert(app.progressionFlow_.state().progressionMode == ProgressionViewModel::Mode::MOVE_REPLACE);
            assert(app.gameState.team[0].move2Id == 33);
            assert(app.gameState.team[0].move3Id == 44);
            tap(app, desktop, 160, 106 + selection * 66);
            assert(app.progressionFlow_.state().progressionSelectedItem == selection);
            assert(app.progressionFlow_.state().progressionMode == ProgressionViewModel::Mode::MOVE_REPLACE);
            assert(app.gameState.team[0].move2Id == 33);
            assert(app.gameState.team[0].move3Id == 44);
            tap(app, desktop, 328, 38);
            assert(app.gameState.team[0].move2Id == (selection == 1 ? learnedMove : 33));
            assert(app.gameState.team[0].move3Id == (selection == 2 ? learnedMove : 44));
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
        app.homeMainActor.x = static_cast<float>(room.doorwayInsideX());
        app.homeMainActor.y = static_cast<float>(room.doorwayInsideY());
        assert(!app.petFootprintInsideWalkArea(app.homeMainActor.x, app.homeMainActor.y));
        assert(app.debugAcceptContact());
        app.syncHomeActors(desktop.millis());
        assert(app.visitorFlow_.state_.motion == AmoledApp::VisitorMotion::HOST_APPROACH);
        assert(app.homeCompanionActor.hidden);
        assert(app.petFootprintInsideWalkArea(app.homeMainActor.x, app.homeMainActor.y));
        bool sawHostClear = false;
        bool sawGuestEnter = false;
        for (int tick = 0;
             tick < 400 && app.visitorFlow_.state_.motion != AmoledApp::VisitorMotion::ACTIVE;
             ++tick) {
            desktop.advanceMs(50);
            app.update(desktop.millis());
            app.markRendered();
            sawHostClear |= app.visitorFlow_.state_.hostDoorPhase == 2;
            sawGuestEnter |= app.visitorFlow_.state_.motion ==
                AmoledApp::VisitorMotion::ENTERING;
        }
        assert(sawHostClear && sawGuestEnter);
        assert(app.visitorFlow_.state_.motion == AmoledApp::VisitorMotion::ACTIVE);
        assert(app.homeRuntime.validate() == Home::INVARIANT_OK);
        HomeViewModel enteredModel;
        app.fillHomePetModel(enteredModel);
        assert(enteredModel.petCenterX == app.worldToScreenX(app.homeMainActor.x));
        assert(enteredModel.petGroundY == app.worldToScreenY(app.homeMainActor.y));
        assert(app.petFootprintInsideWalkArea(app.homeMainActor.x, app.homeMainActor.y));
        assert(app.companionFootprintInsideWalkArea(
            app.homeCompanionActor.x, app.homeCompanionActor.y));
        assert(app.pairFlow_.state().phase == AmoledApp::PairPhase::APPROACH);
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
        assert(app.visitorFlow_.state_.motion == AmoledApp::VisitorMotion::HOST_APPROACH);
        assert(app.homeCompanionActor.hidden);
        // Firmware can service the freshly attached visitor again in the
        // same millisecond. That zero-duration tick must not reveal it.
        app.update(desktop.millis());
        app.markRendered();
        assert(app.visitorFlow_.state_.motion == AmoledApp::VisitorMotion::HOST_APPROACH);
        assert(app.homeCompanionActor.hidden);
        int ticks = 0;
        bool sawVisitorEnter = false;
        while (app.visitorFlow_.state_.motion != AmoledApp::VisitorMotion::ACTIVE) {
            desktop.advanceMs(50);
            app.update(desktop.millis());
            app.markRendered();
            if (app.visitorFlow_.state_.motion == AmoledApp::VisitorMotion::ENTERING) {
                sawVisitorEnter = true;
                assert(!app.homeCompanionActor.hidden);
            }
            assert(++ticks < 300);
        }
        assert(sawVisitorEnter);
        assert(app.pairFlow_.state().phase == AmoledApp::PairPhase::APPROACH);
        ticks = 0;
        while (app.pairFlow_.state().phase == AmoledApp::PairPhase::APPROACH) {
            desktop.advanceMs(50);
            app.update(desktop.millis());
            app.markRendered();
            assert(++ticks < 120);
        }
        if (app.pairFlow_.state().phase != AmoledApp::PairPhase::INVITE) {
            std::fputs(desktop.logs().c_str(), stderr);
        }
        assert(app.pairFlow_.state().phase == AmoledApp::PairPhase::INVITE);
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
            if (app.visitorFlow_.state_.motion == AmoledApp::VisitorMotion::EXITING) {
                assert(std::hypot(app.homeCompanionActor.x - previousX,
                                  app.homeCompanionActor.y - previousY) < 3.0f);
                ++exitFrames;
            }
            assert(++ticks < 900);
        }
        assert(exitFrames > 0);
        assert(app.gameState.teamCount == 1);
        assert(app.homeRuntime.validate() == Home::INVARIANT_OK);
        HomeViewModel exitedModel;
        app.fillHomePetModel(exitedModel);
        assert(exitedModel.petCenterX == app.worldToScreenX(app.homeMainActor.x));
        assert(exitedModel.petGroundY == app.worldToScreenY(app.homeMainActor.y));
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
        const float petX = app.homeMainActor.x;
        const float petY = app.homeMainActor.y;
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
                assert(app.homeMainActor.x == petX && app.homeMainActor.y == petY);
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
                app.selectedExploreArea, app.exploreSession_.state_.exploreRouteMap));
            app.sceneFlow.enter(Scene::EXPLORE_ROUTE);
            app.exploreSession_.state_.exploreRoutePaused = false;
            app.exploreSession_.state_.exploreRouteComplete = false;
            app.exploreSession_.state_.exploreRouteBossPending = false;
            app.exploreSession_.state_.exploreRoutePickupAvailable = false;
            app.exploreSession_.state_.exploreRouteAutoWalk = false;
            app.exploreSession_.state_.exploreRoutePlayerWalkActive = false;
            app.exploreSession_.state_.exploreRouteExiting = false;
            app.autonomousExpedition = false;
            for (uint8_t direction = 0; direction < 4; ++direction) {
                uint32_t now = desktop.millis();
                app.explorePresenter_.state_.exploreRouteDirection = direction;
                app.explorePresenter_.state_.exploreRouteFollowerDirection = direction;
                app.exploreSession_.state_.exploreRouteMoving = app.explorePresenter_.state_.exploreRouteFollowerMoving = true;
                app.explorePresenter_.state_.exploreRouteWorldX = app.explorePresenter_.state_.exploreRouteFromX = 70;
                app.explorePresenter_.state_.exploreRouteWorldY = app.explorePresenter_.state_.exploreRouteFromY = 72;
                app.explorePresenter_.state_.exploreRouteFollowerWorldX = app.explorePresenter_.state_.exploreRouteFollowerFromX = 54;
                app.explorePresenter_.state_.exploreRouteFollowerWorldY = app.explorePresenter_.state_.exploreRouteFollowerFromY = 56;
                const float dx = direction == 2 ? -16 : direction == 3 ? 16 : 0;
                const float dy = direction == 0 ? 16 : direction == 1 ? -16 : 0;
                app.explorePresenter_.state_.exploreRouteTargetX = 70 + dx;
                app.explorePresenter_.state_.exploreRouteTargetY = 72 + dy;
                app.explorePresenter_.state_.exploreRouteFollowerTargetX = 54 + dx;
                app.explorePresenter_.state_.exploreRouteFollowerTargetY = 56 + dy;
                app.explorePresenter_.state_.exploreRouteMoveStartedMs = now;
                app.explorePresenter_.state_.nextExploreRouteFrameMs = now + 90;
                app.explorePresenter_.state_.nextExploreRouteMapFrameMs = now + 10000;
                app.explorePresenter_.updateCamera();
                app.forceFullRender();
                app.render(canvas);
                app.markRendered();
                const uint8_t frame = app.explorePresenter_.state_.exploreRoutePetFrame;
                for (int tick = 1; tick <= 12; ++tick) {
                    const std::vector<uint16_t> before(canvas.rawPixels(),
                        canvas.rawPixels() + AmoledUi::WIDTH * AmoledUi::HEIGHT);
                    desktop.advanceMs(20);
                    app.updateExploreRoute(desktop.millis());
                    assert(app.needsRender());
                    if (tick < 5) assert(app.explorePresenter_.state_.exploreRoutePetFrame == frame);
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
                app.exploreSession_.state_.exploreRouteMoving = app.explorePresenter_.state_.exploreRouteFollowerMoving = false;
                app.requestExploreRouteDynamicRender();
                app.render(canvas);
                const std::vector<uint16_t> stopped(canvas.rawPixels(),
                    canvas.rawPixels() + AmoledUi::WIDTH * AmoledUi::HEIGHT);
                app.markRendered();
                app.forceFullRender();
                app.render(canvas);
                assert(std::equal(stopped.begin(), stopped.end(), canvas.rawPixels()));
                app.markRendered();
                app.exploreSession_.state_.exploreRoutePaused = true;
                desktop.advanceMs(20);
                app.updateExploreRoute(desktop.millis());
                assert(!app.needsRender());
                app.exploreSession_.state_.exploreRoutePaused = false;
            }
            // A changed viewport restores the entire map, including both actors.
            app.explorePresenter_.state_.exploreRouteFromX = app.explorePresenter_.state_.exploreRouteWorldX = 180;
            app.explorePresenter_.state_.exploreRouteTargetX = 196;
            app.explorePresenter_.state_.exploreRouteFromY = app.explorePresenter_.state_.exploreRouteWorldY = 140;
            app.explorePresenter_.state_.exploreRouteTargetY = 140;
            app.exploreSession_.state_.exploreRouteMoving = true;
            app.explorePresenter_.state_.exploreRouteMoveStartedMs = desktop.millis();
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
        app.exploreSession_.state_.exploreRouteMapBlock = 0;
        app.exploreSession_.state_.exploreRouteMapBlockCount = 1;
        for (bool withBoss : {false, true}) {
            uint8_t selected[4] = {};
            for (uint32_t seed = 1; seed <= 96; ++seed) {
                for (auto edge : {ExploreMapGenerator::Edge::TOP, ExploreMapGenerator::Edge::RIGHT,
                                  ExploreMapGenerator::Edge::BOTTOM, ExploreMapGenerator::Edge::LEFT}) {
                    app.exploreSession_.state_.exploreRouteExpeditionSeed = seed;
                    app.exploreSession_.state_.exploreRoutePendingEntryEdge = edge;
                    app.exploreSession_.state_.exploreRouteBossScheduled = withBoss;
                    assert(app.generateExploreRouteMap(desktop.millis()));
                    assert(app.exploreSession_.state_.exploreRouteMap.entry.edge == edge);
                    const uint8_t count = app.exploreSession_.state_.exploreRouteMap.pathCount;
                    assert(count >= 1 && count <= 3 && app.exploreSession_.state_.exploreRoutePath < count);
                    selected[count] |= 1U << app.exploreSession_.state_.exploreRoutePath;
                    const auto& path = app.exploreSession_.state_.exploreRouteMap.paths[app.exploreSession_.state_.exploreRoutePath];
                    auto start = ExploreRouteGeometry::pathPoint(path, 0);
                    assert(app.explorePresenter_.state_.exploreRouteWorldX == start.x && app.explorePresenter_.state_.exploreRouteWorldY == start.y);
                    if (withBoss) {
                        assert(ExploreBoss::canPlaceOnPath(path.pointCount));
                        assert(app.exploreSession_.state_.exploreRouteBossPending);
                        assert(app.exploreSession_.state_.exploreRouteBossIndex > 0 &&
                               app.exploreSession_.state_.exploreRouteBossIndex < path.pointCount - 1);
                    }
                }
            }
            assert(selected[1] == 1 && selected[2] == 3);
            assert((selected[3] & 4) != 0);  // The third exit must be selectable, including boss runs.
            if (!withBoss) assert(selected[3] == 7);
        }
        // Follow the third route to the next map with its actual exit direction.
        app.exploreSession_.state_.exploreRouteBossScheduled = false;
        app.exploreSession_.state_.exploreRouteMapBlockCount = 2;
        app.exploreSession_.state_.exploreRouteMapBlock = 0;
        for (uint32_t seed = 1; ; ++seed) {
            assert(seed < 100);
            app.exploreSession_.state_.exploreRouteExpeditionSeed = seed;
            assert(app.generateExploreRouteMap(desktop.millis()));
            if (app.exploreSession_.state_.exploreRouteMap.pathCount == 3) break;
        }
        app.exploreSession_.state_.exploreRoutePath = 2;
        auto exit = app.exploreSession_.state_.exploreRouteMap.paths[2].exit.edge;
        finishRoute(app, desktop.millis());
        assert(app.exploreSession_.state_.exploreRouteMapBlock == 1);
        assert(app.exploreSession_.state_.exploreRouteMap.entry.edge == ExploreMapGenerator::opposite(exit));
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
                app.exploreSession_.state_.exploreRouteMap.pathCount = 1;
                app.exploreSession_.state_.exploreRouteMap.paths[0].exit.edge = edge;
                app.exploreSession_.state_.exploreRouteMapBlockCount = 1;
                app.exploreSession_.state_.exploreRouteMapBlock = 0;
                app.exploreSession_.state_.exploreRoutePath = 0;
                app.explorePresenter_.state_.exploreRouteWorldX = width / 2;
                app.explorePresenter_.state_.exploreRouteWorldY = height / 2;
                app.explorePresenter_.state_.exploreRouteFollowerWorldX = width / 2 - 12;
                app.explorePresenter_.state_.exploreRouteFollowerWorldY = height / 2 - 12;
                app.exploreSession_.state_.exploreRouteBossPending = false;
                app.exploreSession_.state_.exploreRouteComplete = false;
                app.exploreSession_.state_.exploreRoutePaused = false;
                app.autonomousExpedition = false;
                uint32_t now = desktop.millis();
                app.beginExploreRouteExit(now);
                assert(app.exploreSession_.state_.exploreRouteExiting && app.exploreSession_.state_.exploreRouteMoving);
                assert(app.explorePresenter_.state_.exploreRouteFollowerMoving == (teamCount == 2));
                app.updateExploreRoute(now);
                const uint8_t leaderFrame = app.explorePresenter_.state_.exploreRoutePetFrame;
                const uint8_t followerFrame = app.explorePresenter_.state_.exploreRouteFollowerFrame;
                app.updateExploreRoute(now + 45);
                assert(app.explorePresenter_.state_.exploreRoutePetFrame == leaderFrame);
                assert(app.explorePresenter_.state_.exploreRouteFollowerFrame == followerFrame);
                app.updateExploreRoute(now + 90);
                assert(app.explorePresenter_.state_.exploreRoutePetFrame == static_cast<uint8_t>(leaderFrame + 1));
                assert(app.explorePresenter_.state_.exploreRouteFollowerFrame == static_cast<uint8_t>(
                    followerFrame + (teamCount == 2 ? 1 : 0)));
                assert(!app.exploreSession_.state_.exploreRouteComplete);
                assert(app.explorePresenter_.state_.exploreRouteWorldX != app.explorePresenter_.state_.exploreRouteFromX ||
                       app.explorePresenter_.state_.exploreRouteWorldY != app.explorePresenter_.state_.exploreRouteFromY);
                if (teamCount == 2) {
                    assert(app.explorePresenter_.state_.exploreRouteFollowerWorldX != app.explorePresenter_.state_.exploreRouteFollowerFromX ||
                           app.explorePresenter_.state_.exploreRouteFollowerWorldY != app.explorePresenter_.state_.exploreRouteFollowerFromY);
                }
                int ticks = 0;
                now += 90;
                while (app.exploreSession_.state_.exploreRouteExiting) {
                    now += 90;
                    app.updateExploreRoute(now);
                    assert(++ticks < 1000);
                }
                assert(app.exploreSession_.state_.exploreRouteComplete);
                auto outside = [=](float x, float y) {
                    return x < 0 || x > width || y < 0 || y > height;
                };
                assert(outside(app.explorePresenter_.state_.exploreRouteWorldX, app.explorePresenter_.state_.exploreRouteWorldY));
                if (teamCount == 2) {
                    assert(outside(app.explorePresenter_.state_.exploreRouteFollowerWorldX,
                                   app.explorePresenter_.state_.exploreRouteFollowerWorldY));
                }
                assert(!app.exploreSession_.state_.exploreRouteMoving && !app.explorePresenter_.state_.exploreRouteFollowerMoving);
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
        app.exploreSession_.state_.exploreRouteMapBlock = 0;
        app.exploreSession_.state_.exploreRouteMapBlockCount = 3;
        app.exploreSession_.state_.exploreRouteExpeditionSeed = 0x20260713;
        app.exploreSession_.state_.exploreRoutePendingEntryEdge = ExploreMapGenerator::Edge::TOP;
        app.exploreSession_.state_.exploreRouteBossScheduled = false;
        uint32_t now = desktop.millis() + 5000;
        assert(app.generateExploreRouteMap(now));
        app.exploreSession_.state_.exploreRoutePath = 0;
        const auto& path = app.exploreSession_.state_.exploreRouteMap.paths[0];
        for (uint8_t index = 0; index < path.pointCount; ++index) {
            const auto& point = path.points[index];
            uint16_t cell = point.y * ExploreMapGenerator::WIDTH + point.x;
            assert(app.exploreSession_.state_.exploreRouteMap.layers[0][cell] != 4506);
            assert(app.exploreSession_.state_.exploreRouteMap.layers[0][cell] !=
                   ExploreCaveTiles::FROST_BROKEN_ICE_HOLE);
        }
        assert(!path.fallsToNextLevel);
        uint16_t downCell = path.exit.point.y * ExploreMapGenerator::WIDTH +
                            path.exit.point.x;
        assert(app.exploreSession_.state_.exploreRouteMap.layers[1][downCell] ==
               ExploreCaveTiles::FROST_DOWNWARD_STAIRS);
        assert(app.exploreSession_.state_.exploreRouteMap.layers[1][
            app.exploreSession_.state_.exploreRouteMap.paths[1].exit.point.y * ExploreMapGenerator::WIDTH +
            app.exploreSession_.state_.exploreRouteMap.paths[1].exit.point.x] ==
            ExploreCaveTiles::FROST_UP_LADDER[1]);
        app.exploreSession_.state_.exploreRoutePath = 1;
        app.exploreSession_.state_.exploreRouteBossPending = false;
        assert(finishRoute(app, now + 500));
        assert(app.exploreSession_.state_.exploreRouteMapBlock == 1);
        app.exploreSession_.state_.exploreRoutePath = 1;
        assert(finishRoute(app, now + 1000));
        assert(app.exploreSession_.state_.exploreRouteMapBlock == 2);
        const auto& entry = app.exploreSession_.state_.exploreRouteMap.entry.point;
        assert(app.exploreSession_.state_.exploreRouteMap.layers[1][entry.y * ExploreMapGenerator::WIDTH +
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
            app.exploreSession_.state_.exploreRouteMoving = false;
            app.exploreSession_.state_.exploreRoutePlayerWalkActive = false;
            app.pauseExploreRoute(desktop.millis());
            app.sceneFlow.openExploreMenu();
            tap(app, desktop, point[0], point[1]);
            assert(app.sceneFlow.current() == Scene::EXPLORE_ROUTE);
            assert(!app.exploreSession_.state_.exploreRoutePaused);
            assert(!app.exploreSession_.state_.exploreRouteMoving);
            assert(!app.exploreSession_.state_.exploreRoutePlayerWalkActive);
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
        app.exploreSession_.state_.exploreRouteMap = ExploreMapGenerator::Map{};
        app.exploreSession_.state_.exploreRouteMap.pathCount = 1;
        app.exploreSession_.state_.exploreRoutePath = 0;
        app.exploreSession_.state_.exploreRouteMapBlockCount = 1;
        auto& path = app.exploreSession_.state_.exploreRouteMap.paths[0];
        path.pointCount = 16;
        for (uint8_t index = 0; index < path.pointCount; ++index) {
            path.points[index] = {3, static_cast<uint8_t>(3 + index)};
        }
        app.exploreSession_.state_.exploreRouteEncounterCooldownSteps = 100;
        const uint16_t hpBefore = app.gameState.team[0].hpCur;
        const size_t beforeWalk = blobs.writes;
        for (int step = 0; step < 10; ++step) {
            assert(app.beginExploreRouteStep(desktop.millis()));
            desktop.advanceMs(1000);
            app.updateExploreRoute(desktop.millis());
            assert(!app.exploreSession_.state_.exploreRouteMoving);
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
        // Pickup IDs are local to ExploreSession.cpp: coin=1, potion=2.
        for (uint8_t pickup : {1, 2}) {
            const auto coins = app.gameState.coins;
            const auto potions = Game::ItemInventory::count(
                app.gameState, Game::ItemId::POTION);
            app.exploreSession_.state_.exploreRoutePickupAvailable = true;
            app.exploreSession_.state_.exploreRoutePickupItem = pickup;
            const auto writes = blobs.writes;
            pickupRoute(app, desktop.millis());
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
        app.battleSession_.state_.battlePhase = BattleViewModel::Phase::VICTORY;
        app.battleSession_.state_.battleWild = Game::MonsterFactory::create(7, 5);
        app.battleSession_.state_.battleRewardCoins = 123;
        app.battleSession_.state_.battleRewardExp = 0;  // Keep this case independent of level animations.
        app.battleSession_.state_.battleIsBoss = true;
        app.battleSession_.state_.battleSpecialKind = ExploreSpecial::Kind::FIRST_LATIAS;
        const auto coins = app.gameState.coins;
        const auto writes = blobs.writes;
        std::snprintf(app.battleSession_.battleMessage, sizeof(app.battleSession_.battleMessage), "Victory");
        app.battleSession_.pushBattleLog(desktop.millis());
        app.finishBattleVictory(desktop.millis());
        assert(app.gameState.coins == coins + 123);
        assert(blobs.writes == writes);  // Reward logs still own the battle screen.
        app.battleSession_.clearLog();
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
        assert(app.exploreSession_.state_.exploreRouteAutoWalk);
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
        for (int tries = 0; tries < 20 && app.sceneFlow.current() == Scene::PROGRESSION; ++tries) {
            desktop.advanceMs(20000);
            app.advanceProgression(desktop.millis());
        }
        assert(app.sceneFlow.current() == Scene::EXPLORE_ROUTE);
        load();
        assert(!loaded.pendingLevelUp);

        // Escape uses the same exit, preserving damage even without a reward.
        app.sceneFlow.enter(Scene::BATTLE);
        --app.gameState.team[0].hpCur;
        app.closeBattle(desktop.millis());
        load();
        assert(loaded.team[0].hpCur == app.gameState.team[0].hpCur);

        // A failed return snapshot retries without applying the bond twice.
        ++app.gameState.stepsToday;
        app.gameState.team[0].bond = 10;
        blobs.failSnapshots = true;
        app.completeExploreReturn();
        assert(app.sceneFlow.current() == Scene::HOME);
        assert(app.gameState.team[0].bond == 11 && app.saveCoordinator_.dirty());
        const auto failedReturnWrites = blobs.writes;
        app.completeExploreReturn();
        assert(app.gameState.team[0].bond == 11 && blobs.writes == failedReturnWrites);
        blobs.failSnapshots = false;
        desktop.advanceMs(2500);
        app.updateClockAndCare(desktop.millis());
        load();
        assert(loaded.stepsToday == 11 && loaded.team[0].bond == 11);

        app.sceneFlow.enter(Scene::BATTLE);
        app.battleSession_.state_.battlePhase = BattleViewModel::Phase::DEFEAT;
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
        app.exploreSession_.state_.exploreRouteMapBlock = 0;
        app.exploreSession_.state_.exploreRouteMapBlockCount = 1;
        app.exploreSession_.state_.exploreRoutePath = 0;
        app.exploreSession_.state_.exploreRouteMap = ExploreMapGenerator::Map{};
        app.exploreSession_.state_.exploreRouteMap.pathCount = 1;
        auto& path = app.exploreSession_.state_.exploreRouteMap.paths[0];
        path.pointCount = 4;
        for (uint8_t index = 0; index < path.pointCount; ++index) {
            path.points[index] = {3, static_cast<uint8_t>(3 + index)};
        }

        // Faint the original leader in battle, then finish the automatic switch
        // and return to the route without changing the permanent team order.
        assert(app.beginExploreEncounter(desktop.millis()));
        assert(app.battleSession_.state_.battlePlayerSlot == 0);
        app.gameState.team[0].hpCur = 0;
        assert(app.resolveBattleFaint(desktop.millis()));
        assert(app.gameState.team[0].fainted);
        desktop.advanceMs(360);
        app.battleSession_.updateBattleSwitch(app.gameState, desktop.millis());
        app.applyBattleEffects(desktop.millis());
        desktop.advanceMs(360);
        app.battleSession_.updateBattleSwitch(app.gameState, desktop.millis());
        app.applyBattleEffects(desktop.millis());
        assert(app.battleSession_.state_.battlePlayerSlot == 1);
        assert(app.battleSession_.timeline_.state_.battleSwitchStage == BattleSwitchStage::NONE);
        app.closeBattle(desktop.millis());

        app.exploreSession_.state_.exploreRouteIndex = 1;
        app.exploreSession_.state_.exploreRouteGuaranteedEncounterIndex = 1;
        app.exploreSession_.state_.exploreRouteGuaranteedEncounterPending = true;
        app.exploreSession_.state_.exploreRouteMapEncounterCount = 0;
        resolveRoute(app, desktop.millis(), false, false);
        assert(app.sceneFlow.current() == Scene::BATTLE);
        assert(app.battleSession_.state_.battlePlayerSlot == 1 && !app.battleSession_.state_.battleIsBoss);
        assert(app.exploreSession_.state_.exploreRouteMapEncounterCount == 1);
        assert(!app.exploreSession_.state_.exploreRouteGuaranteedEncounterPending);
        assert(app.toast == nullptr);
        app.closeBattle(desktop.millis());

        app.exploreSession_.state_.exploreRouteBossPending = true;
        app.exploreSession_.state_.exploreRouteBossIndex = 2;
        app.exploreSession_.state_.exploreRouteIndex = 2;
        app.exploreSession_.state_.exploreRouteBossSpeciesId = 7;
        app.exploreSession_.state_.exploreRouteBossLevel = 12;
        app.exploreSession_.state_.exploreRouteBossExperiencePercent = 150;
        resolveRoute(app, desktop.millis(), true, true);
        assert(app.sceneFlow.current() == Scene::BATTLE);
        assert(app.battleSession_.state_.battlePlayerSlot == 1 && app.battleSession_.state_.battleIsBoss);
        assert(app.battleSession_.state_.battleWild.speciesId == 7 && app.battleSession_.state_.battleWild.level == 12);
        assert(app.battleSession_.state_.battleExperiencePercent == 150);
        assert(!app.exploreSession_.state_.exploreRouteBossPending);
        assert(app.gameState.team[0].speciesId == state.team[0].speciesId);
        assert(app.gameState.team[1].speciesId == state.team[1].speciesId);
        assert(app.gameState.activeSlot == state.activeSlot);
        app.closeBattle(desktop.millis());

        // The route-end retry must use the same healthy reserve selection.
        app.exploreSession_.state_.exploreRouteBossPending = true;
        assert(finishRoute(app, desktop.millis()));
        assert(app.battleSession_.state_.battlePlayerSlot == 1 && app.battleSession_.state_.battleIsBoss);
        assert(!app.exploreSession_.state_.exploreRouteBossPending);
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
                    assert(app.battleSession_.state_.battlePlayerSlot == (healthCase == 0 ? 0 : 1));
                    app.closeBattle(desktop.millis());
                } else {
                    assert(app.sceneFlow.current() == Scene::EXPLORE_ROUTE);
                    assert(app.toast && std::strcmp(app.toast, Ui::Menu::PET_REST) == 0);
                }
            }
        }
        std::puts("explore reserve: faint switch, next encounter, boss and empty-team guards verified");
    }

    static void checkHealthyDeparture(DesktopPlatform& desktop, Canvas565& canvas,
                                      const Game::GameState& state) {
        for (bool flagOnly : {false, true}) {
            for (int faintMask = 0; faintMask < 4; ++faintMask) {
                AmoledApp app;
                app.begin(desktop.millis());
                app.gameState = state;
                app.gameState.teamCount = 2;
                app.gameState.team[0] = Game::MonsterFactory::create(8, 20);
                app.gameState.team[1] = Game::MonsterFactory::create(280, 20);
                app.gameState.settings.idleTimeoutIndex = 4;
                app.gameState.settings.speedIndex = 0;
                for (int slot = 0; slot < 2; ++slot) {
                    if (!(faintMask & (1 << slot))) continue;
                    if (flagOnly) app.gameState.team[slot].fainted = true;
                    else app.gameState.team[slot].hpCur = 0;
                }
                app.sceneFlow.enter(Scene::EXPLORE_AREAS);
                const auto activeSlot = app.gameState.activeSlot;
                tap(app, desktop, 98, 412);
                if (faintMask == 3) {
                    assert(!app.pendingExpedition && !app.exploreSessionActive);
                    assert(app.expeditionDeparturePhase == Phase::NONE);
                    assert(app.sceneFlow.current() == Scene::EXPLORE_AREAS);
                    assert(app.toast && std::strcmp(app.toast, Ui::Menu::PET_REST) == 0);
                    continue;
                }
                assert(app.pendingExpedition);
                assert(app.sceneFlow.current() == Scene::HOME);
                const float mainX = app.homeMainActor.x, mainY = app.homeMainActor.y;
                const float companionX = app.homeCompanionActor.x;
                const float companionY = app.homeCompanionActor.y;
                int frames = 0;
                while (app.expeditionDeparturePhase != Phase::FADE_OUT) {
                    tick(app, desktop);
                    assert(++frames < 1000);
                    if (faintMask & 1) {
                        assert(app.petIsResting() && !app.expeditionMainHidden);
                        assert(app.homeMainActor.x == mainX && app.homeMainActor.y == mainY);
                        assert(app.homeMainActor.task == Home::Task::FAINTED);
                        assert(app.expeditionDeparturePhase != Phase::WALK_TO_DOOR);
                        assert(app.expeditionDeparturePhase != Phase::CROSS_DOOR);
                    }
                    if (faintMask & 2) {
                        assert(app.homeCompanionActor.faintRestActive);
                        assert(!app.expeditionCompanionHidden);
                        assert(app.homeCompanionActor.x == companionX);
                        assert(app.homeCompanionActor.y == companionY);
                        assert(app.homeCompanionActor.task == Home::Task::FAINTED);
                    }
                    if (app.expeditionDeparturePhase != Phase::FADE_OUT) present(app, canvas);
                }
                assert(app.expeditionMainHidden == !(faintMask & 1));
                assert(app.expeditionCompanionHidden == !(faintMask & 2));
                fadeOut(app, desktop, canvas, Scene::HOME);
                fadeIn(app, desktop, canvas, Scene::EXPLORE_ROUTE);
                assert(app.gameState.team[0].speciesId == 8);
                assert(app.gameState.team[1].speciesId == 280);
                assert(app.gameState.activeSlot == activeSlot);
                assert(app.beginExploreEncounter(desktop.millis(), false, 7, 12));
                assert(app.battleSession_.state_.battlePlayerSlot == (faintMask == 1 ? 1 : 0));
            }
        }
        AmoledApp solo;
        solo.begin(desktop.millis());
        solo.gameState = state;
        solo.gameState.teamCount = 1;
        solo.gameState.team[0].hpCur = 0;
        assert(!solo.queueExploreDeparture(0, false));
        assert(!solo.pendingExpedition);
        std::puts("healthy departure: both/leader/reserve, HP zero/flag only, all down and solo guards verified");
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
                assert(app.petIsResting());
                assert(app.homeMainActor.x == RoomResource::ins().bedX());
                assert(app.homeMainActor.y == RoomResource::ins().bedY());
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
                assert(app.homeMainActor.x == RoomResource::ins().bedX());
                assert(app.petIsResting());
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
        checkHealthyDeparture(desktop, canvas, state);
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

            app.exploreSession_.state_.exploreRouteMoving = false;
            app.exploreSession_.state_.exploreRouteBossPending = false;
            app.exploreSession_.state_.exploreRouteMapBlockCount = 1;
            app.exploreSession_.state_.exploreRouteMapBlock = 0;
            app.exploreSession_.state_.exploreRouteSteps = 10;
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
                finishRoute(app, desktop.millis());
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
            assert(!app.exploreSession_.state_.exploreRouteMoving && !app.exploreSession_.state_.exploreRouteAutoWalk);
            assert(app.homeMainActor.x == app.homeMainActor.targetX && app.homeMainActor.y == app.homeMainActor.targetY);
            assert(app.homeMainActor.task == Home::Task::IDLE);
            assert(app.homeRuntime.validate() == Home::INVARIANT_OK);
            HomeViewModel returnedModel;
            app.fillHomePetModel(returnedModel);
            assert(returnedModel.petCenterX == app.worldToScreenX(app.homeMainActor.x));
            assert(returnedModel.petGroundY == app.worldToScreenY(app.homeMainActor.y));
            if (app.gameState.teamCount > 1 &&
                app.homeCompanionActor.active &&
                !app.gameState.team[1].fainted &&
                app.gameState.team[1].hpCur > 0) {
                assert(app.homeCompanionActor.task == Home::Task::IDLE);
                const float dx = app.homeCompanionActor.x - app.homeMainActor.x;
                const float dy = app.homeCompanionActor.y - app.homeMainActor.y;
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

void checkSharedBattleExecution(DesktopPlatform& desktop, const Game::GameState& state) {
    AmoledV1::AmoledAppTestAccess::checkSharedBattleExecution(desktop, state);
}

#if STICKMON_ENABLE_DEBUG_FEATURES
void checkBattleDiagnosticsSaveIsolation(DesktopPlatform& desktop,
                                         const Game::GameState& state) {
    AmoledV1::AmoledAppTestAccess::checkBattleDiagnosticsSaveIsolation(desktop, state);
}
#endif

void checkMedicineFlow(DesktopPlatform& desktop, Canvas565& canvas,
                        const Game::GameState& state) {
    AmoledV1::AmoledAppTestAccess::checkMedicineFlow(desktop, canvas, state);
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
