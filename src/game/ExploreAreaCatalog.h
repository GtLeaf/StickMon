#pragma once

#include <cstdint>

#include "game/GameState.h"

namespace ExploreAreaCatalog {

inline constexpr uint8_t recommendedLevel(uint8_t area) {
    return area == 1 ? 12
         : area == 2 ? 22
         : area == 3 ? 34
         : area == 4 ? 47
         : area == 5 ? 60
                     : 5;
}

inline constexpr uint16_t fieldColor(uint8_t area) {
    return area == 1 ? 0x224A
         : area == 2 ? 0x2A66
         : area == 3 ? 0xB6DB
         : area == 4 ? 0x1945
         : area == 5 ? 0x1987
                     : 0x2227;
}

static_assert(recommendedLevel(0) == 5 && recommendedLevel(5) == 60,
              "explore area levels must match progression tuning");
static_assert(fieldColor(0) == 0x2227 && fieldColor(5) == 0x1987,
              "explore field colors must match route rendering");

}  // namespace ExploreAreaCatalog
