#include "game/ExploreMapGenerator.h"

#include <cstring>

namespace ExploreMapGenerator {
namespace {

constexpr uint16_t GRASS_TILES[] = {385, 385, 385, 386, 387, 388, 389};
constexpr uint16_t DENSE_GRASS_TILE = 390;
constexpr uint16_t FLOWER_TILE = 415;
constexpr uint16_t DEEP_SEA_TILE = 144;
constexpr uint16_t DEEP_SEA_EDGE_TILE = 168;
constexpr uint16_t SEA_SHORE_TILE = 72;

constexpr uint16_t FENCE_TOP_LEFT = 1662;
constexpr uint16_t FENCE_LEFT = 1665;
constexpr uint16_t FENCE_TOP = 1681;
constexpr uint16_t FENCE_TOP_RIGHT = 1682;

class Rng {
public:
    explicit Rng(uint32_t seed) : state_(seed ? seed : 0x6D2B79F5U) {}

    uint32_t next() {
        uint32_t value = state_;
        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;
        state_ = value;
        return value;
    }

    uint32_t bounded(uint32_t bound) {
        return bound == 0 ? 0 : next() % bound;
    }

private:
    uint32_t state_;
};

class CellMask {
public:
    void clear() { std::memset(words_, 0, sizeof(words_)); }

    void add(int x, int y) {
        if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
        uint16_t index = static_cast<uint16_t>(y * WIDTH + x);
        words_[index >> 5] |= 1UL << (index & 31);
    }

    bool contains(int x, int y) const {
        if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return false;
        uint16_t index = static_cast<uint16_t>(y * WIDTH + x);
        return (words_[index >> 5] & (1UL << (index & 31))) != 0;
    }

