#pragma once

#include "assets/PokemonMotion.h"
#include "assets/PokemonSprites.h"
#include "core/BuildConfig.h"
#include "core/MainSceneViewState.h"
#include "core/ProgressionUi.h"
#include "core/RoomMovementArea.h"
#include "core/RoomResource.h"
#include "core/Scene.h"
#include "game/MonsterMind.h"
#include "game/HomeActor.h"
#include "game/HomeChase.h"
#include "game/HomeCoordinator.h"
#include "game/Species.h"

struct PetResult;

class MainScene : public Scene {
public:
    void onEnter() override;
    void onExit() override;
    void onBeforeSave() override;
    SceneUpdateResult update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;

private:
    // Legacy values are retained only for save compatibility and sprite
    // presentation. Home::Actor::task is the authoritative gameplay state.
    using AiMode = Home::Task;

    enum class RoomAction : uint8_t {
        NONE,
        PET_HAPPY,
        PET_CALM,
        PET_WITHDRAW,
        ATTENTION_APPROACH,
        ATTENTION_WAIT,
        VOICE_CALL_APPROACH,
        VOICE_CALL_WAIT,
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
        EXIT_SECOND_ROUTE,
        EXIT_SECOND_CROSS,
        EXIT_FADE,
        ENTER_WAIT_FADE,
        ENTER_CROSS,
        ENTER_CLEAR_ROUTE,
        ENTER_SECOND_CROSS,
        FAINT_WAIT_FADE,
    };

    enum class DoorActor : uint8_t {
        MAIN,
        VISITOR,
    };

    enum class ProgressionModal : uint8_t {
        NONE,
        LEVEL_UP,
        EVOLUTION,
        EVOLUTION_CANCELLED,
        LEARN_MOVE,
        MOVE_REPLACED,
    };

    enum class ContactDialog : uint8_t {
        NONE,
        KNOCK,
        PLAY_ARRIVAL,
        GIFT_ARRIVAL,
        EXPLORE_INVITE,
        HAPPY_RETURN,
        BYE_RETURN,
        HAPPY_VISIT,
        BYE_VISIT,
    };

    enum class ContactGuestMotion : uint8_t {
        NONE,
        TEAM_ENTER_CROSS,
        HOST_TO_DOOR,
        HOST_OPEN_DOOR,
        HOST_CLEAR_DOOR,
        ENTER_CROSS,
        MEETING_RETRY,
        HOST_TO_MEET,
        GUEST_TO_MEET,
        ARRIVAL_TALK,
        EXIT_ROUTE,
        EXIT_DIRECT,
        EXIT_CROSS,
    };

    enum class PairInteraction : uint8_t {
        NONE,
        TALK,
        CHASE,
    };

    enum class PairInteractionPhase : uint8_t {
        NONE,
        APPROACH,
        INVITE,
        CONVERSATION,
        ACTIVE,
        SETTLE,
        CELEBRATE,
    };

    enum class PairMovementProfile : uint8_t {
        STANDARD,
        CHASE,
    };

    struct RenderItem {
        int16_t z;
        void (MainScene::*draw)();
    };

    using VisitorState = Home::Task;

    struct VisitorActor : Home::Actor {
        uint32_t frameStartedMs = 0;
        uint8_t frameIndex = 0;
        PokemonSprites::WalkDirection direction = PokemonSprites::WalkDirection::DOWN;
        bool facingRight = true;
        int8_t renderOffsetX = 0;
        int8_t renderOffsetY = 0;
        uint16_t motionCycleMs = 0;
    };

    struct FoodRouteFailure {
        bool valid = false;
        uint8_t bowlFood = 0;
        uint8_t bowlCount = 0;
        uint8_t bowlBites = 0;
        int16_t visitorCellX = 0;
        int16_t visitorCellY = 0;
    };

