#include <cassert>
#include <cstdio>
#include "game/MarketSession.h"

int main() {
    Market::Data d;
    d.entry = Market::node(15, Market::HEIGHT - 1);
    for (int y = 1; y < Market::HEIGHT; ++y) for (int x = 1; x < Market::WIDTH - 1; ++x) {
        d.roads.set(Market::node(x, y)); d.pedestrian.set(Market::node(x, y));
    }
    Market::Session s;
    assert(s.reset(d, 38, 264, 0, 0)); s.enableManual();
    auto& a = const_cast<Market::Actor&>(s.actor(0));
    a.stage = 2; a.from = a.to = Market::node(15, 15); a.goal = Market::NONE;
    s.update(0);
    s.update(200); s.update(400); s.update(600); s.update(800);
    assert(s.playerPose().animationFrame == 0); // Linoone's original 1s hold.
    s.update(1000);
    assert(s.playerPose().animationFrame == 1);
    s.pause(true); s.update(100000);
    assert(s.playerPose().animationFrame == 1);
    s.pause(false); s.update(100100);
    assert(s.playerPose().animationFrame == 1);
    s.setDirection(3); s.update(100117);
    assert(s.playerPose().moving && s.playerPose().animationFrame == 0);
    s.update(100251);
    assert(s.playerPose().animationFrame == 1);
    // Changing direction at the next step must not restart the walking loop.
    s.setDirection(0); s.update(100317);
    assert(s.playerPose().direction == 0);
    assert(s.playerPose().animationFrame == 1);
    s.update(100351);
    assert(s.playerPose().animationFrame == 2);
    s.update(100500, false);
    const auto frozen = s.playerPose().animationFrame;
    s.update(200000, false); s.update(200017);
    assert(s.playerPose().animationFrame == frozen);
    for (const uint16_t species : {1, 11, 41, 93, 94, 147, 148}) {
        Market::Session sample;
        assert(sample.reset(d, 38, species, 0, 0));
        sample.enableManual();
        auto& actor = const_cast<Market::Actor&>(sample.actor(0));
        actor.stage = 2;
        actor.from = actor.to = Market::node(15, 15);
        actor.goal = Market::NONE;
        sample.update(0);
        assert(PokemonAnimationTiming::hasTiming(species));
        PokemonAnimationTiming::Clock expected;
        expected.update(species, PokemonAnimationTiming::Action::Idle, 0);
        for (uint32_t now = 17; now <= 10000; now += 17) {
            if (now > 2000 && now % 170 == 0) sample.setDirection((now / 170) % 4);
            sample.update(now);
            const auto pose = sample.playerPose();
            const auto action = pose.moving ? PokemonAnimationTiming::Action::Walk
                                           : PokemonAnimationTiming::Action::Idle;
            assert(pose.animationFrame == expected.update(species, action, now));
        }
    }
    std::puts("market original timing: idle hold, walk, direction, pause pass");
}