    bool intersectsRect(int left, int top, int right, int bottom) const {
        for (int y = top; y <= bottom; ++y) {
            for (int x = left; x <= right; ++x) {
                if (contains(x, y)) return true;
            }
        }
        return false;
    }

private:
    uint32_t words_[(CELL_COUNT + 31) / 32] = {};
};

struct ForestSpec {
    uint8_t forestX;
    uint8_t topY;
    uint8_t width;
    uint8_t bottomY;
};

constexpr ForestSpec FOREST_SPECS[] = {
    {10, 5, 6, 11},
    {12, 1, 4, 7},
    {1, 5, 6, 11},
    {1, 0, 6, 6},
};

bool samePoint(const Point& first, const Point& second) {
    return first.x == second.x && first.y == second.y;
}

bool appendPoint(Path& path, uint8_t x, uint8_t y) {
    Point point{x, y};
    if (path.pointCount > 0 && samePoint(path.points[path.pointCount - 1], point)) return true;
    if (path.pointCount >= MAX_PATH_POINTS) return false;
    path.points[path.pointCount++] = point;
    return true;
}

bool appendLine(Path& path, uint8_t targetX, uint8_t targetY) {
    if (path.pointCount == 0) return appendPoint(path, targetX, targetY);
    Point point = path.points[path.pointCount - 1];
    while (point.x != targetX || point.y != targetY) {
        if (point.x < targetX) ++point.x;
        else if (point.x > targetX) --point.x;
        else if (point.y < targetY) ++point.y;
        else --point.y;
        if (!appendPoint(path, point.x, point.y)) return false;
    }
    return true;
}

Endpoint endpointForEdge(Edge edge, uint8_t junctionX, uint8_t junctionY,
                         uint8_t edgeCoordinate) {
    switch (edge) {
    case Edge::TOP: return {{edgeCoordinate, 0}, edge};
    case Edge::RIGHT: return {{WIDTH - 1, edgeCoordinate}, edge};
    case Edge::BOTTOM: return {{edgeCoordinate, HEIGHT - 1}, edge};
    case Edge::LEFT: return {{0, edgeCoordinate}, edge};
    }
    return {{junctionX, junctionY}, Edge::TOP};
}

bool buildPath(const Path& trunk, uint8_t junctionX, uint8_t junctionY,
               const Endpoint& exit, Path& out) {
    out = trunk;
    if (exit.edge == Edge::TOP || exit.edge == Edge::BOTTOM) {
        if (!appendLine(out, exit.point.x, junctionY) ||
            !appendLine(out, exit.point.x, exit.point.y)) return false;
    } else {
        if (!appendLine(out, junctionX, exit.point.y) ||
            !appendLine(out, exit.point.x, exit.point.y)) return false;
    }
    out.exit = exit;
    return true;
}

void addRoadPair(CellMask& road, const Point& point, bool vertical) {
    road.add(point.x, point.y);
    if (vertical) road.add(point.x + 1, point.y);
    else road.add(point.x, point.y + 1);
}

void addPathRoad(const Path& path, CellMask& road) {
    for (uint8_t i = 1; i < path.pointCount; ++i) {
        const Point& first = path.points[i - 1];
        const Point& second = path.points[i];
        bool vertical = first.x == second.x;
        addRoadPair(road, first, vertical);
        addRoadPair(road, second, vertical);
    }
}

bool endpointConnects(const Endpoint& endpoint, int x, int y) {
    switch (endpoint.edge) {
    case Edge::TOP:
        return y == -1 && (x == endpoint.point.x || x == endpoint.point.x + 1);
    case Edge::RIGHT:
        return x == WIDTH && (y == endpoint.point.y || y == endpoint.point.y + 1);
    case Edge::BOTTOM:
        return y == HEIGHT && (x == endpoint.point.x || x == endpoint.point.x + 1);
    case Edge::LEFT:
        return x == -1 && (y == endpoint.point.y || y == endpoint.point.y + 1);
    }
    return false;
}

bool connectedRoad(const Map& map, const CellMask& road, int x, int y) {
    if (road.contains(x, y) || endpointConnects(map.entry, x, y)) return true;
    for (uint8_t i = 0; i < map.pathCount; ++i) {
        if (endpointConnects(map.paths[i].exit, x, y)) return true;
    }
    return false;
}

uint16_t roadTile(const Map& map, const CellMask& road, int x, int y) {
    bool north = connectedRoad(map, road, x, y - 1);
    bool east = connectedRoad(map, road, x + 1, y);
    bool south = connectedRoad(map, road, x, y + 1);
    bool west = connectedRoad(map, road, x - 1, y);

    if (north && east && south && west) {
        if (!connectedRoad(map, road, x - 1, y - 1)) return 558;
        if (!connectedRoad(map, road, x + 1, y - 1)) return 556;
        if (!connectedRoad(map, road, x - 1, y + 1)) return 542;
        if (!connectedRoad(map, road, x + 1, y + 1)) return 540;
        return 546;
    }
    if (!north && !west) return 537;
    if (!north && !east) return 539;
    if (!south && !west) return 553;
    if (!south && !east) return 555;
    if (!north) return 538;
    if (!south) return 554;
    if (!west) return 545;
    if (!east) return 547;
    return 546;
}

void stampCoast(Map& map, CellMask& water) {
    for (uint8_t y = 0; y < HEIGHT; ++y) {
        for (uint8_t x = 0; x < 3; ++x) {
            map.layers[0][y * WIDTH + x] = DEEP_SEA_TILE;
            water.add(x, y);
        }
        map.layers[0][y * WIDTH + 3] = DEEP_SEA_EDGE_TILE;
        map.layers[0][y * WIDTH + 4] = SEA_SHORE_TILE;
        water.add(3, y);
        water.add(4, y);
    }
}

bool forestFits(const ForestSpec& spec, const CellMask& road, const CellMask& water) {
    int left = spec.forestX - 1;
    int right = spec.forestX + spec.width - 1;
    return !road.intersectsRect(left, spec.topY, right, spec.bottomY) &&
           !water.intersectsRect(left, spec.topY, right, spec.bottomY);
}

void stampForest(Map& map, const ForestSpec& spec, CellMask& forest) {
    uint8_t forestRight = spec.forestX + spec.width - 1;
    uint8_t fenceRight = forestRight - 1;
    map.layers[1][spec.topY * WIDTH + spec.forestX - 1] = FENCE_TOP_LEFT;
    for (uint8_t x = spec.forestX; x < fenceRight; ++x) {
        map.layers[1][spec.topY * WIDTH + x] = FENCE_TOP;
    }
    map.layers[1][spec.topY * WIDTH + fenceRight] = FENCE_TOP_RIGHT;
    for (uint8_t y = spec.topY + 1; y <= spec.bottomY; ++y) {
        map.layers[1][y * WIDTH + spec.forestX - 1] = FENCE_LEFT;
    }

    for (uint8_t offset = 0; offset < spec.width; ++offset) {
        uint8_t x = spec.forestX + offset;
        map.layers[2][spec.topY * WIDTH + x] = offset % 2 == 0 ? 804 : 805;
        map.layers[0][(spec.topY + 1) * WIDTH + x] = offset % 2 == 0 ? 810 : 811;
        for (uint8_t y = spec.topY + 2; y < spec.bottomY; ++y) {
            uint8_t rowIndex = y - (spec.topY + 2);
            if (rowIndex % 2 == 0) {
                map.layers[0][y * WIDTH + x] =
                    offset == 0 ? 802 : (offset % 2 == 0 ? 800 : 801);
            } else {
                map.layers[0][y * WIDTH + x] =
                    offset == 0 ? 810 : (offset % 2 == 0 ? 808 : 809);
            }
        }
        map.layers[0][spec.bottomY * WIDTH + x] = offset % 2 == 0 ? 818 : 819;
    }

    for (uint8_t y = spec.topY; y <= spec.bottomY; ++y) {
        for (uint8_t x = spec.forestX - 1; x <= forestRight; ++x) forest.add(x, y);
    }
}

uint32_t fnvByte(uint32_t hash, uint8_t value) {
    return (hash ^ value) * 16777619U;
}

uint32_t fnvWord(uint32_t hash, uint16_t value) {
    hash = fnvByte(hash, static_cast<uint8_t>(value & 0xFF));
    return fnvByte(hash, static_cast<uint8_t>(value >> 8));
}

}  // namespace

uint32_t deriveSeed(uint32_t expeditionSeed, uint8_t blockIndex, uint8_t areaIndex) {
    uint32_t value = expeditionSeed ^ (0x9E3779B9U * (static_cast<uint32_t>(blockIndex) + 1U));
    value ^= 0x85EBCA6BU * (static_cast<uint32_t>(areaIndex) + 1U);
    value ^= value >> 16;
    value *= 0x7FEB352DU;
    value ^= value >> 15;
    value *= 0x846CA68BU;
    value ^= value >> 16;
    return value ? value : 0x6D2B79F5U;
}

Edge opposite(Edge edge) {
    switch (edge) {
    case Edge::TOP: return Edge::BOTTOM;
    case Edge::RIGHT: return Edge::LEFT;
    case Edge::BOTTOM: return Edge::TOP;
    case Edge::LEFT: return Edge::RIGHT;
    }
    return Edge::TOP;
}

bool generate(uint32_t seed, Edge entryEdge, Map& out) {
    std::memset(&out, 0, sizeof(out));
    out.seed = seed;
    out.pathCount = PATH_COUNT;
    Rng topology(seed ^ 0xA511E9B3U);
    Rng terrain(seed ^ 0x63D83595U);

    out.hasCoast = entryEdge != Edge::LEFT && topology.bounded(100) < 45;
    Edge candidates[4] = {};
    uint8_t candidateCount = 0;
    for (uint8_t value = 0; value < 4; ++value) {
        Edge edge = static_cast<Edge>(value);
        if (edge == entryEdge || (out.hasCoast && edge == Edge::LEFT)) continue;
        candidates[candidateCount++] = edge;
    }
    if (candidateCount < PATH_COUNT) return false;
    for (uint8_t count = candidateCount; count > 1; --count) {
        uint8_t swapIndex = topology.bounded(count);
        Edge value = candidates[count - 1];
        candidates[count - 1] = candidates[swapIndex];
        candidates[swapIndex] = value;
    }

    uint8_t minimumX = out.hasCoast ? 7 : 3;
    uint8_t junctionX = minimumX + topology.bounded(12 - minimumX);
    uint8_t junctionY = 2 + topology.bounded(7);
    uint8_t entryCoordinate =
        (entryEdge == Edge::TOP || entryEdge == Edge::BOTTOM) ? junctionX : junctionY;
    out.entry = endpointForEdge(entryEdge, junctionX, junctionY, entryCoordinate);

    Path trunk;
    if (!appendPoint(trunk, out.entry.point.x, out.entry.point.y) ||
        !appendLine(trunk, junctionX, junctionY)) return false;

    for (uint8_t i = 0; i < PATH_COUNT; ++i) {
        Edge edge = candidates[i];
        uint8_t coordinate;
        if (edge == Edge::TOP || edge == Edge::BOTTOM) {
            uint8_t minimumExitX = out.hasCoast ? 7 : 2;
            coordinate = minimumExitX + topology.bounded(14 - minimumExitX);
        } else {
            coordinate = 1 + topology.bounded(10);
        }
        Endpoint exit = endpointForEdge(edge, junctionX, junctionY, coordinate);
        if (!buildPath(trunk, junctionX, junctionY, exit, out.paths[i])) return false;
    }

    CellMask road;
    CellMask water;
    CellMask forest;
    road.clear();
    water.clear();
    forest.clear();
    for (uint8_t i = 0; i < out.pathCount; ++i) addPathRoad(out.paths[i], road);

    for (uint8_t layer = 0; layer < LAYER_COUNT; ++layer) {
        for (uint16_t index = 0; index < CELL_COUNT; ++index) out.layers[layer][index] = 0;
    }
    for (uint16_t index = 0; index < CELL_COUNT; ++index) {
        out.layers[0][index] = GRASS_TILES[terrain.bounded(sizeof(GRASS_TILES) / sizeof(GRASS_TILES[0]))];
    }
    if (out.hasCoast) stampCoast(out, water);

    uint8_t patchCount = 3 + terrain.bounded(3);
    for (uint8_t patch = 0; patch < patchCount; ++patch) {
        uint8_t width = 2 + terrain.bounded(4);
        uint8_t height = 1 + terrain.bounded(3);
        uint8_t left = terrain.bounded(WIDTH - width + 1);
        uint8_t top = terrain.bounded(HEIGHT - height + 1);
        for (uint8_t y = top; y < top + height; ++y) {
            for (uint8_t x = left; x < left + width; ++x) {
                if (!road.contains(x, y) && !water.contains(x, y)) {
                    out.layers[0][y * WIDTH + x] = DENSE_GRASS_TILE;
                }
            }
        }
    }

    for (uint8_t y = 0; y < HEIGHT; ++y) {
        for (uint8_t x = 0; x < WIDTH; ++x) {
            if (road.contains(x, y)) out.layers[0][y * WIDTH + x] = roadTile(out, road, x, y);
        }
    }

    if (terrain.bounded(100) < 70) {
        uint8_t order[4] = {0, 1, 2, 3};
        for (uint8_t count = 4; count > 1; --count) {
            uint8_t swapIndex = terrain.bounded(count);
            uint8_t value = order[count - 1];
            order[count - 1] = order[swapIndex];
            order[swapIndex] = value;
        }
        for (uint8_t i = 0; i < 4; ++i) {
            const ForestSpec& spec = FOREST_SPECS[order[i]];
            if (!forestFits(spec, road, water)) continue;
            stampForest(out, spec, forest);
            out.hasForest = true;
            break;
        }
    }

    uint8_t flowerTarget = 6 + terrain.bounded(5);
    uint8_t flowers = 0;
    for (uint8_t attempt = 0; attempt < 64 && flowers < flowerTarget; ++attempt) {
        uint8_t x = terrain.bounded(WIDTH);
        uint8_t y = terrain.bounded(HEIGHT);
        if (road.contains(x, y) || water.contains(x, y) || forest.contains(x, y)) continue;
        uint16_t& tile = out.layers[0][y * WIDTH + x];
        if (tile == FLOWER_TILE) continue;
        tile = FLOWER_TILE;
        ++flowers;
    }

    for (uint8_t pathIndex = 0; pathIndex < out.pathCount; ++pathIndex) {
        const Path& path = out.paths[pathIndex];
        if (path.pointCount < 2) return false;
        for (uint8_t pointIndex = 0; pointIndex < path.pointCount; ++pointIndex) {
            const Point& point = path.points[pointIndex];
            if (!road.contains(point.x, point.y) || water.contains(point.x, point.y) ||
                forest.contains(point.x, point.y)) return false;
        }
    }
    return true;
}

uint32_t fingerprint(const Map& map) {
    uint32_t hash = 2166136261U;
    hash = fnvWord(hash, ALGORITHM_VERSION);
    hash = fnvByte(hash, static_cast<uint8_t>(map.entry.edge));
    hash = fnvByte(hash, map.entry.point.x);
    hash = fnvByte(hash, map.entry.point.y);
    hash = fnvByte(hash, map.pathCount);
    for (uint8_t i = 0; i < map.pathCount; ++i) {
        const Path& path = map.paths[i];
        hash = fnvByte(hash, path.pointCount);
        hash = fnvByte(hash, static_cast<uint8_t>(path.exit.edge));
        hash = fnvByte(hash, path.exit.point.x);
        hash = fnvByte(hash, path.exit.point.y);
        for (uint8_t p = 0; p < path.pointCount; ++p) {
            hash = fnvByte(hash, path.points[p].x);
            hash = fnvByte(hash, path.points[p].y);
        }
    }
    hash = fnvByte(hash, map.hasCoast ? 1 : 0);
    hash = fnvByte(hash, map.hasForest ? 1 : 0);
    for (uint8_t layer = 0; layer < LAYER_COUNT; ++layer) {
        for (uint16_t index = 0; index < CELL_COUNT; ++index) {
            hash = fnvWord(hash, map.layers[layer][index]);
        }
    }
    return hash;
}

bool isRoadTile(uint16_t tileId) {
    switch (tileId) {
    case 537: case 538: case 539: case 540: case 542: case 545: case 546:
    case 547: case 553: case 554: case 555: case 556: case 558:
        return true;
    default:
        return false;
    }
}

bool isWaterTile(uint16_t tileId) {
    return tileId == DEEP_SEA_TILE || tileId == DEEP_SEA_EDGE_TILE || tileId == SEA_SHORE_TILE;
}

bool isForestTile(uint16_t tileId) {
    switch (tileId) {
    case 800: case 801: case 802: case 804: case 805: case 808: case 809:
    case 810: case 811: case 818: case 819: case FENCE_TOP_LEFT: case FENCE_LEFT:
    case FENCE_TOP: case FENCE_TOP_RIGHT:
        return true;
    default:
        return false;
    }
}

}  // namespace ExploreMapGenerator
