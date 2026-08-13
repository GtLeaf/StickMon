#pragma once

#include <cstdint>

#include "game/GameState.h"

enum class FoodPlacementResult : uint8_t {
    ADDED,
    NO_STOCK,
    BOWL_FULL,
    DIFFERENT_FOOD,
};

enum class PetOutcome : uint8_t {
    REWARDED,
    DAILY_LIMIT,
    NEEDS_REST,
};

enum class FoodReaction : uint8_t {
    NORMAL,
    LIKED,
    DISLIKED,
};

struct FoodConsumeResult {
    bool consumed = false;
    uint8_t foodIndex = 0;
    uint8_t satietyBefore = 0;
    uint8_t satietyAfter = 0;
    uint8_t moodBefore = 0;
    uint8_t moodAfter = 0;
    bool lastBite = false;
    bool becameFull = false;
    FoodReaction reaction = FoodReaction::NORMAL;
    uint8_t careExp = 0;
    bool weakCareExp = false;
};

struct PetResult {
    PetOutcome outcome = PetOutcome::DAILY_LIMIT;
    uint8_t moodGain = 0;
    uint8_t affectionGain = 0;
};

namespace Game {
namespace HomeCare {

FoodPlacementResult placeSelectedFoodInBowl(GameState& state);
FoodConsumeResult consumeBowlFood(GameState& state, uint8_t teamSlot = 0);
PetResult petMonster(GameState& state, uint8_t teamSlot,
                     uint32_t gameSeconds);

}  // namespace HomeCare
}  // namespace Game
