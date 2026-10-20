#include "game/ExploreEcology.h"

#include <algorithm>
#include <cmath>

#include "game/ExploreRouteGeometry.h"
#include "game/ExploreTileSemantics.h"
#include "game/ExploreCaveTiles.h"

namespace ExploreEcology {
namespace {
constexpr float TILE = ExploreRouteGeometry::TILE_SIZE;
uint32_t next(uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}
float distanceSquared(float x, float y, float bx, float by) {
    return (x - bx) * (x - bx) + (y - by) * (y - by);
}
uint8_t facing(float dx, float dy) {
    if (std::fabs(dx) > std::fabs(dy)) return dx < 0 ? 1 : 3;
    return dy < 0 ? 2 : 0;
}
bool grass(uint16_t tile) {
    return (tile >= 385 && tile <= 390) || tile == 399 || tile == 415;
}
bool waterOnly(uint16_t species) {
    // Explicit fish family in the current roster; Water typing alone is not enough.
    return species == 129 || species == 130;
}
Surface groundSurface(const ExploreMapGenerator::Map& map, uint16_t cell) {
    const uint16_t base = map.layers[0][cell];
    const uint16_t overlay = map.layers[1][cell];
    const uint16_t top = map.layers[2][cell];
    // A bridge leaves the river connected below its deck. Never reinterpret
    // shore, waterfall, rock or a dry bridge approach as swimming habitat.
    if (!overlay && (!top || ExploreTileSemantics::bridgeDeck(top)) &&
        (base == 48 || base == 144 || base == 1097)) return Surface::WATER;
    if (map.layers[2][cell] && !ExploreTileSemantics::groundBehindDecoration(
            map.layers[2][cell], 2)) return Surface::NONE;
    if (overlay && !ExploreTileSemantics::groundBehindDecoration(overlay, 1))
        return Surface::NONE;
    if (grass(base)) return Surface::GRASS;
    if (base == ExploreCaveTiles::CAVE_FLOOR) return Surface::ROCK;
    if (map.areaIndex == ExploreMapGenerator::FROST_CRYSTAL_CAVE_AREA &&
        (base == 4511 || base == 4501 || (base >= 4780 && base <= 4794) ||
         ExploreMapGenerator::isSmoothIceTile(base))) return Surface::ICE;
    return Surface::NONE;
}
}

Temperament temperamentForSpecies(uint16_t speciesId) {
    switch (speciesId) {
    case 10: case 16: case 17: case 18: case 161: case 162: case 261:
        return Temperament::SHY;
    case 12: case 41: case 42: case 169: case 278: case 279: case 380: case 381:
        return Temperament::AERIAL;
    case 1: case 2: case 4: case 5: case 7: case 8: case 9:
    case 25: case 26: case 133: case 134: case 135: case 136:
    case 172: case 183: case 184: case 196: case 197:
    case 280: case 281: case 282: case 298:
        return Temperament::CURIOUS;
    default: return Temperament::RELAXED;
    }
}

Locomotion locomotionForSpecies(uint16_t speciesId) {
    switch (speciesId) {
    case 12: case 41: case 42: case 169: case 278: case 279:
        return Locomotion::FLAP;
    case 380: case 381: return Locomotion::FLOAT;
    default: return Locomotion::GROUND;
    }
}

bool isEpisode(Activity activity) {
    return activity != Activity::REST && activity != Activity::WALK &&
        activity != Activity::RUSTLE && activity != Activity::HIDE &&
        activity != Activity::CRUISE && activity != Activity::ROAM &&
        activity != Activity::DEPART && activity != Activity::AWAY && activity != Activity::ARRIVE;
}

bool Simulation::safeCell(int x, int y) const {
    return surfaceAt(x, y) != Surface::NONE;
}

Surface Simulation::surfaceAt(int x, int y) const {
    if (x <= 0 || x >= ExploreMapGenerator::WIDTH - 1 ||
        y <= 0 || y >= ExploreMapGenerator::HEIGHT - 1) return Surface::NONE;
    return surfaces_[y * ExploreMapGenerator::WIDTH + x];
}

bool Simulation::canOccupy(uint16_t speciesId, int x, int y) const {
    const Surface surface = surfaceAt(x, y);
    if (surface == Surface::NONE) return false;
    return surface == Surface::WATER ? waterOnly(speciesId) : !waterOnly(speciesId);
}

bool Simulation::canStand(uint16_t speciesId, float x, float y) const {
    const int cx = int(x / TILE), cy = int(y / TILE);
    if (!canOccupy(speciesId, cx, cy)) return false;
    // The upper part is ground behind the prop; its lower footprint remains
    // impassable. A retreat may approach from the side, never through a trunk.
    return !solidFootprint_[cy * ExploreMapGenerator::WIDTH + cx] ||
        y - cy * TILE <= TILE * 0.6f;
}

bool Simulation::safeLeg(uint16_t species, float x, float y, float tx, float ty) const {
    const Surface surface = surfaceAt(int(x / TILE), int(y / TILE));
    for (int sample = 1; sample <= 4; ++sample) {
        const float t = sample / 4.0f;
        const float px = x + (tx - x) * t, py = y + (ty - y) * t;
        if (surfaceAt(int(px / TILE), int(py / TILE)) != surface ||
            !canStand(species, px, py)) return false;
    }
    return true;
}

bool Simulation::occupied(float x, float y, uint8_t except) const {
    for (uint8_t i = 0; i < count_; ++i) {
        if (i == except) continue;
        const auto& actor = actors_[i];
        if (actor.pose.airborne && wideFlightSpecies(actor.pose.speciesId)) continue;
        if (distanceSquared(x, y, actor.pose.x, actor.pose.y) < TILE * TILE ||
            distanceSquared(x, y, actor.toX, actor.toY) < TILE * TILE ||
            ((isEpisode(actor.pose.activity) || actor.pose.activity == Activity::HIDE) &&
             distanceSquared(x, y, actor.episodeX, actor.episodeY) < TILE * TILE)) return true;
    }
    return false;
}

void Simulation::reset(const ExploreMapGenerator::Map& map, uint8_t pathIndex,
                        const ExplorePool::Pool& pool, uint8_t hiddenPoolMask,
                        uint32_t nowMs) {
    count_ = 0;
    group_ = {};
    episodeOwner_ = CAPACITY;
    lastUpdateMs_ = nowMs;
    std::fill_n(surfaces_, ExploreMapGenerator::CELL_COUNT, Surface::NONE);
    std::fill_n(cover_, ExploreMapGenerator::CELL_COUNT, Cover::NONE);
    std::fill_n(solidFootprint_, ExploreMapGenerator::CELL_COUNT, false);
    buildFlightHabitat(map);
    if (map.areaIndex > ExploreMapGenerator::ANCIENT_WATERFALL_VALLEY_AREA ||
        pathIndex >= map.pathCount || pool.count == 0) return;
    const auto& path = map.paths[pathIndex];
    if (path.pointCount < 2) return;
    for (uint16_t cell = 0; cell < ExploreMapGenerator::CELL_COUNT; ++cell) {
        surfaces_[cell] = groundSurface(map, cell);
        if (surfaces_[cell] == Surface::NONE) continue;
        if (ExploreTileSemantics::denseGrass(map.layers[0][cell]) ||
            map.layers[1][cell] == 391) cover_[cell] = Cover::GRASS;
        for (uint8_t layer = 1; layer < ExploreMapGenerator::LAYER_COUNT; ++layer) {
            const uint16_t tile = map.layers[layer][cell];
            if (tile != 391 && ExploreTileSemantics::groundBehindDecoration(tile, layer)) {
                cover_[cell] = Cover::SOLID;
                solidFootprint_[cell] |= ExploreTileSemantics::objectDepthOffsetRows(tile, layer) == 1;
            }
        }
    }
    uint32_t random = map.seed ^ 0xECA10579U;
    if (random == 0) random = 1;
    const uint8_t poolCount = std::min<uint8_t>(pool.count, ExplorePool::POOL_CAP);
    const uint8_t requested = std::min<uint8_t>(poolCount, 2 + (next(random) % 4 == 0));
    if (spawnGroup(path, pool, hiddenPoolMask, random)) return;
    uint8_t usedMask = 0;
    for (uint8_t slot = 0; slot < poolCount && count_ < requested; ++slot) {
        uint32_t total = 0;
        for (uint8_t i = 0; i < poolCount; ++i)
            if (!(usedMask & (1U << i)) && pool.entries[i].speciesId)
                total += std::max<uint8_t>(1, pool.entries[i].weight);
        if (!total) break;
        uint32_t roll = next(random) % total;
        uint8_t selected = 0;
        for (; selected < poolCount; ++selected) {
            if ((usedMask & (1U << selected)) || !pool.entries[selected].speciesId) continue;
            const uint32_t weight = std::max<uint8_t>(1, pool.entries[selected].weight);
            if (roll < weight) break;
            roll -= weight;
        }
        if (selected == poolCount) break;
        usedMask |= 1U << selected;
        int bestCell = -1;
        uint32_t bestScore = 0;
        for (uint8_t y = 1; y < ExploreMapGenerator::HEIGHT - 1; ++y) {
            for (uint8_t x = 1; x < ExploreMapGenerator::WIDTH - 1; ++x) {
                const uint16_t species = pool.entries[selected].speciesId;
                if (!canOccupy(species, x, y)) continue;
                // Start fish in visible water; later steps may pass below a bridge.
                if (ExploreTileSemantics::bridgeDeck(map.layers[2][y * ExploreMapGenerator::WIDTH + x]))
                    continue;
                const float wx = ExploreRouteGeometry::tileCenter(x);
                const float wy = ExploreRouteGeometry::tileCenter(y);
                if (wideFlightSpecies(species) && !canFlyAt(wx, wy)) continue;
                float routeDistance = 1e9f;
                for (uint8_t p = 1; p + 1 < path.pointCount; ++p) {
                    const auto point = ExploreRouteGeometry::pathPoint(path, p);
                    routeDistance = std::min(routeDistance,
                        distanceSquared(wx, wy, point.x, point.y));
                }
                if (routeDistance < TILE * TILE * 0.8f ||
                    routeDistance > TILE * TILE * 6.25f) continue;
                bool crowded = false;
                for (uint8_t i = 0; i < count_; ++i)
                    crowded |= distanceSquared(wx, wy, actors_[i].pose.x,
                        actors_[i].pose.y) < TILE * TILE * 9;
                // A third actor needs a separated part of the route, rather
                // than adding another sprite to the same observation window.
                if (count_ == 2) {
                    const float left = std::min({wx, actors_[0].homeX, actors_[1].homeX});
                    const float right = std::max({wx, actors_[0].homeX, actors_[1].homeX});
                    const float top = std::min({wy, actors_[0].homeY, actors_[1].homeY});
                    const float bottom = std::max({wy, actors_[0].homeY, actors_[1].homeY});
                    crowded |= right - left < TILE * 9 && bottom - top < TILE * 8;
                }
                if (crowded) continue;
                uint32_t score = next(random) % 100;
                unsigned exits = 0;
                for (int direction = 0; direction < 4; ++direction) {
                    constexpr int dx[] = {0, -1, 0, 1}, dy[] = {1, 0, -1, 0};
                    exits += safeLeg(species, wx, wy,
                        wx + dx[direction] * TILE, wy + dy[direction] * TILE);
                }
                // Prefer habitat where the actor can actually take steps.
                // Keep isolated safe cells available when space is limited.
                if (exits) score += 200 + exits * 20;
                const uint16_t tile = map.layers[0][y * ExploreMapGenerator::WIDTH + x];
                // Caterpie and Metapod prefer dense grass; birds prefer tree edges.
                if ((species == 10 || species == 11) && (tile == 390 || tile == 399)) score += 65;
                if (species == 16) {
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dx = -1; dx <= 1; ++dx)
                            if (ExploreMapGenerator::isForestTile(map.layers[1]
                                    [(y + dy) * ExploreMapGenerator::WIDTH + x + dx])) score += 12;
                }
                if (bestCell < 0 || score > bestScore) {
                    bestCell = y * ExploreMapGenerator::WIDTH + x;
                    bestScore = score;
                }
            }
        }
        if (bestCell < 0) continue;
        appendActor(pool.entries[selected].speciesId, bestCell,
            (hiddenPoolMask & (1U << selected)) != 0, random);
    }
}

