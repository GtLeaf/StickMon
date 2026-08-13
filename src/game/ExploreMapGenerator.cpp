#include "game/ExploreMapGenerator.h"

#include "game/ExploreCaveTiles.h"
#include "game/ExploreIceSlide.h"

#include <cstring>

namespace ExploreMapGenerator {
namespace {

constexpr uint16_t GRASS_TILES[] = {385, 385, 385, 386, 387, 388, 389};
constexpr uint16_t DENSE_GRASS_TILE = 390;
constexpr uint16_t FLOWER_TILE = 415;
constexpr uint16_t SHRUB_TILE = 859;
constexpr uint16_t DEEP_SEA_TILE = 144;
constexpr uint16_t DEEP_SEA_EDGE_TILE = 168;
constexpr uint16_t SEA_SHORE_TILE = 72;

// Map069 Route 8 autotile slots. Sea has eight frames; each waterfall strip has four.
constexpr uint16_t SEA_WATER_TILE = 48;
constexpr uint16_t SEA_LEFT_SHORE_TILE = 64;
constexpr uint16_t SEA_TOP_SHORE_TILE = 68;
constexpr uint16_t SEA_RIGHT_SHORE_TILE = 72;
constexpr uint16_t SEA_TOP_RIGHT_CORNER_TILE = 80;
constexpr uint16_t SEA_TOP_LEFT_CORNER_TILE = 84;
constexpr uint16_t SEA_CORNER_UNDERLAY_TILE = 385;

constexpr uint16_t WATERFALL_CREST_TILES[3] = {283, 273, 285};
constexpr uint16_t WATERFALL_BODY_TOP_TILES[3] = {322, 308, 324};
constexpr uint16_t WATERFALL_BODY_MIDDLE_TILES[3] = {304, 288, 312};
constexpr uint16_t WATERFALL_BODY_LOWER_TILES[3] = {328, 316, 326};
constexpr uint16_t WATERFALL_BOTTOM_TILE = 336;

constexpr uint16_t STREAM_LEFT_TILE = 1096;
constexpr uint16_t STREAM_CENTER_TILE = 1097;
constexpr uint16_t STREAM_RIGHT_TILE = 1098;
constexpr uint16_t STREAM_TOP_OUTER_LEFT_TILE = 1088;
constexpr uint16_t STREAM_TOP_OUTER_RIGHT_TILE = 1090;
constexpr uint16_t STREAM_TOP_INNER_LEFT_TILE = 1104;
constexpr uint16_t STREAM_TOP_INNER_RIGHT_TILE = 1106;
constexpr uint16_t STREAM_BOTTOM_OUTER_LEFT_TILE = 4400;
constexpr uint16_t STREAM_BOTTOM_OUTER_RIGHT_TILE = 4401;
constexpr uint16_t STREAM_BOTTOM_INNER_LEFT_TILE = 4402;
constexpr uint16_t STREAM_BOTTOM_INNER_RIGHT_TILE = 4403;

constexpr uint16_t CLIFF_TOP_TILE = 1188;
constexpr uint16_t CLIFF_FACE_TILE = 1185;
constexpr uint16_t CLIFF_STAIR_LEFT_TILE = 1161;
constexpr uint16_t CLIFF_STAIR_RIGHT_TILE = 1162;

constexpr uint16_t BRIDGE_TOP_TILE = 1627;
constexpr uint16_t BRIDGE_BOTTOM_TILE = 1643;
constexpr uint16_t WATER_ROCK_SMALL_TILE = 1532;
constexpr uint16_t WATER_ROCK_LARGE_TILES[2][2] = {
    {1506, 1507},
    {1514, 1515},
};
constexpr uint16_t BOULDER_TILE = 1231;

// IDs 4500+ are runtime aliases into Caves.png. They deliberately live
// outside the Outside.png range so identically numbered source tiles cannot
// select the wrong atlas frame.
constexpr uint16_t SNOW_GROUND_TILES[] = {4500, 4500, 4500, 4500, 4502, 4503};
constexpr uint16_t SNOW_PATH_TILE = 4501;
constexpr uint16_t SNOW_GROUND_DECOR_TILES[] = {4504, 4505, 4506};
constexpr uint16_t SNOW_SCENERY_TILES[] = {4507, 4508, 4509, 4510};

constexpr uint16_t FROST_OUTSIDE_TILE = 4500;
constexpr uint16_t FROST_FLOOR_TILE = 4511;
constexpr uint16_t FROST_WALL_TOP_LEFT_TILE = 4512;
constexpr uint16_t FROST_WALL_TOP_TILE = 4513;
constexpr uint16_t FROST_WALL_TOP_RIGHT_TILE = 4514;
constexpr uint16_t FROST_WALL_LEFT_TILE = 4515;
constexpr uint16_t FROST_WALL_RIGHT_TILE = 4516;
constexpr uint16_t FROST_WALL_BOTTOM_LEFT_TILE = 4517;
constexpr uint16_t FROST_WALL_BOTTOM_TILE = 4518;
constexpr uint16_t FROST_WALL_BOTTOM_RIGHT_TILE = 4519;
constexpr uint16_t FROST_INNER_OUTSIDE_NW_TILE = 4520;
constexpr uint16_t FROST_INNER_OUTSIDE_NE_TILE = 4521;
constexpr uint16_t FROST_INNER_OUTSIDE_SW_TILE = 4522;
constexpr uint16_t FROST_INNER_OUTSIDE_SE_TILE = 4523;
constexpr uint16_t FROST_ICE_TOP_LEFT_TILE = 4524;
constexpr uint16_t FROST_ICE_TOP_TILE = 4525;
constexpr uint16_t FROST_ICE_TOP_RIGHT_TILE = 4526;
constexpr uint16_t FROST_ICE_LEFT_TILE = 4527;
constexpr uint16_t FROST_ICE_CENTER_TILE = 4504;
constexpr uint16_t FROST_ICE_RIGHT_TILE = 4528;
constexpr uint16_t FROST_ICE_BOTTOM_LEFT_TILE = 4529;
constexpr uint16_t FROST_ICE_BOTTOM_TILE = 4530;
constexpr uint16_t FROST_ICE_BOTTOM_RIGHT_TILE = 4531;
constexpr uint16_t FROST_ROCK_HILL_TILES[3][3] = {
    {4532, 4533, 4534},
    {4535, 4536, 4537},
    {4538, 4539, 4540},
};
constexpr uint16_t FROST_CRYSTAL_TOP_TILE = 4541;

constexpr uint16_t TOP_EDGE_FOREST_TILE_ROWS[3][2] = {
    {800, 801},
    {808, 809},
    {818, 819},
};

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

