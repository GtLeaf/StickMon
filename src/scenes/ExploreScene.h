#pragma once

#include "core/Scene.h"
#include "core/ProgressionUi.h"
#include "game/BattleSystem.h"
#include "game/ExploreMapGenerator.h"
#include "game/ExplorePool.h"
#include "game/ExploreSpecialEncounter.h"
#include "game/GameState.h"
#include "game/Species.h"
#include "scenes/MenuScene.h"

namespace PokemonSprites {
struct SpriteFrame;
}

class ExploreScene : public Scene {
public:
    static bool serviceAreaPoolCache(uint8_t loadBudget = 1);

    enum class Area : uint8_t {
        GRASS_PATH,
        CREEK_SLOPE,
        TALL_GRASS_PARK,
        FROST_CRYSTAL_CAVE,
        MIST_FOREST_PATH,
        ANCIENT_WATERFALL_VALLEY,
        COUNT,
    };
    static_assert(static_cast<uint8_t>(Area::COUNT) ==
                      Game::EXPLORE_AREA_COUNT,
                  "explore area cache capacity must cover every route");

    void onEnter() override;
    void onExit() override;
    SceneUpdateResult update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;

private:
    enum class Phase : uint8_t {
        SELECT,
        WALKING,
        ENCOUNTER,
        LEVEL_UP,
        EVOLUTION,
        EVOLUTION_CANCELLED,
        LEARN_MOVE,
        MOVE_REPLACED,
        FRIENDSHIP,
        SPECIAL_PROMPT,
        PICKUP,
        RESULT,
        EXITING,
        ENDING,
    };

    Phase phase = Phase::SELECT;
    uint8_t areaCursor = 0;
    float areaAnimCursor = 0.0f;
    // 栖息地轮换：预览展示当前种子活跃池（§7.5，轮播池成员）
    static constexpr uint8_t AREA_PREVIEW_COUNT = 6;
    static constexpr uint8_t AREA_PRELOAD_CAP =
        AREA_PREVIEW_COUNT * Game::EXPLORE_AREA_COUNT;
    static_assert(AREA_PREVIEW_COUNT == ExplorePool::POOL_CAP,
                  "area preview must show the whole active pool");
    uint32_t areaPreviewStartedAt = 0;
    uint32_t areaPreviewNextLoadAt = 0;
    uint32_t areaPreviewVisualCycle = UINT32_MAX;
    uint16_t areaPreviewSpeciesIds[AREA_PREVIEW_COUNT] = {};
    uint16_t areaPreloadSpeciesIds[AREA_PRELOAD_CAP] = {};
    uint8_t areaPreloadSpeciesCount = 0;
    bool areaPreviewLoadPending = false;
    const PokemonSprites::SpriteFrame*
        areaPreviewFrames[AREA_PREVIEW_COUNT] = {};
    ExplorePool::Pool areaPreviewPool{};
    // 趟内活跃池快照：探险开始时按种子重建，趟内遭遇只在池内滚点（§7.8）
    ExplorePool::Pool activePool{};
    Area activeArea = Area::GRASS_PATH;
    uint16_t steps = 0;
    uint8_t adventureBondGain = 0;
    bool adventureBondSettled = false;
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
    bool fleeExitPending = false;
    bool battleExpVisible = false;
    bool expAnimationPending = false;
    bool expAnimationActive = false;
    uint32_t expAnimationFrom = 0;
    uint32_t expAnimationTo = 0;
    uint32_t expAnimationStarted = 0;
    enum class FriendshipStep : uint8_t {
        CONTACT_INTRO,
        CONTACT_CONFIRM,
        CONTACT_ACQUIRED,
        TEAM_CONFIRM,
        TEAM_JOINED,
        FINISHING,
    };
    bool friendshipOfferPending = false;
    bool friendshipConfirmYes = true;
    FriendshipStep friendshipStep = FriendshipStep::CONTACT_INTRO;
    uint8_t friendshipContactIndex = 0xFF;
    uint8_t battleCursor = 0;
    bool battleIsBoss = false;
    bool battleAllowsFriendship = true;
    uint8_t battleFoodBond = 0;
    bool foodThrowActive = false;
    uint8_t foodThrowIndex = 0;
    uint32_t foodThrowStarted = 0;
    uint8_t pendingBattleSwitchSlot = 0xFF;
    uint8_t battlePlayerSlot = 0;
    enum class BattleSwitchStage : uint8_t {
        NONE,
        RETREATING,
        ENTERING,
    };
    BattleSwitchStage battleSwitchStage = BattleSwitchStage::NONE;
    uint32_t battleSwitchStarted = 0;
    bool battleSwitchConsumesTurn = false;
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
    BattleSystem::BattleAiMemory playerAiMemory;
    BattleSystem::BattleAiMemory wildAiMemory;
    uint8_t battleTurnSpecialSlots[2] = {
        BattleSystem::SPECIAL_SLOT_NONE,
        BattleSystem::SPECIAL_SLOT_NONE,
    };
    uint16_t battleHpFrom = 0;
    uint16_t battleHpTo = 0;
    uint32_t battleActionStarted = 0;
    ProgressionUi::MoveLearnState moveLearnState{};
    uint16_t progressionCancelledSpeciesId = 0;
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
    bool routePickupFinalReward = false;
    uint8_t routeGuaranteedEncounterIndex = 0;
    bool routeGuaranteedEncounterPending = false;
    bool expeditionBossScheduled = false;
    uint16_t expeditionBossSpeciesId = 0;
    uint8_t expeditionBossLevel = 0;
    uint16_t expeditionBossExperiencePercent = 100;
    ExploreSpecial::Kind expeditionSpecialKind = ExploreSpecial::Kind::NONE;
    bool specialRelocationHandled = false;
    bool specialChallengeYes = true;
    uint8_t routeBossIndex = 0;
    bool routeBossPending = false;
    static constexpr uint8_t EXPLORE_MENU_ITEM_COUNT = 4;
    bool exploreMenuOpen = false;
    bool exploreSubViewOpen = false;
    uint8_t exploreMenuCursor = 0;
    uint32_t exploreMenuOpenedAt = 0;
    MenuScene exploreSubView;

