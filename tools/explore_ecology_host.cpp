#include "game/ExploreEcology.h"
#include "game/ExploreEncounters.h"
#include "game/ExploreRouteGeometry.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

using ExploreEcology::Simulation;
using ExploreEcology::Pose;

static void equal(const Simulation& a, const Simulation& b) {
    assert(a.count() == b.count());
    for (uint8_t i = 0; i < a.count(); ++i)
        assert(ExploreEcology::samePose(a.pose(i), b.pose(i)));
}

static ExplorePool::Pool pool() {
    ExplorePool::Pool result{};
    result.count = 6;
    const uint16_t species[] = {10, 161, 16, 261, 280, 133};
    for (uint8_t i = 0; i < result.count; ++i)
        result.entries[i] = {species[i], 10, i == 5 ? ExplorePool::Rarity::RARE
                                                   : ExplorePool::Rarity::COMMON};
    return result;
}

static void checkEpisodes() {
    using ExploreEcology::Activity;
    using ExploreEcology::Surface;
    bool seen[ExploreEcology::ACTIVITY_COUNT]{};
    bool curiousReturned = false, aerialReturned = false;
    // Real episode triggers, with no test-only access to actor state.
    for (uint16_t species : {uint16_t(161), uint16_t(172), uint16_t(285), uint16_t(41), uint16_t(380), uint16_t(129)}) {
        for (uint32_t seed = 1; seed <= 12; ++seed) {
            ExploreMapGenerator::Map map{};
            map.seed = seed;
            map.areaIndex = species == 129 ? 1 : 0;
            std::fill_n(map.layers[0], ExploreMapGenerator::CELL_COUNT,
                species == 129 ? 1097 : 390);
            map.pathCount = 1;
            auto& path = map.paths[0];
            path.pointCount = 14;
            for (uint8_t x = 1; x <= 14; ++x) path.points[x - 1] = {x, 6};
            ExplorePool::Pool active{};
            active.count = 1;
            active.entries[0] = {species, 1, ExplorePool::Rarity::COMMON};
            Simulation sim;
            sim.reset(map, 0, active, 0, 0);
            if (sim.hasGroup()) continue; // Independent episodes use non-group rolls.
            assert(sim.count() == 1);
            const auto origin = sim.pose(0);
            uint32_t lastEnd = 0;
            Activity previous = Activity::REST;
            bool returned = false, testedPause[ExploreEcology::ACTIVITY_COUNT]{};
            for (uint32_t ms = 100; ms <= 90000; ms += 100) {
                const auto before = sim.pose(0);
                float px = -100, py = -100;
                if (species == 161 && ms < 30000) {
                    px = before.x + 18; py = before.y;
                } else if (species == 172) {
                    px = origin.x + 45; py = origin.y;
                } else if (species == 285 && before.activity == Activity::DOZE && ms % 2000 == 0) {
                    px = before.x; py = before.y - 15;
                }
                sim.update(ms, true, px, py, {0, 0, 416, 312});
                const auto pose = sim.pose(0);
                seen[unsigned(pose.activity)] = true;
                assert(sim.canStand(pose.speciesId, pose.x, pose.y));
                assert(pose.surface == (species == 129 ? Surface::WATER : Surface::GRASS));
                const float dx = pose.x - origin.x, dy = pose.y - origin.y;
                assert(dx * dx + dy * dy <= 26 * 26 * 2 + 0.01f);
                if (previous == Activity::RETURN &&
                    (pose.activity == Activity::REST || pose.activity == Activity::CRUISE)) {
                    returned = true;
                    assert(!pose.walking);
                    assert(pose.lift == (pose.airborne ? 8 : 0));
                }
                const bool wasQuiet = previous == Activity::REST || previous == Activity::WALK || previous == Activity::CRUISE;
                if (wasQuiet && ExploreEcology::isEpisode(pose.activity) && lastEnd)
                    assert(ms - lastEnd >= 15000);
                if (ExploreEcology::isEpisode(previous) &&
                    (pose.activity == Activity::REST || pose.activity == Activity::CRUISE))
                    lastEnd = ms;
                if (!testedPause[unsigned(pose.activity)]) {
                    auto running = sim, paused = sim;
                    paused.update(ms + 10000, false, px, py, {0, 0, 416, 312});
                    equal(sim, paused);
                    paused.update(ms + 10100, true, px, py, {0, 0, 416, 312});
                    running.update(ms + 100, true, px, py, {0, 0, 416, 312});
                    equal(running, paused);
                    testedPause[unsigned(pose.activity)] = true;
                }
                previous = pose.activity;
            }
            if (species == 172 && returned) curiousReturned = true;
            if (species == 41 && returned) aerialReturned = true;
        }
    }
    for (Activity activity : {Activity::DOZE, Activity::WAKE, Activity::WATCH,
        Activity::APPROACH, Activity::OBSERVE, Activity::RETURN, Activity::FLEE,
        Activity::HIDE, Activity::PEEK, Activity::TAKEOFF, Activity::FLY,
        Activity::LAND, Activity::GATHER, Activity::JUMP, Activity::SPLASH,
        Activity::BURROW, Activity::EMERGE}) {
        if (!seen[unsigned(activity)]) std::fprintf(stderr, "missing episode activity %u\n", unsigned(activity));
        assert(seen[unsigned(activity)]);
    }
    assert(curiousReturned && aerialReturned);
    std::puts("episodes: sleep/wake, approach/return, flee/hide/peek, flight/landing, jump/splash, cooldown and phase pause verified");
}

