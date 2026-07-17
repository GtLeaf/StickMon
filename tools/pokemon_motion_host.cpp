#include <array>
#include <cassert>
#include <cstdint>

#include "assets/PokemonMotion.h"

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

    assertWalkSequence(2, 300, std::array<uint8_t, 3>{0, 1, 0});
    assertWalkSequence(3, 400, std::array<uint8_t, 4>{0, 1, 2, 0});
    assertWalkSequence(4, 500, std::array<uint8_t, 5>{0, 1, 2, 3, 0});
    return 0;
}
