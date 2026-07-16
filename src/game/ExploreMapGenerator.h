#pragma once

#include <cstdint>

namespace ExploreMapGenerator {

constexpr uint8_t WIDTH = 16;
constexpr uint8_t HEIGHT = 12;
constexpr uint8_t LAYER_COUNT = 3;
constexpr uint8_t PATH_COUNT = 2;
constexpr uint8_t MAX_PATH_POINTS = 48;
constexpr uint16_t CELL_COUNT = WIDTH * HEIGHT;
constexpr uint16_t ALGORITHM_VERSION = 6;
constexpr uint8_t GRASS_PATH_AREA = 0;
constexpr uint8_t CREEK_BRIDGE_SLOPE_AREA = 1;
constexpr uint8_t TALL_GRASS_PARK_AREA = 2;
constexpr uint8_t FROST_CRYSTAL_CAVE_AREA = 3;
constexpr uint8_t MIST_FOREST_PATH_AREA = 4;
constexpr uint8_t ANCIENT_WATERFALL_VALLEY_AREA = 5;

enum class Edge : uint8_t {
    TOP,
    RIGHT,
    BOTTOM,
    LEFT,
};

struct Point {
    uint8_t x;
    uint8_t y;
};

struct Endpoint {
    Point point;
    Edge edge;
};

struct Path {
    Point points[MAX_PATH_POINTS] = {};
    uint8_t pointCount = 0;
    Endpoint exit = {};
};

struct Map {
    uint16_t layers[LAYER_COUNT][CELL_COUNT] = {};
    Endpoint entry = {};
    Point junction = {};
    Path paths[PATH_COUNT] = {};
    uint8_t pathCount = 0;
    uint32_t seed = 0;
    uint8_t areaIndex = 0;
    bool hasCoast = false;
    bool hasForest = false;
    bool hasCreek = false;
    bool hasCliff = false;
    bool hasWaterfall = false;
};

uint32_t deriveSeed(uint32_t expeditionSeed, uint8_t blockIndex, uint8_t areaIndex);
Edge opposite(Edge edge);
bool generate(uint32_t seed, Edge entryEdge, uint8_t areaIndex, Map& out);
uint32_t fingerprint(const Map& map);

bool isRoadTile(uint16_t tileId);
bool isWaterTile(uint16_t tileId);
bool isForestTile(uint16_t tileId);

}  // namespace ExploreMapGenerator
