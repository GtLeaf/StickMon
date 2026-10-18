#include "game/ExploreEcology.h"

#include <algorithm>
#include <cmath>

#include "game/ExploreCaveTiles.h"
#include "game/ExploreRouteGeometry.h"
#include "game/ExploreTileSemantics.h"

namespace ExploreEcology {
namespace {
constexpr float TILE = ExploreRouteGeometry::TILE_SIZE;
constexpr float WIDTH = ExploreMapGenerator::WIDTH * TILE;
constexpr float HEIGHT = ExploreMapGenerator::HEIGHT * TILE;
constexpr float MARGIN = 64;
constexpr float PI = 3.14159265f;
constexpr int DX[] = {0, -1, 0, 1}, DY[] = {1, 0, -1, 0};
uint32_t roll(uint32_t& state) {
    state ^= state << 13; state ^= state >> 17; state ^= state << 5;
    return state;
}
float center(int cell, bool y) {
    return ((y ? cell / ExploreMapGenerator::WIDTH : cell % ExploreMapGenerator::WIDTH) + 0.5f) * TILE;
}
float angleDelta(float target, float current) {
    float delta = std::fmod(target - current, 2 * PI);
    if (delta > PI) delta -= 2 * PI;
    if (delta < -PI) delta += 2 * PI;
    return delta;
}
bool border(int cell) {
    const int x = cell % ExploreMapGenerator::WIDTH, y = cell / ExploreMapGenerator::WIDTH;
    return x == 0 || y == 0 || x == ExploreMapGenerator::WIDTH - 1 || y == ExploreMapGenerator::HEIGHT - 1;
}
uint8_t edgeFor(int cell) {
    if (cell % ExploreMapGenerator::WIDTH == 0) return 1;
    if (cell % ExploreMapGenerator::WIDTH == ExploreMapGenerator::WIDTH - 1) return 3;
    return cell / ExploreMapGenerator::WIDTH == 0 ? 2 : 0;
}
void outside(int cell, uint8_t edge, float& x, float& y) {
    x = center(cell, false); y = center(cell, true);
    if (edge == 1) x = -MARGIN;
    if (edge == 3) x = WIDTH + MARGIN;
    if (edge == 2) y = -MARGIN;
    if (edge == 0) y = HEIGHT + MARGIN;
}
Surface flightSurface(const ExploreMapGenerator::Map& map, int cell) {
    const uint16_t base = map.layers[0][cell];
    Surface surface = Surface::NONE;
    if ((base >= 385 && base <= 390) || base == 399 || base == 415 ||
        ExploreMapGenerator::isRoadTile(base) || base == 4500 || base == 4502 || base == 4503)
        surface = Surface::GRASS;
    if (base == ExploreCaveTiles::CAVE_FLOOR) surface = Surface::ROCK;
    if (base == 4511 || ExploreMapGenerator::isSmoothIceTile(base)) surface = Surface::ICE;
    // Horizontal water and shore edges are flyable; waterfall faces are not.
    if (base == 48 || base == 64 || base == 68 || base == 72 || base == 80 || base == 84 ||
        base == 144 || base == 168 || base == 1088 || base == 1090 ||
        (base >= 1096 && base <= 1098) || base == 1104 || base == 1106 ||
        (base >= 4400 && base <= 4403)) surface = Surface::WATER;
    if (surface == Surface::NONE) return surface;
    for (uint8_t layer = 1; layer < ExploreMapGenerator::LAYER_COUNT; ++layer) {
        const uint16_t tile = map.layers[layer][cell];
        if (!tile || tile == 391 || tile == 859 || tile == 1231 || tile == 1532 ||
            tile == 1627 || tile == 1643 || (tile >= 4507 && tile <= 4510) ||
            (tile >= 4542 && tile <= 4544)) continue;
        // Fly behind canopy contours, but keep the trunk row impassable.
        if (ExploreTileSemantics::foregroundDepthOffsetRows(tile) > 1) continue;
        return Surface::NONE;
    }
    return surface;
}
}

bool wideFlightSpecies(uint16_t species) { return species == 278 || species == 279; }

Surface Simulation::flightSurfaceAt(int x, int y) const {
    if (x < 0 || y < 0 || x >= ExploreMapGenerator::WIDTH || y >= ExploreMapGenerator::HEIGHT)
        return Surface::NONE;
    return flightSurfaces_[y * ExploreMapGenerator::WIDTH + x];
}

bool Simulation::canFlyAt(float x, float y) const {
    return flightSurfaceAt(int(std::floor(x / TILE)), int(std::floor(y / TILE))) != Surface::NONE;
}

void Simulation::buildFlightHabitat(const ExploreMapGenerator::Map& map) {
    std::fill_n(flightRegions_, ExploreMapGenerator::CELL_COUNT, 0);
    for (int cell = 0; cell < ExploreMapGenerator::CELL_COUNT; ++cell)
        flightSurfaces_[cell] = flightSurface(map, cell);
    uint8_t region = 0;
    uint8_t queue[ExploreMapGenerator::CELL_COUNT];
    for (int cell = 0; cell < ExploreMapGenerator::CELL_COUNT; ++cell) {
        if (flightSurfaces_[cell] == Surface::NONE || flightRegions_[cell]) continue;
        unsigned head = 0, tail = 0;
        queue[tail++] = cell;
        flightRegions_[cell] = ++region;
        while (head < tail) {
            const int at = queue[head++], x = at % ExploreMapGenerator::WIDTH, y = at / ExploreMapGenerator::WIDTH;
            for (int d = 0; d < 4; ++d) {
                const int nx = x + DX[d], ny = y + DY[d], to = ny * ExploreMapGenerator::WIDTH + nx;
                if (flightSurfaceAt(nx, ny) == Surface::NONE || flightRegions_[to]) continue;
                flightRegions_[to] = region;
                queue[tail++] = to;
            }
        }
    }
}

void Simulation::beginWideFlight(uint8_t index) {
    auto& actor = actors_[index];
    actor.flight = {};
    const int cell = int(actor.pose.y / TILE) * ExploreMapGenerator::WIDTH + int(actor.pose.x / TILE);
    actor.flight.region = flightRegions_[cell];
    actor.flight.heading = std::atan2(float(DY[actor.pose.direction]), float(DX[actor.pose.direction]));
    actor.flight.departMs = 25000 + roll(actor.random) % 20001;
    start(actor, Activity::ROAM, 1);
    if (!planFlight(index, false)) cruise(index);
}

bool Simulation::planFlight(uint8_t index, bool depart) {
    auto& actor = actors_[index];
    auto& flight = actor.flight;
    const int root = flight.routeCount ? flight.route[flight.routeCount - 1] :
        int(actor.pose.y / TILE) * ExploreMapGenerator::WIDTH + int(actor.pose.x / TILE);
    int16_t parent[ExploreMapGenerator::CELL_COUNT];
    std::fill_n(parent, ExploreMapGenerator::CELL_COUNT, -1);
    uint8_t queue[ExploreMapGenerator::CELL_COUNT], distance[ExploreMapGenerator::CELL_COUNT]{};
    unsigned head = 0, tail = 0;
    queue[tail++] = root; parent[root] = root;
    const int first = roll(actor.random) % 4;
    while (head < tail) {
        const int at = queue[head++], x = at % ExploreMapGenerator::WIDTH, y = at / ExploreMapGenerator::WIDTH;
        for (int offset = 0; offset < 4; ++offset) {
            const int d = (first + offset) % 4, nx = x + DX[d], ny = y + DY[d];
            const int to = ny * ExploreMapGenerator::WIDTH + nx;
            if (flightSurfaceAt(nx, ny) == Surface::NONE || parent[to] >= 0) continue;
            parent[to] = at; distance[to] = distance[at] + 1; queue[tail++] = to;
        }
    }
    int goal = -1;
    float best = -1e9f;
    for (unsigned n = 0; n < tail; ++n) {
        const int cell = queue[n];
        if (cell == root || (depart && !border(cell))) continue;
        float score = float(roll(actor.random) % 80);
        // Choose a far destination, while allowing short corridors to work.
        score += std::min<int>(distance[cell], 6) * 40 - std::max<int>(0, distance[cell] - 6) * 8;
        for (uint8_t recent : flight.recent) if (cell == recent) score -= 140;
        const float dx = center(cell, false) - center(root, false);
        const float dy = center(cell, true) - center(root, true);
        const float length = std::sqrt(dx * dx + dy * dy);
        score += 70 * (dx * std::cos(flight.heading) + dy * std::sin(flight.heading)) / length;
        if (score > best) { best = score; goal = cell; }
    }
    if (goal < 0) return false;
    flight.routeCount = flight.routeIndex = 0;
    for (int cell = goal; cell != root; cell = parent[cell]) flight.route[flight.routeCount++] = cell;
    std::reverse(flight.route, flight.route + flight.routeCount);
    flight.recent[flight.recentIndex++ % 4] = goal;
    flight.edgeLeg = false;
    if (depart) { flight.portal = goal; flight.edge = edgeFor(goal); }
    return true;
}

bool Simulation::flightPoint(const Actor& actor, float x, float y) const {
    const auto& flight = actor.flight;
    if (canFlyAt(x, y)) return true;
    if (actor.pose.activity != Activity::ARRIVE &&
        !(actor.pose.activity == Activity::DEPART && flight.edgeLeg)) return false;
    // Only extend the selected open border cell, never a wall or another edge.
    const float px = center(flight.portal, false), py = center(flight.portal, true);
    if ((flight.edge == 1 && x < 0) || (flight.edge == 3 && x >= WIDTH))
        return std::fabs(y - py) < TILE * 0.5f;
    if ((flight.edge == 2 && y < 0) || (flight.edge == 0 && y >= HEIGHT))
        return std::fabs(x - px) < TILE * 0.5f;
    return false;
}

void Simulation::advanceFlight(uint8_t index, uint32_t dt) {
    auto& actor = actors_[index];
    auto& flight = actor.flight;
    auto& pose = actor.pose;
    if (pose.absent) {
        actor.elapsedMs += dt;
        if (actor.elapsedMs < actor.durationMs) return;
        uint8_t portals[ExploreMapGenerator::CELL_COUNT];
        unsigned count = 0;
        for (int cell = 0; cell < ExploreMapGenerator::CELL_COUNT; ++cell)
            if (border(cell) && flightRegions_[cell] == flight.region && cell != flight.portal)
                portals[count++] = cell;
        if (count) flight.portal = portals[roll(actor.random) % count];
        flight.edge = edgeFor(flight.portal);
        outside(flight.portal, flight.edge, pose.x, pose.y);
        flight.heading = std::atan2(float(-DY[flight.edge]), float(-DX[flight.edge]));
        flight.routeCount = flight.routeIndex = 0;
        pose.absent = false;
        start(actor, Activity::ARRIVE, 1);
        return;
    }
    actor.wingMs = (actor.wingMs + dt) % 7200;
    pose.frame = (actor.wingMs / 120) % 3;
    pose.lift = int8_t(10 + std::lround(std::sin(actor.wingMs * 6.2831853f / 360) * 1.5f));
    pose.airborne = true; pose.walking = false; pose.sway = 0;
    if (pose.activity == Activity::CRUISE) {
        // A truly isolated cell still supports continuous local flight.
        actor.elapsedMs = (actor.elapsedMs + dt) % 2400;
        const float angle = actor.elapsedMs * 2 * PI / 2400;
        pose.x = actor.homeX + std::sin(angle) * 5;
        pose.y = actor.homeY + (1 - std::cos(angle)) * 3;
        flight.heading = std::atan2(std::sin(angle) * 3, std::cos(angle) * 5);
    } else {
        flight.departMs -= std::min(flight.departMs, dt);
        // Bounded substeps preserve rounded turns even at the offscreen cadence.
        for (uint32_t remaining = dt; remaining;) {
            const uint32_t stepMs = std::min<uint32_t>(remaining, 40);
            remaining -= stepMs;
            float tx, ty;
            if (pose.activity == Activity::ARRIVE) {
                tx = center(flight.portal, false); ty = center(flight.portal, true);
            } else if (flight.edgeLeg) outside(flight.portal, flight.edge, tx, ty);
            else {
                const int cell = flight.route[flight.routeIndex];
                tx = center(cell, false); ty = center(cell, true);
            }
            const float dx = tx - pose.x, dy = ty - pose.y;
            if (dx * dx + dy * dy < 11 * 11) {
                if (pose.activity == Activity::ARRIVE) {
                    pose.activity = Activity::ROAM;
                    flight.departMs = 25000 + roll(actor.random) % 20001;
                    if (!planFlight(index, false)) { cruise(index); break; }
                } else if (flight.edgeLeg) {
                    pose.absent = true;
                    start(actor, Activity::AWAY, 12000 + roll(actor.random) % 13001);
                    break;
                } else if (++flight.routeIndex == flight.routeCount) {
                    if (pose.activity == Activity::DEPART) flight.edgeLeg = true;
                    else if (!flight.departMs && planFlight(index, true)) pose.activity = Activity::DEPART;
                    else {
                        if (!flight.departMs) flight.departMs = 15000;
                        if (!planFlight(index, false)) { cruise(index); break; }
                    }
                }
                continue;
            }
            const float turn = 3.2f * stepMs / 1000.0f;
            const float wanted = std::atan2(dy, dx);
            const float delta = std::clamp(angleDelta(wanted, flight.heading), -turn, turn);
            const float speed = pose.speciesId == 279 ? 17.0f : 21.0f;
            const float length = speed * stepMs / 1000.0f;
            bool moved = false;
            // Try nearby headings within the turn limit before any sharp escape.
            for (float adjustment : {delta, turn, -turn, 0.0f}) {
                const float heading = flight.heading + adjustment;
                const float x = pose.x + std::cos(heading) * length;
                const float y = pose.y + std::sin(heading) * length;
                if (!flightPoint(actor, x, y)) continue;
                pose.x = x; pose.y = y; flight.heading = heading; moved = true; break;
            }
            if (!moved) {
                // A narrow dead end can require a tight bank; never cross it.
                for (int d = 0; d < 4; ++d) {
                    const float heading = wanted + d * PI * 0.5f;
                    const float x = pose.x + std::cos(heading) * length;
                    const float y = pose.y + std::sin(heading) * length;
                    if (!flightPoint(actor, x, y)) continue;
                    pose.x = x; pose.y = y; flight.heading = heading; break;
                }
            }
        }
    }
    const float vx = std::cos(flight.heading), vy = std::sin(flight.heading);
    flight.heading = std::fmod(flight.heading, 2 * PI);
    // Hysteresis avoids flickering between two directional sprites at diagonals.
    const float current = pose.direction % 2 ? std::fabs(vx) : std::fabs(vy);
    if (current < std::max(std::fabs(vx), std::fabs(vy)) - 0.15f ||
        (pose.direction == 0 && vy < 0) || (pose.direction == 2 && vy > 0) ||
        (pose.direction == 1 && vx > 0) || (pose.direction == 3 && vx < 0))
        pose.direction = std::fabs(vx) > std::fabs(vy) ? (vx < 0 ? 1 : 3) : (vy < 0 ? 2 : 0);
    pose.surface = flightSurfaceAt(int(std::floor(pose.x / TILE)), int(std::floor(pose.y / TILE)));
    pose.cover = Cover::NONE;
}

}  // namespace ExploreEcology
