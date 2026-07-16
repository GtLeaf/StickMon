#pragma once

#include <cstdint>

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
};
