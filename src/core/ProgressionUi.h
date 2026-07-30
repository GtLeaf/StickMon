#pragma once

#include <cstdint>

namespace ProgressionUi {

enum class MoveLearnStage : uint8_t {
    PROMPT,
    REPLACEMENT,
};

struct MoveLearnState {
    MoveLearnStage stage = MoveLearnStage::PROMPT;
    uint8_t promptCursor = 0;
    uint8_t replacementSlot = 1;
    bool confirmOpen = false;
    bool confirmYes = false;
};

void renderLevelUp(uint8_t level);
bool evolutionAnimationComplete(uint16_t fromSpeciesId,
                                uint16_t toSpeciesId,
                                uint32_t nowMs);
void resetEvolutionAnimation();
void beginEvolutionCancellation(uint16_t fromSpeciesId,
                                uint16_t toSpeciesId,
                                uint32_t nowMs);
bool evolutionCancellationComplete(uint32_t nowMs);
void renderEvolution(uint16_t fromSpeciesId,
                     uint16_t toSpeciesId,
                     uint32_t nowMs);
void renderEvolutionCancelled(uint16_t speciesId, uint32_t nowMs);
void resetMoveLearnState(MoveLearnState& state);
bool handleMoveLearnInput(MoveLearnState& state, uint8_t button);
void renderMoveLearn(const MoveLearnState& state);
void renderMoveReplacement();

} // namespace ProgressionUi
