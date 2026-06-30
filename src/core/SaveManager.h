#pragma once

#include <cstdint>
#include "game/GameState.h"

class SaveManager {
public:
    bool begin();
    bool load(Game::GameState& state);
    bool save(const Game::GameState& state);
    void reset(Game::GameState& state);
    bool loadClock(uint32_t& gameMinutesTotal);
    bool saveClock(uint32_t gameMinutesTotal);
    bool loadHatchProgress(Game::HatchProgress& progress);
    bool saveHatchProgress(const Game::HatchProgress& progress);
    void clearHatchProgress();

private:
    static uint16_t checksum(const Game::GameState& state);
    static uint16_t checksum(const Game::HatchProgress& progress);
};
