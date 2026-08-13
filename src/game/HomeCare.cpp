#include "game/HomeCare.h"

#include <algorithm>

#include "game/FoodTuning.h"
#include "game/Species.h"
#include "game/SpeciesBehavior.h"

namespace Game {
namespace HomeCare {
namespace {

uint8_t selectedFoodIndex(const RoomState& room) {
    return room.selectedFood < ROOM_FOOD_COUNT ? room.selectedFood : 0;
}

void selectFirstAvailableFood(RoomState& room) {
    for (uint8_t i = 0; i < ROOM_FOOD_COUNT; ++i) {
        if (room.food[i] > 0) {
            room.selectedFood = i;
            return;
        }
    }
    room.selectedFood = 0;
}

}  // namespace

FoodPlacementResult placeSelectedFoodInBowl(GameState& state) {
    RoomState& room = state.room;
    uint8_t foodIndex = selectedFoodIndex(room);
    if (room.food[foodIndex] == 0) {
        selectFirstAvailableFood(room);
        foodIndex = selectedFoodIndex(room);
        if (room.food[foodIndex] == 0) return FoodPlacementResult::NO_STOCK;
    }
    if (room.bowlCount > 0) {
        uint8_t bitesPerServing = roomFoodBitesPerServing(room.bowlFood);
        uint8_t remaining = room.bowlBitesRemaining;
        if (remaining == 0 || remaining > bitesPerServing) {
            remaining = bitesPerServing;
        }
        if (remaining > 1) {
            return FoodPlacementResult::BOWL_FULL;
        }
        if (room.bowlFood != foodIndex) {
            return FoodPlacementResult::DIFFERENT_FOOD;
        }
    }

    room.food[foodIndex]--;
    room.bowlFood = foodIndex;
    room.bowlCount = 1;
    room.bowlBitesRemaining = roomFoodBitesPerServing(foodIndex);
    if (room.food[foodIndex] == 0) selectFirstAvailableFood(room);
    return FoodPlacementResult::ADDED;
}

FoodConsumeResult consumeBowlFood(GameState& state, uint8_t teamSlot) {
    FoodConsumeResult result;
    if (state.room.bowlCount == 0 ||
        teamSlot >= state.teamCount || teamSlot >= TEAM_CAP) {
        return result;
    }

    MonsterRuntime& monster = state.team[teamSlot];
    if (monster.origin == Origin::VISITOR || monster.fainted ||
        monster.hpCur == 0 ||
        !speciesCareProfileFor(monster.speciesId).needsFood) {
        return result;
    }

    uint8_t foodIndex = state.room.bowlFood < ROOM_FOOD_COUNT
        ? state.room.bowlFood
        : 0;
    result.foodIndex = foodIndex;
    uint8_t bitesPerServing = roomFoodBitesPerServing(foodIndex);
    if (state.room.bowlBitesRemaining == 0 ||
        state.room.bowlBitesRemaining > bitesPerServing) {
        state.room.bowlBitesRemaining = bitesPerServing;
    }
    state.room.bowlBitesRemaining--;
    if (state.room.bowlBitesRemaining == 0) {
        state.room.bowlCount = 0;
        state.room.bowlFood = 0;
    }

    result.satietyBefore = monster.satiety;
    result.moodBefore = monster.mood;
    bool wasFull = monster.satiety >= 100;
    const FoodTuning::FoodProfile& profile = FoodTuning::PROFILES[foodIndex];
    uint16_t moodGain = profile.moodGain;
    if (foodIndex >= ROOM_SWEET_FOOD_INDEX) {
        if (natureLikedFoodIndex(monster.nature) ==
            static_cast<int8_t>(foodIndex)) {
            moodGain = static_cast<uint16_t>(moodGain) *
                       FoodTuning::LIKED_MOOD_PERCENT / 100;
            result.reaction = FoodReaction::LIKED;
        } else if (natureDislikedFoodIndex(monster.nature) ==
                   static_cast<int8_t>(foodIndex)) {
            moodGain = static_cast<uint16_t>(moodGain) *
                       FoodTuning::DISLIKED_MOOD_PERCENT / 100;
            result.reaction = FoodReaction::DISLIKED;
        }
    }

    monster.satiety = static_cast<uint8_t>(std::min<uint16_t>(
        100, static_cast<uint16_t>(monster.satiety) + profile.satietyGain));
    monster.mood = static_cast<uint8_t>(std::min<uint16_t>(
        100, static_cast<uint16_t>(monster.mood) + moodGain));
    result.consumed = true;
    result.satietyAfter = monster.satiety;
    result.moodAfter = monster.mood;
    result.lastBite = state.room.bowlCount == 0;
    result.becameFull = !wasFull && monster.satiety >= 100;
    result.careExp = profile.careExp;
    result.weakCareExp = wasFull;
    return result;
}

PetResult petMonster(GameState& state, uint8_t teamSlot,
                     uint32_t gameSeconds) {
    PetResult result;
    if (teamSlot >= state.teamCount || teamSlot >= TEAM_CAP) {
        result.outcome = PetOutcome::NEEDS_REST;
        return result;
    }

    MonsterRuntime& monster = state.team[teamSlot];
    if (monster.fainted || monster.hpCur == 0) {
        result.outcome = PetOutcome::NEEDS_REST;
        return result;
    }

    monster.lastPettedAt = gameSeconds;
    if (monster.petCountToday >= 4) {
        result.outcome = PetOutcome::DAILY_LIMIT;
        return result;
    }

    uint8_t oldMood = monster.mood;
    uint8_t oldAffection = monster.affection;
    monster.petCountToday++;
    monster.mood = static_cast<uint8_t>(
        std::min<uint16_t>(100, static_cast<uint16_t>(monster.mood) + 5));
    monster.affection = static_cast<uint8_t>(
        std::min<uint16_t>(255,
                           static_cast<uint16_t>(monster.affection) + 2));
    result.outcome = PetOutcome::REWARDED;
    result.moodGain = monster.mood - oldMood;
    result.affectionGain = monster.affection - oldAffection;
    return result;
}

}  // namespace HomeCare
}  // namespace Game
