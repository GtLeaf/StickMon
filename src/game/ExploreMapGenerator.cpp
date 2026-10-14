#include "game/ExploreMapGenerator.h"

#include "game/ExploreCaveTiles.h"
#include "game/ExploreIceSlide.h"

#include <cstring>

namespace ExploreMapGenerator {
namespace {

constexpr uint16_t GRASS_TILES[] = {385, 385, 385, 386, 387, 388, 389};
constexpr uint16_t DENSE_GRASS_TILE = 390;
constexpr uint16_t TALL_GRASS_TOP_TILE = 391;
constexpr uint16_t TALL_GRASS_BASE_TILE = 399;
constexpr uint16_t PARK_BOUNDARY_TREE_TILES[3][2] = {
    {800, 801}, {808, 809}, {816, 817},
};
constexpr uint16_t PARK_TREE_TILES[3][2] = {
    {804, 805}, {812, 813}, {820, 821},
};
constexpr uint16_t PARK_CENTERED_TREE_TILES[3][3] = {
    {824, 825, 826}, {832, 833, 834}, {840, 841, 842},
};
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

constexpr uint16_t CAVE_FLOOR_TILE = ExploreCaveTiles::CAVE_FLOOR;
constexpr uint16_t CAVE_CLIFF_TILES[3] = {
    ExploreCaveTiles::CAVE_CLIFF_LEFT,
    ExploreCaveTiles::CAVE_CLIFF_MIDDLE,
    ExploreCaveTiles::CAVE_CLIFF_RIGHT,
};

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
    {0, 0, 40, 10, 1},    // Tall grass park uses its own layered scenery pass.
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
    {0, 1, 6, 8}, {5, 4, 10, 7}, {9, 2, 15, 10}, {10, 8, 14, 11}, {1, 0, 5, 3},
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
        false,
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

Point transformParkPoint(Point point, bool mirrorX, bool mirrorY) {
    // Interior route anchors occupy the upper/left cell of a two-cell road.
    // Boundary endpoints must remain on the outermost cell after reflection.
    if (mirrorX) point.x = point.x == 0 ? WIDTH - 1
        : point.x == WIDTH - 1 ? 0 : WIDTH - 2 - point.x;
    if (mirrorY) point.y = point.y == 0 ? HEIGHT - 1
        : point.y == HEIGHT - 1 ? 0 : HEIGHT - 2 - point.y;
    return point;
}

Endpoint parkEndpoint(Point point) {
    Edge edge = point.y == 0 ? Edge::TOP : point.y == HEIGHT - 1 ? Edge::BOTTOM
        : point.x == 0 ? Edge::LEFT : Edge::RIGHT;
    return {point, edge};
}

bool validateParkTopology(const Map& map) {
    CellMask vertices;
    CellMask rightEdges;
    CellMask downEdges;
    vertices.clear();
    rightEdges.clear();
    downEdges.clear();
    for (uint8_t pathIndex = 0; pathIndex < map.pathCount; ++pathIndex) {
        const Path& path = map.paths[pathIndex];
        if (path.pointCount < 2 || !samePoint(path.points[0], map.entry.point) ||
            !samePoint(path.points[path.pointCount - 1], path.exit.point) ||
            path.exit.edge == map.entry.edge) return false;
        for (uint8_t other = 0; other < pathIndex; ++other) {
            if (map.paths[other].exit.edge == path.exit.edge) return false;
        }
        CellMask visited;
        visited.clear();
        for (uint8_t i = 0; i < path.pointCount; ++i) {
            Point point = path.points[i];
            if (point.x >= WIDTH || point.y >= HEIGHT || visited.contains(point.x, point.y))
                return false;
            visited.add(point.x, point.y);
            vertices.add(point.x, point.y);
            if (i == 0) continue;
            Point previous = path.points[i - 1];
            int dx = point.x - previous.x;
            int dy = point.y - previous.y;
            if ((dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy) != 1)
                return false;
            if (point.y == previous.y)
                rightEdges.add(point.x < previous.x ? point.x : previous.x, point.y);
            else downEdges.add(point.x, point.y < previous.y ? point.y : previous.y);
        }
    }
    // All paths start at one entry, so the graph is connected. V-1 edges excludes loops.
    if (rightEdges.count() + downEdges.count() + 1 != vertices.count()) return false;
    uint8_t forkCount = 0;
    uint8_t leafCount = 0;
    bool hasFirstJunction = map.pathCount == 1;
    for (uint8_t y = 0; y < HEIGHT; ++y) {
        for (uint8_t x = 0; x < WIDTH; ++x) {
            if (!vertices.contains(x, y)) continue;
            int degree = rightEdges.contains(x, y) + rightEdges.contains(x - 1, y) +
                         downEdges.contains(x, y) + downEdges.contains(x, y - 1);
            if (degree == 1) ++leafCount;
            if (degree == 3) {
                ++forkCount;
                if (samePoint({x, y}, map.junction)) hasFirstJunction = true;
            }
            if (degree < 1 || degree > 3) return false;
        }
    }
    return hasFirstJunction && forkCount + 1 == map.pathCount &&
           leafCount == map.pathCount + 1;
}

constexpr uint16_t PARK_TOPOLOGY_ATTEMPTS = 192;
constexpr uint8_t PARK_FALLBACK_ATTEMPTS = 32;
constexpr int PARK_DX[4] = {0, 1, 0, -1};
constexpr int PARK_DY[4] = {-1, 0, 1, 0};

bool parkAppendRun(Path& path, uint8_t heading, int distance,
                   uint8_t width, uint8_t height, bool terminal = false) {
    Point start = path.points[path.pointCount - 1];
    int endX = start.x + PARK_DX[heading] * distance;
    int endY = start.y + PARK_DY[heading] * distance;
    if (distance < 2 || endX < 0 || endX >= width || endY < 0 || endY >= height)
        return false;
    if (!terminal && (endX < 1 || endX > width - 3 || endY < 1 || endY > height - 3))
        return false;
    for (int step = 1; step <= distance; ++step) {
        Point point{static_cast<uint8_t>(start.x + PARK_DX[heading] * step),
                    static_cast<uint8_t>(start.y + PARK_DY[heading] * step)};
        for (uint8_t i = 0; i < path.pointCount; ++i)
            if (samePoint(path.points[i], point)) return false;
        if (!appendPoint(path, point.x, point.y)) return false;
    }
    return true;
}

bool parkFinishRun(Path& path, uint8_t heading, uint8_t width, uint8_t height) {
    Point point = path.points[path.pointCount - 1];
    int distances[4] = {point.y, width - 1 - point.x, height - 1 - point.y, point.x};
    return parkAppendRun(path, heading, distances[heading], width, height, true);
}

bool parkGrowBranch(Rng& rng, Point start, uint8_t heading, uint8_t edge,
                    uint8_t width, uint8_t height, Path& branch) {
    branch = {};
    appendPoint(branch, start.x, start.y);
    if (heading == edge && rng.bounded(2) == 0)
        return parkFinishRun(branch, edge, width, height);
    if (!parkAppendRun(branch, heading, 2 + rng.bounded(4), width, height)) return false;
    if (heading == edge || heading == (edge + 2) % 4) {
        heading = (heading + (rng.bounded(2) ? 1 : 3)) % 4;
        if (!parkAppendRun(branch, heading, 2 + rng.bounded(3), width, height)) return false;
    } else if (rng.bounded(2)) {
        if (!parkAppendRun(branch, edge, 2 + rng.bounded(3), width, height)) return false;
        if (!parkAppendRun(branch, heading, 2 + rng.bounded(3), width, height)) return false;
    }
    return parkFinishRun(branch, edge, width, height);
}

uint8_t parkHeading(Point a, Point b) {
    return a.x == b.x ? (b.y < a.y ? 0 : 2) : (b.x > a.x ? 1 : 3);
}

bool parkOwnsRoadCell(const uint8_t* graph, uint8_t vertex, int x, int y) {
    int vx = vertex % WIDTH;
    int vy = vertex / WIDTH;
    if (!graph[vertex]) return false;
    return (vx == x && vy == y) ||
           ((graph[vertex] & 5) && vx + 1 == x && vy == y) ||
           ((graph[vertex] & 10) && vx == x && vy + 1 == y);
}

bool parkRoadCellsAreNear(const uint8_t* graph, int x, int y, int nx, int ny) {
    const int owners[3][2] = {{x, y}, {x - 1, y}, {x, y - 1}};
    for (const auto& owner : owners) {
        if (owner[0] < 0 || owner[1] < 0) continue;
        uint8_t root = owner[1] * WIDTH + owner[0];
        if (!parkOwnsRoadCell(graph, root, x, y)) continue;
        // A degree-three tree has at most 22 vertices within three steps.
        uint8_t queue[32] = {root};
        uint8_t depths[32] = {};
        uint8_t head = 0, tail = 1;
        CellMask seen;
        seen.add(owner[0], owner[1]);
        while (head < tail) {
            uint8_t vertex = queue[head];
            uint8_t depth = depths[head++];
            if (parkOwnsRoadCell(graph, vertex, nx, ny)) return true;
            if (depth == 3) continue;
            int vx = vertex % WIDTH, vy = vertex / WIDTH;
            for (uint8_t dir = 0; dir < 4; ++dir) {
                if (!(graph[vertex] & (1 << dir))) continue;
                int nextX = vx + PARK_DX[dir], nextY = vy + PARK_DY[dir];
                if (seen.contains(nextX, nextY)) continue;
                if (tail >= 32) return false;
                seen.add(nextX, nextY);
                queue[tail] = nextY * WIDTH + nextX;
                depths[tail++] = depth + 1;
            }
        }
    }
    return false;
}

bool validateParkWideRoad(const Map& map, const CellMask& road) {
    uint8_t graph[CELL_COUNT] = {};
    for (uint8_t p = 0; p < map.pathCount; ++p) {
        const Path& path = map.paths[p];
        for (uint8_t i = 1; i < path.pointCount; ++i) {
            Point a = path.points[i - 1], b = path.points[i];
            uint8_t heading = parkHeading(a, b);
            graph[a.y * WIDTH + a.x] |= 1 << heading;
            graph[b.y * WIDTH + b.x] |= 1 << ((heading + 2) % 4);
        }
    }
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            if (!road.contains(x, y)) continue;
            if (road.contains(x + 1, y) && !parkRoadCellsAreNear(graph, x, y, x + 1, y))
                return false;
            if (road.contains(x, y + 1) && !parkRoadCellsAreNear(graph, x, y, x, y + 1))
                return false;
        }
    }
    return true;
}