static void checkLocomotionAndCover() {
    using namespace ExploreEcology;
    const View view{0, 0, 416, 312};
    for (uint16_t species : {uint16_t(41), uint16_t(42), uint16_t(169), uint16_t(12),
            uint16_t(16), uint16_t(17), uint16_t(18), uint16_t(161)}) {
      for (bool isolated : {false, true}) {
        ExploreMapGenerator::Map map{};
        map.seed = 7;
        map.pathCount = 1;
        map.paths[0].pointCount = 14;
        for (uint8_t x = 1; x <= 14; ++x) map.paths[0].points[x - 1] = {x, 6};
        std::fill_n(map.layers[0], ExploreMapGenerator::CELL_COUNT, isolated ? 64 : 385);
        map.layers[0][4 * ExploreMapGenerator::WIDTH + 4] = 385;
        ExplorePool::Pool active{};
        active.count = 1;
        active.entries[0] = {species, 1, ExplorePool::Rarity::COMMON};
        Simulation sim;
        sim.reset(map, 0, active, 0, 0);
        for (uint32_t seed = 8; sim.hasGroup() && seed < 72; ++seed) {
            map.seed = seed;
            sim.reset(map, 0, active, 0, 0);
        }
        assert(!sim.hasGroup() && sim.count() == 1);
        const bool flap = locomotionForSpecies(species) == Locomotion::FLAP;
        bool flew = false;
        for (uint32_t ms = 100; ms <= 60000; ms += 100) {
            const auto before = sim.pose(0);
            sim.update(ms, true, before.x + 18, before.y, view);
            const auto& pose = sim.pose(0);
            assert(pose.cover == Cover::NONE);
            assert(pose.activity != Activity::HIDE && pose.activity != Activity::BURROW);
            if (flap) {
                assert(pose.airborne && pose.lift >= 6);
                assert(pose.activity == Activity::CRUISE || pose.activity == Activity::FLY ||
                    pose.activity == Activity::RETURN);
                assert(pose.frame == ((ms % 7200) / 120) % 3);
                const float dx = pose.x - before.x, dy = pose.y - before.y;
                assert(dx * dx + dy * dy > 0.0001f); // No stationary airborne tick.
                assert(dx * dx + dy * dy < 9); // No state-transition position jump.
                flew |= pose.activity == Activity::FLY;
            } else {
                assert(!pose.airborne && pose.lift == 0);
                assert(pose.activity != Activity::FLY && pose.activity != Activity::TAKEOFF);
            }
            if (ms == 12300) {
                auto paused = sim, running = sim;
                paused.update(ms + 30000, false, 0, 0, view);
                equal(sim, paused);
                paused.update(ms + 30100, true, 0, 0, view);
                running.update(ms + 100, true, 0, 0, view);
                equal(paused, running);
            }
        }
        if (flap) assert(flew != isolated);
        if (flap) {
            Simulation unknown;
            unknown.reset(map, 0, active, 1, 0);
            for (uint32_t ms = 100; ms <= 20000; ms += 100) {
                unknown.update(ms, true, 0, 0, view);
                assert(unknown.pose(0).hidden && !unknown.pose(0).airborne);
                assert(!isEpisode(unknown.pose(0).activity));
            }
        }
      }
    }
    // Actual dense grass, shrubs, trees and rocks are usable as cover. An
    // isolated covered spot still peeks/crouches without sinking into the floor.
    for (uint16_t tile : {uint16_t(390), uint16_t(399), uint16_t(859), uint16_t(804),
            uint16_t(1231), uint16_t(4542)}) {
        ExploreMapGenerator::Map map{};
        map.seed = 7;
        map.pathCount = 1;
        map.paths[0].pointCount = 3;
        map.paths[0].points[0] = {1, 6};
        map.paths[0].points[1] = {4, 6};
        map.paths[0].points[2] = {14, 6};
        const int cell = 4 * ExploreMapGenerator::WIDTH + 4;
        map.layers[0][cell] = tile < 400 ? tile : 385;
        if (tile >= 400) map.layers[tile == 1231 ? 2 : 1][cell] = tile;
        ExplorePool::Pool active{};
        active.count = 1;
        active.entries[0] = {161, 1, ExplorePool::Rarity::COMMON};
        Simulation sim;
        sim.reset(map, 0, active, 0, 0);
        assert(sim.count() == 1);
        const Cover expected = tile < 400 ? Cover::GRASS : Cover::SOLID;
        if (expected == Cover::SOLID && tile != 804) {
            assert(sim.canStand(161, 4 * 26 + 13, 4 * 26 + 13));
            assert(!sim.canStand(161, 4 * 26 + 13, 4 * 26 + 22));
        }
        bool hid = false, peeked = false;
        for (uint32_t ms = 100; ms <= 60000; ms += 100) {
            sim.update(ms, true, -100, -100, view);
            const auto& pose = sim.pose(0);
            assert(pose.cover == expected && pose.lift == 0);
            hid |= pose.activity == Activity::HIDE;
            peeked |= pose.activity == Activity::PEEK;
        }
        assert(hid && peeked);
    }
    std::puts("locomotion/cover: local flapping species never hover, three ground birds never fly, safe-cell fallback, wing continuity, pause, secrecy and real cover verified");
}

static void checkWideFlight() {
    using namespace ExploreEcology;
    // Explicit terrain semantics, independent of route-following assertions.
    for (uint16_t tile : {uint16_t(1231), uint16_t(1532), uint16_t(859), uint16_t(1627),
            uint16_t(804), uint16_t(820), uint16_t(1185), uint16_t(4532), uint16_t(4513)}) {
        ExploreMapGenerator::Map map{};
        std::fill_n(map.layers[0], 192, 385);
        map.layers[1][4 * 16 + 4] = tile;
        Simulation sim;
        sim.reset(map, 0, {}, 0, 0);
        const bool allowed = tile == 1231 || tile == 1532 || tile == 859 || tile == 1627 || tile == 804;
        assert(sim.canFlyAt(4 * 26 + 13, 4 * 26 + 13) == allowed);
        assert(!sim.canFlyAt(-1, 100) && !sim.canFlyAt(416, 100));
    }
    bool departed = false, returned = false, crossedWater = false, crossedRoad = false;
    unsigned leastVisited = 192;
    for (uint16_t species : {uint16_t(278), uint16_t(279)}) {
      for (unsigned scenario = 0; scenario < 4; ++scenario) {
       for (unsigned seed = 1; seed <= 8; ++seed) {
        ExploreMapGenerator::Map map{};
        map.seed = seed;
        map.pathCount = 1;
        map.paths[0].pointCount = 14;
        for (uint8_t x = 1; x <= 14; ++x) map.paths[0].points[x - 1] = {x, 6};
        std::fill_n(map.layers[0], ExploreMapGenerator::CELL_COUNT, scenario == 3 ? 1185 : 385);
        for (unsigned y = 0; y < 12 && scenario < 3; ++y) {
            for (unsigned x = 0; x < 16; ++x) {
                const int cell = y * 16 + x;
                if (x >= 6 && x <= 8) map.layers[0][cell] = 1097;
                if (y == 6) map.layers[0][cell] = 538;
                if (scenario == 1 && x == 10 && y != 5) map.layers[1][cell] = 1185;
                if (scenario == 2 && (x == 0 || y == 0 || x == 15 || y == 11)) map.layers[1][cell] = 1185;
            }
        }
        map.layers[0][4 * 16 + 4] = 385;
        ExplorePool::Pool active{};
        active.count = 1;
        active.entries[0] = {species, 1, ExplorePool::Rarity::COMMON};
        Simulation sim;
        sim.reset(map, 0, active, 0, 0);
        assert(sim.count() == 1);
        bool visited[192]{}, left = false, right = false;
        bool phases[ACTIVITY_COUNT]{};
        uint32_t awayStart = 0;
        for (uint32_t ms = 100; ms <= 240000; ms += 100) {
            const auto before = sim.pose(0);
            // Also verify that a camera far away does not stop route progress.
            const View view = seed % 2 ? View{0, 0, 416, 312} : View{-1000, -1000, 10, 10};
            sim.update(ms, true, -100, -100, view);
            const auto& pose = sim.pose(0);
            assert(pose.airborne && !isEpisode(pose.activity));
            if (pose.absent) {
                assert(pose.activity == Activity::AWAY);
                if (!before.absent) awayStart = ms;
                departed = true;
            } else if (before.absent) {
                assert(ms - awayStart >= 12000 && ms - awayStart <= 25500);
                assert(pose.activity == Activity::ARRIVE);
                returned = true;
            } else {
                const float dx = pose.x - before.x, dy = pose.y - before.y;
                assert(dx * dx + dy * dy < 12 * 12); // No visible teleport.
                if (seed % 2 && dx * dx + dy * dy <= 0.00001f) {
                    std::fprintf(stderr, "stalled gull species=%u scenario=%u seed=%u ms=%u phase=%u x=%.2f y=%.2f\n",
                        species, scenario, seed, ms, unsigned(pose.activity), pose.x, pose.y);
                    assert(false);
                }
                const bool inside = pose.x >= 0 && pose.x < 416 && pose.y >= 0 && pose.y < 312;
                if (inside) {
                    assert(sim.canFlyAt(pose.x, pose.y));
                    const int cell = int(pose.y / 26) * 16 + int(pose.x / 26);
                    assert(map.layers[1][cell] != 1185);
                    visited[cell] = true;
                    left |= pose.x < 9 * 26; right |= pose.x > 11 * 26;
                    crossedWater |= pose.surface == Surface::WATER;
                    crossedRoad |= map.layers[0][cell] == 538;
                } else assert(pose.activity == Activity::DEPART || pose.activity == Activity::ARRIVE);
            }
            if (!phases[unsigned(pose.activity)]) {
                auto paused = sim, running = sim;
                paused.update(ms + 10000, false, 0, 0, view);
                equal(sim, paused);
                paused.update(ms + 10100, true, 0, 0, view);
                running.update(ms + 100, true, 0, 0, view);
                equal(paused, running);
                phases[unsigned(pose.activity)] = true;
            }
        }
        const unsigned cells = std::count(visited, visited + 192, true);
        if (scenario < 2) {
            if (cells < 30 || !left || !right) std::fprintf(stderr,
                "wide flight species=%u scenario=%u seed=%u visited=%u sides=%u/%u\n",
                species, scenario, seed, cells, left, right);
            assert(cells >= 30 && left && right);
            leastVisited = std::min(leastVisited, cells);
            assert(phases[unsigned(Activity::AWAY)] && phases[unsigned(Activity::ARRIVE)]);
        }
        if (scenario >= 2) assert(!phases[unsigned(Activity::AWAY)]);
        if (scenario == 3) assert(cells == 1);
        // Unknown gulls stay environmental hints, even with the wide profile.
        Simulation unknown;
        unknown.reset(map, 0, active, 1, 0);
        for (uint32_t ms = 100; ms <= 20000; ms += 100) {
            unknown.update(ms, true, 0, 0, {0, 0, 416, 312});
            assert(unknown.pose(0).hidden && !unknown.pose(0).airborne && !unknown.pose(0).absent);
        }
        unknown.revealSpecies(species);
        assert(!unknown.pose(0).hidden && unknown.pose(0).airborne);
       }
      }
    }
    assert(departed && returned && crossedWater && crossedRoad);
    std::printf("wide flight: 64 open/wall/enclosed/isolated cases, min %u visited cells on connected outdoor maps; crossing, departure/reentry, offscreen motion, phase pause and secrecy verified\n", leastVisited);
}

