#include "game/MoveManagementService.h"

#include "game/ItemInventory.h"
#include "game/Species.h"

namespace Game {
namespace MoveManagementService {

MoveId learnedMoveId(const Species& species, const MonsterRuntime& monster,
                     uint8_t moveSlot) {
    if (moveSlot == 0) return moveIdForMonster(species, monster, false);
    if (moveSlot <= SPECIAL_MOVE_SLOT_COUNT) {
        return specialMoveIdForMonster(
            monster, static_cast<uint8_t>(moveSlot - SPECIAL_MOVE_SLOT_FIRST));
    }
    return 0;
}

const MoveInfo* learnedMove(const Species& species,
                            const MonsterRuntime& monster,
                            uint8_t moveSlot) {
    return findMove(learnedMoveId(species, monster, moveSlot));
}

uint8_t learnedMoveCount(const Species& species,
                         const MonsterRuntime& monster) {
    uint8_t count = 0;
    for (uint8_t slot = 0; slot < MOVE_SLOT_COUNT; ++slot) {
        if (learnedMove(species, monster, slot)) ++count;
    }
    return count;
}

uint8_t collectRecallable(const Species& species,
                          const MonsterRuntime& monster,
                          MoveId* outMoves, uint8_t capacity) {
    return ::collectRecallableMoves(species, monster, outMoves, capacity);
}

bool forgetSpecialMove(GameState& state, uint8_t teamSlot,
                       uint8_t moveSlot) {
    if (teamSlot >= state.teamCount || teamSlot >= TEAM_CAP ||
        moveSlot < SPECIAL_MOVE_SLOT_FIRST ||
        moveSlot >= MOVE_SLOT_COUNT) {
        return false;
    }
    MonsterRuntime& monster = state.team[teamSlot];
    MoveId* move = moveSlot == 1 ? &monster.move2Id : &monster.move3Id;
    if (*move == 0) return false;
    *move = 0;
    monster.moveProficiency[moveSlot] = 0;
    return true;
}

bool recallMove(GameState& state, uint8_t teamSlot, MoveId moveId,
                uint8_t replacementSlot) {
    if (teamSlot >= state.teamCount || teamSlot >= TEAM_CAP ||
        replacementSlot < SPECIAL_MOVE_SLOT_FIRST ||
        replacementSlot >= MOVE_SLOT_COUNT ||
        ItemInventory::count(state, ItemId::HEART_SCALE) == 0) {
        return false;
    }

    const MonsterRuntime& monster = state.team[teamSlot];
    const Species* species = findSpecies(monster.speciesId);
    if (!species) return false;

    MoveId recallable[MAX_RECALLABLE_MOVE_COUNT] = {};
    uint8_t count = collectRecallable(
        *species, monster, recallable, MAX_RECALLABLE_MOVE_COUNT);
    bool valid = false;
    for (uint8_t index = 0; index < count; ++index) {
        if (recallable[index] == moveId) {
            valid = true;
            break;
        }
    }
    if (!valid || !ItemInventory::remove(state, ItemId::HEART_SCALE)) {
        return false;
    }

    MonsterRuntime& updated = state.team[teamSlot];
    MoveId& target = replacementSlot == 1
        ? updated.move2Id : updated.move3Id;
    target = moveId;
    updated.moveProficiency[replacementSlot] = 0;
    return true;
}

}  // namespace MoveManagementService
}  // namespace Game
