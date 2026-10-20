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
    uint8_t candidates[sizeof(GROUP_FAMILIES) / sizeof(GROUP_FAMILIES[0])]{};
    uint8_t candidateCount = 0;
    for (uint8_t family = 0; family < sizeof(GROUP_FAMILIES) / sizeof(GROUP_FAMILIES[0]); ++family) {
        const auto& candidate = GROUP_FAMILIES[family];
        uint8_t mask = 0;
        bool hidden = false;
        for (uint8_t i = 0; i < std::min<uint8_t>(pool.count, ExplorePool::POOL_CAP); ++i) {
            bool member = false;
            for (uint8_t j = 0; j < candidate.count; ++j)
                member |= pool.entries[i].speciesId == candidate.species[j];
            if (!member) continue;
            hidden |= (hiddenPoolMask & (1U << i)) != 0;
            mask = 1;
        }
        // Any visible family member admits its whitelisted companions.
        // An explicitly hidden member must never be revealed by a group.
        if (!mask || hidden) continue;
        candidates[candidateCount++] = family;
    }
    // One roll per map, then sample eligible families without replacement.
    // More matching families must not increase density or the group chance.
    if (!candidateCount || next(random) % 100 >= GROUP_CHANCE_PERCENT) return false;
    while (candidateCount) {
        const uint8_t chosen = candidateCount == 1 ? 0 : next(random) % candidateCount;
        const auto& family = GROUP_FAMILIES[candidates[chosen]];
        candidates[chosen] = candidates[--candidateCount];
        uint8_t selected = (1U << family.count) - 1;
        if (family.count == 3) {
            // Exactly 10% for each single, 15% for each pair, 25% for all.
            const uint8_t roll = next(random) % 100;
            constexpr uint8_t pairs[] = {3, 5, 6};
            selected = roll < 30 ? 1U << (roll / 10) :
                roll < 75 ? pairs[(roll - 30) / 15] : 7;
        }
        auto members = [](uint8_t mask) {
            return uint8_t((mask & 1) + ((mask >> 1) & 1) + ((mask >> 2) & 1));
        };
        // Try the selected composition first. Unsuitable ground may use a
        // different composition of that size, then a smaller family.
        for (uint8_t size = members(selected); size >= (family.count == 3 ? 1 : 2); --size) {
            uint8_t remaining[7]{}, remainingCount = 0;
            for (uint8_t mask = 1; mask < (1U << family.count); ++mask)
                if (members(mask) == size && mask != selected) remaining[remainingCount++] = mask;
            uint8_t mask = size == members(selected) ? selected : 0;
            while (mask || remainingCount) {
                if (!mask) {
                    const uint8_t pick = remainingCount == 1 ? 0 : next(random) % remainingCount;
                    mask = remaining[pick];
                    remaining[pick] = remaining[--remainingCount];
                }
                group_ = {};
                group_.random = next(random);
                if (!group_.random) group_.random = 1;
                for (uint8_t i = 0; i < family.count; ++i) {
                    if (!(mask & (1U << i))) continue;
                    auto& actor = actors_[group_.members++];
                    actor = {};
                    actor.pose.speciesId = family.species[i];
                }
                mask = 0;
                if (size == 1) {
                    const auto species = actors_[0].pose.speciesId;
                    if (!spawnFamilySingle(path, species, group_.random)) continue;
                    return true;
                }
                if (!planGroupRoute(&path)) continue;
                count_ = group_.members;
                group_.active = true;
                group_.cycleDurationMs = 15000 + next(group_.random) % 10001;
                group_.nextGlanceMs = 5000 + next(group_.random) % 3001;
                for (uint8_t i = 0; i < group_.members; ++i) {
                    auto& actor = actors_[i];
                    groupPoint(group_.travel[i], actor.pose.x, actor.pose.y);
                    actor.homeX = actor.fromX = actor.toX = actor.pose.x;
                    actor.homeY = actor.fromY = actor.toY = actor.pose.y;
                    actor.pose.surface = surfaceAt(int(actor.pose.x / TILE), int(actor.pose.y / TILE));
                    actor.pose.cover = coverAt(actor.pose.x, actor.pose.y);
                    actor.pose.direction = facing(cellX(group_.route[group_.members - i]) - actor.pose.x,
                        cellY(group_.route[group_.members - i]) - actor.pose.y);
                }
                return true;
            }
        }
    }
    group_ = {};
    return false;
}

