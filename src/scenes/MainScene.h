#pragma once

#include "assets/PokemonMotion.h"
#include "assets/PokemonSprites.h"
#include "core/RoomResource.h"
#include "core/Scene.h"
#include "game/MonsterMind.h"
#include "game/Species.h"

struct PetResult;

class MainScene : public Scene {
public:
    void onEnter() override;
    void onExit() override;
    void onBeforeSave() override;
    void update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;

private:
    enum class AiMode : uint8_t {
        IDLE,
        WANDER,
        TURNING,
        SEEK_FOOD,
        SEEK_BED,
        LEAVING_BED,
        WAKING,
        FEEDING,
        RESTING,
        SCRIPTED_MOVE,
    };

    enum class RoomAction : uint8_t {
        NONE,
        PET_HAPPY,
        PET_CALM,
        PET_WITHDRAW,
        ATTENTION_APPROACH,
        ATTENTION_WAIT,
        LOOK_AROUND,
        CIRCLE,
        DASH,
        STEP_BACK,
        QUIET_GAZE,
        FEED_FINISH,
        WINDOW_APPROACH,
        WINDOW_WAIT,
    };

    enum class HeartEffect : uint8_t {
        NONE,
        ONE,
        TWO,
    };

    enum class PmdAction : uint8_t {
        IDLE,
        WALKING,
        STOPPING,
        SLEEPING,
    };

    enum class PmdDirection : uint8_t {
        FRONT,
        DOWN_LEFT,
        LEFT,
        UP_LEFT,
        BACK,
        UP_RIGHT,
        RIGHT,
        DOWN_RIGHT,
    };

    enum class DoorTransitionMode : uint8_t {
        NONE,
        EXIT_ROUTE,
        EXIT_CROSS,
        EXIT_FADE,
        ENTER_WAIT_FADE,
        ENTER_CROSS,
        FAINT_WAIT_FADE,
    };

    enum class ProgressionModal : uint8_t {
        NONE,
        LEVEL_UP,
        EVOLUTION,
        LEARN_MOVE,
        MOVE_REPLACED,
    };

    struct RenderItem {
        int16_t z;
        void (MainScene::*draw)();
    };

    void updateMonsterAi(uint32_t nowMs, float dtSeconds);
    bool updateRoomAction(uint32_t nowMs);
    void cancelRoomAction(uint32_t nowMs);
    void finishScriptedMovement(uint32_t nowMs);
    bool beginScriptedMove(RoomAction action, float x, float y, uint32_t nowMs);
    bool beginScriptedLeg(float x, float y, uint32_t nowMs);
    void finishRoomAction(uint32_t nowMs);
    void scheduleAttention(uint32_t nowMs, bool initial = false);
    void scheduleSpecialAction(uint32_t nowMs);
    bool ambientActionAllowed() const;
    bool startAttention(uint32_t nowMs);
    bool startAutonomousAction(uint32_t nowMs);
    bool startWindowGaze(uint32_t nowMs);
    bool chooseAttentionPose(float& x, float& y) const;
    bool chooseNearbyPose(float originX, float originY, float minDistance,
                          float maxDistance, float preferredAngle,
                          float angleSpread, float& x, float& y) const;
    bool chooseCirclePose(uint8_t phase, float& x, float& y) const;
    bool chooseWindowGazePose(const RoomResource::BehaviorAnchor& anchor,
                              float& x, float& y) const;
    void startPetReaction(uint32_t nowMs, const PetResult& result);
    void startFeedFinish(uint32_t nowMs);
    void showHearts(HeartEffect effect, uint32_t nowMs, uint16_t durationMs);
    float actionRenderYOffset(uint32_t nowMs) const;
    void beginDoorTransition(uint32_t nowMs);
    void updateDoorTransition(uint32_t nowMs);
    bool prepareDoorAnchors();
    bool chooseDoorInsidePose(float& x, float& y) const;
    bool updateDoorRoute(float dtSeconds);
    bool moveDoorToward(float x, float y, float speed, float dtSeconds, bool enforceWalkArea);
    void updateDebugTiltControl(uint32_t nowMs, float dtSeconds);
    void updateCamera();
    int16_t worldToScreenY(float worldY) const;
    float walkBoundaryOffsetY() const;
    float walkFootprintRadiusX() const;
    float walkFootprintRadiusY() const;
    bool monsterFootprintInsideWalkArea(float x, float y) const;
    bool randomMonsterCenterWalkPoint(float& x, float& y) const;
    bool monsterCanUseBedSleepPose(float x, float y) const;
    bool chooseBedApproachPose(float& x, float& y) const;
    bool chooseBedSleepPose(float& x, float& y) const;
    bool chooseFoodApproachPose(float& x, float& y) const;
    bool monsterNearFood() const;
    bool monsterNearBed() const;
    bool monsterAtBedSleepPose() const;
    bool monsterNeedsBedRest() const;
    void updateMind(uint32_t nowMs);
    void beginMovement(AiMode mode, uint32_t nowMs);
    void beginTurn(AiMode nextMode, PmdDirection direction, uint32_t nowMs);
    void beginWaking(uint32_t nowMs, bool forFood);
    void enterResting(uint32_t nowMs);
    void finishMovement(uint32_t nowMs);
    void setFoodTarget(uint32_t nowMs);
    void setBedTarget(uint32_t nowMs);
    void snapMonsterToBed();
    void enterFeeding(uint32_t nowMs);
    void updateFeeding(uint32_t nowMs);
    bool buildMoveRoute(float goalX, float goalY);
    bool pathSegmentInsideWalkArea(float fromX, float fromY, float toX, float toY) const;
    void clearMoveRoute();
    bool currentWaypoint(float& x, float& y) const;
    void updateStuckWatchdog(uint32_t nowMs, float distanceToWaypoint);
    void restoreViewState(uint32_t nowMs);
    void persistViewState(uint32_t nowMs);
    void updatePmdSpriteState(uint32_t nowMs);
    void chooseAiGoal(uint32_t nowMs);
    bool startWander(uint32_t nowMs);
    bool openPendingProgression();
    void drawBackground();
    void drawFloor();
    void drawFood();
    void drawShadow();
    void drawMonster();
    bool drawPmdMonster(int x, int y);
    void drawStateEffect();
    void drawNightOverlay();
    void drawWalkBoundary();
    void drawHud();
    void drawToast();
    void drawProgressionPopup();
    void sortAndDraw(RenderItem* items, uint8_t count);
    PmdDirection pmdDirectionForVelocity(float vx, float vy) const;
    uint16_t pmdDirectionFrameIndex() const;
    bool pmdDirectionFlipX() const;
    PokemonSprites::SpriteKind pmdSpriteKind() const;
    const PokemonSprites::SpriteFrame* currentMonsterFrame() const;
    const PokemonSprites::SpriteFrame* movementBoundsFrame() const;