void Simulation::appendActor(uint16_t species, uint8_t cell, bool hidden, uint32_t& random) {
    Actor actor{};
    actor.pose.speciesId = species;
    actor.pose.x = actor.homeX = actor.fromX = actor.toX =
        ExploreRouteGeometry::tileCenter(cell % ExploreMapGenerator::WIDTH);
    actor.pose.y = actor.homeY = actor.fromY = actor.toY =
        ExploreRouteGeometry::tileCenter(cell / ExploreMapGenerator::WIDTH);
    actor.pose.direction = next(random) % 4;
    actor.pose.hidden = hidden;
    actor.pose.surface = surfaces_[cell];
    actor.pose.cover = cover_[cell];
    actor.random = next(random);
    actor.durationMs = 600 + next(actor.random) % 1401;
    actor.episodeCooldownMs = 3000 + next(actor.random) % 5001;
    actors_[count_++] = actor;
    if (!actor.pose.hidden && wideFlightSpecies(actor.pose.speciesId) &&
        canFlyAt(actor.pose.x, actor.pose.y)) beginWideFlight(count_ - 1);
    else if (!actor.pose.hidden && locomotionForSpecies(actor.pose.speciesId) == Locomotion::FLAP)
        cruise(count_ - 1);
}

Cover Simulation::coverAt(float x, float y) const {
    const int cx = int(x / TILE), cy = int(y / TILE);
    return safeCell(cx, cy) ? cover_[cy * ExploreMapGenerator::WIDTH + cx] : Cover::NONE;
}

