#pragma once

#include "core/Scene.h"

enum class BathRewardStage : uint8_t;

class ShowerScene : public Scene {
public:
    void onEnter() override;
    SceneUpdateResult update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;

private:
    enum class Mode : uint8_t {
        MENU,
        SOAP_SELECT,
        SOAPING,
        BRUSHING,
        RINSING,
        COMPLETE,
        EXIT_CONFIRM,
        INCOMPLETE,
    };

    struct Foam {
        float x = 0.0f;
        float y = 0.0f;
        float restX = 0.0f;
        float restYOffset = 0.0f;
        uint8_t stage = 0;
        uint8_t brushProgress = 0;
        uint32_t bornMs = 0;
        bool active = false;
    };

    struct WaterDrop {
        float x = 0.0f;
        float y = 0.0f;
        float vx = 0.0f;
        float vy = 0.0f;
        bool active = false;
    };

    struct ExpFloat {
        float x = 0.0f;
        float y = 0.0f;
        uint32_t bornMs = 0;
        char text[12] = {};
        bool heart = false;
        bool active = false;
    };

    static constexpr uint8_t MENU_COUNT = 4;
    static constexpr uint8_t SMALL_FOAM_CAP = 8;
    static constexpr uint8_t LEVEL_3_FOAM_CAP = 4;
    static constexpr uint8_t LEVEL_4_FOAM_CAP = 2;
    static constexpr uint8_t FOAM_CAP = SMALL_FOAM_CAP;
    static constexpr uint8_t WATER_CAP = 64;
    static constexpr uint8_t EXP_FLOAT_CAP = 4;
    static_assert(SMALL_FOAM_CAP == LEVEL_3_FOAM_CAP * 2 &&
                  LEVEL_3_FOAM_CAP == LEVEL_4_FOAM_CAP * 2,
                  "shower foam counts must follow the 8 -> 4 -> 2 chain");

    Mode mode = Mode::MENU;
    uint8_t menuCursor = 0;
    float menuAnimCursor = 0.0f;
    uint8_t soapCursor = 0;
    Foam foam[FOAM_CAP] = {};
    WaterDrop water[WATER_CAP] = {};
    uint8_t foamSpawnCursor = 0;
    float gravityX = 0.0f;
    float gravityY = 0.0f;
    float gravityZ = 0.0f;
    bool accelSeeded = false;
    float soapX = 88.0f;
    float soapY = 68.0f;
    float brushX = 88.0f;
    float brushY = 68.0f;
    float waterSpawnCarry = 0.0f;
    uint32_t lastSoapSwingMs = 0;
    uint32_t lastBrushRubMs = 0;
    uint32_t lastFoamGrowthMs = 0;
    uint32_t lastRinseFoamMs = 0;
    uint32_t modeStartedMs = 0;
    uint32_t toastUntilMs = 0;
    const char* toast = nullptr;
    ExpFloat expFloats[EXP_FLOAT_CAP] = {};
    uint32_t hopStartMs = 0;
    float hopHeightPx = 0.0f;
    uint32_t wiggleStartMs = 0;
    uint32_t turnUntilMs = 0;
    bool turnFlip = false;
    uint8_t foamRestSlot = 0;
    uint32_t lastRubReactionMs = 0;
    uint32_t lastRinseShakeMs = 0;
    float atmosphereAlpha = 0.0f;
    bool soapUsed = false;
    bool atmosphereTarget = false;
    bool soapRewarded = false;
    bool brushRewarded = false;
    bool rinseRewarded = false;
    uint8_t completionHearts = 0;
    bool exitConfirmYes = false;

    void enterMode(Mode next, uint32_t nowMs);
    void resetBathSession();
    void requestExit(uint32_t nowMs);
    void chooseSoap();
    void beginSoap();
    void beginRinse(uint32_t nowMs);
    void updateSoaping(uint32_t nowMs, float dtSeconds, float dynX, float dynY,
                       float shake);
    void updateBrushing(uint32_t nowMs, float dtSeconds, float dynX, float dynY,
                        float shake);
    void updateRinsing(uint32_t nowMs, float dtSeconds);
    void updateAtmosphere(float dtSeconds);
    void spawnFoam();
    void rubFoamAt(float x, float y, uint32_t nowMs);
    bool tryMergeFoam(Foam& source, uint32_t nowMs);
    Foam* findMergePartner(const Foam& source);
    void rinseAllFoamOneStage();
    void spawnWaterDrop();
    void spawnSplashDrop(float x, float y, float vx, float vy);
    bool anyFoam() const;
    uint8_t foamLevelTotal() const;
    void checkAtmosphereThreshold(uint32_t nowMs);
    int monsterBottomY() const;
    float foamRestY(uint8_t stage) const;
    int8_t nextOwnedSoap(int8_t from, int8_t direction) const;
    void awardBathStage(BathRewardStage stage);
    void spawnExpFloat(uint8_t amount, uint32_t nowMs);
    void spawnHeartFloat(uint32_t nowMs);
    void startHop(float heightPx, uint32_t nowMs);
    void startWiggle(uint32_t nowMs);
    void startTurn(uint32_t nowMs);
    void drawAtmosphere() const;
    void drawMonster() const;
    void drawFoam() const;
    void drawWater() const;
    void drawTool() const;
    void drawMenu();
    void drawSoapPicker() const;
    void drawHearts() const;
    void drawExitConfirm() const;
    void drawIncomplete() const;
    void drawExpFloats();
    void drawToast();
};