static void checkGeneratedFlight() {
    unsigned maps = 0, largeRegions = 0;
    for (uint8_t area = 0; area < 6; ++area) {
      unsigned generated = 0;
      for (uint32_t seed = 1; seed <= 1024 && generated < 32; ++seed) {
        ExploreMapGenerator::Map map{};
        if (!ExploreMapGenerator::generate(seed, ExploreMapGenerator::Edge(seed % 4), area, map,
                {uint8_t(seed % 3), 3, seed % 2 == 0})) continue;
        ++generated;
        ExplorePool::Pool active{};
        active.count = 1;
        active.entries[0] = {uint16_t(seed % 2 ? 278 : 279), 1, ExplorePool::Rarity::COMMON};
        Simulation sim;
        sim.reset(map, seed % map.pathCount, active, 0, 0);
        if (!sim.count()) continue;
        ++maps;
        bool reachable[192]{}, visited[192]{};
        uint8_t queue[192];
        unsigned head = 0, tail = 0;
        const auto origin = sim.pose(0);
        const int root = int(origin.y / 26) * 16 + int(origin.x / 26);
        queue[tail++] = root; reachable[root] = true;
        while (head < tail) {
            const int cell = queue[head++], x = cell % 16, y = cell / 16;
            constexpr int dx[] = {0, -1, 0, 1}, dy[] = {1, 0, -1, 0};
            for (int d = 0; d < 4; ++d) {
                const int nx = x + dx[d], ny = y + dy[d], to = ny * 16 + nx;
                if (sim.flightSurfaceAt(nx, ny) == ExploreEcology::Surface::NONE || reachable[to]) continue;
                reachable[to] = true; queue[tail++] = to;
            }
        }
        unsigned staticMs = 0;
        for (uint32_t ms = 100; ms <= 180000; ms += 100) {
            const auto before = sim.pose(0);
            sim.update(ms, true, -100, -100, {0, 0, 416, 312});
            const auto pose = sim.pose(0);
            if (pose.absent || before.absent) { staticMs = 0; continue; }
            const float dx = pose.x - before.x, dy = pose.y - before.y;
            if (dx * dx + dy * dy < 0.00001f) staticMs += 100; else staticMs = 0;
            if (staticMs > 500) std::fprintf(stderr, "flight stuck area=%u seed=%u phase=%u at %.2f %.2f\n",
                area, seed, unsigned(pose.activity), pose.x, pose.y);
            assert(staticMs <= 500);
            if (pose.x >= 0 && pose.x < 416 && pose.y >= 0 && pose.y < 312) {
                assert(sim.canFlyAt(pose.x, pose.y));
                const int cell = int(pose.y / 26) * 16 + int(pose.x / 26);
                assert(reachable[cell]); visited[cell] = true;
            }
        }
        const unsigned cells = std::count(visited, visited + 192, true);
        if (tail >= 30) {
            ++largeRegions;
            if (cells < 12) std::fprintf(stderr, "flight confined area=%u seed=%u reachable=%u visited=%u\n",
                area, seed, tail, cells);
            assert(cells >= 12);
        }
      }
      assert(generated == 32);
    }
    assert(maps > 150 && largeRegions > 100);
    std::printf("generated flight: %u real maps, %u large connected regions; 180s movement, boundaries and regional continuity verified\n",
        maps, largeRegions);
}

static void checkGroupRange() {
    using namespace ExploreEcology;
    unsigned groups = 0, minimumVisited = 192;
    for (const auto& family : ExploreEcology::GROUP_FAMILIES) {
      const auto& pair = family.species;
      const uint16_t adult = pair[0];
      for (unsigned terrain = 0; terrain < 4; ++terrain) {
        for (uint32_t seed = 1; seed <= 32; ++seed) {
            ExploreMapGenerator::Map map{};
            map.seed = seed;
            map.areaIndex = adult == 286 ? 2 : 1;
            map.pathCount = 1; map.paths[0].pointCount = 14;
            for (uint8_t x = 1; x <= 14; ++x) map.paths[0].points[x - 1] = {x, 6};
            std::fill_n(map.layers[0], ExploreMapGenerator::CELL_COUNT, 390);
            if (terrain == 1) { // A wall with a single narrow passage.
                for (int y = 1; y < 11; ++y) if (y != 4) map.layers[0][y * 16 + 7] = 64;
            } else if (terrain == 2) { // A spacious patch with a cul-de-sac.
                std::fill_n(map.layers[0], ExploreMapGenerator::CELL_COUNT, 64);
                for (int y = 2; y <= 8; ++y)
                    for (int x = 1; x <= 6; ++x) map.layers[0][y * 16 + x] = 390;
                for (int x = 7; x <= 13; ++x) map.layers[0][4 * 16 + x] = 390;
            } else if (terrain == 3) { // A narrow, continuous habitat loop.
                std::fill_n(map.layers[0], ExploreMapGenerator::CELL_COUNT, 64);
                for (int y = 2; y <= 8; ++y)
                    for (int x = 3; x <= 9; ++x)
                        if (y == 2 || y == 8 || x == 3 || x == 9) map.layers[0][y * 16 + x] = 390;
            }
            ExplorePool::Pool pool{};
            pool.count = 2;
            pool.entries[0] = {adult, 1, ExplorePool::Rarity::COMMON};
            pool.entries[1] = {pair[1], 1, ExplorePool::Rarity::COMMON};
            Simulation sim;
            sim.reset(map, 0, pool, 0, 0);
            if (!sim.hasGroup()) continue;
            ++groups;
            bool visited[192]{}, adultTrail[192]{};
            for (uint8_t i = 0; i < sim.count(); ++i) {
                const auto p = sim.pose(i);
                adultTrail[int(p.y / 26) * 16 + int(p.x / 26)] = true;
            }
            float left = sim.pose(0).x, right = left, top = sim.pose(0).y, bottom = top;
            float lastDx = 0, lastDy = 0;
            for (uint32_t ms = 100; ms <= 120000; ms += 100) {
                const auto before = sim.pose(0);
                sim.update(ms, true, -100, -100, {0, 0, 416, 312});
                for (uint8_t i = 0; i < sim.count(); ++i) {
                    const auto p = sim.pose(i);
                    assert(sim.canStand(p.speciesId, p.x, p.y));
                    if (terrain == 2) assert(p.x <= 6.5f * 26); // Never enter the dead end.
                    const int cell = int(p.y / 26) * 16 + int(p.x / 26);
                    visited[cell] = true;
                    if (i == 0) adultTrail[cell] = true;
                    else assert(adultTrail[cell]);
                }
                const auto p = sim.pose(0);
                const float dx = p.x - before.x, dy = p.y - before.y;
                if (std::hypot(dx, dy) > 0.01f) {
                    assert(dx * lastDx + dy * lastDy >= -0.01f); // No instant U-turn into the young.
                    lastDx = dx; lastDy = dy;
                }
                left = std::min(left, p.x); right = std::max(right, p.x);
                top = std::min(top, p.y); bottom = std::max(bottom, p.y);
            }
            unsigned cells = 0;
            for (bool cell : visited) cells += cell;
            if (cells < 20 || std::max(right - left, bottom - top) < 26 * 5) {
                std::fprintf(stderr, "group range terrain=%u seed=%u: %u cells, span %.1fx%.1f\n",
                    terrain, seed, cells, right - left, bottom - top);
                assert(false);
            }
            minimumVisited = std::min(minimumVisited, cells);
        }
      }
    }
    assert(groups >= 48);
    std::printf("group range: %u open/passage/dead-end/narrow-loop cases, min %u cells in 120s; wide progress, actual trail and no U-turn verified\n",
        groups, minimumVisited);
}