void Simulation::start(Actor& actor, Activity activity, uint32_t durationMs) {
    actor.pose.activity = activity;
    actor.pose.walking = activity == Activity::WALK || activity == Activity::APPROACH ||
        activity == Activity::RETURN || activity == Activity::FLEE || activity == Activity::FLY;
    actor.pose.frame = 0;
    actor.pose.lift = 0;
    actor.pose.sway = 0;
    actor.pose.cover = coverAt(actor.pose.x, actor.pose.y);
    actor.pose.airborne = !actor.pose.hidden &&
        locomotionForSpecies(actor.pose.speciesId) == Locomotion::FLAP;
    if (actor.pose.airborne) {
        actor.pose.frame = (actor.wingMs / 120) % 3;
        actor.pose.lift = 8;
    }
    actor.elapsedMs = 0;
    actor.durationMs = durationMs;
}

bool Simulation::claimEpisode(uint8_t index) {
    if (episodeOwner_ != CAPACITY) return false;
    auto& actor = actors_[index];
    if (actor.pose.hidden || actor.episodeCooldownMs) return false;
    episodeOwner_ = index;
    const bool flapping = locomotionForSpecies(actor.pose.speciesId) == Locomotion::FLAP;
    actor.episodeX = flapping ? actor.homeX : actor.pose.x;
    actor.episodeY = flapping ? actor.homeY : actor.pose.y;
    return true;
}

