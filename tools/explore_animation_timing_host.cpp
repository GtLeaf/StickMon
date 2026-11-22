#include "game/ExploreEcology.h"
#include "assets/PokemonExpansionSpriteProfiles.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <iterator>

int main() {
    using namespace ExploreEcology;
    using PokemonAnimationTiming::Action;
    bool longIdle = false, longWalk = false, longSleep = false, changedActivity = false;
    for (const auto& profile : PokemonSprites::EXPANSION_ROOM_PROFILES) {
        const auto id = profile.speciesId;
        for (uint32_t seed = 1; seed <= 4; ++seed) {
            ExploreMapGenerator::Map map{};
            map.seed = seed;
            std::fill_n(map.layers[0], ExploreMapGenerator::CELL_COUNT,
                waterOnlySpecies(id) ? 1097 : 385);
            map.pathCount = 1;
            map.paths[0].pointCount = 3;
            map.paths[0].points[0] = {1, 5};
            map.paths[0].points[1] = {7, 5};
            map.paths[0].points[2] = {14, 5};
            ExplorePool::Pool pool{};
            pool.count = 1;
            pool.entries[0] = {id, 10, ExplorePool::Rarity::COMMON};
            Simulation simulation;
            // Family-enabled species may admit whitelisted companions even
            // from a one-species pool. Select deterministic solo fixtures so
            // the actor below always represents the requested species.
            for (unsigned attempt = 0; attempt < 64; ++attempt) {
                map.seed = seed + attempt * 4;
                simulation.reset(map, 0, pool, 0, 0);
                if (simulation.count() == 1 && simulation.pose(0).speciesId == id) break;
            }
            assert(simulation.count() == 1 && simulation.pose(0).speciesId == id);
            PokemonAnimationTiming::Clock expected;
            expected.update(id, animationAction(simulation.pose(0)), 0,
                !simulation.pose(0).hidden && !simulation.pose(0).absent);
            uint32_t pauseOffset = 0, activityElapsed = 0, previousNow = 0;
            for (uint32_t elapsed = 100; elapsed <= 120000; elapsed += 100) {
                const auto before = simulation.pose(0);
                const auto previousAction = animationAction(before);
                uint32_t now = elapsed + pauseOffset;
                if (elapsed == 10000) {
                    simulation.update(now, false, -100, -100, {0,0,416,312});
                    expected.update(id, previousAction, now, false);
                    assert(samePose(before, simulation.pose(0)));
                    pauseOffset += 5000;
                    now += 5000;
                }
                simulation.update(now, true, -100, -100, {0,0,416,312});
                const auto& pose = simulation.pose(0);
                const auto action = animationAction(pose);
                activityElapsed = before.activity == pose.activity
                    ? activityElapsed + std::min<uint32_t>(now - previousNow, 250) : 0;
                previousNow = now;
                if (pose.activity == Activity::GATHER || pose.activity == Activity::SPLASH)
                    assert(pose.frame == (activityElapsed / 150) % 6);
                const bool visible = !pose.hidden && !pose.absent;
                const auto frame = expected.update(id, action, now, visible);
                if (visible && pose.artworkFrame() != frame)
                    std::fprintf(stderr, "species=%u now=%u action=%u activity=%u actual=%u expected=%u hidden=%u absent=%u x=%f y=%f\n",
                        id, now, unsigned(action), unsigned(pose.activity), pose.artworkFrame(), frame,
                        pose.hidden, pose.absent, pose.x, pose.y);
                if (visible) {
                    assert(pose.artworkFrame() == frame);
                    if (action != previousAction) assert(pose.artworkFrame() == 0);
                }
                if (pose.activity == Activity::DOZE) {
                    assert(pose.lift >= 0 && pose.lift <= 1);
                    assert(pose.frame == pose.lift);
                }
                changedActivity |= before.activity != pose.activity && action == previousAction && frame != 0;
                longIdle |= action == Action::Idle && frame > 5;
                longWalk |= action == Action::Walk && frame > 5;
                longSleep |= action == Action::Sleep && frame > 1;
            }
        }
    }
    assert(longIdle && longWalk && longSleep && changedActivity);
    std::printf("ecology timing: %zu species; original idle/walk/sleep phases, full frames, continuous actions, pause and legacy sleep lift verified\n",
                std::size(PokemonSprites::EXPANSION_ROOM_PROFILES));
}
