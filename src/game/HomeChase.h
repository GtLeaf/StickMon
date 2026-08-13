#pragma once

#include <cstdint>

namespace Home {

struct GroundFootprintProfile {
    float radiusX;
    float radiusY;
};

GroundFootprintProfile groundFootprintForSpecies(
    uint16_t speciesId, uint8_t visualWidth, uint8_t visualHeight);
GroundFootprintProfile compactChaseFootprint(
    const GroundFootprintProfile& footprint);

}  // namespace Home