void Simulation::cruise(uint8_t index) {
    auto& actor = actors_[index];
    // A closed loop starts at the home anchor and stays well inside its cell.
    actor.pose.x = actor.homeX; actor.pose.y = actor.homeY;
    actor.fromX = actor.toX = actor.homeX;
    actor.fromY = actor.toY = actor.homeY;
    start(actor, Activity::CRUISE, 2400);
}

void Simulation::rest(uint8_t index, bool endEpisode) {
    auto& actor = actors_[index];
    if (!actor.pose.hidden && locomotionForSpecies(actor.pose.speciesId) == Locomotion::FLAP)
        cruise(index);
    else start(actor, Activity::REST, 1400 + next(actor.random) % 1601);
    if (endEpisode && episodeOwner_ == index) {
        episodeOwner_ = CAPACITY;
        actor.episodeCooldownMs = 15000 + next(actor.random) % 15001;
        actor.attentionCooldownMs = 15000;
    }
}

bool Simulation::startStep(uint8_t index, uint8_t direction, float length,
                           Activity activity, uint32_t durationMs) {
    auto& actor = actors_[index];
    constexpr int dx[] = {0, -1, 0, 1}, dy[] = {1, 0, -1, 0};
    const float tx = actor.pose.x + dx[direction] * length;
    const float ty = actor.pose.y + dy[direction] * length;
    if (distanceSquared(tx, ty, actor.homeX, actor.homeY) > TILE * TILE * 2 ||
        occupied(tx, ty, index)) return false;
    if (!safeLeg(actor.pose.speciesId, actor.pose.x, actor.pose.y, tx, ty)) return false;
    actor.fromX = actor.pose.x; actor.fromY = actor.pose.y;
    actor.toX = tx; actor.toY = ty;
    start(actor, activity, durationMs);
    actor.pose.direction = direction;
    return true;
}