    void walk();
    void beginAutoWalk();
    bool updateRouteMovement(uint32_t nowMs);
    void finishCompletedWalkStep();
    void finishProgression();
    void resumeWalk();
    void generateMapBlocks();
    void prepareMapRoutes();
    void placeRouteBoss();
    void placeRoutePickup();
    bool collectRoutePickup();
    void recoverTeamForCompletedSteps();
    void advanceMapBlock(uint8_t nextMap);
    int8_t currentDepthLevelOffset(uint8_t spread) const;
    void beginEncounter(const Species& species, uint8_t level, bool boss = false);
    Game::MonsterRuntime& battlePlayerMonster();
    const Game::MonsterRuntime& battlePlayerMonster() const;
    const Species& battlePlayerSpecies() const;
    void beginRouteBossEncounter();
    void promptOrBeginRouteBoss();
    void finishRoamingEncounter();
    void markFirstSpecialVictory();
    void beginDebugEncounter();
    void rollEncounter();
    bool rollRandomEncounter(bool guaranteed, bool bypassGate,
                             bool repelActiveThisStep);
    void resolvePickup(uint8_t pickupId);
    void clearBattleLogs();
    void enqueueBattleLog(const char* text, BattleLogCue cue = BattleLogCue::NONE);
    bool serviceBattleLog(uint32_t nowMs);
    bool battleLogBusy() const;
    bool battleLogPlaybackBusy() const;
    void updateBattleTurn(uint32_t nowMs);
    void updateBattleSwitch(uint32_t nowMs);
    void beginBattleSwitch(uint8_t slot, bool consumesTurn);
    int battleSwitchOffsetX(uint32_t nowMs) const;
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
    void throwFood(uint8_t foodIndex);
    bool updateFoodThrow(uint32_t nowMs);
    void finishFoodThrow();
    void switchBattleMonster();
    void wildCounterattack();
    void finishPlayerFaint();
    void finishBattleVictoryFlow();
    void clearFriendshipFlow();
    void resolveFriendshipOffer();
    void initializeRouteFollowerPosition(bool useTrailPosition);
    void fleeEncounter();
    void resetWalk();
    bool resetRouteSegment();
    void returnToDebugMenu();
    void beginRouteExit();
    void settleAdventureBond();
    void completeExploreReturn(bool fainted);
    void requestExploreExit(bool fainted = false, bool showEndPrompt = true);
    void closeExploreMenu();
    void renderAreaMenu();
    static ExplorePool::Pool buildAreaPool(uint8_t areaIndex);
    static uint8_t collectAreaPoolSpecies(uint16_t* speciesIds,
                                          uint8_t capacity,
                                          uint8_t priorityArea = 0xFF);
    void loadAreaPreview();
    void updateAreaPreviewLoading(uint32_t nowMs);
    void refreshAreaPreviewFrames();
    void snapshotActivePool();
    void renderWalking();
    void renderEncounter();
    void renderFoodThrow();
    void renderFriendshipPrompt();
    void renderSpecialPrompt();
    void renderPickupPrompt();
    void renderResult();
    void renderEndPrompt();
    void renderExploreMenu();
    void renderRouteBoss(int cameraX, int cameraY);
    void renderRoutePickup(int cameraX, int cameraY);
    void drawRouteMonster(const Species& species, float worldX, float worldY,
                          uint8_t walkDirection, bool follower,
                          int cameraX, int cameraY);
    void drawMonsterSprite(const Species& species, int x, int groundY,
                           int maxWidth, int maxHeight, bool back = false,
                           int spriteOffsetX = 0);
    void renderBattleHud();
    void renderCommandBox();
    bool ownsSpecies(uint16_t speciesId) const;
    ExploreSpecial::Kind specialKindForArea(uint8_t area) const;
};
