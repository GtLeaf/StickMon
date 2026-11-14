#pragma once
#include <cstdint>

namespace Game::MarketSupply {
enum class Advice : uint8_t { NONE, EXPLORE, GROWTH, RARE };
enum class Plan : uint8_t { DEFAULT, EXPLORE, GROWTH, RARE_A, RARE_B, RARE_C };
inline constexpr uint8_t CAPACITIES[][8] = {
    {3,3,1,2,3,2,1,1}, {4,4,1,1,2,2,1,1}, {2,2,1,3,4,2,1,1},
    {2,3,2,2,3,2,1,1}, {3,2,1,2,3,2,1,2}, {2,2,2,2,3,2,1,2}
};
inline constexpr bool validPlan(uint8_t plan) { return plan <= uint8_t(Plan::RARE_C); }
// NONE is valid stored state, but is not a player advice request.
inline constexpr bool validAdvice(uint8_t advice) { return advice >= uint8_t(Advice::EXPLORE) && advice <= uint8_t(Advice::RARE); }
inline constexpr const uint8_t* capacities(uint8_t plan) { return CAPACITIES[validPlan(plan) ? plan : 0]; }
inline constexpr const uint8_t* capacities(Plan plan) { return capacities(uint8_t(plan)); }
inline constexpr uint8_t initialBoxStock(uint8_t plan) { return capacities(plan)[7]; }
inline constexpr uint8_t initialBoxStock(Plan plan) { return initialBoxStock(uint8_t(plan)); }
}
