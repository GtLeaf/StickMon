#pragma once

#include <cstdint>

#include "game/FoodTuning.h"
#include "game/GameState.h"
#include "game/Species.h"

namespace FriendshipSystem {

static constexpr uint16_t OFFER_GATE_PERMILLE = 150;
static constexpr uint16_t OFFER_GATE_MAX_PERMILLE = 300;
static constexpr uint16_t OFFER_CHANCE_MAX_PERMILLE = 250;
static constexpr uint8_t BOSS_ODDS_PERCENT = 60;
static constexpr uint8_t SHAKE_CHECK_COUNT = 4;
static constexpr uint8_t FOOD_BOND_MAX = 100;
// 投掷接受率与羁绊收益的数值统一在 FoodTuning.h 调整。
static constexpr uint8_t NORMAL_FOOD_BOND_GAIN =
    FoodTuning::THROW_BOND_GAIN[(uint8_t)FoodTuning::ThrowClass::NORMAL];
static constexpr uint8_t NORMAL_FOOD_ACCEPT_PERCENT =
    FoodTuning::THROW_ACCEPT_PERCENT[0][(uint8_t)FoodTuning::ThrowClass::NORMAL];
static constexpr uint8_t BOSS_FOOD_ACCEPT_PERCENT =
    FoodTuning::THROW_ACCEPT_PERCENT[1][(uint8_t)FoodTuning::ThrowClass::NORMAL];

constexpr uint8_t levelOddsPercent(uint8_t level) {
    return level <= 10 ? 120
        : level <= 20 ? 110
        : level <= 35 ? 100
        : level <= 50 ? 90
        : level <= 65 ? 80
        : 70;
}

constexpr uint8_t statusOddsPercent(Game::MajorStatus status) {
    return status == Game::MajorStatus::SLEEP ||
                   status == Game::MajorStatus::FREEZE
        ? 200
        : status == Game::MajorStatus::POISON ||
                  status == Game::MajorStatus::TOXIC ||
                  status == Game::MajorStatus::PARALYSIS ||
                  status == Game::MajorStatus::BURN
            ? 150
            : 100;
}

constexpr uint32_t rawAdjustedCatchOdds(uint8_t catchRate, uint8_t level,
                                        Game::MajorStatus status, bool boss) {
    return static_cast<uint32_t>(catchRate) * levelOddsPercent(level) / 100 *
           statusOddsPercent(status) / 100 *
           (boss ? BOSS_ODDS_PERCENT : 100) / 100;
}

constexpr uint16_t adjustedCatchOdds(uint8_t catchRate, uint8_t level,
                                     Game::MajorStatus status, bool boss) {
    return static_cast<uint16_t>(
        rawAdjustedCatchOdds(catchRate, level, status, boss) > 255
            ? 255
            : rawAdjustedCatchOdds(catchRate, level, status, boss));
}

constexpr uint8_t addFoodBond(uint8_t current, uint8_t gain) {
    return current >= FOOD_BOND_MAX ||
                   gain >= static_cast<uint8_t>(FOOD_BOND_MAX - current)
        ? FOOD_BOND_MAX
        : static_cast<uint8_t>(current + gain);
}

constexpr uint16_t offerGatePermille(uint8_t foodBond) {
    return static_cast<uint16_t>(
        static_cast<uint32_t>(OFFER_GATE_PERMILLE) *
        (100U + (foodBond > FOOD_BOND_MAX ? FOOD_BOND_MAX : foodBond)) / 100U);
}

constexpr uint8_t normalFoodAcceptancePercent(bool boss) {
    return boss ? BOSS_FOOD_ACCEPT_PERCENT : NORMAL_FOOD_ACCEPT_PERCENT;
}

constexpr bool acceptsNormalFood(bool boss, uint8_t percentRoll) {
    return percentRoll < normalFoodAcceptancePercent(boss);
}

// 口味投掷：分类由调用方依据野生精灵性格偏好判定（见 FoodTuning::ThrowClass）。
constexpr uint8_t throwAcceptancePercent(bool boss, FoodTuning::ThrowClass throwClass) {
    return FoodTuning::THROW_ACCEPT_PERCENT[boss ? 1 : 0][(uint8_t)throwClass];
}

constexpr bool acceptsFoodThrow(bool boss, FoodTuning::ThrowClass throwClass,
                                uint8_t percentRoll) {
    return percentRoll < throwAcceptancePercent(boss, throwClass);
}

constexpr uint8_t throwBondGain(FoodTuning::ThrowClass throwClass) {
    return FoodTuning::THROW_BOND_GAIN[(uint8_t)throwClass];
}

// 依据野生精灵性格偏好为一次投掷分类；基础粮与非命中口味的口味粮按 NORMAL 结算。
inline FoodTuning::ThrowClass classifyFoodThrow(uint8_t foodIndex, uint8_t wildNature) {
    if (foodIndex == Game::ROOM_TASTY_FOOD_INDEX) return FoodTuning::ThrowClass::TASTY;
    if (foodIndex >= Game::ROOM_SWEET_FOOD_INDEX) {
        if (natureLikedFoodIndex(wildNature) == (int8_t)foodIndex) {
            return FoodTuning::ThrowClass::LIKED;
        }
        if (natureDislikedFoodIndex(wildNature) == (int8_t)foodIndex) {
            return FoodTuning::ThrowClass::DISLIKED;
        }
    }
    return FoodTuning::ThrowClass::NORMAL;
}

uint16_t offerChancePermille(const Species& species,
                             const Game::MonsterRuntime& monster,
                             bool boss, uint8_t foodBond = 0);
bool passesOfferChecks(const Species& species,
                       const Game::MonsterRuntime& monster, bool boss,
                       uint16_t gateRoll,
                       const uint16_t shakeRolls[SHAKE_CHECK_COUNT],
                       uint8_t foodBond = 0);

} // namespace FriendshipSystem
