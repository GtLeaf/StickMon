#include "game/HomeChase.h"

#include <cmath>

#include "core/MathUtil.h"

namespace Home {

namespace {

static constexpr float CHASE_FOOTPRINT_SCALE = 0.60f;
static constexpr float CHASE_MIN_RADIUS_X = 3.0f;
static constexpr float CHASE_MIN_RADIUS_Y = 2.0f;

struct GroundFootprintOverride {
    uint16_t speciesId;
    float radiusX;
    float radiusY;
};

// Visual overhangs such as flowers, wings and tails are not floor occupancy.
// Keep overrides limited to silhouettes where the generic contact patch is
// still materially wider than the feet.
static constexpr GroundFootprintOverride FOOTPRINT_OVERRIDES[] = {
    {3, 11.5f, 5.0f},    // Venusaur
    {6, 10.5f, 4.5f},    // Charizard
    {9, 11.0f, 5.0f},    // Blastoise
    {76, 11.0f, 5.0f},   // Golem
    {130, 10.0f, 4.5f},  // Gyarados
    {143, 12.0f, 5.5f},  // Snorlax
    {149, 10.5f, 4.5f},  // Dragonite
    {262, 10.0f, 4.5f},  // Mightyena
    {323, 11.0f, 5.0f},  // Camerupt
};

}  // namespace

GroundFootprintProfile groundFootprintForSpecies(
    uint16_t speciesId, uint8_t visualWidth, uint8_t visualHeight) {
    for (const GroundFootprintOverride& profile : FOOTPRINT_OVERRIDES) {
        if (profile.speciesId == speciesId) {
            return {profile.radiusX, profile.radiusY};
        }
    }
    return {
        static_cast<float>(MathUtil::clamp(
            static_cast<int>(std::round(visualWidth * 0.18f)), 6, 12)),
        static_cast<float>(MathUtil::clamp(
            static_cast<int>(std::round(visualHeight * 0.08f)), 3, 6)),
    };
}

GroundFootprintProfile compactChaseFootprint(
    const GroundFootprintProfile& footprint) {
    return {
        MathUtil::max(
            CHASE_MIN_RADIUS_X, footprint.radiusX * CHASE_FOOTPRINT_SCALE),
        MathUtil::max(
            CHASE_MIN_RADIUS_Y, footprint.radiusY * CHASE_FOOTPRINT_SCALE),
    };
}

}  // namespace Home