Point orientParkPoint(Point point, uint8_t width, bool vertical, bool mirror, Edge edge) {
    if (mirror) point.x = point.x == 0 ? width - 1
        : point.x == width - 1 ? 0 : width - 2 - point.x;
    if (vertical) return transformParkPoint(point, false, edge == Edge::TOP);
    return transformParkPoint({point.y, point.x}, edge == Edge::LEFT, false);
}

bool parkTopologyCandidate(Rng& rng, Edge entryEdge, uint8_t type,
                           bool fallback, Map& out, CellMask& road) {
    bool vertical = entryEdge == Edge::TOP || entryEdge == Edge::BOTTOM;
    uint8_t width = vertical ? WIDTH : HEIGHT;
    uint8_t height = vertical ? HEIGHT : WIDTH;
    bool mirror = rng.bounded(2) != 0;
    Path main{};
    uint8_t entryX = 2 + rng.bounded(width - 5);
    appendPoint(main, entryX, height - 1);
    Path branches[2] = {};
    uint8_t forks[2] = {};
    if (fallback) {
        uint8_t x = 3 + rng.bounded(2);
        uint8_t middle = width - 6 + rng.bounded(2);
        uint8_t lower = height - 5 + rng.bounded(2);
        uint8_t upper = 3 + rng.bounded(2);
        main = {};
        appendLine(main, x, height - 1);
        appendLine(main, x, lower);
        appendLine(main, middle, lower);
        appendLine(main, middle, upper);
        appendLine(main, middle, 0);
        forks[0] = height - 1 - lower;
        forks[1] = forks[0] + middle - x + lower - upper;
        if (type) {
            appendPoint(branches[0], x, lower);
            appendLine(branches[0], 0, lower);
        }
        if (type == 2) {
            appendPoint(branches[1], middle, upper);
            appendLine(branches[1], width - 1, upper);
        }
    } else {
        constexpr uint8_t edges[3] = {0, 1, 3};
        uint8_t primary = type < 2 ? edges[rng.bounded(3)] : 0;
        uint8_t shifts = 1 + rng.bounded(2);
        for (uint8_t i = 0; i < shifts; ++i) {
            if (!parkAppendRun(main, 0, 2 + rng.bounded(4), width, height)) return false;
            uint8_t heading = rng.bounded(2) ? 1 : 3;
            if (!parkAppendRun(main, heading, 2 + rng.bounded(4), width, height)) return false;
        }
        if (primary && !parkAppendRun(main, 0, 2 + rng.bounded(3), width, height)) return false;
        if (!parkFinishRun(main, primary, width, height)) return false;
        if (type) {
            uint8_t candidates[MAX_PATH_POINTS] = {};
            uint8_t count = 0;
            for (uint8_t i = 3; i + 3 < main.pointCount; ++i) {
                if (parkHeading(main.points[i - 1], main.points[i]) ==
                    parkHeading(main.points[i], main.points[i + 1])) candidates[count++] = i;
            }
            if (!count) return false;
            forks[0] = candidates[rng.bounded(count)];
            if (type == 2) {
                uint8_t later[MAX_PATH_POINTS] = {};
                uint8_t laterCount = 0;
                for (uint8_t i = 0; i < count; ++i)
                    if (candidates[i] >= forks[0] + 4) later[laterCount++] = candidates[i];
                if (!laterCount) return false;
                forks[1] = later[rng.bounded(laterCount)];
            }
            uint8_t exits[2] = {};
            uint8_t exitCount = 0;
            for (uint8_t edge : edges) if (edge != primary) exits[exitCount++] = edge;
            if (rng.bounded(2)) {
                uint8_t swap = exits[0]; exits[0] = exits[1]; exits[1] = swap;
            }
            for (uint8_t i = 0; i < type; ++i) {
                uint8_t heading = parkHeading(main.points[forks[i]], main.points[forks[i] + 1]);
                heading = (heading + (rng.bounded(2) ? 1 : 3)) % 4;
                if (!parkGrowBranch(rng, main.points[forks[i]], heading, exits[i],
                                    width, height, branches[i])) return false;
            }
        }
    }
    out.pathCount = type + 1;
    std::memset(out.paths, 0, sizeof(out.paths));
    for (uint8_t i = 0; i < out.pathCount; ++i) {
        Path control = main;
        if (i) {
            control.pointCount = forks[i - 1];
            for (uint8_t j = 0; j < branches[i - 1].pointCount; ++j) {
                Point point = branches[i - 1].points[j];
                if (!appendPoint(control, point.x, point.y)) return false;
            }
        }
        uint8_t destination = type == 2 ? (i == 0 ? 1 : i == 1 ? 0 : 2) : i;
        Path& path = out.paths[destination];
        for (uint8_t j = 0; j < control.pointCount; ++j) {
            if (j && j + 1 < control.pointCount &&
                parkHeading(control.points[j - 1], control.points[j]) ==
                parkHeading(control.points[j], control.points[j + 1])) continue;
            Point point = orientParkPoint(control.points[j], width, vertical, mirror, entryEdge);
            if (!appendLine(path, point.x, point.y)) return false;
        }
        path.exit = parkEndpoint(path.points[path.pointCount - 1]);
    }
    out.entry = parkEndpoint(out.paths[0].points[0]);
    out.junction = type ? orientParkPoint(main.points[forks[0]], width, vertical, mirror, entryEdge)
                        : out.entry.point;
    if (out.entry.edge != entryEdge || !validateParkTopology(out)) return false;
    road.clear();
    for (uint8_t i = 0; i < out.pathCount; ++i) addPathRoad(out.paths[i], road);
    constexpr uint8_t limits[3] = {64, 76, 84};
    return road.count() <= limits[type] && validateBorderRoad(out, road) &&
           validateParkWideRoad(out, road);
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
                map.layers[0][y * WIDTH + x] = ExploreCaveTiles::ROCK_STEP;
                map.layers[1][y * WIDTH + x] = 0;
                stairs.add(x, y);
            } else {
                uint8_t cliffEdge = x == 0 ? 0 : (x == WIDTH - 1 ? 2 : 1);
                map.layers[0][y * WIDTH + x] = CAVE_CLIFF_TILES[cliffEdge];
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

void stampCaveEntrance(Map& map, const Endpoint& endpoint) {
    const uint8_t x = endpoint.point.x;
    const uint8_t y = endpoint.point.y;
    switch (endpoint.edge) {
    case Edge::LEFT:
        map.layers[1][y * WIDTH + x] = ExploreCaveTiles::ENTRANCE_LEFT;
        break;
    case Edge::RIGHT:
        map.layers[1][y * WIDTH + x] = ExploreCaveTiles::ENTRANCE_RIGHT;
        break;
    case Edge::TOP:
        map.layers[1][y * WIDTH + x] = ExploreCaveTiles::ENTRANCE_BACK;
        break;
    case Edge::BOTTOM:
        if (y > 0) {
            map.layers[1][(y - 1) * WIDTH + x] =
                ExploreCaveTiles::ENTRANCE_FRONT_TOP;
        }
        map.layers[1][y * WIDTH + x] = ExploreCaveTiles::ENTRANCE_FRONT_BOTTOM;
        break;
    }
}

void stampAncientCaveEntrances(Map& map) {
    stampCaveEntrance(map, map.entry);
    for (uint8_t i = 0; i < map.pathCount; ++i) {
        stampCaveEntrance(map, map.paths[i].exit);
    }
}

void stampAncientWaterfallValley(Map& map, CellMask& water, CellMask& forest,
                                 CellMask& cliff, CellMask& stairs,
                                 CellMask& scenery) {
    (void)forest;
    switch (map.entry.edge) {
    case Edge::BOTTOM:
        stampSeaChannel(map, 4, 0, 4, 3, 0, 0, water);
        stampSeaChannel(map, 3, 8, 6, HEIGHT, 4, 4, water);
        stampWaterfall(map, 4, 4, 3, 3, water);
        stampWaterfallWall(map, 3, 8, 4, 4, 0, 0, 11, cliff, stairs);
        stampSmallWaterRock(map, 6, 9);
        break;
    case Edge::LEFT:
        stampSeaChannel(map, 10, 0, 4, 4, 0, 0, water);
        stampSeaChannel(map, 9, 9, 6, HEIGHT, 10, 4, water);
        stampWaterfall(map, 10, 4, 4, 3, water);
        stampWaterfallWall(map, 4, 9, 10, 4, 0, 0, 4, cliff, stairs);
        stampSmallWaterRock(map, 11, 10);
        break;
    case Edge::TOP:
        stampSeaChannel(map, 10, 0, 4, 3, 0, 0, water);
        stampSeaChannel(map, 9, 8, 6, HEIGHT, 10, 4, water);
        stampWaterfall(map, 10, 4, 3, 3, water);
        stampWaterfallWall(map, 3, 8, 10, 4, 0, 0, 5, cliff, stairs);
        stampSmallWaterRock(map, 11, 9);
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
        break;
    }
    stampAncientCaveEntrances(map);
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
                      CellMask& patch, bool reserveGround = true) {
    patch.clear();
    CellMask component = largestConnectedComponent(available);
    int componentCount = component.count();
    if (componentCount == 0) return;
    int reserve = reserveGround
        ? (componentCount >= 32 ? 12 : (componentCount / 4 > 4 ? componentCount / 4 : 4))
        : 0;
    int target = minimum(requestedSize, componentCount - reserve);
    if (target < (reserveGround ? 6 : 3)) return;

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

CellMask parkGrassAvailable(const CellMask& road, const CellMask& trees) {
    CellMask blocked = road;
    blocked.unite(trees);
    CellMask available;
    available.clear();
    for (uint8_t y = 1; y < HEIGHT; ++y) {
        for (uint8_t x = 0; x < WIDTH; ++x) {
            if (!blocked.contains(x, y) && !blocked.contains(x, y - 1)) {
                available.add(x, y);
            }
        }
    }
    return available;
}

int parkTreeDistance(uint8_t left, uint8_t top, uint8_t width,
                     const CellMask& trees) {
    int distance = 6;
    for (uint8_t dy = 0; dy < 3; ++dy) {
        for (uint8_t dx = 0; dx < width; ++dx) {
            for (uint8_t y = 0; y < HEIGHT; ++y) {
                for (uint8_t x = 0; x < WIDTH; ++x) {
                    if (!trees.contains(x, y)) continue;
                    distance = minimum(distance,
                        absolute(left + dx - x) + absolute(top + dy - y));
                }
            }
        }
    }
    return distance;
}

template <typename Visitor>
void forEachParkTreeCandidate(const CellMask& road, const CellMask& trees,
                              uint8_t width, bool topBoundary, uint8_t grassTarget,
                              bool allowAdjacent, Visitor visitor) {
    CellMask forbidden = topBoundary || allowAdjacent ? trees : expandedMask(trees, 1);
    forbidden.unite(road);
    for (uint8_t top = 0; top < HEIGHT - 2; ++top) {
        for (uint8_t left = 0; left <= WIDTH - width; ++left) {
            int margin = minimum(minimum(left, WIDTH - left - width),
                                 minimum(top, HEIGHT - top - 3));
            if (topBoundary && top != 0) continue;
            if (forbidden.intersectsRect(left, top, left + width - 1, top + 2)) continue;
            CellMask afterTrees = trees;
            for (uint8_t dy = 0; dy < 3; ++dy) {
                for (uint8_t dx = 0; dx < width; ++dx) afterTrees.add(left + dx, top + dy);
            }
            CellMask available = parkGrassAvailable(road, afterTrees);
            uint16_t capacity = available.count();
            if (capacity < grassTarget + 8U) continue;
            int score = capacity * 8 + parkTreeDistance(left, top, width, trees) * 2 +
                        (topBoundary ? 0 : minimum(margin, 2) * 3);
            visitor(Point{left, top}, score);
        }
    }
}

bool chooseParkTree(Rng& rng, const CellMask& road, const CellMask& trees,
                     uint8_t width, bool topBoundary, uint8_t grassTarget, Point& selected,
                     bool allowAdjacent = false) {
    int bestScore = -32768;
    forEachParkTreeCandidate(road, trees, width, topBoundary, grassTarget, allowAdjacent,
        [&](const Point&, int score) { if (score > bestScore) bestScore = score; });
    if (bestScore == -32768) return false;
    uint16_t optionCount = 0;
    forEachParkTreeCandidate(road, trees, width, topBoundary, grassTarget, allowAdjacent,
        [&](const Point&, int score) { if (score >= bestScore - 8) ++optionCount; });
    uint16_t choice = static_cast<uint16_t>(rng.bounded(optionCount));
    bool found = false;
    forEachParkTreeCandidate(road, trees, width, topBoundary, grassTarget, allowAdjacent,
        [&](const Point& point, int score) {
            if (found || score < bestScore - 8) return;
            if (choice-- == 0) {
                selected = point;
                found = true;
            }
        });
    return found;
}

template <size_t TreeWidth>
void stampParkTree(Map& map, CellMask& trees, const Point& point,
                    const uint16_t (&tiles)[3][TreeWidth], uint8_t count = 1) {
    for (uint8_t tree = 0; tree < count; ++tree) {
        for (uint8_t dy = 0; dy < 3; ++dy) {
            for (uint8_t dx = 0; dx < TreeWidth; ++dx) {
                uint8_t x = point.x + tree * TreeWidth + dx;
                uint8_t y = point.y + dy;
                map.layers[1][y * WIDTH + x] = tiles[dy][dx];
                trees.add(x, y);
            }
        }
    }
}

void labelParkGrass(const CellMask& grass, uint8_t (&labels)[CELL_COUNT]) {
    std::memset(labels, 0, sizeof(labels));
    uint8_t label = 0;
    Point stack[CELL_COUNT];
    for (uint8_t x = 0; x < WIDTH; ++x) {
        for (uint8_t y = 0; y < HEIGHT; ++y) {
            if (!grass.contains(x, y) || labels[y * WIDTH + x] != 0) continue;
            ++label;
            uint16_t stackCount = 0;
            stack[stackCount++] = {x, y};
            labels[y * WIDTH + x] = label;
            while (stackCount > 0) {
                Point point = stack[--stackCount];
                constexpr int8_t DX[] = {1, -1, 0, 0};
                constexpr int8_t DY[] = {0, 0, 1, -1};
                for (uint8_t i = 0; i < 4; ++i) {
                    int nx = point.x + DX[i];
                    int ny = point.y + DY[i];
                    if (!grass.contains(nx, ny) || labels[ny * WIDTH + nx] != 0) continue;
                    labels[ny * WIDTH + nx] = label;
                    stack[stackCount++] = {static_cast<uint8_t>(nx), static_cast<uint8_t>(ny)};
                }
            }
        }
    }
}

bool parkFrontierFits(const Point& point, const CellMask& available,
                       const CellMask& grass, const uint8_t (&labels)[CELL_COUNT]) {
    if (!available.contains(point.x, point.y) || grass.contains(point.x, point.y)) return false;
    uint8_t adjacentLabel = 0;
    constexpr int8_t DX[] = {1, -1, 0, 0};
    constexpr int8_t DY[] = {0, 0, 1, -1};
    for (uint8_t i = 0; i < 4; ++i) {
        int x = point.x + DX[i];
        int y = point.y + DY[i];
        if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) continue;
        uint8_t label = labels[y * WIDTH + x];
        if (label == 0) continue;
        if (adjacentLabel != 0 && adjacentLabel != label) return false;
        adjacentLabel = label;
    }
    return adjacentLabel != 0;
}

void growParkGrassToTarget(Rng& rng, const CellMask& available,
                           uint8_t target, CellMask& grass) {
    while (grass.count() < target) {
        uint8_t labels[CELL_COUNT];
        labelParkGrass(grass, labels);
        int bestScore = -32768;
        uint16_t optionCount = 0;
        for (uint8_t x = 0; x < WIDTH; ++x) {
            for (uint8_t y = 0; y < HEIGHT; ++y) {
                Point point{x, y};
                if (!parkFrontierFits(point, available, grass, labels)) continue;
                int score = cardinalNeighborCount(point, grass) * 100 +
                            localOpenness(point, available, 1) * 4;
                if (score > bestScore) {
                    bestScore = score;
                    optionCount = 1;
                } else if (score == bestScore) {
                    ++optionCount;
                }
            }
        }
        if (optionCount == 0) return;
        uint16_t choice = static_cast<uint16_t>(rng.bounded(optionCount));
        bool found = false;
        for (uint8_t x = 0; x < WIDTH && !found; ++x) {
            for (uint8_t y = 0; y < HEIGHT; ++y) {
                Point point{x, y};
                if (!parkFrontierFits(point, available, grass, labels)) continue;
                int score = cardinalNeighborCount(point, grass) * 100 +
                            localOpenness(point, available, 1) * 4;
                if (score != bestScore) continue;
                if (choice-- == 0) {
                    grass.add(x, y);
                    found = true;
                    break;
                }
            }
        }
    }
}

void stampTallGrassPark(Map& map, Rng& rng, const CellMask& road, CellMask& trees,
                         const AreaProfile& profile) {
    uint8_t grassTarget = profile.grassMinimum + rng.bounded(profile.grassVariance + 1U);
    trees.clear();
    // Open canopy tiles connect only to forest beyond the top map edge.
    uint8_t boundaryTreeCount = 0;
    for (uint8_t group = 0; group < 2; ++group) {
        Point chosen{};
        if (!chooseParkTree(rng, road, trees, 4, true, grassTarget, chosen)) break;
        stampParkTree(map, trees, chosen, PARK_BOUNDARY_TREE_TILES, 2);
        boundaryTreeCount += 2;
    }
    // Replace unavailable top groups with complete standalone trees.
    uint8_t independentTarget = 6 + rng.bounded(2) - boundaryTreeCount;
    for (uint8_t tree = 0; tree < independentTarget; ++tree) {
        bool centered = rng.bounded(2) != 0;
        Point chosen{};
        bool found = chooseParkTree(rng, road, trees, centered ? 3 : 2,
                                    false, grassTarget, chosen);
        if (!found && centered) {
            centered = false;
            found = chooseParkTree(rng, road, trees, 2, false, grassTarget, chosen);
        }
        // Complete standalone trees may touch when the three-exit layout is tight.
        if (!found) {
            centered = false;
            found = chooseParkTree(rng, road, trees, 2, false, grassTarget, chosen, true);
        }
        // Keep the grass target within its accepted range if more space is needed.
        while (!found && grassTarget > profile.grassMinimum) {
            --grassTarget;
            centered = false;
            found = chooseParkTree(rng, road, trees, 2, false, grassTarget, chosen, true);
        }
        if (!found) break;
        if (centered) stampParkTree(map, trees, chosen, PARK_CENTERED_TREE_TILES);
        else stampParkTree(map, trees, chosen, PARK_TREE_TILES);
    }
    map.hasForest = !trees.empty();

    CellMask available = parkGrassAvailable(road, trees);
    CellMask grass;
    grass.clear();
    for (uint8_t patchIndex = 0; patchIndex < 4; ++patchIndex) {
        int remaining = grassTarget - grass.count();
        int requested = patchIndex == 0 ? remaining - 18
            : (remaining + 3 - patchIndex) / (4 - patchIndex);
        CellMask patchAvailable = available;
        patchAvailable.subtract(orthogonallyExpanded(grass));
        CellMask patch;
        growCompactPatch(rng, patchAvailable, static_cast<uint8_t>(requested), patch, false);
        grass.unite(patch);
    }
    // Fill narrow fragments by extending existing patches while keeping them separate.
    growParkGrassToTarget(rng, available, grassTarget, grass);
    CellMask grassFootprint = grass;
    for (uint8_t y = 1; y < HEIGHT; ++y) {
        for (uint8_t x = 0; x < WIDTH; ++x) {
            if (!grass.contains(x, y)) continue;
            map.layers[0][y * WIDTH + x] = TALL_GRASS_BASE_TILE;
            map.layers[1][(y - 1) * WIDTH + x] = TALL_GRASS_TOP_TILE;
            grassFootprint.add(x, y - 1);
        }
    }
    CellMask flowerBlocked = trees;
    flowerBlocked.unite(grassFootprint);
    CellMask empty;
    empty.clear();
    stampBoundaryFlowers(map, rng, grass, empty, road, empty, flowerBlocked,
                         profile.flowerGroups);
}

bool generateRandomPark(uint32_t seed, Edge entryEdge, Map& out) {
    Rng topology(seed ^ 0xA511E9B3U);
    uint8_t roll = topology.bounded(100);
    uint8_t type = roll < 30 ? 0 : roll < 80 ? 1 : 2;
    for (uint16_t attempt = 0; attempt < PARK_TOPOLOGY_ATTEMPTS + PARK_FALLBACK_ATTEMPTS; ++attempt) {
        CellMask road;
        if (!parkTopologyCandidate(topology, entryEdge, type,
                                   attempt >= PARK_TOPOLOGY_ATTEMPTS, out, road)) continue;
        std::memset(out.layers, 0, sizeof(out.layers));
        Rng terrain(seed ^ 0x63D83595U);
        for (uint16_t i = 0; i < CELL_COUNT; ++i)
            out.layers[0][i] = GRASS_TILES[terrain.bounded(sizeof(GRASS_TILES) / sizeof(GRASS_TILES[0]))];
        for (uint8_t y = 0; y < HEIGHT; ++y)
            for (uint8_t x = 0; x < WIDTH; ++x)
                if (road.contains(x, y)) out.layers[0][y * WIDTH + x] = roadTile(out, road, x, y);
        CellMask trees;
        stampTallGrassPark(out, terrain, road, trees, profileFor(TALL_GRASS_PARK_AREA));
        CellMask grass;
        uint8_t treeCount = 0;
        for (uint16_t i = 0; i < CELL_COUNT; ++i) {
            if (out.layers[0][i] == TALL_GRASS_BASE_TILE) grass.add(i % WIDTH, i / WIDTH);
            uint16_t tile = out.layers[1][i];
            if (tile == 800 || tile == 804 || tile == 824) ++treeCount;
        }
        uint8_t labels[CELL_COUNT];
        labelParkGrass(grass, labels);
        uint8_t patches = 0;
        for (uint8_t label : labels) if (label > patches) patches = label;
        if (grass.count() >= 40 && grass.count() <= 50 && treeCount >= 6 && treeCount <= 7 &&
            patches >= 3 && patches <= 4) return true;
    }
    return false;
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
                          CellMask& scenery, bool allowCracks = true) {
    uint8_t groundTarget = 10 + rng.bounded(7);
    uint8_t groundPlaced = 0;
    for (uint16_t attempt = 0; attempt < groundTarget * 16U && groundPlaced < groundTarget;
         ++attempt) {
        uint8_t x = rng.bounded(WIDTH);
        uint8_t y = rng.bounded(HEIGHT);
        if (road.contains(x, y) || scenery.contains(x, y)) continue;
        uint8_t variant = rng.bounded(sizeof(SNOW_GROUND_DECOR_TILES) /
                                      sizeof(SNOW_GROUND_DECOR_TILES[0]));
        map.layers[0][y * WIDTH + x] =
            SNOW_GROUND_DECOR_TILES[!allowCracks && variant == 2 ? 1 : variant];
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
    int pointX = endpoint.point.x;
    int pointY = endpoint.point.y;
    int cellX = x;
    int cellY = y;
    if (endpoint.edge == Edge::TOP || endpoint.edge == Edge::BOTTOM) {
        return cellY == pointY && cellX >= pointX - 1 && cellX <= pointX + 1;
    }
    return cellX == pointX && cellY >= pointY - 1 && cellY <= pointY + 1;
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
    if (endpoint.edge == Edge::TOP || endpoint.edge == Edge::BOTTOM) {
        return floor.contains(endpoint.point.x - 1, endpoint.point.y) &&
               floor.contains(endpoint.point.x, endpoint.point.y) &&
               floor.contains(endpoint.point.x + 1, endpoint.point.y);
    }
    return floor.contains(endpoint.point.x, endpoint.point.y - 1) &&
           floor.contains(endpoint.point.x, endpoint.point.y) &&
           floor.contains(endpoint.point.x, endpoint.point.y + 1);
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

bool frostInterior(Point point) {
    return point.x > 0 && point.x + 1 < WIDTH &&
           point.y > 0 && point.y + 1 < HEIGHT;
}

void trimFrostEntry(Map& map) {
    for (uint8_t i = 0; i < map.pathCount; ++i) {
        Path& path = map.paths[i];
        uint8_t first = 0;
        while (first + 2 < path.pointCount &&
               !frostInterior(path.points[first])) ++first;
        for (uint8_t j = first; j < path.pointCount; ++j) {
            path.points[j - first] = path.points[j];
        }
        path.pointCount -= first;
    }
    map.entry.point = map.paths[0].points[0];
}

void trimFrostExits(Map& map) {
    for (uint8_t i = 0; i < map.pathCount; ++i) {
        Path& path = map.paths[i];
        while (path.pointCount > 2 &&
               !frostInterior(path.points[path.pointCount - 1])) {
            --path.pointCount;
        }
        path.exit.point = path.points[path.pointCount - 1];
    }
}

void stampFrostExterior(Map& map, const Endpoint& endpoint) {
    const uint16_t* tiles = nullptr;
    int x = endpoint.point.x;
    int y = endpoint.point.y;
    switch (endpoint.edge) {
    case Edge::TOP:
        tiles = ExploreCaveTiles::FROST_EXIT;
        --x;
        break;
    case Edge::RIGHT:
        tiles = ExploreCaveTiles::FROST_EXIT_RIGHT;
        --y;
        break;
    case Edge::BOTTOM:
        tiles = ExploreCaveTiles::FROST_EXIT_BOTTOM;
        --x;
        break;
    case Edge::LEFT:
        tiles = ExploreCaveTiles::FROST_EXIT_LEFT;
        --y;
        break;
    }
    for (int i = 0; i < 3; ++i) {
        int column = x + ((endpoint.edge == Edge::TOP || endpoint.edge == Edge::BOTTOM) ? i : 0);
        int row = y + ((endpoint.edge == Edge::LEFT || endpoint.edge == Edge::RIGHT) ? i : 0);
        if (column >= 0 && column < WIDTH && row >= 0 && row < HEIGHT) {
            map.layers[1][row * WIDTH + column] = tiles[i];
        }
    }
}

bool frostPortalTouchesScene(const Map& map, const Endpoint& endpoint) {
    int x = endpoint.point.x;
    int y = endpoint.point.y;
    switch (endpoint.edge) {
    case Edge::TOP: --y; break;
    case Edge::RIGHT: ++x; break;
    case Edge::BOTTOM: ++y; break;
    case Edge::LEFT: --x; break;
    }
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return true;
    uint16_t index = static_cast<uint16_t>(y * WIDTH + x);
    return map.layers[1][index] != 0 || map.layers[2][index] != 0;
}

bool stampFrostPortals(Map& map, FrostContext frost) {
    if (map.pathCount == 0 || map.paths[0].pointCount == 0) return false;
    if (frost.level == 0) {
        stampFrostExterior(map, map.entry);
    } else if (frost.enteredByLadder) {
        Point point = map.entry.point;
        if (!frostPortalTouchesScene(map, map.entry)) return false;
        map.layers[1][point.y * WIDTH + point.x] =
            ExploreCaveTiles::FROST_DOWNWARD_STAIRS;
    }
    for (uint8_t i = 0; i < map.pathCount; ++i) {
        const Path& path = map.paths[i];
        if (path.pointCount == 0) continue;
        if (frost.level + 1 >= frost.levelCount) {
            stampFrostExterior(map, path.exit);
        } else if (i == 0) {
            Point point = path.exit.point;
            map.layers[1][point.y * WIDTH + point.x] =
                ExploreCaveTiles::FROST_DOWNWARD_STAIRS;
        } else if (i == 1) {
            Point point = path.exit.point;
            if (point.y == 0 || map.layers[2][(point.y - 1) * WIDTH + point.x] != 0 ||
                !frostPortalTouchesScene(map, path.exit)) return false;
            map.layers[2][(point.y - 1) * WIDTH + point.x] =
                ExploreCaveTiles::FROST_UP_LADDER[0];
            map.layers[1][point.y * WIDTH + point.x] =
                ExploreCaveTiles::FROST_UP_LADDER[1];
        } else {
            return false;
        }
    }
    return true;
}

bool generateFrostCave(uint32_t seed, Edge entryEdge, Map& out,
                       FrostContext frost) {
    bool horizontal = entryEdge == Edge::LEFT || entryEdge == Edge::RIGHT;
    uint8_t variant = static_cast<uint8_t>((seed >> 5) & 1U);
    const FrostTemplate& spec = horizontal
        ? FROST_HORIZONTAL_TEMPLATES[variant]
        : FROST_VERTICAL_TEMPLATES[variant];
    bool mirrorX = entryEdge == Edge::RIGHT ||
                   (!horizontal && (seed & 0x80U) != 0);
    bool mirrorY = entryEdge == Edge::TOP ||
                   (horizontal && (seed & 0x80U) != 0);

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
    if (frost.level > 0) trimFrostEntry(out);
    if (frost.level + 1 < frost.levelCount) trimFrostExits(out);

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

    CellMask scenery = blocked;
    scenery.unite(ice);
    for (uint8_t y = 0; y < HEIGHT; ++y) {
        for (uint8_t x = 0; x < WIDTH; ++x) {
            if (!floor.contains(x, y)) scenery.add(x, y);
        }
    }
    Rng decorations(seed ^ 0xA5F1537DU);
    stampSnowDecorations(out, decorations, route, scenery,
                         false);

    if (!stampFrostPortals(out, frost)) return false;

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

bool generate(uint32_t seed, Edge entryEdge, uint8_t areaIndex, Map& out,
              FrostContext frost) {
    if (areaIndex == FROST_CRYSTAL_CAVE_AREA &&
        (frost.levelCount == 0 || frost.level >= frost.levelCount)) return false;
    std::memset(&out, 0, sizeof(out));
    out.seed = seed;
    out.areaIndex = areaIndex;
    if (areaIndex == TALL_GRASS_PARK_AREA) return generateRandomPark(seed, entryEdge, out);
    Rng terrain(seed ^ 0x63D83595U);
    Rng features(seed ^ 0xC2B2AE35U);
    const AreaProfile& profile = profileFor(areaIndex);
    bool hasSnow = areaIndex == FROST_CRYSTAL_CAVE_AREA;
    if (hasSnow) return generateFrostCave(seed, entryEdge, out, frost);

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
        } else if (hasWaterfall) {
            out.layers[0][index] = CAVE_FLOOR_TILE;
        } else {
            out.layers[0][index] =
                GRASS_TILES[terrain.bounded(sizeof(GRASS_TILES) / sizeof(GRASS_TILES[0]))];
        }
    }
    if (out.hasCoast) stampCoast(out, water);

    if (hasWaterfall) {
        stampAncientWaterfallValley(out, water, forest, cliff, stairs, scenery);
        out.hasForest = false;
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
            if (!hasWaterfall && road.contains(x, y) && !water.contains(x, y) &&
                !cliff.contains(x, y) && !forest.contains(x, y)) {
                out.layers[0][y * WIDTH + x] =
                    hasSnow ? SNOW_PATH_TILE : roadTile(out, road, x, y);
            }
        }
    }

    if (!hasCreek && !hasWaterfall && !hasSnow && areaIndex != TALL_GRASS_PARK_AREA &&
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
    } else if (areaIndex == TALL_GRASS_PARK_AREA) {
        stampTallGrassPark(out, terrain, road, forest, profile);
    } else if (!hasWaterfall) {
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
        hash = fnvByte(hash, path.fallsToNextLevel ? 1 : 0);
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
    case 810: case 811: case 812: case 813: case 816: case 817: case 818: case 819:
    case 820: case 821: case 824: case 825: case 826: case 832: case 833: case 834:
    case 840: case 841: case 842: case FENCE_TOP_LEFT: case FENCE_LEFT:
    case FENCE_TOP: case FENCE_TOP_RIGHT: case SHRUB_TILE:
        return true;
    default:
        return false;
    }
}

}  // namespace ExploreMapGenerator