bool Simulation::wander(uint8_t index) {
    auto& actor = actors_[index];
    const uint8_t first = next(actor.random) % 4;
    for (float length : {TILE, TILE * 0.5f}) {
        for (uint8_t offset = 0; offset < 4; ++offset)
            if (startStep(index, (first + offset) % 4, length, Activity::WALK, 1600))
                return true;
    }
    return false;
}

bool Simulation::chooseStep(uint8_t index, float playerX, float playerY, bool retreat,
                            Activity activity, uint32_t durationMs) {
    const auto& actor = actors_[index];
    constexpr int dx[] = {0, -1, 0, 1}, dy[] = {1, 0, -1, 0};
    uint8_t best = 4;
    float bestScore = -1e9f;
    for (uint8_t direction = 0; direction < 4; ++direction) {
        const float length = retreat ? TILE : TILE * 0.5f;
        const float tx = actor.pose.x + dx[direction] * length;
        const float ty = actor.pose.y + dy[direction] * length;
        const float before = distanceSquared(actor.pose.x, actor.pose.y, playerX, playerY);
        const float after = distanceSquared(tx, ty, playerX, playerY);
        if (retreat ? after <= before : (after >= before || after < TILE * TILE * 0.36f)) continue;
        // Probe by value; only the winning leg changes actor state.
        Actor saved = actors_[index];
        const bool safe = startStep(index, direction, length, activity, durationMs);
        actors_[index] = saved;
        if (!safe) continue;
        float score = retreat ? after : -after;
        if (retreat && coverAt(tx, ty) != Cover::NONE) score += TILE * TILE * 16;
        if (score > bestScore) { bestScore = score; best = direction; }
    }
    return best < 4 && startStep(index, best, retreat ? TILE : TILE * 0.5f, activity, durationMs);
}

void Simulation::returnToOrigin(uint8_t index) {
    auto& actor = actors_[index];
    if (distanceSquared(actor.pose.x, actor.pose.y, actor.episodeX, actor.episodeY) < 0.01f) {
        rest(index, true);
        return;
    }
    // Episodes take one cardinal leg; retrace it over the same safe surface.
    // A moving neighbour may temporarily reserve the origin, so wait first.
    if (occupied(actor.episodeX, actor.episodeY, index)) {
        start(actor, Activity::OBSERVE, 600);
        return;
    }
    actor.fromX = actor.pose.x; actor.fromY = actor.pose.y;
    actor.toX = actor.episodeX; actor.toY = actor.episodeY;
    actor.pose.direction = facing(actor.toX - actor.fromX, actor.toY - actor.fromY);
    start(actor, Activity::RETURN, 1500);
}

