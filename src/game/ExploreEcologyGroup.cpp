#include "game/ExploreEcology.h"

#include <algorithm>
#include <cmath>

#include "game/ExploreRouteGeometry.h"

namespace ExploreEcology {
namespace {
constexpr float TILE = ExploreRouteGeometry::TILE_SIZE;
constexpr int WIDTH = ExploreMapGenerator::WIDTH;
uint32_t next(uint32_t& state) {
    state ^= state << 13; state ^= state >> 17; state ^= state << 5;
    return state;
}
float distanceSquared(float x, float y, float bx, float by) {
    return (x - bx) * (x - bx) + (y - by) * (y - by);
}
uint8_t facing(float dx, float dy) {
    if (std::fabs(dx) > std::fabs(dy)) return dx < 0 ? 1 : 3;
    return dy < 0 ? 2 : 0;
}
float cellX(uint8_t cell) { return ExploreRouteGeometry::tileCenter(cell % WIDTH); }
float cellY(uint8_t cell) { return ExploreRouteGeometry::tileCenter(cell / WIDTH); }
}

bool Simulation::spawnGroup(const ExploreMapGenerator::Path& path,
                             const ExplorePool::Pool& pool, uint8_t hiddenPoolMask,
                             uint32_t& random) {
    for (const auto& pair : GROUP_PAIRS) {
        uint8_t mask = 0;
        bool hidden = false;
        for (uint8_t i = 0; i < std::min<uint8_t>(pool.count, ExplorePool::POOL_CAP); ++i) {
            if (pool.entries[i].speciesId != pair[0] && pool.entries[i].speciesId != pair[1]) continue;
            hidden |= (hiddenPoolMask & (1U << i)) != 0;
            if (pool.entries[i].speciesId == pair[0]) mask |= 1;
            if (pool.entries[i].speciesId == pair[1]) mask |= 2;
        }
        // Either visible family member admits its whitelisted companion.
        // An explicitly hidden member must never be revealed by a group.
        if (!mask || hidden) continue;
        if (next(random) % 100 >= GROUP_CHANCE_PERCENT) return false;
        group_.random = next(random);
        if (!group_.random) group_.random = 1;
        actors_[0] = {}; actors_[1] = {};
        actors_[0].pose.speciesId = pair[0];
        actors_[1].pose.speciesId = pair[1];
        if (!planGroupRoute(&path)) return false;
        count_ = 2;
        group_.active = true;
        group_.cycleDurationMs = 15000 + next(group_.random) % 10001;
        group_.nextGlanceMs = 5000 + next(group_.random) % 3001;
        for (uint8_t i = 0; i < 2; ++i) {
            auto& actor = actors_[i];
            groupPoint(i == 0 ? group_.leader : group_.follower, actor.pose.x, actor.pose.y);
            actor.homeX = actor.fromX = actor.toX = actor.pose.x;
            actor.homeY = actor.fromY = actor.toY = actor.pose.y;
            actor.pose.surface = surfaceAt(int(actor.pose.x / TILE), int(actor.pose.y / TILE));
            actor.pose.cover = coverAt(actor.pose.x, actor.pose.y);
            actor.pose.direction = facing(cellX(group_.route[(2 - i) % group_.routeCount]) - actor.pose.x,
                cellY(group_.route[(2 - i) % group_.routeCount]) - actor.pose.y);
        }
        return true;
    }
    return false;
}

bool Simulation::planGroupRoute(const ExploreMapGenerator::Path* path) {
    constexpr int CELLS = ExploreMapGenerator::CELL_COUNT;
    constexpr int DX[] = {0, -1, 0, 1}, DY[] = {1, 0, -1, 0};
    uint8_t queue[CELLS];
    if (path) {
        // Cache shared edges once. Prune dead-end branches so the adult never
        // needs to reverse into the young in a narrow corridor.
        for (int cell = 0; cell < CELLS; ++cell) {
            for (uint8_t direction = 0; direction < 4; ++direction) {
                const int x = cell % WIDTH + DX[direction], y = cell / WIDTH + DY[direction];
                if (x <= 0 || x >= WIDTH - 1 || y <= 0 || y >= ExploreMapGenerator::HEIGHT - 1) continue;
                const uint8_t to = y * WIDTH + x;
                bool safe = true;
                for (uint8_t i = 0; i < 2; ++i) {
                    const auto species = actors_[i].pose.speciesId;
                    safe &= canStand(species, cellX(cell), cellY(cell)) &&
                        safeLeg(species, cellX(cell), cellY(cell), cellX(to), cellY(to));
                }
                if (safe) group_.links[cell] |= 1U << direction;
            }
        }
        auto degree = [&](int cell) {
            const uint8_t bits = group_.links[cell];
            return ((bits >> 0) & 1) + ((bits >> 1) & 1) + ((bits >> 2) & 1) + ((bits >> 3) & 1);
        };
        bool queued[CELLS]{};
        int head = 0, tail = 0;
        for (int cell = 0; cell < CELLS; ++cell) {
            if (degree(cell) < 2) { queue[tail++] = cell; queued[cell] = true; }
        }
        while (head < tail) {
            const uint8_t cell = queue[head++], links = group_.links[cell];
            group_.links[cell] = 0;
            for (uint8_t direction = 0; direction < 4; ++direction) {
                if (!(links & (1U << direction))) continue;
                const int to = cell + DY[direction] * WIDTH + DX[direction];
                group_.links[to] &= ~(1U << ((direction + 2) % 4));
                if (!queued[to] && degree(to) < 2) {
                    queue[tail++] = to; queued[to] = true;
                }
            }
        }
        // Tiny pockets cannot provide a walk to another place. Leave those
        // to independent actors rather than making a pair circle four cells.
        bool seen[CELLS]{};
        for (int cell = 0; cell < CELLS; ++cell) {
            if (seen[cell] || !group_.links[cell]) continue;
            head = 0; tail = 1; queue[0] = cell; seen[cell] = true;
            while (head < tail) {
                const int from = queue[head++];
                for (uint8_t direction = 0; direction < 4; ++direction) {
                    if (!(group_.links[from] & (1U << direction))) continue;
                    const int to = from + DY[direction] * WIDTH + DX[direction];
                    if (!seen[to]) { queue[tail++] = to; seen[to] = true; }
                }
            }
            if (tail < 8) for (int i = 0; i < tail; ++i) group_.links[queue[i]] = 0;
        }
        int best = -1;
        uint32_t bestScore = 0;
        for (int cell = 0; cell < CELLS; ++cell) {
            if (!group_.links[cell]) continue;
            float nearest = 1e9f;
            for (uint8_t point = 1; point + 1 < path->pointCount; ++point) {
                const auto route = ExploreRouteGeometry::pathPoint(*path, point);
                nearest = std::min(nearest, distanceSquared(cellX(cell), cellY(cell), route.x, route.y));
            }
            if (nearest < TILE * TILE * 0.8f || nearest > TILE * TILE * 6.25f) continue;
            uint32_t score = next(group_.random) % 1000;
            if (best < 0 || score > bestScore) { best = cell; bestScore = score; }
        }
        if (best < 0) return false;
        const uint8_t first = next(group_.random) % 4;
        for (uint8_t offset = 0; offset < 4; ++offset) {
            const uint8_t direction = (first + offset) % 4;
            if (!(group_.links[best] & (1U << direction))) continue;
            group_.route[0] = best + DY[direction] * WIDTH + DX[direction];
            group_.route[1] = best;
            group_.routeCount = 2;
            break;
        }
    }
    const uint8_t follower = group_.route[int(std::lround(group_.follower / TILE))];
    const uint8_t leader = group_.route[int(std::lround(group_.leader / TILE))];
    uint8_t parents[CELLS], distances[CELLS];
    std::fill_n(distances, CELLS, 255);
    int head = 0, tail = 1;
    queue[0] = leader; distances[leader] = 0;
    while (head < tail) {
        const uint8_t cell = queue[head++];
        if (distances[cell] >= 7) continue;
        const uint8_t first = next(group_.random) % 4;
        for (uint8_t offset = 0; offset < 4; ++offset) {
            const uint8_t direction = (first + offset) % 4;
            if (!(group_.links[cell] & (1U << direction))) continue;
            const int to = cell + DY[direction] * WIDTH + DX[direction];
            if (to == follower || distances[to] != 255) continue;
            distances[to] = distances[cell] + 1;
            parents[to] = cell; queue[tail++] = to;
        }
    }
    int goal = -1;
    float bestScore = -1e9f;
    for (int i = 1; i < tail; ++i) {
        const uint8_t cell = queue[i], distance = distances[cell];
        float score = next(group_.random) % 1000 + distance * 100;
        if (distance >= 4) score += 100000;
        // Avoid repeatedly picking the same observation spot or its neighbours.
        for (uint8_t recent : group_.recent) {
            if (recent == 255) continue;
            const int gap = std::abs(cell % WIDTH - recent % WIDTH) +
                std::abs(cell / WIDTH - recent / WIDTH);
            if (gap < 4) score -= (4 - gap) * 450;
        }
        if (goal < 0 || score > bestScore) { goal = cell; bestScore = score; }
    }
    if (goal < 0) return false;
    uint8_t reverse[7], length = 0;
    for (int cell = goal; cell != leader; cell = parents[cell]) reverse[length++] = cell;
    group_.route[0] = follower; group_.route[1] = leader;
    for (uint8_t i = 0; i < length; ++i) group_.route[i + 2] = reverse[length - i - 1];
    group_.routeCount = length + 2;
    group_.recent[group_.recentIndex++ % 8] = leader;
    group_.leader = TILE; group_.follower = 0;
    return true;
}

void Simulation::groupPoint(float travel, float& x, float& y) const {
    const float position = std::clamp(travel / TILE, 0.0f, float(group_.routeCount - 1));
    const int index = std::min(int(position), int(group_.routeCount) - 2);
    const float fraction = position - index;
    const uint8_t a = group_.route[index], b = group_.route[index + 1];
    x = cellX(a) + (cellX(b) - cellX(a)) * fraction;
    y = cellY(a) + (cellY(b) - cellY(a)) * fraction;
}

void Simulation::advanceGroup(uint32_t dt, float playerX, float playerY, bool visible) {
    auto& group = group_;
    group.clockMs += dt;
    group.attentionCooldownMs -= std::min(dt, group.attentionCooldownMs);
    const auto phaseBefore = group.phase;
    const float routeEnd = (group.routeCount - 1) * TILE;
    float leader = group.leader, follower = group.follower;
    const bool nearPlayer = visible && distanceSquared(actors_[0].pose.x, actors_[0].pose.y,
        playerX, playerY) < TILE * TILE * 4;
    if (group.phase == Group::Phase::PATROL) {
        group.cycleMs += dt;
        if (nearPlayer && !group.attentionCooldownMs) {
            group.phase = Group::Phase::WATCH;
            group.waitMs = 1600;
            group.attentionCooldownMs = 15000;
        } else if (group.cycleMs >= group.cycleDurationMs || group.leader >= routeEnd - TILE * 0.2f) {
            group.phase = Group::Phase::GATHER;
            group.stop = std::min(routeEnd, std::ceil(group.leader / TILE + 0.2f) * TILE);
        }
    }
    if (group.phase == Group::Phase::PATROL) {
        const bool starting = group.waitMs != 0;
        group.waitMs -= std::min(dt, group.waitMs);
        if (!starting) {
            if (group.clockMs >= group.nextGlanceMs && !group.glanceMs) {
                group.glanceMs = 800 + next(group.random) % 501;
                group.nextGlanceMs = group.clockMs + 5000 + next(group.random) % 4001;
            }
            if (group.leader - group.follower < TILE * 1.55f)
                group.leader = std::min(routeEnd, group.leader + TILE * dt / 1800.0f);
            if (group.glanceMs) group.glanceMs -= std::min(dt, group.glanceMs);
            else if (group.cycleMs >= 1200)
                group.follower = std::max(group.follower, std::min(group.leader - TILE * 1.15f,
                    group.follower + TILE * dt / 1450.0f));
        }
    } else if (group.phase == Group::Phase::GATHER) {
        group.leader = std::min(group.stop, group.leader + TILE * dt / 1800.0f);
        group.follower = std::max(group.follower, std::min(group.leader - TILE,
            group.follower + TILE * dt / 1450.0f));
        if (group.leader == group.stop && group.follower == group.leader - TILE) {
            group.phase = Group::Phase::REUNION;
            group.waitMs = 1000;
        }
    } else if (group.phase == Group::Phase::WATCH) {
        // The young one safely closes along the adult's actual trail.
        group.follower = std::min(group.leader - TILE * 0.85f,
            group.follower + TILE * dt / 1450.0f);
        group.waitMs -= std::min(dt, group.waitMs);
        if (!group.waitMs) group.phase = Group::Phase::PATROL;
    } else {
        group.waitMs -= std::min(dt, group.waitMs);
        if (!group.waitMs) {
            // Keep both anchors and choose a new destination ahead. No wrap
            // to the start of an old loop, reversal into the young, or teleport.
            planGroupRoute();
            leader = group.leader; follower = group.follower;
            group.phase = Group::Phase::PATROL;
            group.cycleMs = 0;
            group.cycleDurationMs = 15000 + next(group.random) % 10001;
            group.waitMs = 500;
            group.glanceMs = 0;
        }
    }
    for (uint8_t i = 0; i < 2; ++i) {
        auto& pose = actors_[i].pose;
        const float oldX = pose.x, oldY = pose.y;
        groupPoint(i == 0 ? group.leader : group.follower, pose.x, pose.y);
        pose.walking = (i == 0 ? group.leader != leader : group.follower != follower);
        pose.activity = pose.walking ? Activity::WALK : Activity::REST;
        pose.frame = (group.clockMs / (pose.walking ? 150 : 600)) % 6;
        if (pose.walking) pose.direction = facing(pose.x - oldX, pose.y - oldY);
        else if (group.phase == Group::Phase::WATCH && i == 0)
            pose.direction = facing(playerX - pose.x, playerY - pose.y);
        else if (group.phase == Group::Phase::REUNION || (i == 0 && !group.waitMs)) {
            float otherX, otherY;
            groupPoint(i == 0 ? group.follower : group.leader, otherX, otherY);
            pose.direction = facing(otherX - pose.x, otherY - pose.y);
        } else if (phaseBefore != group.phase || group.clockMs / 600 != (group.clockMs - dt) / 600)
            pose.direction = (pose.direction + 1) % 4;
        pose.cover = coverAt(pose.x, pose.y);
        pose.surface = surfaceAt(int(pose.x / TILE), int(pose.y / TILE));
        actors_[i].toX = pose.x; actors_[i].toY = pose.y;
    }
}
}  // namespace ExploreEcology
