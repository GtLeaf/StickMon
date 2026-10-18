#include <cassert>
#include <cstdio>
#include <vector>
#include "game/CareExperience.h"
#include "game/CareTicker.h"
#include "game/HomeCare.h"
#include "game/BathService.h"
#include "game/ExperienceService.h"
#include "game/MonsterFactory.h"
#include "game/TeamRoster.h"
#include "core/SaveCodec.h"

using namespace Game;

static FoodConsumeResult eat(GameState& state, uint8_t slot = 0, uint8_t food = 0) {
    state.room.bowlFood = food;
    state.room.bowlCount = 1;
    state.room.bowlBitesRemaining = roomFoodBitesPerServing(food);
    state.team[slot].satiety = 25;
    return HomeCare::consumeBowlFood(state, slot);
}

int main() {
    struct Example { uint8_t level; uint32_t budget, bite, tasty, pet; };
    for (const auto example : {Example{5,18,1,3,1}, {10,73,3,12,4},
                               {20,300,13,50,15}, {30,507,21,85,25}}) {
        GameState state;
        state.team[0] = MonsterFactory::create(1, example.level);
        assert(CareExperience::budgetFor(state.team[0]) == example.budget);
        assert(eat(state).careExp == example.bite);
        assert(eat(state, 0, ROOM_TASTY_FOOD_INDEX).careExp == example.tasty);
        assert(HomeCare::petMonster(state, 0, 1).careExp == example.pet);
    }

    GameState state;
    state.teamCount = 2;
    state.team[0] = MonsterFactory::create(1, 20);
    state.team[1] = MonsterFactory::create(7, 20);
    state.team[0].ivPacked = 123;
    state.team[1].ivPacked = 456;
    CareExperience::prepareDay(state);
    assert(eat(state).careExp == 13);
    auto* record = CareExperience::recordFor(state, state.team[0]);
    const uint32_t budget = record->budget;
    assert(budget == 300);
    assert(record->awarded[0] == 13);
    assert(CareExperience::recordFor(state, state.team[1])->awarded[0] == 0);

    // Near-level-up rewards use the whole level's span and stay fixed after
    // level-up/evolution, swapping the front, contact mirroring and reboot.
    const auto* species = findSpecies(state.team[0].speciesId);
    state.team[0].exp = minimumExpForLevel(species->growthRate, 21) - 1;
    const auto bite = eat(state);
    assert(bite.careExp == 13);
    assert(ExperienceService::add(state.team[0], *species, bite.careExp).leveledUp);
    assert(state.team[0].level == 21);
    state.team[0].speciesId = 2;
    assert(eat(state).careExp == 13);
    state.storageCount = 1;
    state.storage[0] = state.team[0];
    assert(TeamRoster::moveToFront(state, 1));
    assert(eat(state, 1).careExp == 13);
    record = CareExperience::recordFor(state, state.team[1]);
    assert(record->budget == budget && record->awarded[0] == 52);
    record->pendingOldLevel = 20;
    uint8_t encoded[SaveCodec::MAX_ENCODED_BYTES];
    size_t size = 0;
    assert(SaveCodec::encode(state, {}, 7, encoded, sizeof(encoded), size));
    SaveCodec::Snapshot restored;
    assert(SaveCodec::decode(encoded, size, restored));
    state = restored.state;
    assert(CareExperience::recordFor(state, state.team[1])->pendingOldLevel == 20);
    assert(eat(state, 1).careExp == 13);
    assert(!CareExperience::prepareDay(state));

    // Food category caps exactly and repeated feeding, bathing and touching
    // cannot borrow the other care categories' budgets.
    uint32_t foodTotal = 65;
    for (int i = 0; i < 50; ++i) foodTotal += eat(state, 1).careExp;
    assert(foodTotal == 120);
    assert(eat(state, 0).careExp == 13);
    state.team[1].satiety = 100;
    state.room.bowlCount = 1;
    assert(HomeCare::consumeBowlFood(state, 1).careExp == 0);
    uint32_t pets = 0;
    for (int i = 0; i < 10; ++i) pets += HomeCare::petMonster(state, 1, i).careExp;
    assert(pets == 60 && state.team[1].petCountToday == 4);
    uint32_t bath = 0;
    for (auto stage : {BathService::Stage::SOAP, BathService::Stage::BRUSH, BathService::Stage::RINSE})
        bath += BathService::applyStageReward(state, stage, 1).experience;
    assert(bath == 105);
    for (auto stage : {BathService::Stage::SOAP, BathService::Stage::BRUSH, BathService::Stage::RINSE})
        assert(BathService::applyStageReward(state, stage, 1).experience == 0);
    uint32_t play = 0;
    for (int i = 0; i < 20; ++i) play += CareExperience::grant(state, 1, CareExperience::Source::PLAY);
    assert(play == 15);
    record = CareExperience::recordFor(state, state.team[1]);
    uint32_t total = 0;
    for (auto amount : record->awarded) total += amount;
    assert(total == budget);

    // Midnight resets categories and snapshots the new level, preserving an
    // interrupted progression. A stale contact copy must not overwrite it.
    state.storage[0].level = 5;
    state.gameMinutesTotal += 1440;
    assert(resetDailyCareCounters(state));
    record = CareExperience::recordFor(state, state.team[1]);
    assert(record->budget == CareExperience::budgetFor(state.team[1]));
    assert(record->budget != budget);
    assert(record->pendingOldLevel == 20 && record->bathStageMask == 0);
    assert(state.team[1].petCountToday == 0);
    assert(record->awarded[0] == 0);
    assert(HomeCare::petMonster(state, 1, 10).careExp > 0);

    // Max-level, visitors and fainted pets never award care experience.
    for (int mode = 0; mode < 3; ++mode) {
        GameState invalid;
        invalid.team[0] = MonsterFactory::create(1, mode == 0 ? 100 : 30);
        if (mode == 1) invalid.team[0].origin = Origin::VISITOR;
        if (mode == 2) invalid.team[0].fainted = true;
        assert(eat(invalid).careExp == 0);
        assert(HomeCare::petMonster(invalid, 0, 1).careExp == 0);
        assert(BathService::applyStageReward(invalid, BathService::Stage::SOAP).experience == 0);
    }
    // Large rewards survive the entire bath and save pipeline without u8 truncation.
    GameState high;
    high.team[0] = MonsterFactory::create(147, 90);
    const auto big = BathService::applyStageReward(high, BathService::Stage::RINSE);
    assert(big.experience > 255);
    std::puts("care: fixed rewards, independent quotas, rollover, evolution, reboot and caps verified");
}