static void checkSingleMemberGroups() {
    using namespace ExploreEcology;
    int total = 0;
    for (const auto& family : GROUP_FAMILIES) {
        if (family.count == 3) continue; // Variable Marill compositions have dedicated coverage.
        const auto& pair = family.species;
        int outcomes[4]{};
        for (uint32_t seed = 1; seed <= 1024; ++seed) {
            ExploreMapGenerator::Map map{};
            map.seed = seed;
            map.areaIndex = pair[0] == 286 ? 2 : 1;
            map.pathCount = 1;
            map.paths[0].pointCount = 14;
            for (uint8_t x = 1; x <= 14; ++x) map.paths[0].points[x - 1] = {x, 6};
            std::fill_n(map.layers[0], ExploreMapGenerator::CELL_COUNT, 390);
            bool expected = false;
            for (uint8_t variant = 0; variant <= family.count; ++variant) {
                ExplorePool::Pool pool{};
                pool.count = variant == family.count ? family.count : 1;
                for (uint8_t i = 0; i < pool.count; ++i)
                    pool.entries[i] = {pair[variant == family.count ? i : variant], 1, ExplorePool::Rarity::COMMON};
                const auto before = pool;
                Simulation sim, twin;
                sim.reset(map, 0, pool, 0, 0);
                twin.reset(map, 0, pool, 0, 0);
                equal(sim, twin);
                if (variant == 0) expected = sim.hasGroup();
                else assert(sim.hasGroup() == expected);
                if (sim.hasGroup()) {
                    ++outcomes[variant]; ++total;
                    assert(sim.count() == family.count);
                    for (uint8_t i = 0; i < family.count; ++i)
                        assert(sim.pose(i).speciesId == pair[i]);
                    for (uint32_t ms = 100; ms <= 3000; ms += 100) {
                        sim.update(ms, true, -100, -100, {0, 0, 416, 312});
                        for (uint8_t i = 0; i < sim.count(); ++i) {
                            assert(!sim.pose(i).hidden);
                            assert(sim.canStand(sim.pose(i).speciesId, sim.pose(i).x, sim.pose(i).y));
                        }
                    }
                } else assert(sim.count() <= pool.count);
                assert(std::memcmp(&before, &pool, sizeof(pool)) == 0);
                for (uint8_t mask = 1; mask < (1U << pool.count); ++mask) {
                    sim.reset(map, 0, pool, mask, 0);
                    assert(!sim.hasGroup());
                }
            }
            ExplorePool::Pool unrelated{};
            unrelated.count = 1;
            unrelated.entries[0] = {10, 1, ExplorePool::Rarity::COMMON};
            Simulation sim;
            sim.reset(map, 0, unrelated, 0, 0);
            assert(!sim.hasGroup());
        }
        for (uint8_t variant = 0; variant <= family.count; ++variant)
            assert(outcomes[variant] >= 450 && outcomes[variant] <= 574);
        std::printf("single-member groups %u/%u: adult=%d young=%d all=%d /1024, 50%% eligibility, secrecy and pool isolation verified\n",
            pair[0], pair[1], outcomes[0], outcomes[1], outcomes[family.count]);
    }
    assert(groupFamily(10) == 0 && total > 2000);
}

static void checkMarillCompositions() {
    using namespace ExploreEcology;
    const auto& family = *groupFamily(184);
    unsigned outcomes[8]{}, total = 0, moved[8]{};
    for (uint32_t seed = 1; seed <= 8192; ++seed) {
        ExploreMapGenerator::Map map{};
        map.seed = seed; map.areaIndex = 1;
        std::fill_n(map.layers[0], ExploreMapGenerator::CELL_COUNT, 390);
        map.pathCount = 1; map.paths[0].pointCount = 14;
        for (uint8_t x = 1; x <= 14; ++x) map.paths[0].points[x - 1] = {x, 6};
        ExplorePool::Pool pool{};
        pool.count = 1; pool.entries[0] = {298, 1, ExplorePool::Rarity::COMMON};
        Simulation sim;
        sim.reset(map, 0, pool, 0, 0);
        if (!sim.hasFamily()) continue;
        uint8_t mask = 0, previous = 0;
        for (uint8_t i = 0; i < sim.count(); ++i) {
            uint8_t member = 0;
            while (member < family.count && sim.pose(i).speciesId != family.species[member]) ++member;
            assert(member < family.count && (i == 0 || member > previous));
            mask |= 1U << member; previous = member;
        }
        assert(mask && sim.hasGroup() == (sim.count() > 1));
        ++outcomes[mask]; ++total;
        if (seed <= 256) {
            const uint8_t count = sim.count();
            Pose initial[CAPACITY];
            for (uint8_t i = 0; i < count; ++i) initial[i] = sim.pose(i);
            for (uint32_t ms = 100; ms <= 90000; ms += 100) {
                sim.update(ms, true, -1000, -1000, {0, 0, 416, 312});
                assert(sim.count() == count && sim.hasFamily());
                for (uint8_t i = 0; i < count; ++i) {
                    const auto& p = sim.pose(i);
                    assert(p.speciesId == initial[i].speciesId && !p.hidden);
                    assert(sim.canStand(p.speciesId, p.x, p.y));
                    if (p.walking) ++moved[mask];
                }
                if (ms % 1700 == 0) {
                    auto paused = sim, running = sim;
                    paused.update(ms + 30000, false, -1000, -1000, {0, 0, 416, 312});
                    equal(sim, paused);
                    paused.update(ms + 30100, true, -1000, -1000, {0, 0, 416, 312});
                    running.update(ms + 100, true, -1000, -1000, {0, 0, 416, 312});
                    equal(paused, running);
                }
            }
        }
        // A four-cell pocket cannot hold the shared group trail. Every
        // composition must degrade to a single safe independent actor.
        std::fill_n(map.layers[0], ExploreMapGenerator::CELL_COUNT, 64);
        for (int y = 3; y <= 4; ++y)
            for (int x = 4; x <= 5; ++x) map.layers[0][y * 16 + x] = 390;
        sim.reset(map, 0, pool, 0, 0);
        assert(sim.hasFamily() && !sim.hasGroup() && sim.count() == 1);
        assert(sim.canStand(sim.pose(0).speciesId, sim.pose(0).x, sim.pose(0).y));
        std::fill_n(map.layers[0], ExploreMapGenerator::CELL_COUNT, 64);
        sim.reset(map, 0, pool, 0, 0);
        assert(!sim.hasFamily() && sim.count() == 0);
    }
    assert(total > 3800 && total < 4400);
    for (uint8_t mask = 1; mask <= 7; ++mask) {
        const float target = mask == 7 ? 0.25f : (mask == 3 || mask == 5 || mask == 6) ? 0.15f : 0.10f;
        assert(std::fabs(float(outcomes[mask]) / total - target) < 0.025f);
        assert(moved[mask]);
        std::printf("Marill composition mask %u: %u/%u (%.1f%%)\n", mask,
            outcomes[mask], total, 100.0f * outcomes[mask] / total);
    }
    std::puts("Marill: seven compositions, 30/45/25 frequency, independent solo, stable members, pause and terrain fallback verified");
}

