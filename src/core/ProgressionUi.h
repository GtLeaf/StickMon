#pragma once

#include "ui/common/ProgressionUiModel.h"
#include "core/TouchTap.h"

namespace ProgressionUi {

using MoveLearnStage = Ui::MoveLearnStage;
using MoveLearnState = Ui::MoveLearnState;

void renderLevelUp(const char* speciesName, uint8_t level);
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
bool handleMoveLearnTouch(MoveLearnState& state, const TouchTap::Event& event);
bool handleLevelUpTouch(const TouchTap::Event& event);
bool handleMoveReplacementTouch(const TouchTap::Event& event);
void cancelMoveLearnTouch();
void renderMoveLearn(const MoveLearnState& state);
void renderMoveReplacement();

} // namespace ProgressionUi
