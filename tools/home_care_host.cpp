#include <algorithm>
#include <cassert>

#include "game/HomeCare.h"
#include "core/SaveCodec.h"

namespace {
void checkTastyFoodServing() {
    Game::GameState state;
    state.teamCount = 2;
    state.team[1] = state.team[0];
    for (auto& monster : state.team) {
        monster.satiety = 0;
        monster.mood = 60;
    }
    state.room.selectedFood = Game::ROOM_TASTY_FOOD_INDEX;
    state.room.food[Game::ROOM_TASTY_FOOD_INDEX] = 2;
    assert(Game::HomeCare::placeSelectedFoodInBowl(state) ==
           FoodPlacementResult::ADDED);
    assert(state.room.food[Game::ROOM_TASTY_FOOD_INDEX] == 1);
    assert(state.room.bowlBitesRemaining == 5);

    // Both pets can share a serving, including after saving each remaining bite.
    for (uint8_t bite = 0; bite < 5; ++bite) {
        uint8_t encoded[SaveCodec::MAX_ENCODED_BYTES];
        size_t length = 0;
        assert(SaveCodec::encode(state, {}, bite + 1, encoded, sizeof(encoded), length));
        SaveCodec::Snapshot restored;
        assert(SaveCodec::decode(encoded, length, restored));
        state = restored.state;
        assert(state.room.bowlBitesRemaining == 5 - bite);
        if (bite < 4) {
            assert(Game::HomeCare::placeSelectedFoodInBowl(state) ==
                   FoodPlacementResult::BOWL_FULL);
        }
        const auto result = Game::HomeCare::consumeBowlFood(state, bite % 2);
        assert(result.consumed);
        assert(result.satietyAfter == std::min<int>(100, result.satietyBefore + 35));
        assert(result.moodAfter == result.moodBefore + 3);
        assert(result.reaction == FoodReaction::NORMAL);
        assert(result.lastBite == (bite == 4));
        assert(state.room.bowlBitesRemaining == 4 - bite);
        assert(state.room.bowlCount == (bite == 4 ? 0 : 1));
        assert(state.room.food[Game::ROOM_TASTY_FOOD_INDEX] == 1);
    }
    assert(!Game::HomeCare::consumeBowlFood(state, 0).consumed);
    assert(state.team[0].satiety == 100 && state.team[0].mood == 69);
    assert(state.team[1].satiety == 70 && state.team[1].mood == 66);

    state.room.selectedFood = Game::ROOM_TASTY_FOOD_INDEX;
    assert(Game::HomeCare::placeSelectedFoodInBowl(state) == FoodPlacementResult::ADDED);
    assert(state.room.bowlBitesRemaining == 5);
    state.team[0].satiety = 90;
    state.team[0].mood = 99;
    const auto full = Game::HomeCare::consumeBowlFood(state, 0);
    assert(full.becameFull && full.satietyAfter == 100 && full.moodAfter == 100);
    assert(state.room.bowlBitesRemaining == 4);

    // Old saves and partially eaten servings keep their remaining single bite.
    state.room.bowlBitesRemaining = 1;
    const auto last = Game::HomeCare::consumeBowlFood(state, 1);
    assert(last.consumed && last.lastBite && state.room.bowlCount == 0);
    assert(state.room.bowlBitesRemaining == 0);
}
}  // namespace

int main() {
    checkTastyFoodServing();
    Game::GameState state;

    assert(Game::HomeCare::placeSelectedFoodInBowl(state) ==
           FoodPlacementResult::ADDED);
    assert(state.room.food[Game::ROOM_NORMAL_FOOD_INDEX] == 1);
    assert(state.room.bowlCount == 1);
    assert(state.room.bowlBitesRemaining == Game::ROOM_NORMAL_FOOD_BITES);

    state.team[0].satiety = 50;
    state.team[0].mood = 60;
    FoodConsumeResult bite = Game::HomeCare::consumeBowlFood(state, 0);
    assert(bite.consumed);
    assert(bite.foodIndex == Game::ROOM_NORMAL_FOOD_INDEX);
    assert(bite.satietyBefore == 50 && bite.satietyAfter == 75);
    assert(bite.moodBefore == 60 && bite.moodAfter == 63);
    assert(!bite.lastBite);
    assert(state.room.bowlBitesRemaining ==
           Game::ROOM_NORMAL_FOOD_BITES - 1);

    assert(Game::HomeCare::placeSelectedFoodInBowl(state) ==
           FoodPlacementResult::BOWL_FULL);
    assert(state.room.food[Game::ROOM_NORMAL_FOOD_INDEX] == 1);

    bite = Game::HomeCare::consumeBowlFood(state, 0);
    assert(bite.consumed);
    assert(state.room.bowlBitesRemaining == 1);
    assert(Game::HomeCare::placeSelectedFoodInBowl(state) ==
           FoodPlacementResult::ADDED);
    assert(state.room.food[Game::ROOM_NORMAL_FOOD_INDEX] == 0);
    assert(state.room.bowlBitesRemaining == Game::ROOM_NORMAL_FOOD_BITES);

    Game::MonsterRuntime& monster = state.team[0];
    uint8_t initialMood = monster.mood;
    uint8_t initialAffection = monster.affection;
    PetResult pet = Game::HomeCare::petMonster(state, 0, 1234);
    assert(pet.outcome == PetOutcome::REWARDED);
    assert(monster.mood == initialMood + 5);
    assert(monster.affection == initialAffection + 2);
    assert(monster.petCountToday == 1);
    assert(monster.lastPettedAt == 1234);

    Game::HomeCare::petMonster(state, 0, 1235);
    Game::HomeCare::petMonster(state, 0, 1236);
    Game::HomeCare::petMonster(state, 0, 1237);
    pet = Game::HomeCare::petMonster(state, 0, 1238);
    assert(pet.outcome == PetOutcome::DAILY_LIMIT);
    assert(monster.petCountToday == 4);
    assert(monster.lastPettedAt == 1238);

    monster.fainted = true;
    pet = Game::HomeCare::petMonster(state, 0, 1239);
    assert(pet.outcome == PetOutcome::NEEDS_REST);
    assert(monster.lastPettedAt == 1238);
    return 0;
}
