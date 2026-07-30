#include <cassert>

#include "game/Species.h"

namespace {

bool contains(const Game::MoveId* moves, uint8_t count, Game::MoveId moveId) {
    for (uint8_t index = 0; index < count; ++index) {
        if (moves[index] == moveId) return true;
    }
    return false;
}

}  // namespace

int main() {
    const Species* bulbasaur = findSpecies(1);
    assert(bulbasaur != nullptr);

    Game::MonsterRuntime mon{};
    mon.speciesId = bulbasaur->id;
    mon.level = 20;
    resetMovesForLevel(mon, *bulbasaur);
    assert(mon.move2Id != 0);

    Game::MoveId forgotten = mon.move2Id;
    mon.move2Id = 0;
    Game::MoveId moves[MAX_RECALLABLE_MOVE_COUNT] = {};
    uint8_t count = collectRecallableMoves(
        *bulbasaur, mon, moves, MAX_RECALLABLE_MOVE_COUNT);
    assert(contains(moves, count, forgotten));
    assert(!contains(moves, count, mon.move1Id));
    assert(!contains(moves, count, mon.move3Id));

    const Species* eevee = findSpecies(133);
    const Species* jolteon = findSpecies(135);
    assert(eevee != nullptr && jolteon != nullptr);
    Game::MoveId eeveeMove = 0;
    for (uint16_t index = 0; index < learnsetEntryCountForSpecies(*eevee);
         ++index) {
        const LearnsetEntry* entry = learnsetEntryForSpecies(*eevee, index);
        if (entry && entry->level <= Game::LEVEL_MAX &&
            canLearnAsSpecialMove(*eevee, entry->moveId)) {
            eeveeMove = entry->moveId;
            break;
        }
    }
    assert(eeveeMove != 0);
    Game::MonsterRuntime evolved{};
    evolved.speciesId = jolteon->id;
    evolved.level = Game::LEVEL_MAX;
    evolved.move1Id = basicMoveIdForSpecies(*jolteon);
    Game::MoveId evolvedMoves[MAX_RECALLABLE_MOVE_COUNT] = {};
    uint8_t evolvedCount = collectRecallableMoves(
        *jolteon, evolved, evolvedMoves, MAX_RECALLABLE_MOVE_COUNT);
    assert(contains(evolvedMoves, evolvedCount, eeveeMove));

    for (uint8_t speciesIndex = 0; speciesIndex < speciesCount(); ++speciesIndex) {
        const Species& species = speciesTable()[speciesIndex];
        Game::MonsterRuntime candidate{};
        candidate.speciesId = species.id;
        candidate.level = Game::LEVEL_MAX;
        candidate.move1Id = basicMoveIdForSpecies(species);
        candidate.move2Id = 0;
        candidate.move3Id = 0;

        Game::MoveId allMoves[MAX_RECALLABLE_MOVE_COUNT] = {};
        uint8_t allCount = collectRecallableMoves(
            species, candidate, allMoves, MAX_RECALLABLE_MOVE_COUNT);
        assert(allCount < MAX_RECALLABLE_MOVE_COUNT);
        for (uint8_t index = 0; index < allCount; ++index) {
            assert(canRetainSpecialMove(
                species, allMoves[index], candidate.level));
            for (uint8_t previous = 0; previous < index; ++previous) {
                assert(allMoves[previous] != allMoves[index]);
            }
        }
    }
    return 0;
}
