#pragma once

#include <cstdint>

struct SecondarySceneViewState {
    bool valid = false;
    uint16_t speciesId = 0;
    uint32_t ivPacked = 0;
    uint32_t metAt = 0;
    uint8_t nature = 0;
    uint8_t metArea = 0;
    uint8_t origin = 0;
    float x = 0.0f;
    float y = 0.0f;
    float targetX = 0.0f;
    float targetY = 0.0f;
    float sleepX = 0.0f;
    float sleepY = 0.0f;
    uint8_t state = 0;
    uint8_t direction = 0;
    uint8_t frameIndex = 0;
    bool facingRight = true;
    bool sleepSpotValid = false;
    uint32_t stateRemainingMs = 0;
    uint32_t foodRetryRemainingMs = 0;
};

struct MainSceneViewState {
    bool valid = false;
    uint16_t speciesId = 0;
    float monsterX = 98.0f;
    float monsterY = 91.0f;
    float targetX = 98.0f;
    float targetY = 91.0f;
    uint8_t aiMode = 0;
    uint8_t pmdAction = 0;
    uint8_t pmdDirection = 0;
    uint8_t pmdFrame = 0;
    bool facingRight = true;
    bool faintRestActive = false;
    uint32_t nextDecisionRemainingMs = 0;
    uint32_t postFeedAwakeRemainingMs = 0;
    SecondarySceneViewState secondary;
};
