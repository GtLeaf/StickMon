#pragma once

#include <cstdint>

#include "game/GameState.h"

struct Species;

namespace Game {
namespace MonsterFactory {

// Creates the shared gameplay state for a newly encountered monster. Callers
// add platform- or scene-specific metadata such as metArea afterward.
MonsterRuntime create(uint16_t speciesId, uint8_t level);

}  // namespace MonsterFactory
}  // namespace Game