    struct ActorGeometry {
        float groundOffsetY = 18.0f;
        RoomMovementArea::Footprint footprint = {9.0f, 5.0f};
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
    bool startVoiceCallReaction(uint32_t nowMs);
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
    bool chooseDoorInsidePoseForGeometry(
        const ActorGeometry& geometry, float& x, float& y) const;
    bool chooseDoorWaitPose(float actorX, float actorY,
                            const ActorGeometry& geometry,
                            float& x, float& y) const;
    bool updateDoorRoute(float dtSeconds);
    bool updateVisitorDoorRoute(float dtSeconds);
    bool moveDoorToward(float x, float y, float speed, float dtSeconds,
                        bool enforceWalkArea, bool avoidVisitor = false,
                        const ActorGeometry* movementGeometry = nullptr);
    bool moveVisitorDoorToward(float x, float y, float speed, float dtSeconds,
                               bool enforceWalkArea, bool avoidMain = false,
                               const ActorGeometry* movementGeometry = nullptr);
    bool updateDoorWaitingActor(float dtSeconds);
    void beginSecondDoorExit(uint32_t nowMs);
    void finishDoorDeparture();
    bool doorStepKeepsSpacing(float currentX, float currentY,
                              float nextX, float nextY,
                              float otherX, float otherY,
                              bool movingVisitor) const;
    float doorActorMinSeparation() const;
    float pairConversationSeparation() const;
    void updateDebugTiltControl(uint32_t nowMs, float dtSeconds);
    void updateCamera();
    int16_t worldToScreenY(float worldY) const;
    float walkBoundaryOffsetY() const;
    float walkFootprintRadiusX() const;
    float walkFootprintRadiusY() const;
    bool monsterFootprintInsideWalkArea(float x, float y) const;
    bool randomMonsterCenterWalkPoint(float& x, float& y) const;
    ActorGeometry geometryForSpecies(uint16_t speciesId) const;
    ActorGeometry mainGeometry() const;
    ActorGeometry visitorGeometry() const;
    ActorGeometry pairActorGeometry(
        bool mainActor, PairMovementProfile profile) const;
    bool actorFootprintInsideWalkArea(float x, float y,
                                      const ActorGeometry& geometry) const;
    bool randomActorCenterWalkPoint(const ActorGeometry& geometry,
                                    float& x, float& y) const;
    bool monsterCanUseBedSleepPose(float x, float y) const;
    bool chooseBedApproachPose(float& x, float& y) const;
    bool chooseBedSleepPose(float& x, float& y) const;
    bool buildFoodApproachRoute(bool movingMain, float& x, float& y);
    bool monsterNearFood() const;
    bool monsterNearBed() const;
    bool monsterAtBedSleepPose() const;
    bool monsterNeedsBedRest() const;
    void updateMind(uint32_t nowMs);
    void beginMovement(AiMode mode, uint32_t nowMs);
    void beginPreparedMovement(AiMode mode, uint32_t nowMs);
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
    bool buildVisitorMoveRoute(float goalX, float goalY);
    bool buildMoveRouteFrom(float startX, float startY, float goalX, float goalY,
                            float* routeX, float* routeY,
                            uint8_t& routeCount, uint8_t& routeIndex,
                            bool allowOutsideStart,
                            const ActorGeometry& geometry,
                            bool avoidOther, float otherX, float otherY,
                            float otherGroundOffsetY);
    bool pathSegmentInsideWalkArea(float fromX, float fromY, float toX, float toY,
                                   bool allowOutsideStart = false) const;
    bool actorPathSegmentInsideWalkArea(float fromX, float fromY,
                                        float toX, float toY,
                                        const ActorGeometry& geometry,
                                        bool allowOutsideStart = false) const;
    bool routeSegmentKeepsSpacing(float fromX, float fromY,
                                  float toX, float toY,
                                  float otherX, float otherY,
                                  float minSeparation) const;
    bool mainTargetKeepsVisitorSpacing(float targetX, float targetY) const;
    bool actorTargetsKeepSpacing(float targetX, float targetY,
                                 const ActorGeometry& movingGeometry,
                                 float otherX, float otherY,
                                 const ActorGeometry& otherGeometry,
                                 float minSeparation = 0.0f) const;
    void clearMoveRoute();
    void clearVisitorMoveRoute();
    void deactivateVisitor();
    bool currentWaypoint(float& x, float& y) const;
    bool currentVisitorWaypoint(float& x, float& y) const;
    void updateStuckWatchdog(uint32_t nowMs, float distanceToWaypoint);
    void abortMovement(uint32_t nowMs, uint32_t retryDelayMs);
    void handleVisitorMoveBlocked(uint32_t nowMs, bool movingToSleep);
    void restoreViewState(uint32_t nowMs);
    void persistViewState(uint32_t nowMs);
    bool restoreVisitorViewState(const SecondarySceneViewState& saved,
                                 const Game::MonsterRuntime& monster,
                                 uint32_t nowMs);
    void persistVisitorViewState(SecondarySceneViewState& saved,
                                 const VisitorActor& actor,
                                 const Game::MonsterRuntime& monster,
                                 uint32_t nowMs) const;
    void updatePmdSpriteState(uint32_t nowMs);
    void chooseAiGoal(uint32_t nowMs);
    bool startWander(uint32_t nowMs);
    bool openPendingProgression();
    void schedulePairInteraction(uint32_t nowMs, bool immediate = false);
    bool pairInteractionAllowed() const;
    bool startPairInteraction(uint32_t nowMs);
    bool updatePairInteraction(uint32_t nowMs, float dtSeconds);
    void beginPairActive(uint32_t nowMs);
    void beginPairSettle(uint32_t nowMs);
    void completePairSettle(uint32_t nowMs);
    void beginPairCelebrate(uint32_t nowMs);
    void finishPairInteraction(uint32_t nowMs, bool reward);
    void cancelPairInteraction(uint32_t nowMs);
    bool choosePairApproachPose(bool moverMain, float& x, float& y) const;
    bool buildPairActorRoute(bool mainActor, float x, float y,
                             bool avoidOther = true,
                             bool allowOutsideStart = false,
                             PairMovementProfile profile =
                                 PairMovementProfile::STANDARD);
    bool buildPairChasePlan(uint32_t nowMs);
    void degradePairChaseToTalk(uint32_t nowMs, const char* reason);
    bool restorePairActorToStandardArea(bool mainActor,
                                        bool requireSpacing = false);
    bool updatePairActorRoute(bool mainActor, float speed, float dtSeconds,
                              uint32_t nowMs, bool keepSpacing,
                              bool preserveEndMotion = false,
                              PairMovementProfile profile =
                                  PairMovementProfile::STANDARD);
    void facePairActors();
    float pairConversationHopOffset(bool mainActor,
                                    uint32_t nowMs) const;
    void updateContactVisit(uint32_t nowMs, float dtSeconds);
    void beginContactGuestEntry(uint32_t nowMs);
    bool beginContactHostClearDoor(uint32_t nowMs);
    bool beginContactMeetingArrangement(uint32_t nowMs);
    bool beginContactGuestApproach(uint32_t nowMs);
    void deferContactMeeting(uint32_t nowMs);
    void beginContactArrivalConversation(uint32_t nowMs);
    void finishContactArrivalConversation(uint32_t nowMs);
    void showContactArrivalDialog();
    void beginTeamMemberEntry(uint32_t nowMs);
    void beginContactGuestExit(uint32_t nowMs);
    bool handleContactDialogButton(const ButtonEvent& event);
    void drawContactDialog();
    void drawTutorial();
    bool visitorHostActive() const;
    bool visitorCanUseDoor() const;
    bool teamMemberCanEatFromBowl(uint8_t teamSlot) const;
    bool visitorCanSeekFood() const;
    int8_t preferredBowlEater() const;
    bool claimBowl(uint8_t teamSlot);
    void releaseBowl(uint8_t teamSlot);
    bool startMainFoodYield(uint32_t nowMs);
    bool startVisitorFoodSeek(uint32_t nowMs);
    bool startVisitorFoodYield(uint32_t nowMs);
    bool visitorFoodRouteFailureStillValid() const;
    void clearVisitorFoodRouteFailure();
    void rememberVisitorFoodRouteFailure();
    void enterVisitorFeeding(uint32_t nowMs);
    void updateVisitorFoodSeek(uint32_t nowMs, float dtSeconds);
    void updateVisitorFeeding(uint32_t nowMs);
    void restFaintedVisitor(uint32_t nowMs);
    void spawnVisitor(uint32_t nowMs, bool dropIn);
    bool pickVisitorPoint(float& x, float& y) const;
    float visitorSleepMinDistance() const;
    bool visitorSleepSpotUsableWithDistance(float x, float y,
                                            float minDistance) const;
    bool visitorSleepSpotUsable(float x, float y) const;
    bool pickVisitorSleepSpot(float& x, float& y,
                              bool avoidBedRoute = false) const;
    bool visitorPointBlocksBedRoute(float x, float y) const;
    bool startVisitorBedYield(uint32_t nowMs);
    void finishVisitorBedYield(uint32_t nowMs, bool reachedTarget);
    void logVisitorSleepEvent(const char* event, uint32_t nowMs,
                              const Game::MonsterRuntime& mon) const;
    void updateVisitor(uint32_t nowMs, float dtSeconds);
    void advanceVisitorFrames(uint32_t nowMs, bool walking);
    MonsterBehaviorProfile visitorBehaviorProfile() const;
    uint32_t visitorIdleDelayMs(const MonsterBehaviorProfile& profile) const;
    float visitorMoveSpeed(const MonsterBehaviorProfile& profile,
                           bool purposeful) const;
    const PokemonSprites::SpriteFrame* visitorCurrentFrame(bool& flipX) const;
    void drawVisitorShadow();
    void drawVisitor();
    void drawVisitorSleepZz(int screenX, int spriteTopY) const;
    void drawBackground();
    void drawFloor();
    void drawFood();
    void drawShadow();
    void drawMonster();
    bool drawPmdMonster(int x, int y);
    void drawStateEffect();
    void drawNightOverlay();
#if STICKMON_ENABLE_DEBUG_FEATURES
    void drawWalkBoundary();
#endif
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
    float cameraY = 0.0f;
    PmdDirection turnTargetDirection = PmdDirection::FRONT;
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
    bool feedingConsumed = false;
    bool feedingHadTastyBite = false;
    bool feedingHadDislikedBite = false;
    bool feedingBecameFull = false;
    uint8_t feedingMoodAfter = 0;
    uint32_t visitorFeedingBiteMs = 0;
    uint32_t visitorFeedingUntilMs = 0;
    FoodRouteFailure visitorFoodRouteFailure;
    bool mainYieldingForVisitorFood = false;
    bool visitorBedYieldHandled = false;
    uint32_t hungerAnimStartedMs = 0;
    uint32_t hungerAnimUntilMs = 0;
    uint8_t hungerAnimFrom = 0;
    uint8_t hungerAnimTo = 0;
    ProgressionModal progressionModal = ProgressionModal::NONE;
    ContactDialog contactDialog = ContactDialog::NONE;
    ContactGuestMotion contactGuestMotion = ContactGuestMotion::NONE;
    uint32_t contactGuestMotionStartedMs = 0;
    uint32_t contactNextRouteAttemptMs = 0;
    bool contactMeetingLayoutFailureLogged = false;
    bool contactGuestMeetingFailureLogged = false;
    float contactGuestMeetX = 0.0f;
    float contactGuestMeetY = 0.0f;
    bool contactMeetingPoseReady = false;
    bool contactDialogYes = true;
    PairInteraction pairInteraction = PairInteraction::NONE;
    PairInteractionPhase pairInteractionPhase =
        PairInteractionPhase::NONE;
    bool pairLeaderMain = true;
    bool pairForcedPlay = false;
    bool pairForcedChase = false;
    uint8_t pairLegsRemaining = 0;
    uint32_t nextPairInteractionMs = 0;
    uint32_t pairPhaseStartedMs = 0;
    uint32_t pairPhaseUntilMs = 0;
    uint32_t pairInteractionUntilMs = 0;
    uint32_t pairFollowerDelayUntilMs = 0;
    uint16_t progressionCancelledSpeciesId = 0;
    ProgressionUi::MoveLearnState progressionMoveLearn{};
    DoorTransitionMode doorTransition = DoorTransitionMode::NONE;
    float doorInsideX = 0.0f;
    float doorInsideY = 0.0f;
    float doorOutsideX = 0.0f;
    float doorOutsideY = 0.0f;
    float visitorDoorInsideX = 0.0f;
    float visitorDoorInsideY = 0.0f;
    float visitorDoorOutsideY = 0.0f;
    uint32_t doorLastUpdateMs = 0;
    uint32_t doorPhaseStartedMs = 0;
    uint32_t doorLastProgressMs = 0;
    bool doorRouteEnteringWalkArea = false;
    bool visitorDoorRouteEnteringWalkArea = false;
    bool doorDepartureHasVisitor = false;
    bool doorWaitingActorReady = false;
    bool doorMainHidden = false;
    bool doorVisitorHidden = false;
    DoorActor doorFirstActor = DoorActor::MAIN;
    float doorWaitX = 0.0f;
    float doorWaitY = 0.0f;
    VisitorActor visitorBeforeDoorDeparture;
    bool visitorDepartureSnapshotValid = false;
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
#if STICKMON_ENABLE_DEBUG_FEATURES
    uint32_t comboStartMs = 0;
    bool comboSaved = false;
#endif
    VisitorActor visitor;
    Home::Actor mainActor;
    Home::Coordinator homeCoordinator;
};
