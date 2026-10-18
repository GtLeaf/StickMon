#include "game/ExploreGrass.h"
#include <cassert>
#include <cstdio>

using namespace ExploreGrass;
int main() {
    ExploreMapGenerator::Map map{};
    map.seed = 42;
    constexpr int width = ExploreMapGenerator::WIDTH;
    constexpr int low = 4 * width + 3, high = 4 * width + 5;
    map.layers[0][low] = 390;
    map.layers[0][high] = 399;
    map.layers[1][high - width] = 391;
    assert(Simulation::kindAt(map, low) == Kind::SHORT);
    assert(Simulation::kindAt(map, high) == Kind::HIGH);
    assert(Simulation::kindAt(map, high - width) == Kind::HIGH);
    assert(Simulation::kindAt(map, 192) == Kind::NONE);
    Simulation simulation;
    Contact contact{81, 116, true};
    simulation.update(map, 0, true, &contact, 1);
    simulation.update(map, 1000, true, &contact, 1);
    assert(simulation.frame(low).phase == 0); // Standing never retriggers.
    contact.x += 3;
    simulation.update(map, 1020, true, &contact, 1);
    assert(simulation.frame(low).phase == 0);
    contact.x += 3;
    simulation.update(map, 1040, true, &contact, 1);
    assert(simulation.frame(low).kind == Kind::SHORT);
    assert(simulation.frame(low).phase == 1);
    contact.grounded = false;
    const Frame paused = simulation.frame(low);
    simulation.update(map, 10000, false, &contact, 1);
    assert(simulation.frame(low) == paused);
    for (int i = 1; i <= 8; ++i)
        simulation.update(map, 10000 + i * 90, true, &contact, 1);
    assert(simulation.frame(low).phase == 0); // Finishes after departure.
    contact = {134, 90, true}; // Tall top canonicalizes to its base.
    simulation.update(map, 11000, true, &contact, 1);
    contact.x += 7;
    simulation.update(map, 11080, true, &contact, 1);
    assert(simulation.frame(high).kind == Kind::HIGH);
    assert(simulation.frame(high - width).phase == 0);
    const Frame before = simulation.frame(high);
    contact.x += 6;
    simulation.update(map, 11120, true, &contact, 1);
    assert(simulation.frame(high) == before); // Contact cooldown.
    contact = {81, 116, false};
    simulation.update(map, 11180, true, &contact, 1);
    contact.x += 8;
    simulation.update(map, 11200, true, &contact, 1);
    assert(simulation.frame(low).phase == 0); // Fliers never trample.
    contact.grounded = true;
    simulation.update(map, 11300, true, &contact, 1);
    contact.x = 160;
    simulation.update(map, 11400, true, &contact, 1);
    assert(simulation.frame(low).phase == 0); // Teleport does not paint a trail.
    map.seed++;
    simulation.update(map, 11420, true, &contact, 1);
    assert(simulation.frame(high).phase == 0);
    // A trailing young actor can start a second impulse after the first step.
    Contact pair[] = {{81, 116, true}, {58, 116, true}};
    simulation.update(map, 11500, true, pair, 2);
    pair[0].x += 6;
    simulation.update(map, 11580, true, pair, 2);
    assert(simulation.frame(low).phase == 1);
    simulation.update(map, 11780, true, pair, 2);
    pair[1].x = 85;
    simulation.update(map, 11880, true, pair, 2);
    assert(simulation.frame(low).phase == 1);
    // Independent instances and slots produce identical results without RNG.
    Simulation other;
    simulation.reset(12000);
    other.reset(12000);
    Contact actors[ACTORS]{};
    for (int tick = 0; tick < 150; ++tick) {
        for (int i = 0; i < ACTORS; ++i) actors[i] = {float(65 + (tick + i * 5) % 80), 116, true};
        simulation.update(map, 12000 + tick * 80, true, actors, ACTORS);
        other.update(map, 12000 + tick * 80, true, actors, ACTORS);
        for (int c = 0; c < 192; ++c) assert(simulation.frame(c) == other.frame(c));
    }
    std::puts("grass simulation: movement, idle, rebound, pause, flight, paired steps, map reset and determinism verified");
}
