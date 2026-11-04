#pragma once

#include "game/GameState.h"
#include "game/Species.h"

namespace Features { namespace Moves {

enum class PageMode : uint8_t { MANAGE, RECALL_SELECT, RECALL_REPLACE };
enum class Selection : uint8_t { BACK, RECALL_CHOICE, REPLACE, FORGET, BASIC_LOCKED, NONE };
struct Action {
    Selection kind = Selection::NONE;
    uint8_t slot = 0;
    Game::MoveId move = 0;
};

// Semantic selection and catalog state, independent of input and drawing.
class Flow {
public:
    using MovePageMode = PageMode;
    uint8_t moveMonsterIndex = 0;
    uint8_t moveCursor = 0;
    PageMode movePageMode = PageMode::MANAGE;
    Game::MoveId recallMoveIds[MAX_RECALLABLE_MOVE_COUNT] = {};
    uint8_t recallMoveCount = 0;
    uint8_t recallSelectedIndex = 0;

    void resetFlow();
    void manage(uint8_t slot);
    bool recall(uint8_t slot, uint8_t count);
    bool valid(const Game::GameState& state) const;
    uint8_t rowCount(const Game::GameState& state) const;
    const MoveInfo* info(const Game::GameState& state, uint8_t row,
                         uint8_t* proficiency = nullptr) const;
    int descriptionKey(const Game::GameState& state, uint8_t row) const;
    void clampCursor(const Game::GameState& state);
    Action select(const Game::GameState& state);
    void returnToRecall();

    static const Species& speciesFor(const Game::MonsterRuntime& monster);
    static const MoveInfo* moveInfoForSlot(const Species& species,
        const Game::MonsterRuntime& monster, uint8_t slot);
    static uint8_t learnedCount(const Species& species, const Game::MonsterRuntime& monster);
    static uint8_t learnedSlotAt(const Species& species, const Game::MonsterRuntime& monster, uint8_t row);
};

} }  // namespace Features::Moves