static void checkThreeMemberFamily() {
    using namespace ExploreEcology;
    const auto& family = *groupFamily(184);
    assert(family.count == 3 && family.species[0] == 184 &&
        family.species[1] == 183 && family.species[2] == 298);
    int groups = 0, tailViews = 0;
    for (uint32_t seed = 1; seed <= 128; ++seed) {
        ExploreMapGenerator::Map map{};
        map.seed = seed; map.areaIndex = 1;
        std::fill_n(map.layers[0], ExploreMapGenerator::CELL_COUNT, 390);
        map.pathCount = 1; map.paths[0].pointCount = 14;
        for (uint8_t x = 1; x <= 14; ++x) map.paths[0].points[x - 1] = {x, 6};
        Simulation reference;
        bool expected = false;
        for (uint8_t subset = 1; subset < 8; ++subset) {
            ExplorePool::Pool pool{};
            for (uint8_t i = 0; i < 3; ++i)
                if (subset & (1U << i))
                    pool.entries[pool.count++] = {family.species[i], 1, ExplorePool::Rarity::COMMON};
            const auto before = pool;
            Simulation sim;
            sim.reset(map, 0, pool, 0, 0);
            if (subset == 1) { reference = sim; expected = sim.hasFamily(); }
            assert(sim.hasFamily() == expected);
            if (sim.hasFamily()) {
                assert(sim.count() == reference.count());
                equal(sim, reference);
            }
            for (uint8_t hidden = 1; hidden < (1U << pool.count); ++hidden) {
                Simulation secret;
                secret.reset(map, 0, pool, hidden, 0);
                assert(!secret.hasGroup());
                for (uint8_t i = 0; i < secret.count(); ++i) {
                    if (!secret.pose(i).hidden) continue;
                    bool explicitlyHidden = false;
                    for (uint8_t member = 0; member < pool.count; ++member)
                        explicitlyHidden |= (hidden & (1U << member)) &&
                            pool.entries[member].speciesId == secret.pose(i).speciesId;
                    assert(explicitlyHidden);
                }
            }
            assert(std::memcmp(&before, &pool, sizeof(pool)) == 0);
        }
        if (!expected || reference.count() != 3) continue;
        ++groups;
        const auto tail = reference.pose(2), middle = reference.pose(1);
        const float dx = tail.x - middle.x, dy = tail.y - middle.y;
        const View tailOnly{tail.x + dx, tail.y + dy, 1, 1};
        assert(tailOnly.contains(tail.x, tail.y) && !tailOnly.contains(middle.x, middle.y));
        assert(!tailOnly.contains(reference.pose(0).x, reference.pose(0).y));
        auto narrow = reference, full = reference;
        for (uint32_t ms = 100; ms <= 800; ms += 100) {
            narrow.update(ms, true, -1000, -1000, tailOnly);
            full.update(ms, true, -1000, -1000, {0, 0, 416, 312});
            equal(narrow, full);
        }
        ++tailViews;
        // Both followers must use the preceding member's actual visited cells.
        bool trail[CAPACITY][ExploreMapGenerator::CELL_COUNT]{};
        for (uint8_t i = 0; i < 3; ++i)
            for (uint8_t ahead = 0; ahead <= i; ++ahead) {
                const auto p = reference.pose(i);
                trail[ahead][int(p.y / 26) * 16 + int(p.x / 26)] = true;
            }
        for (uint32_t ms = 100; ms <= 180000; ms += 100) {
            const auto leader = reference.pose(0);
            const bool approach = ms % 17000 < 4000;
            reference.update(ms, true, approach ? leader.x + 18 : -1000,
                approach ? leader.y : -1000, {0, 0, 416, 312});
            assert(reference.count() == 3);
            for (uint8_t i = 0; i < 3; ++i) {
                const auto p = reference.pose(i);
                const int cell = int(p.y / 26) * 16 + int(p.x / 26);
                if (i) assert(trail[i - 1][cell]);
                trail[i][cell] = true;
                assert(reference.canStand(p.speciesId, p.x, p.y));
            }
        }
    }
    assert(groups > 5 && tailViews == groups);
    std::printf("three-member family: %d groups; seven pool subsets, hidden masks, tail-only visibility and 180s follow trails verified\n", groups);
}

static void checkFamilySelection() {
    using namespace ExploreEcology;
    constexpr size_t families = sizeof(GROUP_FAMILIES) / sizeof(GROUP_FAMILIES[0]);
    for (size_t i = 0; i < families; ++i) {
        const auto& family = GROUP_FAMILIES[i];
        assert(family.count >= 2 && family.count <= CAPACITY);
        for (uint8_t member = 0; member < family.count; ++member) {
            assert(groupFamily(family.species[member]) == &family);
            for (size_t j = i + 1; j < families; ++j)
                for (uint8_t other = 0; other < GROUP_FAMILIES[j].count; ++other)
                    assert(family.species[member] != GROUP_FAMILIES[j].species[other]);
        }
    }
    for (size_t start : {size_t(0), families - ExplorePool::POOL_CAP}) {
        int selected[ExplorePool::POOL_CAP]{};
        for (uint32_t seed = 1; seed <= 2048; ++seed) {
            ExploreMapGenerator::Map map{};
            map.seed = seed;
            std::fill_n(map.layers[0], ExploreMapGenerator::CELL_COUNT, 390);
            map.pathCount = 1; map.paths[0].pointCount = 14;
            for (uint8_t x = 1; x <= 14; ++x) map.paths[0].points[x - 1] = {x, 6};
            ExplorePool::Pool pool{};
            pool.count = ExplorePool::POOL_CAP;
            for (uint8_t i = 0; i < pool.count; ++i)
                pool.entries[i] = {GROUP_FAMILIES[start + i].species[i % 2], 1, ExplorePool::Rarity::COMMON};
            const auto before = pool;
            Simulation sim, reordered, hidden;
            sim.reset(map, 0, pool, 0, 0);
            std::reverse(pool.entries, pool.entries + pool.count);
            reordered.reset(map, 0, pool, 0, 0);
            assert(sim.hasFamily() == reordered.hasFamily());
            if (sim.hasFamily()) equal(sim, reordered);
            pool = before;
            hidden.reset(map, 0, pool, 1, 0);
            assert(hidden.hasFamily() == sim.hasFamily());
            if (sim.hasFamily()) {
                assert(sim.count() <= groupFamily(sim.pose(0).speciesId)->count);
                assert(hidden.count() <= groupFamily(hidden.pose(0).speciesId)->count);
                assert(groupFamily(hidden.pose(0).speciesId) != &GROUP_FAMILIES[start]);
                bool found = false;
                for (uint8_t i = 0; i < pool.count; ++i) {
                    if (groupFamily(sim.pose(0).speciesId) != &GROUP_FAMILIES[start + i]) continue;
                    ++selected[i]; found = true;
                }
                assert(found);
            }
            assert(std::memcmp(&before, &pool, sizeof(pool)) == 0);
        }
        int total = 0;
        for (int count : selected) {
            assert(count > 100 && count < 240);
            total += count;
        }
        assert(total >= 920 && total <= 1130);
        std::printf("family selection: %d/2048 groups across six eligible families; variety, one roll, pool order and hidden exclusion verified\n", total);
    }
}

