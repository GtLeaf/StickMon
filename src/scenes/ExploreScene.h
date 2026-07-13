#pragma once

#include "core/Scene.h"
#include "game/ExploreMapGenerator.h"
#include "game/GameState.h"
#include "game/Species.h"

class ExploreScene : public Scene {
public:
    enum class Area : uint8_t {
        GRASS_PATH,
        CREEK_SLOPE,
        TALL_GRASS_PARK,
        LAKESIDE_CAUSEWAY,
        MIST_FOREST_PATH,
        ANCIENT_WATERFALL_VALLEY,
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
        LEARN_MOVE,
        RESULT,
    };

    Phase phase = Phase::SELECT;
    uint8_t areaCursor = 0;
    Area activeArea = Area::GRASS_PATH;
    uint16_t steps = 0;
    uint16_t mapTargetSteps = 1;
    const Species* wild = nullptr;
    Game::MonsterRuntime wildRuntime;
    uint16_t wildHp = 0;
    uint16_t wildHpMax = 0;
    const char* resultMessage = nullptr;
    char resultBuf[64] = {};
    enum class BattleLogCue : uint8_t {
        NONE,
        EXP_GAIN,
    };
    static constexpr uint8_t BATTLE_LOG_QUEUE_CAP = 8;
    static constexpr uint8_t BATTLE_LOG_VISIBLE_CAP = 2;
    static constexpr uint8_t BATTLE_LOG_LEN = 64;
    char battleLogQueue[BATTLE_LOG_QUEUE_CAP][BATTLE_LOG_LEN] = {};
    BattleLogCue battleLogCues[BATTLE_LOG_QUEUE_CAP] = {};
    char battleLogVisible[BATTLE_LOG_VISIBLE_CAP][BATTLE_LOG_LEN] = {};
    uint8_t battleLogHead = 0;
    uint8_t battleLogCount = 0;
    uint8_t battleLogVisibleCount = 0;
    uint32_t battleLogUntil = 0;
    bool battleLogActive = false;
    bool battleResultPending = false;
    bool expAnimationPending = false;
    bool expAnimationActive = false;
    uint32_t expAnimationFrom = 0;
    uint32_t expAnimationTo = 0;
    uint32_t expAnimationStarted = 0;
    bool lastCaptureSuccess = false;
    uint8_t battleCursor = 0;
    uint8_t captureCursor = 0;
    uint8_t battleTurns = 0;
    bool captureMenuOpen = false;
    bool captureAnimationActive = false;
    bool captureOutcome = false;
    Game::ItemId captureBall = Game::ItemId::POKE_BALL;
    uint32_t captureAnimationStarted = 0;
    uint8_t learnCursor = 0;
    Phase learnReturnPhase = Phase::WALKING;
    uint8_t fleeAttempts = 0;
    bool exitAfterFaint = false;
    static constexpr uint8_t MAP_BLOCK_CAP = 4;
    static constexpr uint8_t MAP_EXIT_CAP = 2;
    uint8_t mapBlocks[MAP_BLOCK_CAP] = {};
    uint8_t mapBlockCount = 0;
    uint8_t currentMapBlock = 0;
    uint8_t usedMapMask = 0;
    uint8_t currentRoutePath = 0;
    uint8_t activeExitMask = 0;
    uint8_t exitNextMaps[MAP_EXIT_CAP] = {0xFF, 0xFF};
    ExploreMapGenerator::Map generatedMap;
    uint32_t expeditionSeed = 0;
    ExploreMapGenerator::Edge pendingEntryEdge = ExploreMapGenerator::Edge::TOP;
    uint8_t routeIndex = 0;
    bool routeMoving = false;
    uint8_t routeWalkDirection = 0;
    float routeWorldX = 0.0f;
    float routeWorldY = 0.0f;
    float routeFromX = 0.0f;
    float routeFromY = 0.0f;
    float routeTargetX = 0.0f;
    float routeTargetY = 0.0f;
    uint32_t routeMoveStarted = 0;
    bool endConfirmOpen = false;
    uint8_t endConfirmCursor = 0;
    uint32_t endConfirmOpenedAt = 0;

    void walk();
    void updateRouteMovement(uint32_t nowMs);
    void resumeWalk();
    void generateMapBlocks();
    void prepareMapRoutes();
    void advanceMapBlock(uint8_t nextMap);
    void rollEncounter();
    bool rollSceneEvent(bool forceEvent);
    void resolvePickup(uint8_t pickupId);
    void clearBattleLogs();
    void enqueueBattleLog(const char* text, BattleLogCue cue = BattleLogCue::NONE);
    void serviceBattleLog(uint32_t nowMs);
    bool battleLogBusy() const;
    void prepareExpAnimation(uint32_t fromExp, uint32_t toExp);
    void startExpAnimation(uint32_t nowMs);
    void updateExpAnimation(uint32_t nowMs);
    uint32_t battleExpForRender(uint32_t nowMs) const;
    bool enterPendingMoveLearn(Phase returnPhase);
    void attackWild();
    void wildCounterattack();
    void finishPlayerFaint();
    void openCaptureMenu();
    void tryCapture(Game::ItemId ball);
    void updateCaptureAnimation(uint32_t nowMs);
    void finishCaptureAnimation();
    void fleeEncounter();
    void resetWalk();
    void resetRouteSegment();
    void requestExploreExit(bool fainted = false);
    void renderAreaMenu();
    void renderWalking();
    void renderEncounter();
    void renderCaptureAnimation();
    void renderLearnMove();
    void renderResult();
    void renderEndConfirm();
    void drawRouteEndpoints(int cameraX, int cameraY);
    void drawWildBlock(int x, int y);
    void drawMonsterBlock(const Species& species, int x, int y, bool back = false);
    void renderBattleHud();
    void renderCommandBox();
};
