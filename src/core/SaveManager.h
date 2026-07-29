#pragma once

#include <cstdint>
#include "core/MainSceneViewState.h"
#include "game/EncounterHistory.h"
#include "game/GameState.h"

class SaveManager {
public:
    bool begin();
    bool load(Game::GameState& state,
              MainSceneViewState& viewState,
              bool* normalized = nullptr);
    bool saveSnapshot(const Game::GameState& state,
                      const MainSceneViewState& viewState);
    bool loadEncounterHistory(Game::EncounterHistory& history,
                              bool* normalized = nullptr);
    bool saveEncounterHistory(const Game::EncounterHistory& history);
    void reset(Game::GameState& state);
    bool clearAll();
    bool loadHatchProgress(Game::HatchProgress& progress);
    bool saveHatchProgress(const Game::HatchProgress& progress);
    void clearHatchProgress();

private:
    static uint16_t checksum(const Game::GameState& state);
    static uint16_t checksum(const Game::HatchProgress& progress);
};