void Simulation::beginPeek(uint8_t index) {
    auto& actor = actors_[index];
    const uint8_t first = (actor.pose.direction + 1) % 4;
    // Use a small safe lateral excursion; return to the covered point before
    // emerging. The body is always at its real ground position.
    const Actor saved = actor;
    float bestLength = 0, bestScore = -1e9f;
    uint8_t bestDirection = first;
    for (float length : {TILE * 0.55f, TILE * 0.35f}) {
        for (uint8_t offset = 0; offset < 4; ++offset) {
            const uint8_t direction = (first + offset) % 4;
            if (startStep(index, direction, length, Activity::PEEK, 2000)) {
                const float score = (coverAt(actor.toX, actor.toY) == Cover::NONE ? 100 : 0) +
                    length - offset;
                if (score > bestScore) {
                    bestScore = score; bestLength = length; bestDirection = direction;
                }
            }
            actor = saved;
        }
    }
    if (bestLength && startStep(index, bestDirection, bestLength, Activity::PEEK, 2000)) return;
    start(actor, Activity::PEEK, 2000);
    actor.fromX = actor.toX = actor.pose.x;
    actor.fromY = actor.toY = actor.pose.y;
}

void Simulation::advance(uint8_t index, uint32_t dt, float playerX, float playerY,
                         bool visible) {
    auto& actor = actors_[index];
    if (!actor.pose.hidden && wideFlightSpecies(actor.pose.speciesId) && actor.flight.region) {
        advanceFlight(index, dt);
        return;
    }
    auto& pose = actor.pose;
    actor.attentionCooldownMs -= std::min(actor.attentionCooldownMs, dt);
    actor.episodeCooldownMs -= std::min(actor.episodeCooldownMs, dt);
    actor.wingMs = (actor.wingMs + dt) % 7200;
    actor.elapsedMs = std::min(actor.elapsedMs + dt, actor.durationMs);
    const Activity action = pose.activity;
    const float t = actor.elapsedMs / float(std::max<uint32_t>(1, actor.durationMs));
    const bool fastFrames = pose.walking || action == Activity::TAKEOFF ||
        action == Activity::LAND || action == Activity::SPLASH || action == Activity::GATHER;
    pose.frame = uint8_t((actor.elapsedMs / (fastFrames ? 150 : 600)) % 6);
    if (pose.walking) {
        // Smooth endpoints for short, readable approaches and retreats.
        const float eased = pose.airborne ? t : t * t * (3.0f - 2.0f * t);
        pose.x = actor.fromX + (actor.toX - actor.fromX) * eased;
        pose.y = actor.fromY + (actor.toY - actor.fromY) * eased;
    }
    pose.lift = 0; pose.sway = 0;
    switch (action) {
    case Activity::DOZE:
        pose.frame = (actor.elapsedMs / 900) % 2;
        pose.lift = pose.frame;
        break;
    case Activity::WAKE:
        pose.lift = int8_t(std::lround(std::sin(t * 3.14159265f) * 2));
        break;
    case Activity::TAKEOFF: pose.lift = int8_t(std::lround(t * 10)); break;
    case Activity::FLY:
        pose.lift = int8_t(10 + std::lround(std::sin(t * 3.14159265f) * 4));
        pose.sway = int8_t(std::lround(std::sin(t * 6.2831853f) * 2));
        break;
    case Activity::LAND: pose.lift = int8_t(std::lround((1 - t) * 10)); break;
    case Activity::JUMP: pose.lift = int8_t(std::lround(4 * t * (1 - t) * 12)); break;
    case Activity::PEEK: {
        const float peek = t >= 1 ? 0 : std::sin(t * 3.14159265f);
        pose.x = actor.fromX + (actor.toX - actor.fromX) * peek;
        pose.y = actor.fromY + (actor.toY - actor.fromY) * peek;
        pose.direction = (actor.elapsedMs / 550) % 2 ? 1 : 3;
        break;
    }
    case Activity::CRUISE: {
        const float angle = t * 6.2831853f;
        pose.x = actor.homeX + std::sin(angle) * 5;
        const int cell = int(actor.homeY / TILE) * ExploreMapGenerator::WIDTH + int(actor.homeX / TILE);
        const float radiusY = solidFootprint_[cell] ? 1.0f : 3.0f;
        pose.y = actor.homeY + (1 - std::cos(angle)) * radiusY;
        pose.direction = facing(std::cos(angle) * 5, std::sin(angle) * radiusY);
        break;
    }
    case Activity::RUSTLE:
        pose.frame = 1 + (actor.elapsedMs / 180) % 2;
        break;
    default: break;
    }
    pose.cover = coverAt(pose.x, pose.y);
    if (pose.airborne) {
        pose.frame = (actor.wingMs / 120) % 3;
        pose.lift = int8_t(8 + std::lround(std::sin(actor.wingMs * 6.2831853f / 360) * 1.5f));
        pose.sway = 0;
    }
    if (pose.hidden && action != Activity::RUSTLE) pose.frame = 0;
    // Finish jumps and landings before considering any new player reaction.
    const float playerDistance = distanceSquared(pose.x, pose.y, playerX, playerY);
    if (action == Activity::DOZE && visible && playerDistance < TILE * TILE * 2.25f) {
        pose.direction = facing(playerX - pose.x, playerY - pose.y);
        start(actor, Activity::WAKE, 1000);
        return;
    }
    if (action == Activity::REST && visible && !pose.hidden &&
        !actor.attentionCooldownMs && playerDistance < TILE * TILE * 4 && claimEpisode(index)) {
        pose.direction = facing(playerX - pose.x, playerY - pose.y);
        start(actor, Activity::WATCH, 800);
        return;
    }
    if (actor.elapsedMs < actor.durationMs) return;
    switch (action) {
    case Activity::WATCH: {
        const auto personality = temperamentForSpecies(pose.speciesId);
        if (pose.surface != Surface::WATER && personality == Temperament::SHY &&
            playerDistance < TILE * TILE * 1.44f &&
            chooseStep(index, playerX, playerY, true, Activity::FLEE, 850)) return;
        if (personality == Temperament::CURIOUS &&
            chooseStep(index, playerX, playerY, false, Activity::APPROACH, 1400)) return;
        // If there is no safe approach/retreat, visibly look away rather than
        // restarting another idle timer in exactly the same pose.
        pose.direction = (pose.direction + (next(actor.random) % 2 ? 1 : 3)) % 4;
        rest(index, true);
        return;
    }
    case Activity::APPROACH: start(actor, Activity::OBSERVE, 1600); return;
    case Activity::OBSERVE: returnToOrigin(index); return;
    case Activity::RETURN: rest(index, true); return;
    case Activity::FLEE:
        start(actor, pose.cover != Cover::NONE ? Activity::HIDE : Activity::OBSERVE,
            pose.cover != Cover::NONE ? 2000 + next(actor.random) % 2001 : 1200);
        if (pose.activity == Activity::HIDE) episodeOwner_ = CAPACITY;
        return;
    case Activity::BURROW:
        start(actor, Activity::HIDE, 2500); episodeOwner_ = CAPACITY; return;
    case Activity::HIDE:
        if (episodeOwner_ != CAPACITY && episodeOwner_ != index) {
            start(actor, Activity::HIDE, 1000); return;
        }
        episodeOwner_ = index;
        beginPeek(index); return;
    case Activity::PEEK:
        if (playerDistance < TILE * TILE * 2.25f) {
            start(actor, Activity::HIDE, 2500); episodeOwner_ = CAPACITY; return;
        }
        start(actor, Activity::EMERGE, 800); return;
    case Activity::EMERGE: returnToOrigin(index); return;
    case Activity::DOZE: start(actor, Activity::WAKE, 1000); return;
    case Activity::WAKE:
        pose.direction = facing(playerX - pose.x, playerY - pose.y);
        if (!startStep(index, pose.direction, TILE * 0.5f, Activity::APPROACH, 1500))
            start(actor, Activity::OBSERVE, 1000);
        return;
    case Activity::TAKEOFF: {
        const uint8_t direction = next(actor.random) % 4;
        if (!startStep(index, direction, TILE, Activity::FLY, 2400)) {
            start(actor, Activity::LAND, 600); pose.lift = 10;
        } else pose.lift = 10;
        return;
    }
    case Activity::FLY:
        if (pose.airborne) returnToOrigin(index);
        else { start(actor, Activity::LAND, 600); pose.lift = 10; }
        return;
    case Activity::LAND: returnToOrigin(index); return;
    case Activity::GATHER: start(actor, Activity::JUMP, 900); return;
    case Activity::JUMP: start(actor, Activity::SPLASH, 700); return;
    case Activity::SPLASH: rest(index, true); return;
    case Activity::WALK: case Activity::RUSTLE: rest(index, false); return;
    case Activity::CRUISE:
        // Start the larger flight at the loop seam, avoiding position jumps.
        pose.x = actor.homeX; pose.y = actor.homeY;
        if (visible && claimEpisode(index)) {
            const uint8_t first = next(actor.random) % 4;
            for (uint8_t offset = 0; offset < 4; ++offset)
                if (startStep(index, (first + offset) % 4, TILE, Activity::FLY, 1600)) return;
            rest(index, true);
        } else cruise(index);
        return;
    case Activity::REST: break;
    case Activity::ROAM: case Activity::DEPART: case Activity::AWAY: case Activity::ARRIVE:
        return; // Handled by the separate flight follower above.
    }
    if (pose.hidden) {
        start(actor, Activity::RUSTLE, 720);
        return;
    }
    if (visible && claimEpisode(index)) {
        const auto personality = temperamentForSpecies(pose.speciesId);
        if (pose.surface == Surface::WATER) start(actor, Activity::GATHER, 700);
        else if (personality == Temperament::AERIAL) start(actor, Activity::TAKEOFF, 600);
        else if (pose.cover != Cover::NONE &&
                 personality == Temperament::SHY) {
            start(actor, Activity::BURROW, 600);
        } else if (personality == Temperament::RELAXED) start(actor, Activity::DOZE,
            6000 + next(actor.random) % 6001);
        else {
            pose.direction = (pose.direction + (next(actor.random) % 2 ? 1 : 3)) % 4;
            start(actor, Activity::OBSERVE, 1500);
        }
        return;
    }
    if (next(actor.random) % 100 >= 30 && wander(index)) return;
    // Small looks and steps never queue behind another actor's big episode.
    // With single-frame idle art, changing a frame counter is not visible.
    pose.direction = (pose.direction + (next(actor.random) % 2 ? 1 : 3)) % 4;
    rest(index, false);
}

