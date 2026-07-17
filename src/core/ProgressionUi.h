#pragma once

#include <cstdint>

namespace ProgressionUi {

void renderLevelUp(uint8_t level);
void renderEvolution(uint16_t fromSpeciesId, uint16_t toSpeciesId);
void renderMoveLearn(uint8_t cursor);
void renderMoveReplacement();

} // namespace ProgressionUi
