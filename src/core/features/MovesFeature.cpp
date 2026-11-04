#include "core/features/MovesFeature.h"

#include <algorithm>
#include "game/MoveManagementService.h"

namespace Features { namespace Moves {

void Flow::resetFlow() {
    moveMonsterIndex = moveCursor = recallMoveCount = recallSelectedIndex = 0;
    movePageMode = PageMode::MANAGE;
}

void Flow::manage(uint8_t slot) {
    resetFlow();
    moveMonsterIndex = slot;
}

bool Flow::recall(uint8_t slot, uint8_t count) {
    moveMonsterIndex = slot;
    moveCursor = recallSelectedIndex = 0;
    recallMoveCount = std::min<uint8_t>(count, MAX_RECALLABLE_MOVE_COUNT);
    movePageMode = PageMode::RECALL_SELECT;
    return recallMoveCount != 0;
}

const Species& Flow::speciesFor(const Game::MonsterRuntime& monster) {
    const Species* species = findSpecies(monster.speciesId);
    return species ? *species : starterSpecies();
}

const MoveInfo* Flow::moveInfoForSlot(const Species& species,
        const Game::MonsterRuntime& monster, uint8_t slot) {
    return Game::MoveManagementService::learnedMove(species, monster, slot);
}

uint8_t Flow::learnedCount(const Species& species, const Game::MonsterRuntime& monster) {
    return Game::MoveManagementService::learnedMoveCount(species, monster);
}

uint8_t Flow::learnedSlotAt(const Species& species,
        const Game::MonsterRuntime& monster, uint8_t row) {
    for (uint8_t slot = 0; slot < Game::MOVE_SLOT_COUNT; ++slot) {
        if (!moveInfoForSlot(species, monster, slot)) continue;
        if (row == 0) return slot;
        --row;
    }
    return Game::MOVE_SLOT_COUNT;
}

bool Flow::valid(const Game::GameState& state) const {
    return moveMonsterIndex < state.teamCount && moveMonsterIndex < Game::TEAM_CAP;
}

uint8_t Flow::rowCount(const Game::GameState& state) const {
    if (!valid(state)) return 0;
    const auto& monster = state.team[moveMonsterIndex];
    return movePageMode == PageMode::MANAGE ? learnedCount(speciesFor(monster), monster) + 1
        : movePageMode == PageMode::RECALL_SELECT ? recallMoveCount + 1 : SPECIAL_MOVE_SLOT_COUNT + 1;
}

const MoveInfo* Flow::info(const Game::GameState& state, uint8_t row, uint8_t* proficiency) const {
    if (proficiency) *proficiency = 0;
    if (!valid(state)) return nullptr;
    if (movePageMode == PageMode::RECALL_SELECT)
        return row < recallMoveCount ? findMove(recallMoveIds[row]) : nullptr;
    const auto& monster = state.team[moveMonsterIndex];
    const auto& species = speciesFor(monster);
    const uint8_t slot = movePageMode == PageMode::MANAGE
        ? learnedSlotAt(species, monster, row)
        : row < SPECIAL_MOVE_SLOT_COUNT ? row + 1 : Game::MOVE_SLOT_COUNT;
    if (slot >= Game::MOVE_SLOT_COUNT) return nullptr;
    if (proficiency) *proficiency = monster.moveProficiency[slot];
    return moveInfoForSlot(species, monster, slot);
}

int Flow::descriptionKey(const Game::GameState& state, uint8_t row) const {
    if (!valid(state)) return -1;
    if (movePageMode == PageMode::RECALL_SELECT)
        return row < recallMoveCount ? 0x7000 | recallMoveIds[row] : -1;
    const auto& monster = state.team[moveMonsterIndex];
    const uint8_t slot = movePageMode == PageMode::MANAGE
        ? learnedSlotAt(speciesFor(monster), monster, row)
        : row < SPECIAL_MOVE_SLOT_COUNT ? row + 1 : Game::MOVE_SLOT_COUNT;
    return slot < Game::MOVE_SLOT_COUNT
        ? (movePageMode == PageMode::MANAGE ? 0x6000 : 0x7800) | (moveMonsterIndex << 4) | slot : -1;
}

void Flow::clampCursor(const Game::GameState& state) {
    const auto count = rowCount(state);
    if (moveCursor >= count) moveCursor = count ? count - 1 : 0;
}

Action Flow::select(const Game::GameState& state) {
    if (!valid(state)) return {Selection::BACK};
    clampCursor(state);
    if (movePageMode == PageMode::RECALL_SELECT) {
        if (moveCursor >= recallMoveCount) return {Selection::BACK};
        recallSelectedIndex = moveCursor;
        movePageMode = PageMode::RECALL_REPLACE;
        moveCursor = 0;
        return {Selection::RECALL_CHOICE};
    }
    if (movePageMode == PageMode::RECALL_REPLACE) {
        if (moveCursor >= SPECIAL_MOVE_SLOT_COUNT) {
            returnToRecall();
            return {Selection::RECALL_CHOICE};
        }
        if (recallSelectedIndex >= recallMoveCount) return {};
        return {Selection::REPLACE, static_cast<uint8_t>(moveCursor + 1), recallMoveIds[recallSelectedIndex]};
    }
    const auto& monster = state.team[moveMonsterIndex];
    const auto slot = learnedSlotAt(speciesFor(monster), monster, moveCursor);
    if (slot >= Game::MOVE_SLOT_COUNT) return {Selection::BACK};
    return {slot == 0 ? Selection::BASIC_LOCKED : Selection::FORGET, slot};
}

void Flow::returnToRecall() {
    movePageMode = PageMode::RECALL_SELECT;
    moveCursor = recallSelectedIndex;
}

} }  // namespace Features::Moves