    void remove(int x, int y) {
        if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
        uint16_t index = static_cast<uint16_t>(y * WIDTH + x);
        words_[index >> 5] &= ~(1UL << (index & 31));
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

    bool empty() const {
        for (uint8_t i = 0; i < (CELL_COUNT + 31) / 32; ++i) {
            if (words_[i] != 0) return false;
        }
        return true;
    }

    uint16_t count() const {
        uint16_t total = 0;
        for (uint8_t i = 0; i < (CELL_COUNT + 31) / 32; ++i) {
            total += static_cast<uint16_t>(__builtin_popcount(words_[i]));
        }
        return total;
    }

    void unite(const CellMask& other) {
        for (uint8_t i = 0; i < (CELL_COUNT + 31) / 32; ++i) words_[i] |= other.words_[i];
    }

    void subtract(const CellMask& other) {
        for (uint8_t i = 0; i < (CELL_COUNT + 31) / 32; ++i) words_[i] &= ~other.words_[i];
    }

    bool intersects(const CellMask& other) const {
        for (uint8_t i = 0; i < (CELL_COUNT + 31) / 32; ++i) {
            if ((words_[i] & other.words_[i]) != 0) return true;
        }
        return false;
    }

    bool equals(const CellMask& other) const {
        for (uint8_t i = 0; i < (CELL_COUNT + 31) / 32; ++i) {
            if (words_[i] != other.words_[i]) return false;
        }
        return true;
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

struct PointGroup {
    Point points[5] = {};
    uint8_t count = 0;
};

struct FlowerShape {
    Point points[5];
    uint8_t count;
    uint8_t width;
    uint8_t height;
};

struct CreekSpec {
    uint8_t left;
    uint8_t width;
    uint8_t bridgeTop;
};

struct StreamShape {
    uint8_t upstreamLeft;
    uint8_t upstreamWidth;
    uint8_t downstreamLeft;
    uint8_t downstreamWidth;
    uint8_t boundary;
};

struct CliffSpec {
    uint8_t top;
    uint8_t left;
    uint8_t right;
    uint8_t stairLeft;
};

struct FrostRect {
    uint8_t left;
    uint8_t top;
    uint8_t right;
    uint8_t bottom;
};

struct FrostScenerySpec {
    Point point;
    uint16_t tileId;
};

struct FrostTemplate {
    const FrostRect* rects;
    uint8_t rectCount;
    Endpoint entry;
    Endpoint exits[PATH_COUNT];
    Point junction;
    const Point* routeControls[PATH_COUNT];
    uint8_t routeControlCounts[PATH_COUNT];
    const FrostRect* iceRects;
    uint8_t iceRectCount;
    bool hasRockHill;
    Point rockHill;
    const FrostScenerySpec* crystals;
    uint8_t crystalCount;
    const FrostScenerySpec* boulders;
    uint8_t boulderCount;
};

struct AreaProfile {
    uint8_t coastChance;
    uint8_t forestChance;
    uint8_t grassMinimum;
    uint8_t grassVariance;
    uint8_t flowerGroups;
};

constexpr AreaProfile AREA_PROFILES[] = {
    {0, 20, 24, 8, 2},    // Grass path: open and resource friendly.
    {0, 0, 16, 8, 1},     // Creek slope: stream and cliff carry the scene.
    {0, 25, 44, 10, 2},   // Tall grass park: one large encounter patch.
    {0, 0, 0, 0, 0},      // Frost cave uses its own snow decoration pass.
    {0, 100, 30, 8, 1},   // Mist forest: guaranteed forest enclosure.
    {0, 0, 18, 7, 1},     // Ancient valley: waterfall scenery dominates.
};

constexpr FrostRect FROST_H0_RECTS[] = {
    {0, 5, 9, 11}, {5, 1, 12, 8}, {9, 0, 12, 3}, {11, 3, 15, 7},
};
constexpr Point FROST_H0_ROUTE0[] = {
    {0, 9}, {7, 9}, {7, 6}, {10, 6}, {10, 0},
};
constexpr Point FROST_H0_ROUTE1[] = {
    {0, 9}, {7, 9}, {7, 6}, {15, 6},
};
constexpr FrostScenerySpec FROST_H0_CRYSTALS[] = {
    {{6, 5}, 4508}, {{13, 5}, 4509},
};
constexpr FrostScenerySpec FROST_H0_BOULDERS[] = {
    {{8, 10}, 4542}, {{11, 7}, 4510},
};

constexpr FrostRect FROST_H1_RECTS[] = {
    {0, 1, 6, 8}, {5, 4, 10, 7}, {9, 2, 15, 10}, {11, 8, 14, 11}, {2, 0, 5, 3},
};
constexpr Point FROST_H1_ROUTE0[] = {
    {0, 6}, {7, 6}, {13, 6}, {13, 11}, {12, 11},
};
constexpr Point FROST_H1_ROUTE1[] = {
    {0, 6}, {3, 6}, {3, 0},
};
constexpr FrostScenerySpec FROST_H1_CRYSTALS[] = {
    {{7, 5}, 4508}, {{4, 7}, 4509},
};
constexpr FrostScenerySpec FROST_H1_BOULDERS[] = {
    {{2, 5}, 4542}, {{14, 8}, 4544},
};

constexpr FrostRect FROST_V0_RECTS[] = {
    {0, 3, 15, 8}, {5, 0, 11, 11},
};
constexpr Point FROST_V0_ROUTE0[] = {
    {8, 11}, {8, 4}, {1, 4}, {1, 5}, {0, 5},
};
constexpr Point FROST_V0_ROUTE1[] = {
    {8, 11}, {8, 4}, {15, 4},
};
constexpr FrostRect FROST_V0_ICE[] = {
    {6, 5, 10, 9},
};
constexpr FrostScenerySpec FROST_V0_CRYSTALS[] = {
    {{2, 7}, 4509}, {{10, 2}, 4508},
};
constexpr FrostScenerySpec FROST_V0_BOULDERS[] = {
    {{3, 6}, 4543}, {{10, 9}, 4510},
};

constexpr FrostRect FROST_V1_RECTS[] = {
    {2, 0, 13, 4}, {0, 2, 5, 10}, {10, 2, 15, 10}, {4, 7, 11, 11},
};
constexpr Point FROST_V1_ROUTE0[] = {
    {7, 11}, {7, 8}, {4, 8}, {4, 2}, {11, 2}, {11, 4}, {15, 4},
};
constexpr Point FROST_V1_ROUTE1[] = {
    {7, 11}, {7, 8}, {4, 8}, {4, 2}, {11, 2}, {11, 8}, {0, 8},
};
constexpr FrostRect FROST_V1_ICE[] = {
    {5, 1, 10, 3},
};
constexpr FrostScenerySpec FROST_V1_CRYSTALS[] = {
    {{3, 2}, 4509}, {{13, 5}, 4508},
};
constexpr FrostScenerySpec FROST_V1_BOULDERS[] = {
    {{4, 9}, 4544}, {{11, 9}, 4542},
};

constexpr FrostTemplate FROST_HORIZONTAL_TEMPLATES[] = {
    {
        FROST_H0_RECTS,
        static_cast<uint8_t>(sizeof(FROST_H0_RECTS) / sizeof(FROST_H0_RECTS[0])),
        {{0, 9}, Edge::LEFT},
        {{{10, 0}, Edge::TOP}, {{15, 6}, Edge::RIGHT}},
        {7, 6},
        {FROST_H0_ROUTE0, FROST_H0_ROUTE1},
        {
            static_cast<uint8_t>(sizeof(FROST_H0_ROUTE0) / sizeof(FROST_H0_ROUTE0[0])),
            static_cast<uint8_t>(sizeof(FROST_H0_ROUTE1) / sizeof(FROST_H0_ROUTE1[0])),
        },
        nullptr,
        0,
        true,
        {2, 6},
        FROST_H0_CRYSTALS,
        static_cast<uint8_t>(
            sizeof(FROST_H0_CRYSTALS) / sizeof(FROST_H0_CRYSTALS[0])),
        FROST_H0_BOULDERS,
        static_cast<uint8_t>(
            sizeof(FROST_H0_BOULDERS) / sizeof(FROST_H0_BOULDERS[0])),
    },
    {
        FROST_H1_RECTS,
        static_cast<uint8_t>(sizeof(FROST_H1_RECTS) / sizeof(FROST_H1_RECTS[0])),
        {{0, 6}, Edge::LEFT},
        {{{12, 11}, Edge::BOTTOM}, {{3, 0}, Edge::TOP}},
        {3, 6},
        {FROST_H1_ROUTE0, FROST_H1_ROUTE1},
        {
            static_cast<uint8_t>(sizeof(FROST_H1_ROUTE0) / sizeof(FROST_H1_ROUTE0[0])),
            static_cast<uint8_t>(sizeof(FROST_H1_ROUTE1) / sizeof(FROST_H1_ROUTE1[0])),
        },
        nullptr,
        0,
        false,
        {0, 0},
        FROST_H1_CRYSTALS,
        static_cast<uint8_t>(
            sizeof(FROST_H1_CRYSTALS) / sizeof(FROST_H1_CRYSTALS[0])),
        FROST_H1_BOULDERS,
        static_cast<uint8_t>(
            sizeof(FROST_H1_BOULDERS) / sizeof(FROST_H1_BOULDERS[0])),
    },
};

constexpr FrostTemplate FROST_VERTICAL_TEMPLATES[] = {
    {
        FROST_V0_RECTS,
        static_cast<uint8_t>(sizeof(FROST_V0_RECTS) / sizeof(FROST_V0_RECTS[0])),
        {{8, 11}, Edge::BOTTOM},
        {{{0, 5}, Edge::LEFT}, {{15, 4}, Edge::RIGHT}},
        {8, 4},
        {FROST_V0_ROUTE0, FROST_V0_ROUTE1},
        {
            static_cast<uint8_t>(sizeof(FROST_V0_ROUTE0) / sizeof(FROST_V0_ROUTE0[0])),
            static_cast<uint8_t>(sizeof(FROST_V0_ROUTE1) / sizeof(FROST_V0_ROUTE1[0])),
        },
        FROST_V0_ICE,
        static_cast<uint8_t>(sizeof(FROST_V0_ICE) / sizeof(FROST_V0_ICE[0])),
        false,
        {0, 0},
        FROST_V0_CRYSTALS,
        static_cast<uint8_t>(
            sizeof(FROST_V0_CRYSTALS) / sizeof(FROST_V0_CRYSTALS[0])),
        FROST_V0_BOULDERS,
        static_cast<uint8_t>(
            sizeof(FROST_V0_BOULDERS) / sizeof(FROST_V0_BOULDERS[0])),
    },
    {
        FROST_V1_RECTS,
        static_cast<uint8_t>(sizeof(FROST_V1_RECTS) / sizeof(FROST_V1_RECTS[0])),
        {{7, 11}, Edge::BOTTOM},
        {{{15, 4}, Edge::RIGHT}, {{0, 8}, Edge::LEFT}},
        {11, 4},
        {FROST_V1_ROUTE0, FROST_V1_ROUTE1},
        {
            static_cast<uint8_t>(sizeof(FROST_V1_ROUTE0) / sizeof(FROST_V1_ROUTE0[0])),
            static_cast<uint8_t>(sizeof(FROST_V1_ROUTE1) / sizeof(FROST_V1_ROUTE1[0])),
        },
        FROST_V1_ICE,
        static_cast<uint8_t>(sizeof(FROST_V1_ICE) / sizeof(FROST_V1_ICE[0])),
        false,
        {0, 0},
        FROST_V1_CRYSTALS,
        static_cast<uint8_t>(
            sizeof(FROST_V1_CRYSTALS) / sizeof(FROST_V1_CRYSTALS[0])),
        FROST_V1_BOULDERS,
        static_cast<uint8_t>(
            sizeof(FROST_V1_BOULDERS) / sizeof(FROST_V1_BOULDERS[0])),
    },
};

const AreaProfile& profileFor(uint8_t areaIndex) {
    return AREA_PROFILES[
        areaIndex < sizeof(AREA_PROFILES) / sizeof(AREA_PROFILES[0]) ? areaIndex : 0];
}

constexpr ForestSpec FOREST_SPECS[] = {
    {10, 5, 6, 11},
    {12, 1, 4, 7},
    {1, 5, 6, 11},
    {1, 0, 6, 6},
};

constexpr FlowerShape FLOWER_SHAPES[] = {
    {{{0, 0}, {0, 1}, {1, 0}, {}, {}}, 3, 2, 2},
    {{{0, 0}, {0, 1}, {1, 1}, {}, {}}, 3, 2, 2},
    {{{0, 0}, {1, 0}, {1, 1}, {}, {}}, 3, 2, 2},
    {{{0, 1}, {1, 0}, {1, 1}, {}, {}}, 3, 2, 2},
    {{{0, 0}, {0, 1}, {0, 2}, {1, 1}, {}}, 4, 2, 3},
    {{{0, 0}, {0, 1}, {1, 0}, {1, 1}, {}}, 4, 2, 2},
    {{{0, 0}, {1, 0}, {1, 1}, {2, 0}, {}}, 4, 3, 2},
    {{{0, 1}, {1, 0}, {1, 1}, {1, 2}, {}}, 4, 2, 3},
    {{{0, 1}, {1, 0}, {1, 1}, {2, 1}, {}}, 4, 3, 2},
    {{{0, 0}, {0, 1}, {0, 2}, {1, 0}, {1, 1}}, 5, 2, 3},
    {{{0, 0}, {0, 1}, {0, 2}, {1, 1}, {1, 2}}, 5, 2, 3},
    {{{0, 0}, {0, 1}, {1, 0}, {1, 1}, {1, 2}}, 5, 2, 3},
    {{{0, 0}, {0, 1}, {1, 0}, {1, 1}, {2, 0}}, 5, 3, 2},
    {{{0, 0}, {0, 1}, {1, 0}, {1, 1}, {2, 1}}, 5, 3, 2},
    {{{0, 0}, {1, 0}, {1, 1}, {2, 0}, {2, 1}}, 5, 3, 2},
    {{{0, 1}, {0, 2}, {1, 0}, {1, 1}, {1, 2}}, 5, 2, 3},
    {{{0, 1}, {1, 0}, {1, 1}, {2, 0}, {2, 1}}, 5, 3, 2},
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
    switch (exit.edge) {
    case Edge::TOP:
        if (!appendLine(out, junctionX, 1) || !appendLine(out, exit.point.x, 1)) return false;
        break;
    case Edge::RIGHT:
        if (!appendLine(out, WIDTH - 3, junctionY) ||
            !appendLine(out, WIDTH - 3, exit.point.y)) return false;
        break;
    case Edge::BOTTOM:
        if (!appendLine(out, junctionX, HEIGHT - 3) ||
            !appendLine(out, exit.point.x, HEIGHT - 3)) return false;
        break;
    case Edge::LEFT:
        if (!appendLine(out, 1, junctionY) || !appendLine(out, 1, exit.point.y)) return false;
        break;
    }
    if (!appendLine(out, exit.point.x, exit.point.y)) return false;
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

void addPathRoadFrom(const Path& path, uint8_t startIndex, CellMask& road) {
    for (uint8_t i = startIndex + 1; i < path.pointCount; ++i) {
        const Point& first = path.points[i - 1];
        const Point& second = path.points[i];
        bool vertical = first.x == second.x;
        addRoadPair(road, first, vertical);
        addRoadPair(road, second, vertical);
    }
}

CellMask expandedMask(const CellMask& source, uint8_t radius) {
    CellMask result;
    result.clear();
    for (uint8_t y = 0; y < HEIGHT; ++y) {
        for (uint8_t x = 0; x < WIDTH; ++x) {
            if (!source.contains(x, y)) continue;
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) result.add(x + dx, y + dy);
            }
        }
    }
    return result;
}

CellMask orthogonallyExpanded(const CellMask& source) {
    CellMask result = source;
    for (uint8_t y = 0; y < HEIGHT; ++y) {
        for (uint8_t x = 0; x < WIDTH; ++x) {
            if (!source.contains(x, y)) continue;
            result.add(x + 1, y);
            result.add(x - 1, y);
            result.add(x, y + 1);
            result.add(x, y - 1);
        }
    }
    return result;
}

void addEndpointEdgeCells(const Endpoint& endpoint, CellMask& cells) {
    cells.add(endpoint.point.x, endpoint.point.y);
    if (endpoint.edge == Edge::TOP || endpoint.edge == Edge::BOTTOM) {
        cells.add(endpoint.point.x + 1, endpoint.point.y);
    } else {
        cells.add(endpoint.point.x, endpoint.point.y + 1);
    }
}

bool validateBorderRoad(const Map& map, const CellMask& road) {
    CellMask boundary;
    CellMask allowed;
    boundary.clear();
    allowed.clear();
    for (uint8_t y = 0; y < HEIGHT; ++y) {
        for (uint8_t x = 0; x < WIDTH; ++x) {
            if (road.contains(x, y) &&
                (x == 0 || x == WIDTH - 1 || y == 0 || y == HEIGHT - 1)) {
                boundary.add(x, y);
            }
        }
    }
    addEndpointEdgeCells(map.entry, allowed);
    for (uint8_t i = 0; i < map.pathCount; ++i) addEndpointEdgeCells(map.paths[i].exit, allowed);
    return boundary.equals(allowed);
}

bool validateTopology(const Map& map) {
    if (map.pathCount != PATH_COUNT) return false;
    uint8_t sharedCount = 0;
    while (sharedCount < map.paths[0].pointCount && sharedCount < map.paths[1].pointCount &&
           samePoint(map.paths[0].points[sharedCount], map.paths[1].points[sharedCount])) {
        ++sharedCount;
    }
    if (sharedCount == 0 ||
        !samePoint(map.paths[0].points[sharedCount - 1], map.junction)) return false;

    CellMask branches[PATH_COUNT];
    for (uint8_t pathIndex = 0; pathIndex < PATH_COUNT; ++pathIndex) {
        const Path& path = map.paths[pathIndex];
        if (path.pointCount - (sharedCount - 1) < 2) return false;
        for (uint8_t i = 0; i < path.pointCount; ++i) {
            for (uint8_t j = i + 1; j < path.pointCount; ++j) {
                if (samePoint(path.points[i], path.points[j])) return false;
            }
        }
        branches[pathIndex].clear();
        addPathRoadFrom(path, sharedCount - 1, branches[pathIndex]);
    }

    CellMask junctionZone;
    junctionZone.clear();
    for (int y = map.junction.y - 1; y <= map.junction.y + 1; ++y) {
        for (int x = map.junction.x - 1; x <= map.junction.x + 1; ++x) {
            junctionZone.add(x, y);
        }
    }
    branches[0].subtract(junctionZone);
    branches[1].subtract(junctionZone);
    if (branches[0].intersects(branches[1])) return false;
    return !orthogonallyExpanded(branches[0]).intersects(branches[1]);
}

bool buildTopology(uint32_t seed, Edge entryEdge, uint8_t coastChance,
                   Map& out, CellMask& road) {
    out.entry = {};
    out.junction = {};
    std::memset(out.paths, 0, sizeof(out.paths));
    out.pathCount = PATH_COUNT;
    out.hasCoast = false;
    road.clear();

    Rng topology(seed ^ 0xA511E9B3U);
    uint8_t coastRoll = topology.bounded(100);
    out.hasCoast = coastChance > 0 && entryEdge != Edge::LEFT &&
                   coastRoll < coastChance;

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

    uint8_t minimumX = out.hasCoast ? 7 : 4;
    uint8_t junctionX = minimumX + topology.bounded(11 - minimumX);
    uint8_t junctionY = 4 + topology.bounded(3);
    out.junction = {junctionX, junctionY};
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
            uint8_t minimumExitX = out.hasCoast ? 7 : 4;
            coordinate = minimumExitX + topology.bounded(11 - minimumExitX);
        } else {
            coordinate = 4 + topology.bounded(3);
        }
        Endpoint exit = endpointForEdge(edge, junctionX, junctionY, coordinate);
        if (!buildPath(trunk, junctionX, junctionY, exit, out.paths[i])) return false;
    }
    if (!validateTopology(out)) return false;
    for (uint8_t i = 0; i < out.pathCount; ++i) addPathRoad(out.paths[i], road);
    return validateBorderRoad(out, road);
}

bool buildAncientWaterfallTopology(Edge entryEdge, Map& out, CellMask& road) {
    out.entry = {};
    out.junction = {};
    std::memset(out.paths, 0, sizeof(out.paths));
    out.pathCount = PATH_COUNT;
    out.hasCoast = false;
    road.clear();

    Endpoint exits[PATH_COUNT] = {};
    switch (entryEdge) {
    case Edge::BOTTOM:
        out.entry = {{11, HEIGHT - 1}, Edge::BOTTOM};
        out.junction = {11, 9};
        exits[0] = {{WIDTH - 1, 9}, Edge::RIGHT};
        exits[1] = {{11, 0}, Edge::TOP};
        break;
    case Edge::LEFT:
        out.entry = {{0, 2}, Edge::LEFT};
        out.junction = {4, 2};
        exits[0] = {{4, HEIGHT - 1}, Edge::BOTTOM};
        exits[1] = {{4, 0}, Edge::TOP};
        break;
    case Edge::TOP:
        out.entry = {{5, 0}, Edge::TOP};
        out.junction = {5, 9};
        exits[0] = {{0, 9}, Edge::LEFT};
        exits[1] = {{5, HEIGHT - 1}, Edge::BOTTOM};
        break;
    case Edge::RIGHT:
        out.entry = {{WIDTH - 1, 9}, Edge::RIGHT};
        out.junction = {12, 9};
        exits[0] = {{12, 0}, Edge::TOP};
        exits[1] = {{12, HEIGHT - 1}, Edge::BOTTOM};
        break;
    }

    Path trunk;
    if (!appendPoint(trunk, out.entry.point.x, out.entry.point.y) ||
        !appendLine(trunk, out.junction.x, out.junction.y)) return false;
    for (uint8_t i = 0; i < PATH_COUNT; ++i) {
        out.paths[i] = trunk;
        if (!appendLine(out.paths[i], exits[i].point.x, exits[i].point.y)) return false;
        out.paths[i].exit = exits[i];
    }
    if (!validateTopology(out)) return false;
    for (uint8_t i = 0; i < out.pathCount; ++i) addPathRoad(out.paths[i], road);
    return validateBorderRoad(out, road);
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

void stampSeaChannel(Map& map, uint8_t left, uint8_t top, uint8_t width,
                     uint8_t bottom, uint8_t openLeft, uint8_t openWidth,
                     CellMask& water) {
    for (uint8_t y = top; y < bottom; ++y) {
        for (uint8_t column = 0; column < width; ++column) {
            uint8_t x = left + column;
            uint16_t tileId;
            bool open = openWidth > 0 && x >= openLeft && x < openLeft + openWidth;
            if (y == top && openWidth > 0) {
                if (open) tileId = SEA_WATER_TILE;
                else if (column == 0) tileId = SEA_TOP_LEFT_CORNER_TILE;
                else if (column == width - 1) tileId = SEA_TOP_RIGHT_CORNER_TILE;
                else tileId = SEA_TOP_SHORE_TILE;
            } else if (column == 0) {
                tileId = SEA_LEFT_SHORE_TILE;
            } else if (column == width - 1) {
                tileId = SEA_RIGHT_SHORE_TILE;
            } else {
                tileId = SEA_WATER_TILE;
            }

            uint16_t index = y * WIDTH + x;
            if (tileId == SEA_TOP_LEFT_CORNER_TILE ||
                tileId == SEA_TOP_RIGHT_CORNER_TILE) {
                // These autotile corners are transparent outside the bank curve.
                map.layers[0][index] = SEA_CORNER_UNDERLAY_TILE;
                map.layers[1][index] = tileId;
            } else {
                map.layers[0][index] = tileId;
            }
            water.add(x, y);
        }
    }
}

void stampWaterfall(Map& map, uint8_t left, uint8_t width, uint8_t crestTop,
                    uint8_t bodyHeight, CellMask& water) {
    auto stampRow = [&](uint8_t y, const uint16_t tiles[3]) {
        for (uint8_t column = 0; column < width; ++column) {
            uint8_t x = left + column;
            uint8_t tileIndex = column == 0 ? 0 : (column == width - 1 ? 2 : 1);
            map.layers[0][y * WIDTH + x] = tiles[tileIndex];
            water.add(x, y);
        }
    };

    stampRow(crestTop, WATERFALL_CREST_TILES);
    for (uint8_t row = 0; row < bodyHeight; ++row) {
        const uint16_t* tiles = row == 0
                                    ? WATERFALL_BODY_TOP_TILES
                                    : (row == bodyHeight - 1
                                           ? WATERFALL_BODY_LOWER_TILES
                                           : WATERFALL_BODY_MIDDLE_TILES);
        stampRow(crestTop + 1 + row, tiles);
    }
    const uint16_t bottomTiles[3] = {
        WATERFALL_BOTTOM_TILE, WATERFALL_BOTTOM_TILE, WATERFALL_BOTTOM_TILE,
    };
    stampRow(crestTop + bodyHeight + 1, bottomTiles);
}

void stampWaterfallWall(Map& map, uint8_t top, uint8_t bottom,
                        uint8_t firstGapLeft, uint8_t firstGapWidth,
                        uint8_t secondGapLeft, uint8_t secondGapWidth,
                        uint8_t stairLeft, CellMask& cliff, CellMask& stairs) {
    for (uint8_t y = top; y < bottom; ++y) {
        for (uint8_t x = 0; x < WIDTH; ++x) {
            bool inFirstGap = x >= firstGapLeft && x < firstGapLeft + firstGapWidth;
            bool inSecondGap = secondGapWidth > 0 &&
                               x >= secondGapLeft && x < secondGapLeft + secondGapWidth;
            if (inFirstGap || inSecondGap) continue;
            cliff.add(x, y);
            if (x == stairLeft || x == stairLeft + 1) {
                map.layers[0][y * WIDTH + x] =
                    x == stairLeft ? CLIFF_STAIR_LEFT_TILE : CLIFF_STAIR_RIGHT_TILE;
                map.layers[1][y * WIDTH + x] = 0;
                stairs.add(x, y);
            } else {
                map.layers[0][y * WIDTH + x] = CLIFF_TOP_TILE;
                map.layers[1][y * WIDTH + x] = 0;
            }
        }
    }
}

void stampTopEdgeForestCluster(Map& map, uint8_t left, uint8_t width,
                               CellMask& forest) {
    for (uint8_t row = 0; row < 3; ++row) {
        for (uint8_t column = 0; column < width; ++column) {
            uint8_t x = left + column;
            map.layers[0][row * WIDTH + x] = TOP_EDGE_FOREST_TILE_ROWS[row][column % 2];
            forest.add(x, row);
        }
    }
}

void stampSmallWaterRock(Map& map, uint8_t x, uint8_t y) {
    map.layers[2][y * WIDTH + x] = WATER_ROCK_SMALL_TILE;
}

void stampLargeWaterRock(Map& map, uint8_t left, uint8_t top) {
    for (uint8_t row = 0; row < 2; ++row) {
        for (uint8_t column = 0; column < 2; ++column) {
            map.layers[2][(top + row) * WIDTH + left + column] =
                WATER_ROCK_LARGE_TILES[row][column];
        }
    }
}

void stampBoulder(Map& map, uint8_t x, uint8_t y, CellMask& scenery) {
    map.layers[2][y * WIDTH + x] = BOULDER_TILE;
    scenery.add(x, y);
}

void stampAncientWaterfallValley(Map& map, CellMask& water, CellMask& forest,
                                 CellMask& cliff, CellMask& stairs,
                                 CellMask& scenery) {
    switch (map.entry.edge) {
    case Edge::BOTTOM:
        stampSeaChannel(map, 4, 0, 4, 3, 0, 0, water);
        stampSeaChannel(map, 3, 8, 6, HEIGHT, 4, 4, water);
        stampWaterfall(map, 4, 4, 3, 3, water);
        stampWaterfallWall(map, 3, 8, 4, 4, 0, 0, 11, cliff, stairs);
        stampSmallWaterRock(map, 6, 9);
        stampTopEdgeForestCluster(map, 0, 4, forest);
        stampTopEdgeForestCluster(map, 13, 2, forest);
        stampBoulder(map, 9, 1, scenery);
        stampBoulder(map, 14, 11, scenery);
        break;
    case Edge::LEFT:
        stampSeaChannel(map, 10, 0, 4, 4, 0, 0, water);
        stampSeaChannel(map, 9, 9, 6, HEIGHT, 10, 4, water);
        stampWaterfall(map, 10, 4, 4, 3, water);
        stampWaterfallWall(map, 4, 9, 10, 4, 0, 0, 4, cliff, stairs);
        stampSmallWaterRock(map, 11, 10);
        stampTopEdgeForestCluster(map, 6, 4, forest);
        stampTopEdgeForestCluster(map, 14, 2, forest);
        stampBoulder(map, 7, 3, scenery);
        stampBoulder(map, 8, 10, scenery);
        break;
    case Edge::TOP:
        stampSeaChannel(map, 10, 0, 4, 3, 0, 0, water);
        stampSeaChannel(map, 9, 8, 6, HEIGHT, 10, 4, water);
        stampWaterfall(map, 10, 4, 3, 3, water);
        stampWaterfallWall(map, 3, 8, 10, 4, 0, 0, 5, cliff, stairs);
        stampSmallWaterRock(map, 11, 9);
        stampTopEdgeForestCluster(map, 0, 4, forest);
        stampTopEdgeForestCluster(map, 7, 2, forest);
        stampTopEdgeForestCluster(map, 14, 2, forest);
        stampBoulder(map, 2, 8, scenery);
        stampBoulder(map, 15, 9, scenery);
        break;
    case Edge::RIGHT:
        stampSeaChannel(map, 2, 0, 3, 3, 0, 0, water);
        stampSeaChannel(map, 7, 0, 3, 3, 0, 0, water);
        stampSeaChannel(map, 1, 8, 10, HEIGHT, 2, 3, water);
        // Keep the second landing opening in the same top basin row.
        for (uint8_t x = 7; x < 10; ++x) map.layers[0][8 * WIDTH + x] = SEA_WATER_TILE;
        stampWaterfall(map, 2, 3, 3, 3, water);
        stampWaterfall(map, 7, 3, 3, 3, water);
        stampWaterfallWall(map, 3, 8, 2, 3, 7, 3, 12, cliff, stairs);
        stampLargeWaterRock(map, 5, 9);
        stampSmallWaterRock(map, 3, 10);
        stampTopEdgeForestCluster(map, 0, 2, forest);
        stampTopEdgeForestCluster(map, 10, 2, forest);
        stampTopEdgeForestCluster(map, 14, 2, forest);
        stampBoulder(map, 6, 1, scenery);
        stampBoulder(map, 11, 9, scenery);
        break;
    }
}

bool creekCandidateFits(const CellMask& road, uint8_t left, uint8_t width,
                        uint8_t bridgeTop) {
    bool crossing = false;
    for (uint8_t y = 0; y < HEIGHT; ++y) {
        for (uint8_t x = left; x < left + width; ++x) {
            if (!road.contains(x, y)) continue;
            crossing = true;
            if (y != bridgeTop && y != bridgeTop + 1) return false;
        }
    }
    if (!crossing) return false;
    return road.contains(left - 1, bridgeTop) &&
           road.contains(left - 1, bridgeTop + 1) &&
           road.contains(left + width, bridgeTop) &&
           road.contains(left + width, bridgeTop + 1);
}

uint16_t creekCandidateCount(const CellMask& road) {
    uint16_t count = 0;
    for (uint8_t width = 3; width <= 5; ++width) {
        for (uint8_t left = 2; left < WIDTH - width - 1; ++left) {
            for (uint8_t top = 1; top < HEIGHT - 2; ++top) {
                if (creekCandidateFits(road, left, width, top)) ++count;
            }
        }
    }
    return count;
}

bool selectCreekCandidate(const CellMask& road, uint16_t choice, CreekSpec& out) {
    for (uint8_t width = 3; width <= 5; ++width) {
        for (uint8_t left = 2; left < WIDTH - width - 1; ++left) {
            for (uint8_t top = 1; top < HEIGHT - 2; ++top) {
                if (!creekCandidateFits(road, left, width, top)) continue;
                if (choice-- == 0) {
                    out = {left, width, top};
                    return true;
                }
            }
        }
    }
    return false;
}

bool streamShapeFits(const CellMask& road, const CreekSpec& creek,
                     const StreamShape& shape) {
    for (uint8_t y = 0; y < HEIGHT; ++y) {
        uint8_t left = shape.boundary && y >= shape.boundary
                           ? shape.downstreamLeft : shape.upstreamLeft;
        uint8_t width = shape.boundary && y >= shape.boundary
                            ? shape.downstreamWidth : shape.upstreamWidth;
        for (uint8_t x = left; x < left + width; ++x) {
            if (!road.contains(x, y)) continue;
            bool inBridge = (y == creek.bridgeTop || y == creek.bridgeTop + 1) &&
                            x >= creek.left && x < creek.left + creek.width;
            if (!inBridge) return false;
        }
    }
    return true;
}

bool transitionShape(const CreekSpec& creek, uint8_t boundary, uint8_t mode,
                     StreamShape& out) {
    int remoteLeft = creek.left;
    int remoteWidth = creek.width;
    switch (mode) {
    case 0: ++remoteLeft; --remoteWidth; break;
    case 1: --remoteWidth; break;
    case 2: --remoteLeft; ++remoteWidth; break;
    default: ++remoteWidth; break;
    }
    if (remoteWidth < 3 || remoteLeft < 1 ||
        remoteLeft + remoteWidth > WIDTH - 1) return false;

    bool bridgeIsDownstream = boundary <= static_cast<int>(creek.bridgeTop) - 2;
    if (bridgeIsDownstream) {
        out = {
            static_cast<uint8_t>(remoteLeft), static_cast<uint8_t>(remoteWidth),
            creek.left, creek.width, boundary,
        };
    } else {
        out = {
            creek.left, creek.width,
            static_cast<uint8_t>(remoteLeft), static_cast<uint8_t>(remoteWidth),
            boundary,
        };
    }
    return true;
}

uint16_t transitionCandidateCount(const CellMask& road, const CreekSpec& creek) {
    uint16_t count = 0;
    for (uint8_t boundary = 1; boundary < HEIGHT; ++boundary) {
        if (boundary > static_cast<int>(creek.bridgeTop) - 2 &&
            boundary < creek.bridgeTop + 4) continue;
        for (uint8_t mode = 0; mode < 4; ++mode) {
            StreamShape shape{};
            if (transitionShape(creek, boundary, mode, shape) &&
                streamShapeFits(road, creek, shape)) ++count;
        }
    }
    return count;
}

bool selectTransitionCandidate(const CellMask& road, const CreekSpec& creek,
                               uint16_t choice, StreamShape& out) {
    for (uint8_t boundary = 1; boundary < HEIGHT; ++boundary) {
        if (boundary > static_cast<int>(creek.bridgeTop) - 2 &&
            boundary < creek.bridgeTop + 4) continue;
        for (uint8_t mode = 0; mode < 4; ++mode) {
            StreamShape shape{};
            if (!transitionShape(creek, boundary, mode, shape) ||
                !streamShapeFits(road, creek, shape)) continue;
            if (choice-- == 0) {
                out = shape;
                return true;
            }
        }
    }
    return false;
}

StreamShape chooseStreamShape(Rng& features, const CellMask& road,
                              const CreekSpec& creek) {
    StreamShape straight = {
        creek.left, creek.width, creek.left, creek.width, 0,
    };
    uint16_t count = transitionCandidateCount(road, creek);
    if (features.bounded(100) >= 55 || count == 0) return straight;
    StreamShape selected{};
    return selectTransitionCandidate(road, creek, features.bounded(count), selected)
               ? selected : straight;
}

void stampStream(Map& map, const StreamShape& shape, CellMask& water,
                 CellMask& transitionCells) {
    water.clear();
    transitionCells.clear();
    for (uint8_t y = 0; y < HEIGHT; ++y) {
        uint8_t left = shape.boundary && y >= shape.boundary
                           ? shape.downstreamLeft : shape.upstreamLeft;
        uint8_t width = shape.boundary && y >= shape.boundary
                            ? shape.downstreamWidth : shape.upstreamWidth;
        for (uint8_t column = 0; column < width; ++column) {
            uint8_t x = left + column;
            uint16_t tileId = column == 0 ? STREAM_LEFT_TILE
                                : column == width - 1 ? STREAM_RIGHT_TILE
                                                      : STREAM_CENTER_TILE;
            map.layers[0][y * WIDTH + x] = tileId;
            water.add(x, y);
        }
    }
    if (shape.boundary == 0) return;

    uint8_t boundary = shape.boundary;
    uint8_t upstreamRight = shape.upstreamLeft + shape.upstreamWidth;
    uint8_t downstreamRight = shape.downstreamLeft + shape.downstreamWidth;
    auto stampCorner = [&](uint8_t x, uint8_t y, uint16_t tileId) {
        map.layers[0][y * WIDTH + x] = tileId;
        transitionCells.add(x, y);
    };
    if (shape.downstreamLeft > shape.upstreamLeft) {
        stampCorner(shape.upstreamLeft, boundary - 1, STREAM_BOTTOM_OUTER_LEFT_TILE);
        stampCorner(shape.downstreamLeft, boundary - 1, STREAM_BOTTOM_INNER_LEFT_TILE);
    } else if (shape.downstreamLeft < shape.upstreamLeft) {
        stampCorner(shape.downstreamLeft, boundary, STREAM_TOP_OUTER_LEFT_TILE);
        stampCorner(shape.upstreamLeft, boundary, STREAM_TOP_INNER_LEFT_TILE);
    }
    if (downstreamRight < upstreamRight) {
        stampCorner(upstreamRight - 1, boundary - 1, STREAM_BOTTOM_OUTER_RIGHT_TILE);
        stampCorner(downstreamRight - 1, boundary - 1, STREAM_BOTTOM_INNER_RIGHT_TILE);
    } else if (downstreamRight > upstreamRight) {
        stampCorner(downstreamRight - 1, boundary, STREAM_TOP_OUTER_RIGHT_TILE);
        stampCorner(upstreamRight - 1, boundary, STREAM_TOP_INNER_RIGHT_TILE);
    }
}

void stampBridge(Map& map, const CreekSpec& creek, CellMask& bridge) {
    bridge.clear();
    uint8_t left = creek.left - 1;
    uint8_t length = creek.width + 2;
    for (uint8_t x = left; x < left + length; ++x) {
        map.layers[2][creek.bridgeTop * WIDTH + x] = BRIDGE_TOP_TILE;
        map.layers[2][(creek.bridgeTop + 1) * WIDTH + x] = BRIDGE_BOTTOM_TILE;
        bridge.add(x, creek.bridgeTop);
        bridge.add(x, creek.bridgeTop + 1);
    }
}

bool cliffCandidateFits(const CellMask& road, const CellMask& water,
                        const CellMask& bridge, uint8_t top, uint8_t left,
                        uint8_t right, uint8_t stairLeft) {
    for (uint8_t x = left; x < right; ++x) {
        for (uint8_t y = top; y <= top + 1; ++y) {
            bool stair = x >= stairLeft && x < stairLeft + 2;
            if (water.contains(x, y) || bridge.contains(x, y) ||
                (road.contains(x, y) && !stair)) return false;
            if (stair && !road.contains(x, y)) return false;
        }
    }
    for (uint8_t x = stairLeft; x < stairLeft + 2; ++x) {
        if (!road.contains(x, top - 1) || !road.contains(x, top + 2)) return false;
    }
    return true;
}

uint16_t cliffCandidateCount(const CellMask& road, const CellMask& water,
                             const CellMask& bridge, const StreamShape& shape) {
    uint8_t minimumLeft = shape.upstreamLeft < shape.downstreamLeft
                              ? shape.upstreamLeft : shape.downstreamLeft;
    uint8_t upstreamRight = shape.upstreamLeft + shape.upstreamWidth;
    uint8_t downstreamRight = shape.downstreamLeft + shape.downstreamWidth;
    uint8_t maximumRight = upstreamRight > downstreamRight ? upstreamRight : downstreamRight;
    uint8_t sides[2][2] = {{0, minimumLeft}, {maximumRight, WIDTH}};
    uint16_t count = 0;
    for (uint8_t side = 0; side < 2; ++side) {
        uint8_t left = sides[side][0];
        uint8_t right = sides[side][1];
        if (right - left < 5) continue;
        for (uint8_t top = 1; top < HEIGHT - 2; ++top) {
            for (uint8_t stairLeft = left + 1; stairLeft < right - 2; ++stairLeft) {
                if (cliffCandidateFits(road, water, bridge, top, left, right, stairLeft)) {
                    ++count;
                }
            }
        }
    }
    return count;
}

bool selectCliffCandidate(const CellMask& road, const CellMask& water,
                          const CellMask& bridge, const StreamShape& shape,
                          uint16_t choice, CliffSpec& out) {
    uint8_t minimumLeft = shape.upstreamLeft < shape.downstreamLeft
                              ? shape.upstreamLeft : shape.downstreamLeft;
    uint8_t upstreamRight = shape.upstreamLeft + shape.upstreamWidth;
    uint8_t downstreamRight = shape.downstreamLeft + shape.downstreamWidth;
    uint8_t maximumRight = upstreamRight > downstreamRight ? upstreamRight : downstreamRight;
    uint8_t sides[2][2] = {{0, minimumLeft}, {maximumRight, WIDTH}};
    for (uint8_t side = 0; side < 2; ++side) {
        uint8_t left = sides[side][0];
        uint8_t right = sides[side][1];
        if (right - left < 5) continue;
        for (uint8_t top = 1; top < HEIGHT - 2; ++top) {
            for (uint8_t stairLeft = left + 1; stairLeft < right - 2; ++stairLeft) {
                if (!cliffCandidateFits(road, water, bridge, top, left, right, stairLeft)) {
                    continue;
                }
                if (choice-- == 0) {
                    out = {top, left, right, stairLeft};
                    return true;
                }
            }
        }
    }
    return false;
}

void stampCliff(Map& map, const CliffSpec& spec, CellMask& cliff, CellMask& stairs) {
    cliff.clear();
    stairs.clear();
    for (uint8_t x = spec.left; x < spec.right; ++x) {
        for (uint8_t y = spec.top; y <= spec.top + 1; ++y) {
            cliff.add(x, y);
            if (x >= spec.stairLeft && x < spec.stairLeft + 2) {
                map.layers[0][y * WIDTH + x] =
                    x == spec.stairLeft ? CLIFF_STAIR_LEFT_TILE : CLIFF_STAIR_RIGHT_TILE;
                map.layers[1][y * WIDTH + x] = 0;
                stairs.add(x, y);
            } else if (y == spec.top) {
                map.layers[0][y * WIDTH + x] = CLIFF_TOP_TILE;
                map.layers[1][y * WIDTH + x] = 0;
            } else {
                map.layers[1][y * WIDTH + x] = CLIFF_FACE_TILE;
            }
        }
    }
}

bool waterRockFits(const Map& map, const CellMask& water, const CellMask& forbidden,
                   uint8_t left, uint8_t top, uint8_t size) {
    for (uint8_t dy = 0; dy < size; ++dy) {
        for (uint8_t dx = 0; dx < size; ++dx) {
            uint8_t x = left + dx;
            uint8_t y = top + dy;
            if (!water.contains(x, y) || forbidden.contains(x, y) ||
                map.layers[0][y * WIDTH + x] != STREAM_CENTER_TILE) return false;
        }
    }
    return true;
}

uint16_t waterRockCandidateCount(const Map& map, const CellMask& water,
                                 const CellMask& forbidden, uint8_t size) {
    uint16_t count = 0;
    for (uint8_t y = 0; y <= HEIGHT - size; ++y) {
        for (uint8_t x = 0; x <= WIDTH - size; ++x) {
            if (waterRockFits(map, water, forbidden, x, y, size)) ++count;
        }
    }
    return count;
}

bool selectWaterRockCandidate(const Map& map, const CellMask& water,
                              const CellMask& forbidden, uint8_t size,
                              uint16_t choice, Point& out) {
    for (uint8_t y = 0; y <= HEIGHT - size; ++y) {
        for (uint8_t x = 0; x <= WIDTH - size; ++x) {
            if (!waterRockFits(map, water, forbidden, x, y, size)) continue;
            if (choice-- == 0) {
                out = {x, y};
                return true;
            }
        }
    }
    return false;
}

void stampWaterRocks(Map& map, Rng& features, const CellMask& water,
                     const CellMask& road, const CellMask& bridge,
                     const CellMask& transitionCells) {
    CellMask rocks;
    rocks.clear();
    CellMask transitionExclusion = expandedMask(transitionCells, 1);
    uint8_t target = 1 + features.bounded(2);
    for (uint8_t index = 0; index < target; ++index) {
        uint8_t size = features.bounded(100) < 35 ? 2 : 1;
        CellMask forbidden = road;
        forbidden.unite(bridge);
        forbidden.unite(transitionExclusion);
        forbidden.unite(expandedMask(rocks, 1));
        uint16_t count = waterRockCandidateCount(map, water, forbidden, size);
        if (count == 0 && size == 2) {
            size = 1;
            count = waterRockCandidateCount(map, water, forbidden, size);
        }
        if (count == 0) continue;
        Point selected{};
        if (!selectWaterRockCandidate(
                map, water, forbidden, size, features.bounded(count), selected)) continue;
        if (size == 1) {
            map.layers[2][selected.y * WIDTH + selected.x] = WATER_ROCK_SMALL_TILE;
            rocks.add(selected.x, selected.y);
        } else {
            for (uint8_t dy = 0; dy < 2; ++dy) {
                for (uint8_t dx = 0; dx < 2; ++dx) {
                    map.layers[2][(selected.y + dy) * WIDTH + selected.x + dx] =
                        WATER_ROCK_LARGE_TILES[dy][dx];
                    rocks.add(selected.x + dx, selected.y + dy);
                }
            }
        }
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

int absolute(int value) {
    return value < 0 ? -value : value;
}

int minimum(int first, int second) {
    return first < second ? first : second;
}

int edgeMargin(const Point& point) {
    int margin = minimum(point.x, WIDTH - 1 - point.x);
    margin = minimum(margin, point.y);
    return minimum(margin, HEIGHT - 1 - point.y);
}

uint8_t localOpenness(const Point& point, const CellMask& available, uint8_t radius) {
    uint8_t total = 0;
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (absolute(dx) + absolute(dy) <= radius &&
                available.contains(point.x + dx, point.y + dy)) {
                ++total;
            }
        }
    }
    return total;
}

void addCardinalNeighbors(const Point& point, const CellMask& allowed,
                          const CellMask& excluded, CellMask& target) {
    constexpr int8_t DX[] = {1, -1, 0, 0};
    constexpr int8_t DY[] = {0, 0, 1, -1};
    for (uint8_t i = 0; i < 4; ++i) {
        int x = point.x + DX[i];
        int y = point.y + DY[i];
        if (allowed.contains(x, y) && !excluded.contains(x, y)) target.add(x, y);
    }
}

uint8_t cardinalNeighborCount(const Point& point, const CellMask& cells) {
    uint8_t count = 0;
    count += cells.contains(point.x + 1, point.y) ? 1 : 0;
    count += cells.contains(point.x - 1, point.y) ? 1 : 0;
    count += cells.contains(point.x, point.y + 1) ? 1 : 0;
    count += cells.contains(point.x, point.y - 1) ? 1 : 0;
    return count;
}

CellMask largestConnectedComponent(const CellMask& available) {
    CellMask remaining = available;
    CellMask best;
    best.clear();
    uint16_t bestCount = 0;
    Point stack[CELL_COUNT];

    // Python tuple ordering is x first, then y.
    for (uint8_t x = 0; x < WIDTH; ++x) {
        for (uint8_t y = 0; y < HEIGHT; ++y) {
            if (!remaining.contains(x, y)) continue;
            CellMask component;
            component.clear();
            uint16_t stackCount = 0;
            stack[stackCount++] = {x, y};
            remaining.remove(x, y);
            while (stackCount > 0) {
                Point point = stack[--stackCount];
                component.add(point.x, point.y);
                constexpr int8_t DX[] = {1, -1, 0, 0};
                constexpr int8_t DY[] = {0, 0, 1, -1};
                for (uint8_t i = 0; i < 4; ++i) {
                    int nx = point.x + DX[i];
                    int ny = point.y + DY[i];
                    if (!remaining.contains(nx, ny)) continue;
                    remaining.remove(nx, ny);
                    stack[stackCount++] = {
                        static_cast<uint8_t>(nx), static_cast<uint8_t>(ny)
                    };
                }
            }
            uint16_t componentCount = component.count();
            if (componentCount > bestCount) {
                best = component;
                bestCount = componentCount;
            }
        }
    }
    return best;
}

void growCompactPatch(Rng& rng, const CellMask& available, uint8_t requestedSize,
                      CellMask& patch) {
    patch.clear();
    CellMask component = largestConnectedComponent(available);
    int componentCount = component.count();
    if (componentCount == 0) return;
    int reserve = componentCount >= 32 ? 12 : (componentCount / 4 > 4 ? componentCount / 4 : 4);
    int target = minimum(requestedSize, componentCount - reserve);
    if (target < 6) return;

    int bestSeedScore = -32768;
    for (uint8_t x = 0; x < WIDTH; ++x) {
        for (uint8_t y = 0; y < HEIGHT; ++y) {
            if (!component.contains(x, y)) continue;
            Point point{x, y};
            int score = localOpenness(point, component, 2) * 4 + edgeMargin(point);
            if (score > bestSeedScore) bestSeedScore = score;
        }
    }
    uint16_t seedOptionCount = 0;
    for (uint8_t x = 0; x < WIDTH; ++x) {
        for (uint8_t y = 0; y < HEIGHT; ++y) {
            if (!component.contains(x, y)) continue;
            Point point{x, y};
            int score = localOpenness(point, component, 2) * 4 + edgeMargin(point);
            if (score >= bestSeedScore - 2) ++seedOptionCount;
        }
    }
    uint16_t seedChoice = static_cast<uint16_t>(rng.bounded(seedOptionCount));
    Point seed{};
    for (uint8_t x = 0; x < WIDTH; ++x) {
        for (uint8_t y = 0; y < HEIGHT; ++y) {
            if (!component.contains(x, y)) continue;
            Point point{x, y};
            int score = localOpenness(point, component, 2) * 4 + edgeMargin(point);
            if (score < bestSeedScore - 2) continue;
            if (seedChoice-- == 0) {
                seed = point;
                x = WIDTH;
                break;
            }
        }
    }

    patch.add(seed.x, seed.y);
    CellMask frontier;
    frontier.clear();
    addCardinalNeighbors(seed, component, patch, frontier);
    while (patch.count() < target && !frontier.empty()) {
        int bestScore = -32768;
        for (uint8_t x = 0; x < WIDTH; ++x) {
            for (uint8_t y = 0; y < HEIGHT; ++y) {
                if (!frontier.contains(x, y)) continue;
                Point point{x, y};
                int distance = absolute(point.x - seed.x) + absolute(point.y - seed.y);
                int score = cardinalNeighborCount(point, patch) * 100 +
                            localOpenness(point, component, 1) * 4 - distance;
                if (score > bestScore) bestScore = score;
            }
        }
        uint16_t optionCount = 0;
        for (uint8_t x = 0; x < WIDTH; ++x) {
            for (uint8_t y = 0; y < HEIGHT; ++y) {
                if (!frontier.contains(x, y)) continue;
                Point point{x, y};
                int distance = absolute(point.x - seed.x) + absolute(point.y - seed.y);
                int score = cardinalNeighborCount(point, patch) * 100 +
                            localOpenness(point, component, 1) * 4 - distance;
                if (score == bestScore) ++optionCount;
            }
        }
        uint16_t choice = static_cast<uint16_t>(rng.bounded(optionCount));
        Point selected{};
        bool found = false;
        for (uint8_t x = 0; x < WIDTH && !found; ++x) {
            for (uint8_t y = 0; y < HEIGHT; ++y) {
                if (!frontier.contains(x, y)) continue;
                Point point{x, y};
                int distance = absolute(point.x - seed.x) + absolute(point.y - seed.y);
                int score = cardinalNeighborCount(point, patch) * 100 +
                            localOpenness(point, component, 1) * 4 - distance;
                if (score != bestScore) continue;
                if (choice-- == 0) {
                    selected = point;
                    found = true;
                    break;
                }
            }
        }
        patch.add(selected.x, selected.y);
        frontier.remove(selected.x, selected.y);
        addCardinalNeighbors(selected, component, patch, frontier);
    }
}

bool groupIntersects(const PointGroup& group, const CellMask& cells) {
    for (uint8_t i = 0; i < group.count; ++i) {
        if (cells.contains(group.points[i].x, group.points[i].y)) return true;
    }
    return false;
}

bool groupSubset(const PointGroup& group, const CellMask& cells) {
    for (uint8_t i = 0; i < group.count; ++i) {
        if (!cells.contains(group.points[i].x, group.points[i].y)) return false;
    }
    return true;
}

void addGroup(const PointGroup& group, CellMask& cells) {
    for (uint8_t i = 0; i < group.count; ++i) cells.add(group.points[i].x, group.points[i].y);
}

int groupEdgeMargin(const PointGroup& group) {
    int margin = WIDTH + HEIGHT;
    for (uint8_t i = 0; i < group.count; ++i) margin = minimum(margin, edgeMargin(group.points[i]));
    return margin;
}

int minimumDistance(const PointGroup& group, const CellMask& cells) {
    if (group.count == 0 || cells.empty()) return WIDTH + HEIGHT;
    int distance = WIDTH + HEIGHT;
    for (uint8_t i = 0; i < group.count; ++i) {
        for (uint8_t y = 0; y < HEIGHT; ++y) {
            for (uint8_t x = 0; x < WIDTH; ++x) {
                if (!cells.contains(x, y)) continue;
                int value = absolute(group.points[i].x - x) + absolute(group.points[i].y - y);
                distance = minimum(distance, value);
            }
        }
    }
    return distance;
}

template <typename Visitor>
void forEachShrubCandidate(const CellMask& denseGrass, const CellMask& forbidden,
                           const CellMask& road, Visitor visitor) {
    for (int length = 4; length >= 2; --length) {
        for (uint8_t orientation = 0; orientation < 2; ++orientation) {
            bool horizontal = orientation == 0;
            int width = horizontal ? length : 1;
            int height = horizontal ? 1 : length;
            for (int top = 1; top < HEIGHT - height; ++top) {
                for (int left = 1; left < WIDTH - width; ++left) {
                    PointGroup group;
                    group.count = static_cast<uint8_t>(length);
                    for (int offset = 0; offset < length; ++offset) {
                        group.points[offset] = {
                            static_cast<uint8_t>(horizontal ? left + offset : left),
                            static_cast<uint8_t>(horizontal ? top : top + offset),
                        };
                    }
                    if (groupIntersects(group, forbidden)) continue;
                    bool firstSide = true;
                    bool secondSide = true;
                    for (uint8_t i = 0; i < group.count; ++i) {
                        const Point& point = group.points[i];
                        if (horizontal) {
                            firstSide &= denseGrass.contains(point.x, point.y - 1);
                            secondSide &= denseGrass.contains(point.x, point.y + 1);
                        } else {
                            firstSide &= denseGrass.contains(point.x - 1, point.y);
                            secondSide &= denseGrass.contains(point.x + 1, point.y);
                        }
                    }
                    if (!firstSide && !secondSide) continue;
                    int roadDistance = minimumDistance(group, road);
                    int score = length * 100 - minimum(roadDistance, 8) * 3 + groupEdgeMargin(group);
                    visitor(group, score);
                }
            }
        }
    }
}

void stampBoundaryShrubs(Map& map, Rng& rng, const CellMask& denseGrass,
                         const CellMask& road, const CellMask& water,
                         const CellMask& forest, CellMask& shrubs) {
    shrubs.clear();
    CellMask forbidden = denseGrass;
    forbidden.unite(water);
    forbidden.unite(forest);
    forbidden.unite(road);
    int bestScore = -32768;
    forEachShrubCandidate(denseGrass, forbidden, road,
        [&](const PointGroup&, int score) { if (score > bestScore) bestScore = score; });
    if (bestScore == -32768) return;
    uint16_t optionCount = 0;
    forEachShrubCandidate(denseGrass, forbidden, road,
        [&](const PointGroup&, int score) { if (score >= bestScore - 3) ++optionCount; });
    uint16_t choice = static_cast<uint16_t>(rng.bounded(optionCount));
    PointGroup selected;
    bool found = false;
    forEachShrubCandidate(denseGrass, forbidden, road,
        [&](const PointGroup& group, int score) {
            if (found || score < bestScore - 3) return;
            if (choice-- == 0) {
                selected = group;
                found = true;
            }
        });
    if (!found) return;
    addGroup(selected, shrubs);
    for (uint8_t i = 0; i < selected.count; ++i) {
        const Point& point = selected.points[i];
        map.layers[1][point.y * WIDTH + point.x] = SHRUB_TILE;
    }
}

template <typename Visitor>
void forEachFlowerCandidate(const CellMask& denseGrass, const CellMask& halo,
                            const CellMask& forbidden, const CellMask& road,
                            Visitor visitor) {
    for (const FlowerShape& shape : FLOWER_SHAPES) {
        for (uint8_t top = 0; top <= HEIGHT - shape.height; ++top) {
            for (uint8_t left = 0; left <= WIDTH - shape.width; ++left) {
                PointGroup group;
                group.count = shape.count;
                for (uint8_t i = 0; i < shape.count; ++i) {
                    group.points[i] = {
                        static_cast<uint8_t>(left + shape.points[i].x),
                        static_cast<uint8_t>(top + shape.points[i].y),
                    };
                }
                if (groupIntersects(group, forbidden) || !groupSubset(group, halo)) continue;
                uint8_t adjacency = 0;
                for (uint8_t i = 0; i < group.count; ++i) {
                    adjacency += cardinalNeighborCount(group.points[i], denseGrass);
                }
                if (adjacency == 0) continue;
                int roadDistance = minimumDistance(group, road);
                int score = adjacency * 30 + minimum(roadDistance, 6) * 2 +
                            groupEdgeMargin(group);
                visitor(group, score);
            }
        }
    }
}

bool chooseFlowerGroup(Rng& rng, const CellMask& denseGrass, const CellMask& halo,
                       const CellMask& forbidden, const CellMask& road,
                       PointGroup& selected) {
    int bestScore = -32768;
    forEachFlowerCandidate(denseGrass, halo, forbidden, road,
        [&](const PointGroup&, int score) { if (score > bestScore) bestScore = score; });
    if (bestScore == -32768) return false;
    uint16_t optionCount = 0;
    forEachFlowerCandidate(denseGrass, halo, forbidden, road,
        [&](const PointGroup&, int score) { if (score >= bestScore - 4) ++optionCount; });
    uint16_t choice = static_cast<uint16_t>(rng.bounded(optionCount));
    bool found = false;
    forEachFlowerCandidate(denseGrass, halo, forbidden, road,
        [&](const PointGroup& group, int score) {
            if (found || score < bestScore - 4) return;
            if (choice-- == 0) {
                selected = group;
                found = true;
            }
        });
    return found;
}

void stampBoundaryFlowers(Map& map, Rng& rng, const CellMask& denseGrass,
                          const CellMask& shrubs, const CellMask& road,
                          const CellMask& water, const CellMask& forest,
                          uint8_t groupCount) {
    CellMask halo = expandedMask(denseGrass, 2);
    halo.subtract(denseGrass);
    CellMask strictForbidden = denseGrass;
    strictForbidden.unite(water);
    strictForbidden.unite(forest);
    strictForbidden.unite(expandedMask(road, 1));
    strictForbidden.unite(expandedMask(shrubs, 1));
    CellMask relaxedForbidden = denseGrass;
    relaxedForbidden.unite(shrubs);
    relaxedForbidden.unite(water);
    relaxedForbidden.unite(forest);
    relaxedForbidden.unite(road);

    CellMask flowers;
    flowers.clear();
    for (uint8_t groupIndex = 0; groupIndex < groupCount; ++groupIndex) {
        CellMask flowerClearance = expandedMask(flowers, 2);
        CellMask forbidden = strictForbidden;
        forbidden.unite(flowerClearance);
        PointGroup selected;
        if (!chooseFlowerGroup(rng, denseGrass, halo, forbidden, road, selected)) {
            forbidden = relaxedForbidden;
            forbidden.unite(flowerClearance);
            if (!chooseFlowerGroup(rng, denseGrass, halo, forbidden, road, selected)) break;
        }
        addGroup(selected, flowers);
        for (uint8_t i = 0; i < selected.count; ++i) {
            const Point& point = selected.points[i];
            map.layers[0][point.y * WIDTH + point.x] = FLOWER_TILE;
        }
    }
}

void stampGroundDecorations(Map& map, Rng& rng, const CellMask& road,
                            const CellMask& water, const CellMask& forest,
                            const AreaProfile& profile) {
    uint8_t requestedGrass = profile.grassMinimum + rng.bounded(profile.grassVariance + 1U);
    CellMask available;
    available.clear();
    for (uint8_t y = 0; y < HEIGHT; ++y) {
        for (uint8_t x = 0; x < WIDTH; ++x) {
            if (!road.contains(x, y) && !water.contains(x, y) && !forest.contains(x, y)) {
                available.add(x, y);
            }
        }
    }
    CellMask denseGrass;
    growCompactPatch(rng, available, requestedGrass, denseGrass);
    for (uint8_t y = 0; y < HEIGHT; ++y) {
        for (uint8_t x = 0; x < WIDTH; ++x) {
            if (denseGrass.contains(x, y)) map.layers[0][y * WIDTH + x] = DENSE_GRASS_TILE;
        }
    }
    CellMask shrubs;
    stampBoundaryShrubs(map, rng, denseGrass, road, water, forest, shrubs);
    stampBoundaryFlowers(map, rng, denseGrass, shrubs, road, water, forest,
                         profile.flowerGroups);
}

void stampSnowDecorations(Map& map, Rng& rng, const CellMask& road,
                          CellMask& scenery) {
    uint8_t groundTarget = 10 + rng.bounded(7);
    uint8_t groundPlaced = 0;
    for (uint16_t attempt = 0; attempt < groundTarget * 16U && groundPlaced < groundTarget;
         ++attempt) {
        uint8_t x = rng.bounded(WIDTH);
        uint8_t y = rng.bounded(HEIGHT);
        if (road.contains(x, y) || scenery.contains(x, y)) continue;
        map.layers[0][y * WIDTH + x] =
            SNOW_GROUND_DECOR_TILES[
                rng.bounded(sizeof(SNOW_GROUND_DECOR_TILES) /
                            sizeof(SNOW_GROUND_DECOR_TILES[0]))];
        scenery.add(x, y);
        ++groundPlaced;
    }

    CellMask roadHalo = expandedMask(road, 1);
    uint8_t sceneryTarget = 6 + rng.bounded(5);
    uint8_t sceneryPlaced = 0;
    for (uint16_t attempt = 0;
         attempt < sceneryTarget * 20U && sceneryPlaced < sceneryTarget; ++attempt) {
        uint8_t x = rng.bounded(WIDTH);
        uint8_t y = rng.bounded(HEIGHT);
        if (roadHalo.contains(x, y) || scenery.contains(x, y)) continue;
        map.layers[1][y * WIDTH + x] =
            SNOW_SCENERY_TILES[
                rng.bounded(sizeof(SNOW_SCENERY_TILES) /
                            sizeof(SNOW_SCENERY_TILES[0]))];
        scenery.add(x, y);
        ++sceneryPlaced;
    }
}

Point transformFrostPoint(Point point, bool mirrorX, bool mirrorY) {
    if (mirrorX) point.x = WIDTH - 1 - point.x;
    if (mirrorY) point.y = HEIGHT - 1 - point.y;
    return point;
}

Edge transformFrostEdge(Edge edge, bool mirrorX, bool mirrorY) {
    if (mirrorX) {
        if (edge == Edge::LEFT) edge = Edge::RIGHT;
        else if (edge == Edge::RIGHT) edge = Edge::LEFT;
    }
    if (mirrorY) {
        if (edge == Edge::TOP) edge = Edge::BOTTOM;
        else if (edge == Edge::BOTTOM) edge = Edge::TOP;
    }
    return edge;
}

Endpoint transformFrostEndpoint(const Endpoint& endpoint, bool mirrorX, bool mirrorY) {
    Endpoint transformed;
    transformed.point = transformFrostPoint(endpoint.point, mirrorX, mirrorY);
    if (mirrorX && (endpoint.edge == Edge::TOP || endpoint.edge == Edge::BOTTOM)) {
        --transformed.point.x;
    }
    if (mirrorY && (endpoint.edge == Edge::LEFT || endpoint.edge == Edge::RIGHT)) {
        --transformed.point.y;
    }
    transformed.edge = transformFrostEdge(endpoint.edge, mirrorX, mirrorY);
    return transformed;
}

void addFrostRect(CellMask& mask, const FrostRect& rect, bool mirrorX, bool mirrorY) {
    for (uint8_t y = rect.top; y <= rect.bottom; ++y) {
        for (uint8_t x = rect.left; x <= rect.right; ++x) {
            Point point = transformFrostPoint({x, y}, mirrorX, mirrorY);
            mask.add(point.x, point.y);
        }
    }
}

bool buildFrostPath(Path& path, const Point* controls, uint8_t controlCount,
                    const Endpoint& exit, bool mirrorX, bool mirrorY) {
    path = {};
    path.exit = exit;
    if (!controls || controlCount == 0) return false;
    Point first = transformFrostPoint(controls[0], mirrorX, mirrorY);
    if (!appendPoint(path, first.x, first.y)) return false;
    for (uint8_t i = 1; i < controlCount; ++i) {
        Point target = transformFrostPoint(controls[i], mirrorX, mirrorY);
        if (!appendLine(path, target.x, target.y)) return false;
    }
    if (!appendLine(path, exit.point.x, exit.point.y)) return false;
    return path.pointCount >= 2;
}

bool frostEndpointContains(const Endpoint& endpoint, uint8_t x, uint8_t y) {
    if (endpoint.edge == Edge::TOP || endpoint.edge == Edge::BOTTOM) {
        return y == endpoint.point.y &&
               (x == endpoint.point.x || x == endpoint.point.x + 1);
    }
    return x == endpoint.point.x &&
           (y == endpoint.point.y || y == endpoint.point.y + 1);
}

bool frostPortalOpening(const Map& map, uint8_t x, uint8_t y, Edge edge) {
    if (map.entry.edge == edge && frostEndpointContains(map.entry, x, y)) return true;
    for (uint8_t i = 0; i < map.pathCount; ++i) {
        if (map.paths[i].exit.edge == edge &&
            frostEndpointContains(map.paths[i].exit, x, y)) return true;
    }
    return false;
}

bool frostEndpointFits(const Endpoint& endpoint, const CellMask& floor) {
    if (!floor.contains(endpoint.point.x, endpoint.point.y)) return false;
    if (endpoint.edge == Edge::TOP || endpoint.edge == Edge::BOTTOM) {
        return floor.contains(endpoint.point.x + 1, endpoint.point.y);
    }
    return floor.contains(endpoint.point.x, endpoint.point.y + 1);
}

bool frostBoundaryTile(const Map& map, const CellMask& floor,
                       uint8_t x, uint8_t y, uint16_t& tileId) {
    constexpr uint8_t OUT_TOP = 1;
    constexpr uint8_t OUT_RIGHT = 2;
    constexpr uint8_t OUT_BOTTOM = 4;
    constexpr uint8_t OUT_LEFT = 8;
    uint8_t outside = 0;
    if (!floor.contains(x, y - 1) && !frostPortalOpening(map, x, y, Edge::TOP)) {
        outside |= OUT_TOP;
    }
    if (!floor.contains(x + 1, y) && !frostPortalOpening(map, x, y, Edge::RIGHT)) {
        outside |= OUT_RIGHT;
    }
    if (!floor.contains(x, y + 1) && !frostPortalOpening(map, x, y, Edge::BOTTOM)) {
        outside |= OUT_BOTTOM;
    }
    if (!floor.contains(x - 1, y) && !frostPortalOpening(map, x, y, Edge::LEFT)) {
        outside |= OUT_LEFT;
    }
    switch (outside) {
    case 0: tileId = 0; return true;
    case OUT_TOP: tileId = FROST_WALL_TOP_TILE; return true;
    case OUT_RIGHT: tileId = FROST_WALL_RIGHT_TILE; return true;
    case OUT_BOTTOM: tileId = FROST_WALL_BOTTOM_TILE; return true;
    case OUT_LEFT: tileId = FROST_WALL_LEFT_TILE; return true;
    case OUT_TOP | OUT_LEFT: tileId = FROST_WALL_TOP_LEFT_TILE; return true;
    case OUT_TOP | OUT_RIGHT: tileId = FROST_WALL_TOP_RIGHT_TILE; return true;
    case OUT_BOTTOM | OUT_LEFT: tileId = FROST_WALL_BOTTOM_LEFT_TILE; return true;
    case OUT_BOTTOM | OUT_RIGHT: tileId = FROST_WALL_BOTTOM_RIGHT_TILE; return true;
    default: return false;
    }
}

uint16_t frostIceTile(const CellMask& ice, uint8_t x, uint8_t y) {
    bool north = !ice.contains(x, y - 1);
    bool east = !ice.contains(x + 1, y);
    bool south = !ice.contains(x, y + 1);
    bool west = !ice.contains(x - 1, y);
    if (north && west) return FROST_ICE_TOP_LEFT_TILE;
    if (north && east) return FROST_ICE_TOP_RIGHT_TILE;
    if (south && west) return FROST_ICE_BOTTOM_LEFT_TILE;
    if (south && east) return FROST_ICE_BOTTOM_RIGHT_TILE;
    if (north) return FROST_ICE_TOP_TILE;
    if (south) return FROST_ICE_BOTTOM_TILE;
    if (west) return FROST_ICE_LEFT_TILE;
    if (east) return FROST_ICE_RIGHT_TILE;
    return FROST_ICE_CENTER_TILE;
}

void stampFrostPortal(Map& map, const Endpoint& endpoint, const Point& routeAnchor,
                      const CellMask& route) {
    uint8_t x = routeAnchor.x;
    uint8_t y = routeAnchor.y;
    if (endpoint.edge == Edge::TOP && x > 0 && x + 1 < WIDTH &&
        !route.contains(x - 1, y) && !route.contains(x + 1, y)) {
        for (uint8_t offset = 0; offset < 3; ++offset) {
            map.layers[1][y * WIDTH + x + offset - 1] =
                ExploreCaveTiles::FROST_EXIT[offset];
        }
    } else if (endpoint.edge == Edge::BOTTOM) {
        map.layers[1][y * WIDTH + x] =
            ExploreCaveTiles::FROST_DOWNWARD_STAIRS;
    }
}

void stampFrostPortals(Map& map, const CellMask& route) {
    if (map.pathCount == 0 || map.paths[0].pointCount == 0) return;
    stampFrostPortal(map, map.entry, map.paths[0].points[0], route);
    for (uint8_t i = 0; i < map.pathCount; ++i) {
        const Path& path = map.paths[i];
        if (path.pointCount == 0) continue;
        stampFrostPortal(map, path.exit, path.points[path.pointCount - 1], route);
    }
}

bool generateFrostCave(uint32_t seed, Edge entryEdge, Map& out) {
    bool horizontal = entryEdge == Edge::LEFT || entryEdge == Edge::RIGHT;
    uint8_t variant = static_cast<uint8_t>((seed >> 5) & 1U);
    const FrostTemplate& spec = horizontal
        ? FROST_HORIZONTAL_TEMPLATES[variant]
        : FROST_VERTICAL_TEMPLATES[variant];
    bool mirrorX = entryEdge == Edge::RIGHT;
    bool mirrorY = entryEdge == Edge::TOP;

    out.seed = seed;
    out.areaIndex = FROST_CRYSTAL_CAVE_AREA;
    out.entry = transformFrostEndpoint(spec.entry, mirrorX, mirrorY);
    out.junction = transformFrostPoint(spec.junction, mirrorX, mirrorY);
    out.pathCount = PATH_COUNT;
    for (uint8_t i = 0; i < PATH_COUNT; ++i) {
        Endpoint exit = transformFrostEndpoint(spec.exits[i], mirrorX, mirrorY);
        if (!buildFrostPath(out.paths[i], spec.routeControls[i],
                            spec.routeControlCounts[i], exit, mirrorX, mirrorY)) {
            return false;
        }
    }

    CellMask floor;
    CellMask walls;
    CellMask ice;
    CellMask route;
    CellMask blocked;
    floor.clear();
    walls.clear();
    ice.clear();
    route.clear();
    blocked.clear();

    for (uint8_t i = 0; i < spec.rectCount; ++i) {
        addFrostRect(floor, spec.rects[i], mirrorX, mirrorY);
    }
    if (!frostEndpointFits(out.entry, floor)) return false;
    for (uint8_t i = 0; i < PATH_COUNT; ++i) {
        if (!frostEndpointFits(out.paths[i].exit, floor)) return false;
        const Path& path = out.paths[i];
        for (uint8_t p = 0; p < path.pointCount; ++p) {
            route.add(path.points[p].x, path.points[p].y);
        }
    }

    for (uint16_t index = 0; index < CELL_COUNT; ++index) {
        out.layers[0][index] = FROST_OUTSIDE_TILE;
    }
    for (uint8_t y = 0; y < HEIGHT; ++y) {
        for (uint8_t x = 0; x < WIDTH; ++x) {
            if (floor.contains(x, y)) out.layers[0][y * WIDTH + x] = FROST_FLOOR_TILE;
        }
    }

    for (uint8_t y = 0; y < HEIGHT; ++y) {
        for (uint8_t x = 0; x < WIDTH; ++x) {
            if (!floor.contains(x, y)) continue;
            uint16_t tileId = 0;
            if (!frostBoundaryTile(out, floor, x, y, tileId)) return false;
            if (tileId) {
                out.layers[1][y * WIDTH + x] = tileId;
                walls.add(x, y);
            }
        }
    }

    for (uint8_t y = 0; y < HEIGHT; ++y) {
        for (uint8_t x = 0; x < WIDTH; ++x) {
            uint16_t index = y * WIDTH + x;
            if (!floor.contains(x, y) || out.layers[1][index] != 0) continue;
            bool outsideNw = !floor.contains(x - 1, y - 1) &&
                             floor.contains(x - 1, y) && floor.contains(x, y - 1);
            bool outsideNe = !floor.contains(x + 1, y - 1) &&
                             floor.contains(x + 1, y) && floor.contains(x, y - 1);
            bool outsideSw = !floor.contains(x - 1, y + 1) &&
                             floor.contains(x - 1, y) && floor.contains(x, y + 1);
            bool outsideSe = !floor.contains(x + 1, y + 1) &&
                             floor.contains(x + 1, y) && floor.contains(x, y + 1);
            uint8_t matches = static_cast<uint8_t>(outsideNw) +
                              static_cast<uint8_t>(outsideNe) +
                              static_cast<uint8_t>(outsideSw) +
                              static_cast<uint8_t>(outsideSe);
            if (matches > 1) return false;
            if (outsideNw) out.layers[1][index] = FROST_INNER_OUTSIDE_NW_TILE;
            else if (outsideNe) out.layers[1][index] = FROST_INNER_OUTSIDE_NE_TILE;
            else if (outsideSw) out.layers[1][index] = FROST_INNER_OUTSIDE_SW_TILE;
            else if (outsideSe) out.layers[1][index] = FROST_INNER_OUTSIDE_SE_TILE;
            if (out.layers[1][index]) walls.add(x, y);
        }
    }
    blocked.unite(walls);

    stampFrostPortals(out, route);

    for (uint8_t i = 0; i < spec.iceRectCount; ++i) {
        addFrostRect(ice, spec.iceRects[i], mirrorX, mirrorY);
    }
    for (uint8_t y = 0; y < HEIGHT; ++y) {
        for (uint8_t x = 0; x < WIDTH; ++x) {
            if (!ice.contains(x, y)) continue;
            if (!floor.contains(x, y) || walls.contains(x, y)) return false;
            out.layers[0][y * WIDTH + x] = frostIceTile(ice, x, y);
        }
    }

    if (spec.hasRockHill) {
        Point first = transformFrostPoint(spec.rockHill, mirrorX, mirrorY);
        Point last = transformFrostPoint(
            {static_cast<uint8_t>(spec.rockHill.x + 2),
             static_cast<uint8_t>(spec.rockHill.y + 2)},
            mirrorX, mirrorY);
        uint8_t left = first.x < last.x ? first.x : last.x;
        uint8_t top = first.y < last.y ? first.y : last.y;
        for (uint8_t row = 0; row < 3; ++row) {
            for (uint8_t column = 0; column < 3; ++column) {
                uint8_t x = left + column;
                uint8_t y = top + row;
                if (!floor.contains(x, y) || blocked.contains(x, y) ||
                    route.contains(x, y)) return false;
                out.layers[1][y * WIDTH + x] = FROST_ROCK_HILL_TILES[row][column];
                blocked.add(x, y);
            }
        }
    }

    for (uint8_t i = 0; i < spec.crystalCount; ++i) {
        Point point = transformFrostPoint(spec.crystals[i].point, mirrorX, mirrorY);
        if (point.y == 0 || !floor.contains(point.x, point.y) ||
            !floor.contains(point.x, point.y - 1) ||
            blocked.contains(point.x, point.y) || route.contains(point.x, point.y)) {
            return false;
        }
        out.layers[1][point.y * WIDTH + point.x] = spec.crystals[i].tileId;
        out.layers[2][(point.y - 1) * WIDTH + point.x] = FROST_CRYSTAL_TOP_TILE;
        blocked.add(point.x, point.y);
    }

    for (uint8_t i = 0; i < spec.boulderCount; ++i) {
        Point point = transformFrostPoint(spec.boulders[i].point, mirrorX, mirrorY);
        if (!floor.contains(point.x, point.y) || blocked.contains(point.x, point.y) ||
            route.contains(point.x, point.y)) return false;
        out.layers[1][point.y * WIDTH + point.x] = spec.boulders[i].tileId;
        blocked.add(point.x, point.y);
    }

    for (uint8_t i = 0; i < out.pathCount; ++i) {
        const Path& path = out.paths[i];
        if (!ExploreIceSlide::routeCrossesIceStraight(out, path)) return false;
        for (uint8_t p = 0; p < path.pointCount; ++p) {
            const Point& point = path.points[p];
            if (!floor.contains(point.x, point.y) || blocked.contains(point.x, point.y)) {
                return false;
            }
        }
    }
    return true;
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

bool generate(uint32_t seed, Edge entryEdge, uint8_t areaIndex, Map& out) {
    std::memset(&out, 0, sizeof(out));
    out.seed = seed;
    out.areaIndex = areaIndex;
    Rng terrain(seed ^ 0x63D83595U);
    Rng features(seed ^ 0xC2B2AE35U);
    const AreaProfile& profile = profileFor(areaIndex);
    bool hasSnow = areaIndex == FROST_CRYSTAL_CAVE_AREA;
    if (hasSnow) return generateFrostCave(seed, entryEdge, out);

    CellMask road;
    road.clear();
    CreekSpec creek{};
    bool hasCreek = false;
    bool hasWaterfall = areaIndex == ANCIENT_WATERFALL_VALLEY_AREA;
    if (hasWaterfall) {
        if (!buildAncientWaterfallTopology(entryEdge, out, road)) return false;
    } else if (areaIndex == CREEK_BRIDGE_SLOPE_AREA) {
        for (uint8_t attempt = 0; attempt < 16; ++attempt) {
            uint32_t topologySeed = seed ^ (0x9E3779B9U * (attempt + 1U));
            if (!buildTopology(topologySeed, entryEdge, 0, out, road)) continue;
            uint16_t count = creekCandidateCount(road);
            if (count == 0) continue;
            if (!selectCreekCandidate(road, features.bounded(count), creek)) return false;
            hasCreek = true;
            break;
        }
        if (!hasCreek) return false;
    } else if (areaIndex == MIST_FOREST_PATH_AREA) {
        CellMask emptyWater;
        emptyWater.clear();
        bool hasForestCandidate = false;
        for (uint8_t attempt = 0; attempt < 16; ++attempt) {
            uint32_t topologySeed =
                attempt == 0 ? seed : seed ^ (0x9E3779B9U * attempt);
            if (!buildTopology(topologySeed, entryEdge, profile.coastChance, out, road)) continue;
            for (const ForestSpec& spec : FOREST_SPECS) {
                if (!forestFits(spec, road, emptyWater)) continue;
                hasForestCandidate = true;
                break;
            }
            if (hasForestCandidate) break;
        }
        if (!hasForestCandidate) return false;
    } else if (!buildTopology(seed, entryEdge, profile.coastChance, out, road)) {
        return false;
    }
    out.seed = seed;
    out.areaIndex = areaIndex;
    out.hasCreek = hasCreek;
    out.hasWaterfall = hasWaterfall;

    CellMask water;
    CellMask forest;
    CellMask cliff;
    CellMask stairs;
    CellMask bridge;
    CellMask transitionCells;
    CellMask scenery;
    water.clear();
    forest.clear();
    cliff.clear();
    stairs.clear();
    bridge.clear();
    transitionCells.clear();
    scenery.clear();

    for (uint16_t index = 0; index < CELL_COUNT; ++index) {
        if (hasSnow) {
            out.layers[0][index] = SNOW_GROUND_TILES[
                terrain.bounded(sizeof(SNOW_GROUND_TILES) / sizeof(SNOW_GROUND_TILES[0]))];
        } else {
            out.layers[0][index] =
                GRASS_TILES[terrain.bounded(sizeof(GRASS_TILES) / sizeof(GRASS_TILES[0]))];
        }
    }
    if (out.hasCoast) stampCoast(out, water);

    if (hasWaterfall) {
        stampAncientWaterfallValley(out, water, forest, cliff, stairs, scenery);
        out.hasForest = true;
        out.hasCliff = true;
    }

    if (hasCreek) {
        StreamShape shape = chooseStreamShape(features, road, creek);
        stampStream(out, shape, water, transitionCells);
        stampBridge(out, creek, bridge);
        if (features.bounded(100) < 60) {
            uint16_t count = cliffCandidateCount(road, water, bridge, shape);
            if (count > 0) {
                CliffSpec spec{};
                if (!selectCliffCandidate(
                        road, water, bridge, shape, features.bounded(count), spec)) return false;
                stampCliff(out, spec, cliff, stairs);
                out.hasCliff = true;
            }
        }
        stampWaterRocks(out, features, water, road, bridge, transitionCells);
    }

    for (uint8_t y = 0; y < HEIGHT; ++y) {
        for (uint8_t x = 0; x < WIDTH; ++x) {
            if (road.contains(x, y) && !water.contains(x, y) &&
                !cliff.contains(x, y) && !forest.contains(x, y)) {
                out.layers[0][y * WIDTH + x] =
                    hasSnow ? SNOW_PATH_TILE : roadTile(out, road, x, y);
            }
        }
    }

    if (!hasCreek && !hasWaterfall && !hasSnow &&
        terrain.bounded(100) < profile.forestChance) {
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

    CellMask decorationBlocked = forest;
    decorationBlocked.unite(cliff);
    decorationBlocked.unite(scenery);
    if (hasSnow) {
        stampSnowDecorations(out, terrain, road, scenery);
    } else {
        stampGroundDecorations(out, terrain, road, water, decorationBlocked, profile);
    }

    for (uint8_t pathIndex = 0; pathIndex < out.pathCount; ++pathIndex) {
        const Path& path = out.paths[pathIndex];
        if (path.pointCount < 2) return false;
        for (uint8_t pointIndex = 0; pointIndex < path.pointCount; ++pointIndex) {
            const Point& point = path.points[pointIndex];
            bool blockedWater = water.contains(point.x, point.y) &&
                                !bridge.contains(point.x, point.y);
            bool blockedCliff = cliff.contains(point.x, point.y) &&
                                !stairs.contains(point.x, point.y);
            if (!road.contains(point.x, point.y) || blockedWater || blockedCliff ||
                forest.contains(point.x, point.y)) return false;
        }
    }
    return true;
}

uint32_t fingerprint(const Map& map) {
    uint32_t hash = 2166136261U;
    hash = fnvWord(hash, ALGORITHM_VERSION);
    hash = fnvByte(hash, map.areaIndex);
    hash = fnvByte(hash, static_cast<uint8_t>(map.entry.edge));
    hash = fnvByte(hash, map.entry.point.x);
    hash = fnvByte(hash, map.entry.point.y);
    hash = fnvByte(hash, map.junction.x);
    hash = fnvByte(hash, map.junction.y);
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
    hash = fnvByte(hash, map.hasCreek ? 1 : 0);
    hash = fnvByte(hash, map.hasCliff ? 1 : 0);
    hash = fnvByte(hash, map.hasWaterfall ? 1 : 0);
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
    case SNOW_PATH_TILE: case FROST_FLOOR_TILE:
        return true;
    default:
        return false;
    }
}

bool isWaterTile(uint16_t tileId) {
    switch (tileId) {
    case SEA_WATER_TILE: case SEA_LEFT_SHORE_TILE: case SEA_TOP_SHORE_TILE:
    case SEA_TOP_RIGHT_CORNER_TILE: case SEA_TOP_LEFT_CORNER_TILE:
    case DEEP_SEA_TILE: case DEEP_SEA_EDGE_TILE: case SEA_SHORE_TILE:
    case WATERFALL_CREST_TILES[0]: case WATERFALL_CREST_TILES[1]:
    case WATERFALL_CREST_TILES[2]:
    case WATERFALL_BODY_TOP_TILES[0]: case WATERFALL_BODY_TOP_TILES[1]:
    case WATERFALL_BODY_TOP_TILES[2]:
    case WATERFALL_BODY_MIDDLE_TILES[0]: case WATERFALL_BODY_MIDDLE_TILES[1]:
    case WATERFALL_BODY_MIDDLE_TILES[2]:
    case WATERFALL_BODY_LOWER_TILES[0]: case WATERFALL_BODY_LOWER_TILES[1]:
    case WATERFALL_BODY_LOWER_TILES[2]: case WATERFALL_BOTTOM_TILE:
    case STREAM_LEFT_TILE: case STREAM_CENTER_TILE: case STREAM_RIGHT_TILE:
    case STREAM_TOP_OUTER_LEFT_TILE: case STREAM_TOP_OUTER_RIGHT_TILE:
    case STREAM_TOP_INNER_LEFT_TILE: case STREAM_TOP_INNER_RIGHT_TILE:
    case STREAM_BOTTOM_OUTER_LEFT_TILE: case STREAM_BOTTOM_OUTER_RIGHT_TILE:
    case STREAM_BOTTOM_INNER_LEFT_TILE: case STREAM_BOTTOM_INNER_RIGHT_TILE:
        return true;
    default:
        return false;
    }
}

bool isForestTile(uint16_t tileId) {
    switch (tileId) {
    case 800: case 801: case 802: case 804: case 805: case 808: case 809:
    case 810: case 811: case 818: case 819: case FENCE_TOP_LEFT: case FENCE_LEFT:
    case FENCE_TOP: case FENCE_TOP_RIGHT: case SHRUB_TILE:
        return true;
    default:
        return false;
    }
}

}  // namespace ExploreMapGenerator
