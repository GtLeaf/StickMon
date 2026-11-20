#pragma once

#include <cstdint>

namespace ExploreHabitat {
inline constexpr uint8_t OPEN_WATER = 1U << 0;
inline constexpr uint8_t CAVE = 1U << 1;
inline constexpr uint8_t KNOWN = OPEN_WATER | CAVE;

constexpr bool matches(uint8_t required, uint8_t available) {
    return (required & available) == required;
}
} // namespace ExploreHabitat
