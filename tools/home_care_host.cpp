#include <cassert>

#include "game/HomeCare.h"

int main() {
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
