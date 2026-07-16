#pragma once

#include "core/Scene.h"
#include "game/BattleSystem.h"
#include "game/ExploreMapGenerator.h"
#include "game/GameState.h"
#include "game/Species.h"
#include "scenes/MenuScene.h"

class ExploreScene : public Scene {
public:
    enum class Area : uint8_t {
        GRASS_PATH,
        CREEK_SLOPE,
        TALL_GRASS_PARK,
        FROST_CRYSTAL_CAVE,
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
        LEVEL_UP,
        LEARN_MOVE,
        PICKUP,
        RESULT,
        EXITING,
        ENDING,
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
        TEAM_SWITCH,
    };
    // Holds a complete two-sided turn plus faint and defeat messages.
    static constexpr uint8_t BATTLE_LOG_QUEUE_CAP = 24;
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
    bool battleExpVisible = false;
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
    uint8_t pendingBattleSwitchSlot = 0xFF;
    enum class BattleTurnStage : uint8_t {
        IDLE,
        WAIT_ACTION_START,
        ANIMATING,
        WAIT_ACTION_LOGS,
        WAIT_END_TURN_LOGS,
    };
    BattleTurnStage battleTurnStage = BattleTurnStage::IDLE;
    bool battleActionOrder[2] = {};
    uint8_t battleActionCount = 0;
    uint8_t battleActionIndex = 0;
    bool battleActionAttackerWild = false;
    bool battleActionSelfHit = false;
    BattleSystem::DamageResult battleActionResult;
    BattleSystem::ActionCheckResult battleActionCheck;
    BattleSystem::EffectResolution battleEffectResolution;
    BattleSystem::BattleActorState playerBattleState;
    BattleSystem::BattleActorState wildBattleState;
    uint8_t battleTurnSpecialSlots[2] = {
        BattleSystem::SPECIAL_SLOT_NONE,
        BattleSystem::SPECIAL_SLOT_NONE,
    };
    uint16_t battleHpFrom = 0;
    uint16_t battleHpTo = 0;
    uint32_t battleActionStarted = 0;
    uint8_t learnCursor = 0;
    Phase progressionReturnPhase = Phase::WALKING;
    bool walkStepResolutionPending = false;
    uint8_t fleeAttempts = 0;
    bool defeatAwaitInput = false;
    bool debugBattleMode = false;
    static constexpr uint8_t MAP_BLOCK_CAP = 9;
    static constexpr uint8_t MAP_EXIT_CAP = 2;
    uint8_t mapBlocks[MAP_BLOCK_CAP] = {};
    uint8_t mapBlockCount = 0;
    uint8_t currentMapBlock = 0;
    uint8_t mapEncounterCount = 0;
    uint8_t encounterCooldownSteps = 0;
    uint8_t currentRoutePath = 0;
    uint8_t activeExitMask = 0;
    uint8_t exitNextMaps[MAP_EXIT_CAP] = {0xFF, 0xFF};
    ExploreMapGenerator::Map generatedMap;
    uint32_t expeditionSeed = 0;
    ExploreMapGenerator::Edge pendingEntryEdge = ExploreMapGenerator::Edge::TOP;
    uint8_t routeIndex = 0;
    bool routeMoving = false;
    uint8_t routeWalkDirection = 0;
    uint8_t routeFollowerWalkDirection = 0;
    uint8_t routeVisualWalkDirection = 0;
    uint8_t routeFollowerVisualWalkDirection = 0;
    float routeWorldX = 0.0f;
    float routeWorldY = 0.0f;
    float routeFromX = 0.0f;
    float routeFromY = 0.0f;
    float routeTargetX = 0.0f;
    float routeTargetY = 0.0f;
    float routeFollowerWorldX = 0.0f;
    float routeFollowerWorldY = 0.0f;
    float routeFollowerFromX = 0.0f;
    float routeFollowerFromY = 0.0f;
    float routeFollowerTargetX = 0.0f;
    float routeFollowerTargetY = 0.0f;
    uint32_t routeMoveStarted = 0;
    uint16_t routeMoveDurationMs = 0;
    uint16_t routeLeaderMoveDurationMs = 0;
    uint16_t routeFollowerMoveDurationMs = 0;
    bool routeFollowerMoving = false;
    bool autoWalkActive = false;
    uint8_t routePickupIndex = 0;
    uint8_t routePickupItem = 0;
    bool routePickupAvailable = false;
    uint8_t routeGuaranteedEncounterIndex = 0;
    bool routeGuaranteedEncounterPending = false;
    static constexpr uint8_t EXPLORE_MENU_ITEM_COUNT = 4;
    bool exploreMenuOpen = false;
    bool exploreSubViewOpen = false;
    uint8_t exploreMenuCursor = 0;
    uint32_t exploreMenuOpenedAt = 0;
    MenuScene exploreSubView;