static void checkGroups() {
    using namespace ExploreEcology;
    const View view{0, 0, 416, 312};
    int groups = 0, realGroups = 0, waited = 0, reunited = 0, reacted = 0;
    for (const auto& family : ExploreEcology::GROUP_FAMILIES) {
        if (family.count == 3) continue; // Variable Marill compositions have dedicated coverage.
      const auto& pair = family.species;
      const uint16_t adult = pair[0];
      const uint16_t young = pair[1];
      for (bool water : {false, true}) {
        int eligible = 0;
        for (uint32_t seed = 1; seed <= 128; ++seed) {
            ExploreMapGenerator::Map map{};
            map.seed = seed;
            map.areaIndex = adult == 286 ? 2 : 1;
            map.pathCount = 1;
            map.paths[0].pointCount = 14;
            for (uint8_t x = 1; x <= 14; ++x) map.paths[0].points[x - 1] = {x, 6};
            std::fill_n(map.layers[0], ExploreMapGenerator::CELL_COUNT, water ? 1097 : 390);
            ExplorePool::Pool active{};
            active.count = 2;
            active.entries[0] = {young, 1, ExplorePool::Rarity::COMMON};
            active.entries[1] = {adult, 1, ExplorePool::Rarity::RARE};
            Simulation sim, twin;
            sim.reset(map, 0, active, 0, 0);
            twin.reset(map, 0, active, 0, 0);
            equal(sim, twin);
            if (water) {
                assert(!sim.hasGroup() && sim.count() == 0);
                continue;
            }
            for (uint8_t mask : {uint8_t(1), uint8_t(2), uint8_t(3)}) {
                Simulation hidden;
                hidden.reset(map, 0, active, mask, 0);
                assert(!hidden.hasGroup());
                hidden.revealSpecies(adult); hidden.revealSpecies(young);
                assert(!hidden.hasGroup()); // No surprise regrouping after reveal.
            }
            if (!sim.hasGroup()) continue;
            ++eligible; ++groups;
            assert(sim.count() == family.count && sim.pose(0).speciesId == adult && sim.pose(1).speciesId == young);
            auto largerPool = active;
            largerPool.count = 6;
            for (uint8_t i = 2; i < 6; ++i)
                largerPool.entries[i] = {uint16_t(14 + i), 10, ExplorePool::Rarity::COMMON};
            Simulation crowded;
            crowded.reset(map, 0, largerPool, 0, 0);
            assert(crowded.hasGroup() && crowded.count() == groupFamily(crowded.pose(0).speciesId)->count);
            bool moved[CAPACITY]{}, turns[CAPACITY]{};
            uint32_t unchanged[CAPACITY]{};
            const auto first = sim.pose(0), second = sim.pose(1);
            for (uint32_t ms = 100; ms <= 90000; ms += 100) {
                Pose before[CAPACITY];
                for (uint8_t i = 0; i < sim.count(); ++i) before[i] = sim.pose(i);
                const bool approach = ms >= 10000 && ms < 18000;
                const float px = approach ? before[0].x + 15 : -100;
                const float py = approach ? before[0].y : -100;
                sim.update(ms, true, px, py, view);
                twin.update(ms, true, px, py, view);
                equal(sim, twin);
                assert(sim.hasGroup() && sim.count() == family.count);
                const auto& a = sim.pose(0); const auto& b = sim.pose(1);
                const float gap = std::hypot(a.x - b.x, a.y - b.y);
                assert(gap >= 26 * 0.58f && gap <= 26 * 1.7f);
                waited += !a.walking && b.walking && !approach;
                reunited += !a.walking && !b.walking && a.direction != before[0].direction && !approach;
                reacted += approach && !a.walking && a.direction == 3 && b.walking;
                for (uint8_t i = 0; i < sim.count(); ++i) {
                    const auto& p = sim.pose(i);
                    assert(!p.hidden && !p.airborne && !isEpisode(p.activity));
                    assert(sim.canStand(p.speciesId, p.x, p.y));
                    assert(p.surface == (water ? Surface::WATER : Surface::GRASS));
                    const float travel = std::hypot(p.x - before[i].x, p.y - before[i].y);
                    assert(travel <= 26 * 0.1f / 1.45f + 0.01f);
                    moved[i] |= travel > 0.01f;
                    turns[i] |= p.direction != before[i].direction;
                    unchanged[i] = travel < 0.01f && p.direction == before[i].direction ? unchanged[i] + 100 : 0;
                    assert(unchanged[i] <= 5000);
                    for (uint8_t other = 0; other < i; ++other) {
                        const auto& preceding = sim.pose(other);
                        const float gap = std::hypot(p.x - preceding.x, p.y - preceding.y);
                        assert(gap >= 26 * 0.58f);
                        if (other + 1 == i) assert(gap <= 26 * 1.7f);
                    }
                }
                if (ms % 1700 == 0) {
                    auto paused = sim, running = sim;
                    paused.update(ms + 30000, false, px, py, view);
                    equal(sim, paused);
                    paused.update(ms + 30100, true, px, py, view);
                    running.update(ms + 100, true, px, py, view);
                    equal(paused, running);
                }
            }
            for (uint8_t i = 0; i < sim.count(); ++i) assert(moved[i] && turns[i]);
            assert(std::hypot(first.x - second.x, first.y - second.y) == 26);
            // Reduced offscreen cadence retains safety and keeps the pair moving.
            const auto before = sim.pose(0);
            for (uint32_t ms = 90100; ms <= 120000; ms += 100) {
                sim.update(ms, true, -100, -100, {-1000, -1000, 10, 10});
                for (uint8_t i = 0; i < sim.count(); ++i)
                    assert(sim.canStand(sim.pose(i).speciesId, sim.pose(i).x, sim.pose(i).y));
            }
            assert(!samePose(before, sim.pose(0)));
        }
        if (!water) assert(eligible >= 45 && eligible <= 85); // 50% roll on land.
      }
      for (uint32_t seed = 1; seed <= 128; ++seed) {
        ExploreMapGenerator::Map map{};
        if (!ExploreMapGenerator::generate(seed, ExploreMapGenerator::Edge(seed % 4),
            adult == 286 ? 2 : 1, map)) continue;
        ExplorePool::Pool active{};
        active.count = 2;
        active.entries[0] = {adult, 1, ExplorePool::Rarity::COMMON};
        active.entries[1] = {young, 1, ExplorePool::Rarity::COMMON};
        Simulation sim;
        sim.reset(map, seed % map.pathCount, active, 0, 0);
        if (sim.hasGroup()) {
            ++realGroups;
            for (uint32_t ms = 100; ms <= 60000; ms += 100) {
                sim.update(ms, true, -100, -100, view);
                for (uint8_t i = 0; i < sim.count(); ++i)
                    assert(sim.canStand(sim.pose(i).speciesId, sim.pose(i).x, sim.pose(i).y));
            }
        }
        // Single-member pools can bring the same family, while fragmented
        // unsafe terrain must still reject a pair.
        const bool both = sim.hasGroup();
        active.count = 1;
        sim.reset(map, seed % map.pathCount, active, 0, 0);
        assert(sim.hasGroup() == both);
        active.entries[0].speciesId = young;
        sim.reset(map, seed % map.pathCount, active, 0, 0);
        assert(sim.hasGroup() == both);
        active.entries[0].speciesId = adult;
        active.count = 2;
        std::fill_n(map.layers[0], ExploreMapGenerator::CELL_COUNT, 64);
        std::fill_n(map.layers[1], ExploreMapGenerator::CELL_COUNT, 0);
        std::fill_n(map.layers[2], ExploreMapGenerator::CELL_COUNT, 0);
        map.layers[0][4 * 16 + 4] = 390;
        map.layers[0][4 * 16 + 8] = 390;
        sim.reset(map, 0, active, 0, 0);
        assert(!sim.hasGroup());
        std::fill_n(map.layers[0], ExploreMapGenerator::CELL_COUNT, 64);
        for (int y = 3; y <= 4; ++y)
            for (int x = 4; x <= 5; ++x) map.layers[0][y * 16 + x] = 390;
        sim.reset(map, 0, active, 0, 0);
        assert(!sim.hasGroup()); // Do not replace a four-cell loop with more circling.
        std::fill_n(map.layers[0], ExploreMapGenerator::CELL_COUNT, 64);
        for (int x = 1; x < 15; ++x) map.layers[0][4 * 16 + x] = 390;
        sim.reset(map, 0, active, 0, 0);
        assert(!sim.hasGroup()); // A dead-end-only corridor has no safe group turn.
      }
    }
    assert(realGroups > 20 && waited && reunited && reacted);
    std::printf("groups: %d synthetic pairs, %d real-map pairs; follow/wait/reunion/player, safety, secrecy, density and pause verified\n",
        groups, realGroups);
}

