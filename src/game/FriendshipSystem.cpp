#include "game/FriendshipSystem.h"

#include <cmath>

namespace FriendshipSystem {
namespace {

uint16_t gbaShakeThreshold(uint16_t odds) {
    if (odds >= 255) return 65535;
    if (odds == 0) return 0;
    float divisor = 16711680.0f / static_cast<float>(odds);
    float root = sqrtf(sqrtf(divisor));
    if (root <= 0.0f) return 65535;
    uint32_t threshold = static_cast<uint32_t>(1048560.0f / root);
    return static_cast<uint16_t>(threshold > 65535 ? 65535 : threshold);
}

float shakeSuccessChance(uint16_t odds) {
    if (odds >= 255) return 1.0f;
    if (odds == 0) return 0.0f;
    float shakeChance =
        static_cast<float>(gbaShakeThreshold(odds)) / 65536.0f;
    return shakeChance * shakeChance * shakeChance * shakeChance;
}

uint16_t effectiveOfferGatePermille(uint16_t odds, uint8_t foodBond) {
    uint16_t gate = offerGatePermille(foodBond);
    float shakeChance = shakeSuccessChance(odds);
    if (shakeChance <= 0.0f) return 0;
    if (shakeChance * gate <= OFFER_CHANCE_MAX_PERMILLE) return gate;
    uint16_t cappedGate = static_cast<uint16_t>(
        static_cast<float>(OFFER_CHANCE_MAX_PERMILLE) / shakeChance);
    return gate < cappedGate ? gate : cappedGate;
}

} // namespace

uint16_t offerChancePermille(const Species& species,
                             const Game::MonsterRuntime& monster,
                             bool boss, uint8_t foodBond) {
    uint16_t odds = adjustedCatchOdds(
        species.catchRate, monster.level, monster.majorStatus, boss);
    if (odds == 0) return 0;

    uint16_t gate = effectiveOfferGatePermille(odds, foodBond);
    float gbaChance = shakeSuccessChance(odds);
    uint32_t chance = static_cast<uint32_t>(
        roundf(gbaChance * gate));
    return static_cast<uint16_t>(
        chance > OFFER_CHANCE_MAX_PERMILLE
            ? OFFER_CHANCE_MAX_PERMILLE : chance);
}

bool passesOfferChecks(const Species& species,
                       const Game::MonsterRuntime& monster, bool boss,
                       uint16_t gateRoll,
                       const uint16_t shakeRolls[SHAKE_CHECK_COUNT],
                       uint8_t foodBond) {
    uint16_t odds = adjustedCatchOdds(
        species.catchRate, monster.level, monster.majorStatus, boss);
    if (!shakeRolls ||
        gateRoll >= effectiveOfferGatePermille(odds, foodBond)) {
        return false;
    }
    if (odds >= 255) return true;

    uint16_t threshold = gbaShakeThreshold(odds);
    for (uint8_t check = 0; check < SHAKE_CHECK_COUNT; ++check) {
        if (shakeRolls[check] >= threshold) return false;
    }
    return true;
}

static_assert(levelOddsPercent(5) == 120 &&
                  levelOddsPercent(25) == 100 &&
                  levelOddsPercent(70) == 70,
              "friendship level bands must remain ordered");
static_assert(statusOddsPercent(Game::MajorStatus::SLEEP) == 200 &&
                  statusOddsPercent(Game::MajorStatus::POISON) == 150 &&
                  statusOddsPercent(Game::MajorStatus::NONE) == 100,
              "friendship status bonuses must follow Gen III capture rules");
static_assert(adjustedCatchOdds(
                  45, 25, Game::MajorStatus::NONE, false) == 45 &&
                  adjustedCatchOdds(
                      45, 25, Game::MajorStatus::SLEEP, false) == 90 &&
                  adjustedCatchOdds(
                      45, 25, Game::MajorStatus::NONE, true) == 27,
              "friendship odds must include status and boss modifiers");
static_assert(addFoodBond(0, NORMAL_FOOD_BOND_GAIN) == 30 &&
                  addFoodBond(90, NORMAL_FOOD_BOND_GAIN) == FOOD_BOND_MAX &&
                  offerGatePermille(0) == OFFER_GATE_PERMILLE &&
                  offerGatePermille(FOOD_BOND_MAX) == OFFER_GATE_MAX_PERMILLE &&
                  acceptsNormalFood(false, NORMAL_FOOD_ACCEPT_PERCENT - 1) &&
                  !acceptsNormalFood(false, NORMAL_FOOD_ACCEPT_PERCENT),
              "food friendship tuning must remain bounded");
static_assert(acceptsFoodThrow(false, FoodTuning::ThrowClass::LIKED, 59) &&
                  !acceptsFoodThrow(false, FoodTuning::ThrowClass::LIKED, 60) &&
                  acceptsFoodThrow(true, FoodTuning::ThrowClass::DISLIKED, 19) &&
                  !acceptsFoodThrow(true, FoodTuning::ThrowClass::DISLIKED, 20) &&
                  throwBondGain(FoodTuning::ThrowClass::TASTY) == 35,
              "flavored food throw tuning must stay in sync with FoodTuning.h");

} // namespace FriendshipSystem
