#pragma once

#include "assets/PokemonSprites.h"
#include "core/Scene.h"
#include "game/MonsterMind.h"
#include "game/Species.h"

class MainScene : public Scene {
public:
    void onEnter() override;
    void onExit() override;
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

    struct RenderItem {
        int16_t z;
        void (MainScene::*draw)();
    };

    void updateMonsterAi(uint32_t nowMs, float dtSeconds);
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
    bool pmdLongMove = false;
    uint32_t pmdFrameStartedMs = 0;
    uint32_t toastUntil = 0;
    const char* toast = nullptr;
    char toastBuffer[32] = {};
    uint32_t feedingBiteMs = 0;
    uint32_t feedingUntilMs = 0;
    uint32_t postFeedAwakeUntilMs = 0;
    bool feedingConsumed = false;
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
    uint32_t comboStartMs = 0;
    bool comboSaved = false;
};
