#include <cassert>
#include <cstring>

#include "core/features/MovesFeature.h"
#include "game/ItemInventory.h"
#include "game/MonsterFactory.h"
#include "game/MoveManagementService.h"

namespace {
void unchanged(const Game::GameState& state, const Game::GameState& before) {
    assert(std::memcmp(&state, &before, sizeof(state)) == 0);
}
}

int main() {
    using Features::Moves::Flow;
    using Features::Moves::PageMode;
    using Features::Moves::Selection;
    namespace Service = Game::MoveManagementService;

    Game::GameState state;
    state.teamCount = 2;
    state.team[0] = Game::MonsterFactory::create(1, 30);
    state.team[1] = Game::MonsterFactory::create(4, 30);
    const auto companion = state.team[1];
    auto& monster = state.team[0];
    monster.move2Id = monster.move3Id = 0;
    Flow flow;
    flow.manage(0);
    assert(flow.valid(state) && flow.rowCount(state) == 2);
    const Species& species = Flow::speciesFor(monster);
    assert(flow.info(state, 0) == Service::learnedMove(species, monster, 0));
    assert(flow.info(state, 1) == nullptr);
    const auto beforeSelect = state;
    assert(flow.select(state).kind == Selection::BASIC_LOCKED);
    unchanged(state, beforeSelect);

    const uint8_t count = Service::collectRecallable(species, monster,
        flow.recallMoveIds, MAX_RECALLABLE_MOVE_COUNT);
    assert(count > 1);
    assert(flow.recall(0, count));
    assert(flow.rowCount(state) == count + 1);
    flow.moveCursor = 1;
    const auto recalledId = flow.recallMoveIds[1];
    assert(flow.info(state, 1)->id == recalledId);
    assert(flow.select(state).kind == Selection::RECALL_CHOICE);
    assert(flow.movePageMode == PageMode::RECALL_REPLACE);
    assert(flow.recallSelectedIndex == 1 && flow.moveCursor == 0);
    assert(flow.rowCount(state) == SPECIAL_MOVE_SLOT_COUNT + 1);
    auto action = flow.select(state);
    assert(action.kind == Selection::REPLACE && action.slot == 1 && action.move == recalledId);
    unchanged(state, beforeSelect);

    // Returning from replacement retains the chosen recall row, including its id.
    flow.moveCursor = SPECIAL_MOVE_SLOT_COUNT;
    assert(flow.select(state).kind == Selection::RECALL_CHOICE);
    assert(flow.movePageMode == PageMode::RECALL_SELECT && flow.moveCursor == 1);
    assert(flow.select(state).kind == Selection::RECALL_CHOICE);
    flow.moveCursor = 1;
    action = flow.select(state);
    assert(action.kind == Selection::REPLACE && action.slot == 2 && action.move == recalledId);

    // Feature actions retain the shared service's atomic stock and slot rules.
    state.bag.heartScale = 0;
    const auto noStock = state;
    assert(!Service::recallMove(state, flow.moveMonsterIndex, action.move, action.slot));
    unchanged(state, noStock);
    assert(Game::ItemInventory::add(state, Game::ItemId::HEART_SCALE));
    const auto oneScale = state;
    assert(!Service::recallMove(state, 0, action.move, 0));
    unchanged(state, oneScale);
    assert(!Service::recallMove(state, 0, 0xffff, action.slot));
    unchanged(state, oneScale);
    const auto basic = monster.move1Id;
    monster.moveProficiency[2] = 99;
    assert(Service::recallMove(state, flow.moveMonsterIndex, action.move, action.slot));
    assert(monster.move1Id == basic && monster.move3Id == recalledId);
    assert(monster.moveProficiency[2] == 0 && state.bag.heartScale == 0);
    assert(std::memcmp(&companion, &state.team[1], sizeof(companion)) == 0);

    // A sparse special slot still maps to the real slot, rather than list index.
    flow.manage(0);
    assert(flow.rowCount(state) == 3);
    flow.moveCursor = 1;
    uint8_t proficiency = 255;
    assert(flow.info(state, 1, &proficiency)->id == recalledId && proficiency == 0);
    const auto beforeForgetSelection = state;
    action = flow.select(state);
    assert(action.kind == Selection::FORGET && action.slot == 2);
    unchanged(state, beforeForgetSelection);
    assert(!Service::forgetSpecialMove(state, 0, 0));
    assert(Service::forgetSpecialMove(state, flow.moveMonsterIndex, action.slot));
    assert(monster.move3Id == 0 && monster.move1Id == basic);
    flow.moveCursor = 255;
    flow.clampCursor(state);
    assert(flow.moveCursor + 1 == flow.rowCount(state));
    assert(flow.select(state).kind == Selection::BACK);

    assert(!flow.recall(0, 0));
    assert(flow.rowCount(state) == 1 && flow.select(state).kind == Selection::BACK);
    flow.manage(Game::TEAM_CAP);
    assert(!flow.valid(state) && flow.rowCount(state) == 0);
    assert(flow.info(state, 0, &proficiency) == nullptr && proficiency == 0);
    assert(flow.select(state).kind == Selection::BACK);
    flow.resetFlow();
    assert(flow.moveMonsterIndex == 0 && flow.moveCursor == 0 && flow.recallMoveCount == 0);
    assert(flow.movePageMode == PageMode::MANAGE);
}
