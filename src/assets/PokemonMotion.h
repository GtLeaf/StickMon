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
    bool registeredSlither;
};

enum class PlaybackContext : uint8_t {
    ROUTE,
    AMBIENT,
};

struct Pose {
    uint8_t frameIndex;
    uint8_t phaseIndex;
    int8_t offsetX;
    int8_t offsetY;
    bool directionChangeSafe;
};

static constexpr uint8_t SLITHER_PHASE_COUNT = 4;
static constexpr uint8_t SLITHER_TIMING_UNITS = 34;
static constexpr uint8_t SLITHER_EXTENDED_START_UNIT = 10;
static constexpr uint8_t SLITHER_RETURN_START_UNIT = 18;
static constexpr uint8_t SLITHER_IDLE_START_UNIT = 26;
static constexpr uint8_t SLITHER_RETURN_PHASE_INDEX = 2;
static constexpr uint8_t SLITHER_IDLE_PHASE_INDEX = 3;
static constexpr uint16_t WALK_ROUTE_PHASE_MS = 100;
static constexpr uint16_t SLITHER_ROUTE_CYCLE_MS = 700;
static constexpr uint16_t SLITHER_AMBIENT_MIN_CYCLE_MS = 1100;
static constexpr uint16_t SLITHER_AMBIENT_MAX_CYCLE_MS = 1400;

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
        return {Mode::HOP, 260, 520, false};
    case Mode::SLITHER:
        return {Mode::SLITHER, 170, 220, false};
    case Mode::WALK:
    default:
        return {Mode::WALK, 170, 220, false};
    }
}

inline Behavior behaviorForSpecies(uint16_t speciesId) {
    Behavior behavior = behaviorForMode(modeForSpecies(speciesId));
    // Dratini's extracted frames need their original per-phase registration restored.
    behavior.registeredSlither = speciesId == 147;
    return behavior;
}

inline uint16_t routeStepDurationMs(const Behavior& behavior,
                                    uint8_t frameCount) {
    if (behavior.mode != Mode::WALK || frameCount <= 1) {
        return behavior.stepDurationMs;
    }

    uint32_t durationMs =
        static_cast<uint32_t>(frameCount + 1U) * WALK_ROUTE_PHASE_MS;
    if (durationMs < behavior.stepDurationMs) {
        durationMs = behavior.stepDurationMs;
    }
    if (durationMs > 0xFFFFU) durationMs = 0xFFFFU;
    return static_cast<uint16_t>(durationMs);
}

inline uint16_t cycleDurationMs(const Behavior& behavior, PlaybackContext context,
                                float speedPxPerSecond = 0.0f) {
    if (behavior.mode != Mode::SLITHER) return behavior.stepDurationMs;
    if (context == PlaybackContext::ROUTE) return SLITHER_ROUTE_CYCLE_MS;

    uint32_t durationMs = SLITHER_AMBIENT_MAX_CYCLE_MS;
    if (speedPxPerSecond > 0.1f) {
        durationMs = static_cast<uint32_t>(13000.0f / speedPxPerSecond + 0.5f);
    }
    if (durationMs < SLITHER_AMBIENT_MIN_CYCLE_MS) {
        durationMs = SLITHER_AMBIENT_MIN_CYCLE_MS;
    }
    if (durationMs > SLITHER_AMBIENT_MAX_CYCLE_MS) {
        durationMs = SLITHER_AMBIENT_MAX_CYCLE_MS;
    }
    return static_cast<uint16_t>(durationMs);
}

inline uint8_t slitherPhaseIndex(uint32_t elapsedMs, uint16_t cycleMs) {
    if (cycleMs == 0) cycleMs = 1;
    uint16_t phaseMs = static_cast<uint16_t>(elapsedMs % cycleMs);
    uint8_t unit = static_cast<uint8_t>(
        (static_cast<uint32_t>(phaseMs) * SLITHER_TIMING_UNITS) / cycleMs);
    if (unit < SLITHER_EXTENDED_START_UNIT) return 0;
    if (unit < SLITHER_RETURN_START_UNIT) return 1;
    if (unit < SLITHER_IDLE_START_UNIT) return 2;
    return 3;
}

inline bool slitherDirectionChangeSafe(uint8_t phaseIndex) {
    return phaseIndex == 0 || phaseIndex >= SLITHER_RETURN_PHASE_INDEX;
}

inline uint32_t slitherReturnPhaseStartMs(uint16_t cycleMs) {
    if (cycleMs == 0) cycleMs = 1;
    return (static_cast<uint32_t>(cycleMs) * SLITHER_RETURN_START_UNIT +
            SLITHER_TIMING_UNITS - 1U) /
           SLITHER_TIMING_UNITS;
}

inline Pose slitherPose(const Behavior& behavior, uint8_t frameCount,
                        uint32_t elapsedMs, uint16_t cycleMs,
                        uint8_t directionIndex) {
    Pose pose{0, 0, 0, 0, true};
    if (frameCount <= 1) return pose;

    if (frameCount != 3) {
        uint8_t phaseCount = static_cast<uint8_t>((frameCount - 1) * 2);
        uint16_t frameMs = behavior.moveFrameMs > 0 ? behavior.moveFrameMs : 1;
        uint8_t phase = static_cast<uint8_t>((elapsedMs / frameMs) % phaseCount);
        pose.frameIndex = phase < frameCount
            ? phase
            : static_cast<uint8_t>(phaseCount - phase);
        return pose;
    }

    static constexpr uint8_t FRAMES[SLITHER_PHASE_COUNT] = {1, 2, 1, 0};
    static constexpr int8_t OFFSET_X[8][SLITHER_PHASE_COUNT] = {
        {-1,  3, -1, 0},
        {-3, -5, -5, 0},
        { 0, -6, -4, 0},
        {-1, -6, -3, 0},
        { 1,  0,  1, 0},
        { 1,  6,  3, 0},
        { 2,  6,  6, 0},
        { 1,  5,  3, 0},
    };
    static constexpr int8_t OFFSET_Y[8][SLITHER_PHASE_COUNT] = {
        { 2,  8,  6, 0},
        { 4,  8,  6, 0},
        { 2,  2,  2, 0},
        {-2, -2, -4, 0},
        {-2, -2, -6, 0},
        { 0, -4, -2, 0},
        { 0, -4,  0, 0},
        {-2,  2,  0, 0},
    };

    pose.phaseIndex = slitherPhaseIndex(elapsedMs, cycleMs);
    pose.frameIndex = FRAMES[pose.phaseIndex];
    pose.directionChangeSafe = slitherDirectionChangeSafe(pose.phaseIndex);
    if (behavior.registeredSlither) {
        directionIndex %= 8;
        pose.offsetX = OFFSET_X[directionIndex][pose.phaseIndex];
        pose.offsetY = OFFSET_Y[directionIndex][pose.phaseIndex];
    }
    return pose;
}

inline uint16_t slitherSettleFrameMs(uint16_t cycleMs) {
    if (cycleMs == 0) cycleMs = 1;
    uint32_t duration = (static_cast<uint32_t>(cycleMs) * 8U +
                         SLITHER_TIMING_UNITS - 1U) /
                        SLITHER_TIMING_UNITS;
    return static_cast<uint16_t>(duration > 0 ? duration : 1);
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
            return slitherPose(
                behavior, frameCount, movementElapsedMs,
                cycleDurationMs(behavior, PlaybackContext::ROUTE), 0).frameIndex;
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
