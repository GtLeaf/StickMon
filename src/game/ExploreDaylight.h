#pragma once

#include <cstdint>
#include "game/ExploreMapGenerator.h"

namespace ExploreDaylight {
struct Tone {
    uint16_t red = 256, green = 256, blue = 256;
};
inline bool operator==(Tone a, Tone b) {
    return a.red == b.red && a.green == b.green && a.blue == b.blue;
}
inline bool operator!=(Tone a, Tone b) { return !(a == b); }

inline Tone interpolate(Tone from, Tone to, uint16_t elapsed, uint16_t duration) {
    const auto mix = [elapsed, duration](int a, int b) {
        return uint16_t((a * (duration - elapsed) + b * elapsed + duration / 2) / duration);
    };
    return {mix(from.red, to.red), mix(from.green, to.green), mix(from.blue, to.blue)};
}

// Sixteen hours of daylight/twilight, eight hours of full night. This is
// presentation only: sleep, encounters and the game clock retain their rules.
inline Tone at(uint32_t gameMinutes, uint8_t area) {
    if (area == ExploreMapGenerator::FROST_CRYSTAL_CAVE_AREA) return {};
    constexpr Tone day{}, dawn{234, 218, 210}, dusk{256, 208, 172}, night{128, 152, 192};
    const uint16_t minute = gameMinutes % 1440;
    if (minute < 240 || minute >= 1200) return night;
    if (minute < 300) return interpolate(night, dawn, minute - 240, 60);
    if (minute < 360) return interpolate(dawn, day, minute - 300, 60);
    if (minute < 1080) return day;
    if (minute < 1140) return interpolate(day, dusk, minute - 1080, 60);
    return interpolate(dusk, night, minute - 1140, 60);
}
}  // namespace ExploreDaylight
