#pragma once

#include "core/Scene.h"
#include "game/GameState.h"
#include "game/Species.h"

class ExploreScene : public Scene {
public:
    enum class Biome : uint8_t {
        GRASS,
        RIVERSIDE,
        DEEP_FOREST,
        COUNT,
    };

    void onEnter() override;
    void onExit() override {}
    void update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;

private:
    enum class Phase : uint8_t {
        SELECT,
        WALKING,
        ENCOUNTER,
        RESULT,
    };

    Phase phase = Phase::SELECT;
    uint8_t biomeCursor = 0;
    Biome activeBiome = Biome::GRASS;
    uint16_t steps = 0;
    uint16_t targetSteps = 60;
    const Species* wild = nullptr;
    Game::MonsterRuntime wildRuntime;
    uint16_t wildHp = 0;
    uint16_t wildHpMax = 0;
    const char* resultMessage = nullptr;
    char resultBuf[64] = {};
    static constexpr uint8_t BATTLE_LOG_QUEUE_CAP = 8;
    static constexpr uint8_t BATTLE_LOG_VISIBLE_CAP = 2;
    static constexpr uint8_t BATTLE_LOG_LEN = 64;
    char battleLogQueue[BATTLE_LOG_QUEUE_CAP][BATTLE_LOG_LEN] = {};
    char battleLogVisible[BATTLE_LOG_VISIBLE_CAP][BATTLE_LOG_LEN] = {};
    uint8_t battleLogHead = 0;
    uint8_t battleLogCount = 0;
    uint8_t battleLogVisibleCount = 0;
    uint32_t battleLogUntil = 0;
    bool battleLogActive = false;
    bool battleResultPending = false;
    bool lastCaptureSuccess = false;
    uint8_t battleCursor = 0;
    uint8_t fleeAttempts = 0;
    bool exitAfterFaint = false;

    void walk();
    void rollEncounter();
    bool rollSceneEvent(bool forceEvent);
    void resolvePickup(uint8_t pickupId);
    void clearBattleLogs();
    void enqueueBattleLog(const char* text);
    void serviceBattleLog(uint32_t nowMs);
    bool battleLogBusy() const;
    void attackWild();
    void wildCounterattack();
    void finishPlayerFaint();
    void tryCapture();
    void fleeEncounter();
    void resetWalk();
    void resetRouteSegment();
    void renderBiomeMenu();
    void renderWalking();
    void renderEncounter();
    void renderResult();
    void drawWildBlock(int x, int y);
    void drawMonsterBlock(const Species& species, int x, int y, bool back = false);
    void renderBattleHud();
    void renderCommandBox();
};
