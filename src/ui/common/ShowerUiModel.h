#pragma once

#include <cstdint>

#include "game/BathInteraction.h"
#include "game/GameState.h"
#include "presentation/ShowerTiming.h"

namespace Ui {

enum class ShowerMode : uint8_t {
    MENU, SOAP_SELECT, SOAPING, BRUSHING, RINSING, COMPLETE,
    EXIT_CONFIRM, INCOMPLETE,
};

inline constexpr uint8_t SHOWER_MENU_COUNT = 4;
inline constexpr uint8_t SHOWER_WATER_CAP = 64;
inline constexpr uint8_t SHOWER_EXP_FLOAT_CAP = 4;

struct ShowerWaterDrop {
    float x = 0.0f, y = 0.0f;
    float vx = 0.0f, vy = 0.0f;
    bool active = false;
};

struct ShowerExpFloat {
    float x = 0.0f, y = 0.0f;
    uint32_t bornMs = 0;
    char text[12] = {};
    bool heart = false, active = false;
};

struct ShowerUiState {
    uint8_t menuCursor = 0;
    float menuAnimCursor = 0.0f;
    uint8_t soapCursor = 0;
    bool exitConfirmYes = false;
    ShowerWaterDrop water[SHOWER_WATER_CAP] = {};
    ShowerExpFloat expFloats[SHOWER_EXP_FLOAT_CAP] = {};
    float gravityX = 0.0f, gravityY = 0.0f, gravityZ = 0.0f;
    bool accelSeeded = false;
    float soapX = 88.0f, soapY = 68.0f;
    float brushX = 88.0f, brushY = 68.0f;
    float waterSpawnCarry = 0.0f;
    uint32_t lastRinseShakeMs = 0, lastRubReactionMs = 0;
    const char* toast = nullptr;
    uint32_t toastUntilMs = 0;
    uint32_t hopStartMs = 0;
    float hopHeightPx = 0.0f;
    uint32_t wiggleStartMs = 0, turnUntilMs = 0;
    bool turnFlip = false;
    float atmosphereAlpha = 0.0f;
};

// The coordinator supplies rules and outcomes; drawing only reads this frame.
struct ShowerFrame {
    uint32_t nowMs = 0, modeStartedMs = 0;
    ShowerMode mode = ShowerMode::MENU;
    const ShowerUiState* visuals = nullptr;
    const Game::BathInteraction::State* bath = nullptr;
    uint16_t speciesId = 0, soapCount = 0;
    uint16_t soapStocks[Game::SOAP_VARIANT_COUNT] = {};
    uint8_t completionHearts = 0;
    const char* completionMessage = nullptr;
};

struct ShowerLayout {
    int width, height;
    int dx, dy, centerX, centerY;
    int menuCenterX, menuCenterY, menuSpacing, menuIndicatorX, menuGradientX;
    int waterRightX, waterFloorY, waterBounceY, waterBounceLeftX, waterBounceRightX;
    int waterSpawnX;
    int toolLeftX, toolRightX, toolTopY, toolBottomY;
    float expFloatBaseX, heartFloatX, heartFloatY;

    constexpr ShowerLayout(int width, int height)
        : width(width), height(height), dx((width - 240) / 3), dy((height - 135) / 2),
          centerX(99 + dx), centerY(78 + dy),
          menuCenterX(width - 30), menuCenterY(height / 2), menuSpacing(46),
          menuIndicatorX(width - 64), menuGradientX(menuIndicatorX),
          waterRightX(176 + dx), waterFloorY(121 + dy), waterBounceY(120 + dy),
          waterBounceLeftX(25 + dx), waterBounceRightX(155 + dx),
          waterSpawnX(22 + dx),
          toolLeftX(44 + dx), toolRightX(161 + dx),
          toolTopY(33 + dy), toolBottomY(122 + dy),
          expFloatBaseX(138.0f + dx), heartFloatX(centerX - 3.0f),
          heartFloatY(46.0f + dy) {}

    static float spriteScale(uint8_t spriteWidth, uint8_t spriteHeight);
    int monsterBottomY(uint16_t speciesId) const;
    Game::BathInteraction::Layout bathLayout(uint16_t speciesId) const;
};

}  // namespace Ui
