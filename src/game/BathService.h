#pragma once

#include <cstdint>

#include "game/GameState.h"

namespace Game {
namespace BathService {

enum class Stage : uint8_t {
    SOAP = 0,
    BRUSH,
    RINSE,
};

struct RewardResult {
    uint8_t experience = 0;
    uint8_t moodGain = 0;
};

// Completion healing scales with the final bath score: one star restores 45%,
// two stars 70%, and three stars restores the monster to full HP.
uint8_t hpRecoveryPercentForScore(uint8_t score);
uint16_t applyCompletionRecovery(GameState& state, uint8_t score,
                                 uint8_t teamSlot = 0);
uint16_t careDailyCapForLevel(uint8_t level);
uint8_t fullBathExperienceForLevel(uint8_t level);
int8_t nextOwnedSoap(const GameState& state, int8_t from,
                     int8_t direction = 1);
bool consumeSoap(GameState& state, uint8_t soapIndex);
RewardResult applyStageReward(GameState& state, Stage stage,
                              uint8_t teamSlot = 0);

}  // namespace BathService
}  // namespace Game
