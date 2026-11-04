#pragma once

#include <cstdint>

// Shared by the exploration coordinator and its battle presentation.
namespace ExploreBattleTiming {

constexpr uint32_t EXP_ANIMATION_MS = 900;
constexpr uint32_t BATTLE_HIT_DELAY_MS = 260;
constexpr uint32_t BATTLE_HIT_SHAKE_MS = 280;
constexpr uint32_t BATTLE_HP_DRAIN_MS = 420;
constexpr uint32_t BATTLE_ACTION_MS =
    BATTLE_HIT_DELAY_MS + BATTLE_HIT_SHAKE_MS + BATTLE_HP_DRAIN_MS;
constexpr uint32_t BATTLE_SWITCH_PHASE_MS = 360;
constexpr uint32_t FOOD_THROW_DURATION_MS = 700;
constexpr int BATTLE_SWITCH_TRAVEL_X = 120;

}  // namespace ExploreBattleTiming
