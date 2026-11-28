#pragma once

#include <cstdint>

// Original SpriteCollab 60 Hz durations. Rendering may sample less frequently;
// elapsed time, rather than render count, determines the current pose.
namespace PokemonAnimationTiming {

enum class Action : uint8_t { Idle, Walk, Sleep };

namespace detail {
struct Timeline { uint16_t offset; uint8_t count; };
struct Record { uint16_t species; Timeline actions[3]; uint8_t stepFrames; };
#include "game/PokemonAnimationDurations.inc"

inline const Record* find(uint16_t species) {
    unsigned lo = 0, hi = sizeof(RECORDS) / sizeof(RECORDS[0]);
    while (lo < hi) {
        const unsigned mid = lo + (hi - lo) / 2;
        if (RECORDS[mid].species < species) lo = mid + 1;
        else hi = mid;
    }
    return lo < sizeof(RECORDS) / sizeof(RECORDS[0]) &&
        RECORDS[lo].species == species ? &RECORDS[lo] : nullptr;
}
}  // namespace detail

inline bool hasTiming(uint16_t species) { return detail::find(species) != nullptr; }

// Keep movement speed independent of the number of restored artwork frames.
inline uint8_t stepTimingFrameCount(uint16_t species, uint8_t fallback) {
    const auto* record = detail::find(species);
    return record && record->stepFrames ? record->stepFrames : fallback;
}

inline uint8_t frameAt(uint16_t species, Action action, uint64_t elapsedMs,
                       uint8_t fallback = 0) {
    const auto* record = detail::find(species);
    if (!record || unsigned(action) >= 3) return fallback;
    const auto& timeline = record->actions[unsigned(action)];
    const auto* ends = detail::END_TICKS + timeline.offset;
    const uint32_t period = uint32_t(ends[timeline.count - 1]) * 1000U;
    // Modulo before multiplication also handles UINT64_MAX without overflow.
    const uint32_t phase = uint32_t(((elapsedMs % period) * 60U) % period);
    for (uint8_t frame = 0; frame < timeline.count; ++frame)
        if (phase < uint32_t(ends[frame]) * 1000U) return frame;
    return 0;
}

// One clock per actor; facing is deliberately not part of its identity.
class Clock {
public:
    uint8_t update(uint16_t species, Action action, uint32_t nowMs, bool active = true) {
        if (!initialized_ || species_ != species || action_ != action) {
            elapsedMs_ = 0;
            species_ = species;
            action_ = action;
            initialized_ = true;
        } else if (active && active_) {
            elapsedMs_ += uint32_t(nowMs - lastMs_);
        }
        lastMs_ = nowMs;
        active_ = active;
        return frameAt(species, action, elapsedMs_);
    }
    void reset() { *this = {}; }
    uint64_t elapsedMs() const { return elapsedMs_; }

private:
    uint64_t elapsedMs_ = 0;
    uint32_t lastMs_ = 0;
    uint16_t species_ = 0;
    Action action_ = Action::Idle;
    bool initialized_ = false, active_ = false;
};

}  // namespace PokemonAnimationTiming
