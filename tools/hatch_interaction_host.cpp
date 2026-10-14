#include "game/HatchInteraction.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>

int main() {
    Game::HatchShakeDetector detector;
    uint32_t now = 0;
    auto sample = [&](float x, float y = 0, float z = 1) {
        now += 20;
        return detector.sample(x, y, z, now);
    };
    for (int i = 0; i < 100; ++i) assert(!sample(0.01f * std::sin(i)));
    // A slow 180 degree rotation must not become a shake.
    for (int i = 0; i < 1000; ++i) {
        const float angle = i * 3.14159265f / 1000;
        assert(!sample(std::sin(angle), 0, std::cos(angle)));
    }
    detector.reset();
    for (int i = 0; i < 20; ++i) assert(!sample(0));
    assert(!sample(1.2f));  // A single tap/impact is ignored.
    for (int i = 0; i < 60; ++i) assert(!sample(0));
    assert(!sample(1.2f));
    assert(sample(-1.2f));
    // Continuous movement is one gesture, including after the cooldown.
    for (int i = 0; i < 80; ++i) assert(!sample(i % 2 ? -1.2f : 1.2f));
    for (int i = 0; i < 60; ++i) assert(!sample(0));
    assert(!sample(1.2f));
    assert(sample(-1.2f));
    assert(!sample(std::numeric_limits<float>::quiet_NaN()));
    now += 10000;
    assert(!sample(0, 1, 0));  // Resume in a new orientation.
    detector.reset();
    now = UINT32_MAX - 300;
    for (int i = 0; i < 20; ++i) assert(!sample(0));
    assert(!sample(1.2f));
    assert(sample(-1.2f));
    assert(Game::HatchShakeDetector::animationOffset(40) != 0);
    assert(Game::HatchShakeDetector::animationOffset(320) != 0);
    assert(Game::HatchShakeDetector::animationOffset(999) != 0);
    assert(Game::HatchShakeDetector::animationOffset(1000) == 0);
}
