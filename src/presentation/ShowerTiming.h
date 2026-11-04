#pragma once

#include <cstdint>

// Shared by the bath coordinator and its visual effects.
namespace ShowerTiming {

constexpr float SHAKE_THRESHOLD_G = 0.38f;
constexpr float GRAVITY_SMOOTHING_HZ = 2.0f;
constexpr float TOOL_SHAKE_GAIN = 60.0f;
constexpr uint32_t SOAP_SWING_COOLDOWN_MS = 180;
constexpr uint32_t BRUSH_RUB_COOLDOWN_MS = 150;
constexpr uint32_t RINSE_FOAM_INTERVAL_MS = 350;
constexpr uint32_t RINSE_MIN_DURATION_MS = 1800;
constexpr uint16_t ATMOSPHERE_FADE_OUT_PER_SECOND = 420;
constexpr uint32_t COMPLETE_DURATION_MS = 1500;
constexpr uint32_t INCOMPLETE_DURATION_MS = 700;
constexpr uint32_t EXP_FLOAT_DURATION_MS = 1100;
constexpr uint32_t EXP_FLOAT_FADE_START_MS = 500;
constexpr float EXP_FLOAT_RISE_PX = 24.0f;
constexpr uint32_t HOP_DURATION_MS = 320;
constexpr uint32_t WIGGLE_DURATION_MS = 220;
constexpr uint32_t WIGGLE_FLIP_INTERVAL_MS = 70;
constexpr uint32_t RUB_REACTION_COOLDOWN_MS = 1500;
constexpr uint32_t TURN_HOLD_MS = 2600;
constexpr uint32_t SOAP_HOP_INTERVAL_MS = 900;
constexpr uint32_t RINSE_SHAKE_INTERVAL_MS = 800;
constexpr uint8_t BODY_FOAM_STAGE_COUNT = 4;
constexpr uint32_t RINSE_WORST_CASE_MS =
    BODY_FOAM_STAGE_COUNT * RINSE_FOAM_INTERVAL_MS +
    (255000UL + ATMOSPHERE_FADE_OUT_PER_SECOND - 1) /
        ATMOSPHERE_FADE_OUT_PER_SECOND;
static_assert(RINSE_WORST_CASE_MS <= 3000,
              "all body foam and atmosphere must rinse away within 3 seconds");

}  // namespace ShowerTiming
