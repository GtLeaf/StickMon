#pragma once

#include <cstdint>

namespace PokemonMotion {

enum class Mode : uint8_t {
    WALK,
    HOP,
    SLITHER,
};

struct Behavior {
    Mode mode;
    uint16_t moveFrameMs;
    uint16_t stepDurationMs;
};

inline Mode modeForSpecies(uint16_t speciesId) {
    switch (speciesId) {
    case 1:
        return Mode::HOP;
    case 147:
    case 148:
        return Mode::SLITHER;
    default:
        return Mode::WALK;
    }
}

inline Behavior behaviorForMode(Mode mode) {
    switch (mode) {
    case Mode::HOP:
        // One cycle covers crouching, takeoff, and landing.
        return {Mode::HOP, 260, 520};
    case Mode::SLITHER:
        return {Mode::SLITHER, 170, 220};
    case Mode::WALK:
    default:
        return {Mode::WALK, 170, 220};
    }
}

inline Behavior behaviorForSpecies(uint16_t speciesId) {
    return behaviorForMode(modeForSpecies(speciesId));
}

inline float stepPosition(const Behavior& behavior, float progress) {
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    if (behavior.mode == Mode::SLITHER) return progress;
    return progress * progress * (3.0f - 2.0f * progress);
}

inline uint8_t movementFrame(const Behavior& behavior, uint8_t frameCount,
                             uint32_t movementElapsedMs, uint32_t stepElapsedMs,
                             uint16_t stepDurationMs) {
    if (frameCount <= 1) return 0;

    if (behavior.mode == Mode::SLITHER) {
        if (frameCount == 3) {
            static constexpr uint16_t COILED_MS = 70;
            static constexpr uint16_t EXTENDING_MS = 180;
            static constexpr uint16_t EXTENDED_MS = 180;
            static constexpr uint16_t RETURNING_MS = 230;
            static constexpr uint16_t CYCLE_MS =
                COILED_MS + EXTENDING_MS + EXTENDED_MS + RETURNING_MS;
            static_assert(CYCLE_MS == 220 * 3,
                          "slither cycle must align with three route steps");

            uint16_t phaseMs = static_cast<uint16_t>(movementElapsedMs % CYCLE_MS);
            if (phaseMs < COILED_MS) return 0;
            phaseMs -= COILED_MS;
            if (phaseMs < EXTENDING_MS) return 1;
            phaseMs -= EXTENDING_MS;
            if (phaseMs < EXTENDED_MS) return 2;
            return 1;
        }

        uint16_t frameMs = behavior.moveFrameMs > 0 ? behavior.moveFrameMs : 1;
        uint8_t phaseCount = static_cast<uint8_t>((frameCount - 1) * 2);
        uint8_t phase = static_cast<uint8_t>(
            (movementElapsedMs / frameMs) % phaseCount);
        return phase < frameCount ? phase : static_cast<uint8_t>(phaseCount - phase);
    }

    if (stepDurationMs == 0) stepDurationMs = 1;
    if (stepElapsedMs >= stepDurationMs) stepElapsedMs = stepDurationMs - 1;
    uint8_t cycleFrames = static_cast<uint8_t>(frameCount + 1);
    uint8_t cycleIndex = static_cast<uint8_t>(
        (static_cast<uint64_t>(stepElapsedMs) * cycleFrames) / stepDurationMs);
    if (cycleIndex >= cycleFrames) cycleIndex = cycleFrames - 1;
    return cycleIndex < frameCount ? cycleIndex : 0;
}

}  // namespace PokemonMotion
