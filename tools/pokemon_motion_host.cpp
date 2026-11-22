#include <array>
#include <cassert>
#include <cstdint>

#include "assets/PokemonMotion.h"
#include "game/PokemonAnimationTiming.h"

namespace {

template <size_t N>
void assertWalkSequence(uint8_t frameCount, uint16_t stepDurationMs,
                        const std::array<uint8_t, N>& expected) {
    const PokemonMotion::Behavior walk =
        PokemonMotion::behaviorForMode(PokemonMotion::Mode::WALK);
    assert(stepDurationMs == expected.size() * PokemonMotion::WALK_ROUTE_PHASE_MS);
    for (size_t index = 0; index < expected.size(); ++index) {
        uint32_t elapsedMs =
            static_cast<uint32_t>(index) * PokemonMotion::WALK_ROUTE_PHASE_MS;
        assert(PokemonMotion::movementFrame(
                   walk, frameCount, elapsedMs, elapsedMs, stepDurationMs) ==
               expected[index]);
    }
}

}  // namespace

int main() {
    const PokemonMotion::Behavior walk =
        PokemonMotion::behaviorForMode(PokemonMotion::Mode::WALK);
    const PokemonMotion::Behavior hop =
        PokemonMotion::behaviorForMode(PokemonMotion::Mode::HOP);
    const PokemonMotion::Behavior slither =
        PokemonMotion::behaviorForMode(PokemonMotion::Mode::SLITHER);

    assert(PokemonMotion::routeStepDurationMs(walk, 1) == 220);
    assert(PokemonMotion::routeStepDurationMs(walk, 2) == 300);
    assert(PokemonMotion::routeStepDurationMs(walk, 3) == 400);
    assert(PokemonMotion::routeStepDurationMs(walk, 4) == 500);
    assert(PokemonMotion::routeStepDurationMs(hop, 3) == 520);
    assert(PokemonMotion::routeStepDurationMs(slither, 3) == 220);
    // Restoring source frame counts cannot change existing path travel speed.
    for (const uint16_t species : {1, 11, 41, 94, 129, 147, 148, 362}) {
        const auto behavior = PokemonMotion::behaviorForSpecies(species);
        const uint8_t legacyFrames = species == 129 || species == 362 ? 1 : 3;
        assert(PokemonAnimationTiming::hasTiming(species));
        for (uint8_t artworkFrames : {uint8_t(4), uint8_t(6), uint8_t(8), uint8_t(12)}) {
            assert(PokemonMotion::routeStepDurationMs(behavior,
                PokemonAnimationTiming::stepTimingFrameCount(species, artworkFrames)) ==
                PokemonMotion::routeStepDurationMs(behavior, legacyFrames));
        }
    }
    for (const uint16_t species : {380, 381}) {
        const auto air = PokemonMotion::airProfileForSpecies(species);
        assert(air.height == 18 && air.bobAmplitude == 0);
    }
    assert(PokemonMotion::airProfileForSpecies(41).bobAmplitude == 2);
    assert(PokemonMotion::airProfileForSpecies(362).bobAmplitude == 2);

    assertWalkSequence(2, 300, std::array<uint8_t, 3>{0, 1, 0});
    assertWalkSequence(3, 400, std::array<uint8_t, 4>{0, 1, 2, 0});
    assertWalkSequence(4, 500, std::array<uint8_t, 5>{0, 1, 2, 3, 0});
    return 0;
}