    const Species* active = nullptr;
    float monsterX = 98.0f;
    float monsterY = 91.0f;
    float targetX = 98.0f;
    float targetY = 91.0f;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    float cameraY = 0.0f;
    AiMode aiMode = AiMode::IDLE;
    uint32_t nextAiDecisionMs = 0;
    uint32_t nextMindUpdateMs = 0;
    uint32_t stateUntilMs = 0;
    AiMode turnNextMode = AiMode::IDLE;
    PmdDirection turnTargetDirection = PmdDirection::FRONT;
    bool wakingForFood = false;
    bool facingRight = true;
    PmdAction pmdAction = PmdAction::IDLE;
    PmdDirection pmdDirection = PmdDirection::FRONT;
    uint8_t pmdFrame = 0;
    uint8_t pmdMotionPhase = PokemonMotion::SLITHER_IDLE_PHASE_INDEX;
    int8_t pmdRenderOffsetX = 0;
    int8_t pmdRenderOffsetY = 0;
    uint16_t pmdMotionCycleMs = PokemonMotion::SLITHER_AMBIENT_MAX_CYCLE_MS;
    bool pmdLongMove = false;
    uint32_t pmdFrameStartedMs = 0;
    uint32_t toastUntil = 0;
    const char* toast = nullptr;
    uint32_t feedingBiteMs = 0;
    uint32_t feedingUntilMs = 0;
    uint32_t postFeedAwakeUntilMs = 0;
    bool feedingConsumed = false;
    bool feedingHadTastyBite = false;
    bool feedingBecameFull = false;
    uint8_t feedingMoodAfter = 0;
    uint32_t hungerAnimStartedMs = 0;
    uint32_t hungerAnimUntilMs = 0;
    uint8_t hungerAnimFrom = 0;
    uint8_t hungerAnimTo = 0;
    bool faintRestActive = false;
    ProgressionModal progressionModal = ProgressionModal::NONE;
    uint8_t progressionLearnCursor = 0;
    DoorTransitionMode doorTransition = DoorTransitionMode::NONE;
    float doorInsideX = 0.0f;
    float doorInsideY = 0.0f;
    float doorOutsideX = 0.0f;
    float doorOutsideY = 0.0f;
    uint32_t doorLastUpdateMs = 0;
    uint32_t doorPhaseStartedMs = 0;
    bool doorRouteEnteringWalkArea = false;
    MonsterMind mind;
    MonsterBehaviorProfile behaviorProfile;
    static constexpr uint8_t MOVE_ROUTE_CAP = 20;
    float moveRouteX[MOVE_ROUTE_CAP] = {};
    float moveRouteY[MOVE_ROUTE_CAP] = {};
    uint8_t moveRouteCount = 0;
    uint8_t moveRouteIndex = 0;
    float lastWaypointDistance = 0.0f;
    uint32_t lastMoveProgressMs = 0;
    uint8_t stuckRecoveryCount = 0;
    RoomAction roomAction = RoomAction::NONE;
    uint8_t roomActionPhase = 0;
    uint32_t roomActionStartedMs = 0;
    uint32_t roomActionUntilMs = 0;
    float roomActionOriginX = 0.0f;
    float roomActionOriginY = 0.0f;
    float roomActionRadius = 0.0f;
    PmdDirection roomActionBaseDirection = PmdDirection::FRONT;
    PmdDirection windowGazeDirection = PmdDirection::BACK;
    HeartEffect heartEffect = HeartEffect::NONE;
    uint32_t heartEffectUntilMs = 0;
    uint32_t nextAttentionMs = 0;
    uint32_t nextSpecialActionMs = 0;
    uint32_t comboStartMs = 0;
    bool comboSaved = false;
};
