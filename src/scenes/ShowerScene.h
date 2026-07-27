#pragma once

#include "core/Scene.h"

enum class BathRewardStage : uint8_t;

class ShowerScene : public Scene {
public:
    void onEnter() override;
    void update(uint32_t nowMs, float dtSeconds) override;
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

    static constexpr uint8_t MENU_COUNT = 4;
    static constexpr uint8_t FOAM_CAP = 12;
    static constexpr uint8_t WATER_CAP = 64;

    Mode mode = Mode::MENU;
    uint8_t menuCursor = 0;
    float menuAnimCursor = 0.0f;
    uint8_t soapCursor = 0;
    Foam foam[FOAM_CAP] = {};
    WaterDrop water[WATER_CAP] = {};
    uint8_t foamSpawnCursor = 0;
    int8_t lastSoapTiltSign = 0;
    float soapX = 88.0f;
    float soapY = 68.0f;
    float brushX = 88.0f;
    float brushY = 68.0f;
    float previousBrushX = 88.0f;
    float previousBrushY = 68.0f;
    float waterSpawnCarry = 0.0f;
    uint32_t lastSoapSwingMs = 0;
    uint32_t lastBrushRubMs = 0;
    uint32_t lastFoamGrowthMs = 0;
    uint32_t lastRinseFoamMs = 0;
    uint32_t modeStartedMs = 0;
    uint32_t toastUntilMs = 0;
    const char* toast = nullptr;
    char toastBuffer[24] = {};
    float atmosphereAlpha = 0.0f;
    bool soapUsed = false;
    bool atmosphereTarget = false;
    bool soapRewarded = false;
    bool brushRewarded = false;
    bool rinseRewarded = false;
    bool exitConfirmYes = false;

    void enterMode(Mode next, uint32_t nowMs);
    void resetBathSession();
    void requestExit(uint32_t nowMs);
    void chooseSoap();
    void beginSoap();
    void beginRinse(uint32_t nowMs);
    void updateSoaping(uint32_t nowMs, float dtSeconds, float ax, float ay);
    void updateBrushing(uint32_t nowMs, float dtSeconds, float ax, float ay);
    void updateRinsing(uint32_t nowMs, float dtSeconds);
    void updateAtmosphere(float dtSeconds);
    void spawnFoam();
    void rubFoamAt(float x, float y, uint32_t nowMs);
    void rinseAllFoamOneStage();
    void spawnWaterDrop();
    bool anyFoam() const;
    bool hasFoamStage(uint8_t stage, const Foam* except = nullptr) const;
    int8_t nextOwnedSoap(int8_t from, int8_t direction) const;
    void awardBathStage(BathRewardStage stage);
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
    void drawToast();
};