    void walk();
    void beginAutoWalk();
    void updateRouteMovement(uint32_t nowMs);
    void finishCompletedWalkStep();
    void finishProgression();
    void resumeWalk();
    void generateMapBlocks();
    void prepareMapRoutes();
    void placeRoutePickup();
    bool collectRoutePickup();
    void advanceMapBlock(uint8_t nextMap);
    int8_t currentDepthLevelOffset(uint8_t spread) const;
    void beginEncounter(const Species& species, uint8_t level);
    void beginDebugEncounter();
    void rollEncounter();
    bool rollRandomEncounter(bool guaranteed);
    void resolvePickup(uint8_t pickupId);
    void clearBattleLogs();
    void enqueueBattleLog(const char* text, BattleLogCue cue = BattleLogCue::NONE);
    void serviceBattleLog(uint32_t nowMs);
    bool battleLogBusy() const;
    bool battleLogPlaybackBusy() const;
    void updateBattleTurn(uint32_t nowMs);
    void beginBattleAction();
    void finishBattleAction();
    void applyBattleDamage();
    void resolveBattleEndTurn();
    void finishBattleEndTurn();
    void enqueueBattleEffectLogs(const BattleSystem::EffectResolution& effects,
                                 bool attackerWild);
    void finishWildFaint();
    uint16_t battleHpForRender(bool wildSide, uint16_t currentHp, uint32_t nowMs) const;
    int battleHitShakeX(bool wildSide, uint32_t nowMs) const;
    void prepareExpAnimation(uint32_t fromExp, uint32_t toExp);
    void startExpAnimation(uint32_t nowMs);
    void updateExpAnimation(uint32_t nowMs);
    uint32_t battleExpForRender(uint32_t nowMs) const;
    bool enterPendingProgression(Phase returnPhase);
    void enqueueBattleProgressionLogs(uint8_t teamSlot);
    void attackWild();
    void wildCounterattack();
    void finishPlayerFaint();
    void activatePendingBattleSwitch();
    void openCaptureMenu();
    void tryCapture(Game::ItemId ball);
    void updateCaptureAnimation(uint32_t nowMs);
    void finishCaptureAnimation();
    void fleeEncounter();
    void resetWalk();
    bool resetRouteSegment();
    void returnToDebugMenu();
    void beginRouteExit();
    void requestExploreExit(bool fainted = false, bool showEndPrompt = true);
    void closeExploreMenu();
    void renderAreaMenu();
    void renderWalking();
    void renderEncounter();
    void renderCaptureAnimation();
    void renderPickupPrompt();
    void renderResult();
    void renderEndPrompt();
    void renderExploreMenu();
    void renderRoutePickup(int cameraX, int cameraY);
    void drawRouteMonster(const Species& species, float worldX, float worldY,
                          uint8_t walkDirection, bool follower,
                          int cameraX, int cameraY);
    void drawMonsterSprite(const Species& species, int x, int groundY,
                           int maxWidth, int maxHeight, bool back = false,
                           int spriteOffsetX = 0);
    void renderBattleHud();
    void renderCommandBox();
};