int main() {
    checkSingleMemberGroups();
    checkMarillCompositions();
    checkThreeMemberFamily();
    checkFamilySelection();
    checkGroupRange();
    checkGroups();
    auto activePool = pool();
    const auto originalPool = activePool;
    int populated = 0, moved = 0, turned = 0, hidden = 0;
    int maps = 0, waterActors = 0, iceActors = 0, rockActors = 0;
    for (uint8_t area = 0; area < Game::EXPLORE_AREA_COUNT; ++area) {
      int areaPopulated = 0;
      int generated = 0;
      for (uint32_t seed = 1; seed <= 1024 && generated < 128; ++seed) {
        activePool = area == 0 ? pool() : ExploreEncounters::poolForArea(area, seed * 480, seed % 4);
        ExploreMapGenerator::Map map;
        // Generation can reject a forest topology; the app retries as well.
        if (!ExploreMapGenerator::generate(seed, ExploreMapGenerator::Edge(seed % 4), area, map,
            {uint8_t(seed % 3), 3, seed % 2 == 0})) continue;
        ++generated;
        ++maps;
        const uint32_t fingerprint = ExploreMapGenerator::fingerprint(map);
        const auto poolBefore = activePool;
        Simulation first, second;
        const uint8_t path = seed % map.pathCount;
        first.reset(map, path, activePool, 1U << 5, 0);
        second.reset(map, path, activePool, 1U << 5, 0);
        equal(first, second);
        assert(first.count() <= 3);
        if (first.count() >= 2) ++populated;
        if (first.count()) ++areaPopulated;
        Pose initial[ExploreEcology::CAPACITY];
        for (uint8_t i = 0; i < first.count(); ++i) {
            initial[i] = first.pose(i);
            bool member = false;
            for (uint8_t p = 0; p < activePool.count; ++p)
                member |= initial[i].speciesId == activePool.entries[p].speciesId;
            if (!member) {
                assert(first.hasFamily());
                bool familyMember = false;
                for (uint8_t p = 0; p < activePool.count; ++p)
                    familyMember |= ExploreEcology::groupFamily(initial[i].speciesId) == ExploreEcology::groupFamily(activePool.entries[p].speciesId);
                assert(familyMember);
            }
            assert(first.safeCell(int(initial[i].x / 26), int(initial[i].y / 26)));
            assert(first.canOccupy(initial[i].speciesId, int(initial[i].x / 26), int(initial[i].y / 26)));
            assert(initial[i].surface == first.surfaceAt(int(initial[i].x / 26), int(initial[i].y / 26)));
            assert(initial[i].hidden == (activePool.count > 5 &&
                initial[i].speciesId == activePool.entries[5].speciesId));
            hidden += initial[i].hidden;
            waterActors += initial[i].surface == ExploreEcology::Surface::WATER;
            if (initial[i].surface == ExploreEcology::Surface::WATER && !initial[i].airborne)
                assert(initial[i].speciesId == 129 || initial[i].speciesId == 130);
            iceActors += initial[i].surface == ExploreEcology::Surface::ICE;
            rockActors += initial[i].surface == ExploreEcology::Surface::ROCK;
        }
        if (first.count() == 3 && !first.hasGroup()) {
            const float left = std::min({initial[0].x, initial[1].x, initial[2].x});
            const float right = std::max({initial[0].x, initial[1].x, initial[2].x});
            const float top = std::min({initial[0].y, initial[1].y, initial[2].y});
            const float bottom = std::max({initial[0].y, initial[1].y, initial[2].y});
            assert(right - left >= 26 * 9 || bottom - top >= 26 * 8);
        }
        const ExploreEcology::View view{0, 0, 416, 312};
        for (uint32_t ms = 100; ms <= 30000; ms += 100) {
            first.update(ms, true, -100, -100, view);
            second.update(ms, true, -100, -100, view);
            equal(first, second);
            int episodes = 0;
            for (uint8_t i = 0; i < first.count(); ++i) {
                const auto& pose = first.pose(i);
                if (pose.surface == ExploreEcology::Surface::WATER && !pose.airborne)
                    assert(pose.speciesId == 129 || pose.speciesId == 130);
                episodes += ExploreEcology::isEpisode(pose.activity);
                if (pose.hidden) {
                    assert(!ExploreEcology::isEpisode(pose.activity));
                    assert(pose.lift == 0);
                }
                if (ExploreEcology::wideFlightSpecies(pose.speciesId) && !pose.hidden) {
                    const bool inside = pose.x >= 0 && pose.x < 416 && pose.y >= 0 && pose.y < 312;
                    if (inside) assert(first.canFlyAt(pose.x, pose.y));
                    else assert(pose.activity == ExploreEcology::Activity::DEPART ||
                        pose.activity == ExploreEcology::Activity::AWAY || pose.activity == ExploreEcology::Activity::ARRIVE);
                } else {
                    assert(first.safeCell(int(pose.x / 26), int(pose.y / 26)));
                    assert(first.canStand(pose.speciesId, pose.x, pose.y));
                    assert(pose.surface == first.surfaceAt(int(pose.x / 26), int(pose.y / 26)));
                    const float dx = pose.x - initial[i].x, dy = pose.y - initial[i].y;
                    if (!first.hasGroup()) assert(dx * dx + dy * dy <= 26 * 26 * 2 + 0.01f);
                }
                moved += pose.walking;
                turned += pose.direction != initial[i].direction;
            }
            assert(episodes <= 1);
        }
        // Pausing rebases time; its duration cannot move or age an actor.
        first.update(60000, false, 0, 0, view);
        second.update(30000, false, 0, 0, view);
        equal(first, second);
        for (uint32_t ms = 100; ms <= 1000; ms += 100) {
            first.update(60000 + ms, true, 0, 0, view);
            second.update(30000 + ms, true, 0, 0, view);
            equal(first, second);
        }
        for (const auto& entry : activePool.entries) first.revealSpecies(entry.speciesId);
        for (uint8_t i = 0; i < first.count(); ++i) assert(!first.pose(i).hidden);
        assert(fingerprint == ExploreMapGenerator::fingerprint(map));
        assert(std::memcmp(&activePool, &poolBefore, sizeof(activePool)) == 0);
      }
      std::printf("area %u: %d/128 maps populated\n", area, areaPopulated);
      assert(generated == 128);
      assert(areaPopulated == 128);
    }
    assert(populated > 600 && moved > 0 && turned > 0 && hidden > 0);
    assert(waterActors > 0 && iceActors > 0 && rockActors > 0);
    activePool = pool();
    assert(std::memcmp(&activePool, &originalPool, sizeof(activePool)) == 0);
    ExploreMapGenerator::Map map{};
    map.pathCount = 1;
    map.paths[0].pointCount = 3;
    Simulation empty;
    empty.reset(map, 0, activePool, 0, 0);
    assert(empty.count() == 0);  // No habitat: do not invent ground or overwrite the map.
    assert(!empty.safeCell(-1, 0) && !empty.safeCell(16, 12));
    for (uint8_t area = 1; area < 6; ++area) {
        map.areaIndex = area;
        empty.reset(map, 0, activePool, 0, 0);
        assert(empty.count() == 0);
    }
    // Species with no suitable habitat are skipped, then remaining pool
    // members are considered. Fish may never be placed on dry land.
    map.areaIndex = 1;
    map.paths[0].points[0] = {1, 5};
    map.paths[0].points[1] = {7, 5};
    map.paths[0].points[2] = {14, 5};
    std::fill_n(map.layers[0], ExploreMapGenerator::CELL_COUNT, 385);
    ExplorePool::Pool fishPool{};
    fishPool.count = 2;
    fishPool.entries[0] = {129, 250, ExplorePool::Rarity::COMMON};
    fishPool.entries[1] = {74, 1, ExplorePool::Rarity::COMMON};
    empty.reset(map, 0, fishPool, 0, 0);
    assert(empty.count() == 1 && empty.pose(0).speciesId == 74);
    assert(!empty.canOccupy(129, 7, 3));
    std::fill_n(map.layers[0], ExploreMapGenerator::CELL_COUNT, 1097);
    empty.reset(map, 0, fishPool, 0, 0);
    assert(empty.count() == 1 && empty.pose(0).speciesId == 129);
    assert(!empty.canOccupy(74, 7, 3));
    // Aquatic mammals, turtles, amphibians and dragons remain on land.
    for (uint16_t species : {uint16_t(7), uint16_t(8), uint16_t(9), uint16_t(134),
            uint16_t(147), uint16_t(148), uint16_t(183), uint16_t(184),
            uint16_t(194), uint16_t(195), uint16_t(298)}) {
        auto nonFish = fishPool;
        nonFish.count = 1;
        nonFish.entries[0].speciesId = species;
        empty.reset(map, 0, nonFish, 0, 0);
        assert(empty.count() == 0 && !empty.canOccupy(species, 7, 3));
    }
    for (uint16_t species : {uint16_t(129), uint16_t(130)}) {
        auto fish = fishPool;
        fish.count = 1;
        fish.entries[0].speciesId = species;
        for (uint16_t base : {uint16_t(48), uint16_t(144), uint16_t(1097)}) {
            std::fill_n(map.layers[0], ExploreMapGenerator::CELL_COUNT, base);
            // A continuous narrow river passing under a two-row bridge.
            for (int y = 1; y < 11; ++y)
                for (int x = 1; x < 15; ++x)
                    if (x != 7) map.layers[0][y * 16 + x] = 64;
            map.layers[2][4 * 16 + 7] = 1627;
            map.layers[2][5 * 16 + 7] = 1643;
            bool visitedBridge = false;
            for (uint32_t seed = 1; seed <= 32; ++seed) {
                map.seed = seed;
                empty.reset(map, 0, fish, 0, 0);
                assert(empty.count() == 1);
                const auto origin = empty.pose(0);
                assert(map.layers[2][int(origin.y / 26) * 16 + 7] == 0);
                assert(empty.canOccupy(species, 7, 4) && empty.canOccupy(species, 7, 5));
                for (uint32_t ms = 100; ms <= 30000; ms += 100) {
                    empty.update(ms, true, -100, -100, {0, 0, 416, 312});
                    const auto& p = empty.pose(0);
                    assert(p.surface == ExploreEcology::Surface::WATER);
                    assert(empty.canStand(species, p.x, p.y));
                    visitedBridge |= map.layers[2][int(p.y / 26) * 16 + int(p.x / 26)] != 0;
                }
            }
            assert(visitedBridge);
            // A bridge does not turn its dry approach or a water rock into water.
            map.layers[0][4 * 16 + 7] = 385;
            map.layers[2][5 * 16 + 7] = 1532;
            empty.reset(map, 0, fish, 0, 0);
            assert(!empty.canOccupy(species, 7, 4) && !empty.canOccupy(species, 7, 5));
            std::fill_n(map.layers[2], ExploreMapGenerator::CELL_COUNT, 0);
        }
    }
    for (uint16_t hazard : {uint16_t(64), uint16_t(288), uint16_t(1185), uint16_t(4744)}) {
        std::fill_n(map.layers[0], ExploreMapGenerator::CELL_COUNT, hazard);
        empty.reset(map, 0, fishPool, 0, 0);
        assert(empty.count() == 0);
    }
    // Offscreen actors wait for a slower cadence; the player can prompt a turn.
    assert(ExploreMapGenerator::generate(7, ExploreMapGenerator::Edge::BOTTOM, 0, map));
    Simulation visible, offscreen;
    visible.reset(map, 0, activePool, 0, 0);
    offscreen.reset(map, 0, activePool, 0, 0);
    assert(visible.count() > 0);
    const auto pose = visible.pose(0);
    const float playerX = pose.x, playerY = pose.y - 10;
    visible.update(100, true, playerX, playerY, {0, 0, 416, 312});
    offscreen.update(100, true, playerX, playerY, {-1000, -1000, 10, 10});
    assert(ExploreEcology::samePose(pose, offscreen.pose(0)));
    checkEpisodes();
    checkLocomotionAndCover();
    checkWideFlight();
    checkGeneratedFlight();
    std::printf("ecology: %d/%d maps have 2-3 actors; %d water, %d ice, %d rock actors; all areas, habitats, behavior, pause and pool verified\n",
        populated, maps, waterActors, iceActors, rockActors);
}
