#pragma once

#include <cstdint>

#include "game/GameState.h"

struct MoveInfo;
struct Species;

namespace Game {
namespace MoveManagementService {

constexpr uint8_t SPECIAL_MOVE_SLOT_FIRST = 1;
constexpr uint8_t SPECIAL_MOVE_SLOT_COUNT = 2;
constexpr uint8_t MAX_RECALLABLE_MOVE_COUNT = 80;

uint8_t learnedMoveCount(const Species& species,
                         const MonsterRuntime& monster);
MoveId learnedMoveId(const Species& species, const MonsterRuntime& monster,
                     uint8_t moveSlot);
const MoveInfo* learnedMove(const Species& species,
                            const MonsterRuntime& monster,
                            uint8_t moveSlot);

uint8_t collectRecallable(const Species& species,
                          const MonsterRuntime& monster,
                          MoveId* outMoves, uint8_t capacity);

bool forgetSpecialMove(GameState& state, uint8_t teamSlot,
                       uint8_t moveSlot);
bool recallMove(GameState& state, uint8_t teamSlot, MoveId moveId,
                uint8_t replacementSlot);

}  // namespace MoveManagementService
}  // namespace Game