bool Simulation::spawnFamilySingle(const ExploreMapGenerator::Path& path, uint16_t species,
                                    uint32_t& random) {
    int best = -1;
    uint32_t bestScore = 0;
    for (int cell = 0; cell < ExploreMapGenerator::CELL_COUNT; ++cell) {
        const int x = cell % WIDTH, y = cell / WIDTH;
        if (x <= 0 || x >= WIDTH - 1 || y <= 0 || y >= ExploreMapGenerator::HEIGHT - 1 ||
            !canStand(species, cellX(cell), cellY(cell))) continue;
        float nearest = 1e9f;
        for (uint8_t point = 1; point + 1 < path.pointCount; ++point) {
            const auto route = ExploreRouteGeometry::pathPoint(path, point);
            nearest = std::min(nearest, distanceSquared(cellX(cell), cellY(cell), route.x, route.y));
        }
        if (nearest < TILE * TILE * 0.8f || nearest > TILE * TILE * 6.25f) continue;
        uint32_t score = next(random) % 100;
        constexpr int DX[] = {0, -1, 0, 1}, DY[] = {1, 0, -1, 0};
        for (uint8_t direction = 0; direction < 4; ++direction)
            if (safeLeg(species, cellX(cell), cellY(cell),
                cellX(cell) + DX[direction] * TILE, cellY(cell) + DY[direction] * TILE)) score += 100;
        if (best < 0 || score > bestScore) { best = cell; bestScore = score; }
    }
    if (best < 0) return false;
    appendActor(species, best, false, random);
    return true;
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
                for (uint8_t i = 0; i < group_.members; ++i) {
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
            const uint8_t behind = best + DY[direction] * WIDTH + DX[direction];
            group_.route[group_.members - 1] = best;
            group_.route[group_.members - 2] = behind;
            if (group_.members == 3) {
                for (uint8_t back = 0; back < 4; ++back) {
                    const int tail = behind + DY[back] * WIDTH + DX[back];
                    if ((group_.links[behind] & (1U << back)) && tail != best) {
                        group_.route[0] = tail;
                        break;
                    }
                }
            }
            group_.routeCount = group_.members;
            for (uint8_t i = 0; i < group_.members; ++i)
                group_.travel[i] = (group_.members - 1 - i) * TILE;
            break;
        }
    }
    uint8_t anchors[CAPACITY];
    for (uint8_t i = 0; i < group_.members; ++i)
        anchors[i] = group_.route[int(std::lround(group_.travel[group_.members - 1 - i] / TILE))];
    const uint8_t leader = anchors[group_.members - 1];
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
            bool occupied = false;
            for (uint8_t i = 0; i + 1 < group_.members; ++i) occupied |= to == anchors[i];
            if (occupied || distances[to] != 255) continue;
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
    std::copy_n(anchors, group_.members, group_.route);
    for (uint8_t i = 0; i < length; ++i) group_.route[i + group_.members] = reverse[length - i - 1];
    group_.routeCount = length + group_.members;
    group_.recent[group_.recentIndex++ % 8] = leader;
    for (uint8_t i = 0; i < group_.members; ++i)
        group_.travel[i] = (group_.members - 1 - i) * TILE;
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
    float before[CAPACITY];
    std::copy_n(group.travel, group.members, before);
    auto& leader = group.travel[0];
    const bool nearPlayer = visible && distanceSquared(actors_[0].pose.x, actors_[0].pose.y,
        playerX, playerY) < TILE * TILE * 4;
    if (group.phase == Group::Phase::PATROL) {
        group.cycleMs += dt;
        if (nearPlayer && !group.attentionCooldownMs) {
            group.phase = Group::Phase::WATCH;
            group.waitMs = 1600;
            group.attentionCooldownMs = 15000;
        } else if (group.cycleMs >= group.cycleDurationMs || leader >= routeEnd - TILE * 0.2f) {
            group.phase = Group::Phase::GATHER;
            group.stop = std::min(routeEnd, std::ceil(leader / TILE + 0.2f) * TILE);
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
            if (leader - group.travel[1] < TILE * 1.55f)
                leader = std::min(routeEnd, leader + TILE * dt / 1800.0f);
            const bool glancing = group.glanceMs != 0;
            if (glancing) group.glanceMs -= std::min(dt, group.glanceMs);
            for (uint8_t i = 1; i < group.members; ++i) {
                if ((glancing && i + 1 == group.members) || group.cycleMs < 1200U * i) continue;
                if (i + 1 < group.members && group.travel[i] - group.travel[i + 1] >= TILE * 1.55f) continue;
                group.travel[i] = std::max(group.travel[i], std::min(group.travel[i - 1] - TILE * 1.15f,
                    group.travel[i] + TILE * dt / 1450.0f));
            }
        }
    } else if (group.phase == Group::Phase::GATHER) {
        leader = std::min(group.stop, leader + TILE * dt / 1800.0f);
        bool gathered = leader == group.stop;
        for (uint8_t i = 1; i < group.members; ++i) {
            group.travel[i] = std::max(group.travel[i], std::min(group.travel[i - 1] - TILE,
                group.travel[i] + TILE * dt / 1450.0f));
            gathered &= group.travel[i] == leader - TILE * i;
        }
        if (gathered) {
            group.phase = Group::Phase::REUNION;
            group.waitMs = 1000;
        }
    } else if (group.phase == Group::Phase::WATCH) {
        // Each follower closes along the preceding member's actual trail.
        for (uint8_t i = 1; i < group.members; ++i)
            group.travel[i] = std::max(group.travel[i], std::min(group.travel[i - 1] - TILE,
                group.travel[i] + TILE * dt / 1450.0f));
        group.waitMs -= std::min(dt, group.waitMs);
        if (!group.waitMs) group.phase = Group::Phase::PATROL;
    } else {
        group.waitMs -= std::min(dt, group.waitMs);
        if (!group.waitMs) {
            // Keep all anchors and choose a new destination ahead. No wrap
            // to the start of an old loop, reversal into the young, or teleport.
            planGroupRoute();
            std::copy_n(group.travel, group.members, before);
            group.phase = Group::Phase::PATROL;
            group.cycleMs = 0;
            group.cycleDurationMs = 15000 + next(group.random) % 10001;
            group.waitMs = 500;
            group.glanceMs = 0;
        }
    }
    for (uint8_t i = 0; i < group_.members; ++i) {
        auto& pose = actors_[i].pose;
        const float oldX = pose.x, oldY = pose.y;
        groupPoint(group.travel[i], pose.x, pose.y);
        pose.walking = group.travel[i] != before[i];
        pose.activity = pose.walking ? Activity::WALK : Activity::REST;
        pose.frame = (group.clockMs / (pose.walking ? 150 : 600)) % 6;
        if (pose.walking) pose.direction = facing(pose.x - oldX, pose.y - oldY);
        else if (group.phase == Group::Phase::WATCH && i == 0)
            pose.direction = facing(playerX - pose.x, playerY - pose.y);
        else if (group.phase == Group::Phase::REUNION || (i == 0 && !group.waitMs)) {
            float otherX, otherY;
            groupPoint(group.travel[i == 0 ? 1 : i - 1], otherX, otherY);
            pose.direction = facing(otherX - pose.x, otherY - pose.y);
        } else if (phaseBefore != group.phase || group.clockMs / 600 != (group.clockMs - dt) / 600)
            pose.direction = (pose.direction + 1) % 4;
        pose.cover = coverAt(pose.x, pose.y);
        pose.surface = surfaceAt(int(pose.x / TILE), int(pose.y / TILE));
        actors_[i].toX = pose.x; actors_[i].toY = pose.y;
    }
}
}  // namespace ExploreEcology