void Simulation::update(uint32_t nowMs, bool active, float playerX, float playerY,
                         View view) {
    const uint32_t dt = std::min<uint32_t>(nowMs - lastUpdateMs_, 250);
    lastUpdateMs_ = nowMs;
    if (!active) return;
    if (group_.active) {
        group_.pendingMs += dt;
        bool visible = false;
        for (uint8_t i = 0; i < count_; ++i)
            visible |= view.contains(actors_[i].pose.x, actors_[i].pose.y);
        if (group_.pendingMs >= (visible ? 100U : 500U)) {
            const uint32_t elapsed = group_.pendingMs;
            group_.pendingMs = 0;
            advanceGroup(elapsed, playerX, playerY, visible);
        }
        return;
    }
    for (uint8_t i = 0; i < count_; ++i) {
        auto& actor = actors_[i];
        actor.pendingMs += dt;
        const uint32_t interval = view.contains(actor.pose.x, actor.pose.y,
            wideFlightSpecies(actor.pose.speciesId) ? 96 : 30) ? 100 : 500;
        if (actor.pendingMs < interval) continue;
        const uint32_t elapsed = actor.pendingMs;
        actor.pendingMs = 0;
        advance(i, elapsed, playerX, playerY, view.contains(actor.pose.x, actor.pose.y, 0));
    }
}

void Simulation::revealSpecies(uint16_t speciesId) {
    for (uint8_t i = 0; i < count_; ++i) {
        if (actors_[i].pose.speciesId != speciesId) continue;
        actors_[i].pose.hidden = false;
        actors_[i].pose.frame = 0;
        if (wideFlightSpecies(speciesId) && canFlyAt(actors_[i].pose.x, actors_[i].pose.y))
            beginWideFlight(i);
        else if (locomotionForSpecies(speciesId) == Locomotion::FLAP) cruise(i);
    }
}

}  // namespace ExploreEcology
