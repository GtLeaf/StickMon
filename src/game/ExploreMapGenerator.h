#pragma once

#include <cstdint>

namespace ExploreMapGenerator {

constexpr uint8_t WIDTH = 16;
constexpr uint8_t HEIGHT = 12;
constexpr uint8_t LAYER_COUNT = 3;
constexpr uint8_t PATH_COUNT = 2;
constexpr uint8_t MAX_PATH_COUNT = 3;
constexpr uint8_t MAX_PATH_POINTS = 48;
constexpr uint16_t CELL_COUNT = WIDTH * HEIGHT;
constexpr uint16_t ALGORITHM_VERSION = 26;
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

template<class V> inline void contentFields(V& v, Point& x) { v(x.x,x.y); }
template<class V> inline void contentFields(V& v, Endpoint& x) { v(x.point,x.edge); }

struct FrostContext {
    // Segment progress is also used by the mist forest's tree progression.
    uint8_t level = 0;
    uint8_t levelCount = 1;
    bool enteredByLadder = false;
};

struct ValleyContext {
    uint8_t node = 0;
    uint8_t entrance = 0;
};

struct Path {
    Point points[MAX_PATH_POINTS] = {};
    uint8_t pointCount = 0;
    Endpoint exit = {};
    bool fallsToNextLevel = false;
    bool cellCentered = false;  // Caves use cell centers, outdoor roads use paired cells.
    uint8_t nextValleyNode = 255;
    uint8_t nextValleyEntrance = 0;
    bool terminal = false;
    bool interiorExit = false;
    // Presentation offsets in world pixels. Route indices and cells stay unchanged.
    struct Offset { int8_t x, y; };
    Offset visualOffsets[MAX_PATH_POINTS] = {};
};

struct Map {
    uint16_t layers[LAYER_COUNT][CELL_COUNT] = {};
    Endpoint entry = {};
    Point junction = {};  // First fork; the entry for a single park route.
    Path paths[MAX_PATH_COUNT] = {};
    uint8_t pathCount = 0;
    uint32_t seed = 0;
    uint8_t areaIndex = 0;
    bool hasCoast = false;
    bool hasForest = false;
    bool hasCreek = false;
    bool hasCliff = false;
    bool hasWaterfall = false;
    bool valleyJourney = false;
    uint8_t valleyNode = 0;
};

inline bool finalPath(const Map& map, uint8_t path, uint8_t block, uint8_t count) {
    return map.valleyJourney ? path < map.pathCount && map.paths[path].terminal
                            : block + 1 >= count;
}

inline bool internalValleyTransfer(const Map& map, uint8_t path) {
    return map.valleyJourney && path < map.pathCount && map.valleyNode >= 10 &&
           map.paths[path].nextValleyNode >= 10 && map.paths[path].nextValleyNode < 14;
}

uint32_t deriveSeed(uint32_t expeditionSeed, uint8_t blockIndex, uint8_t areaIndex);
Edge opposite(Edge edge);
bool generate(uint32_t seed, Edge entryEdge, uint8_t areaIndex, Map& out,
              FrostContext frost = {});
bool generateValleyRegion(uint32_t expeditionSeed, ValleyContext context,
                          bool finalBlock, Map& out);
uint32_t fingerprint(const Map& map);

bool isRoadTile(uint16_t tileId);
bool isWaterTile(uint16_t tileId);
bool isForestTile(uint16_t tileId);

constexpr bool isSmoothIceTile(uint16_t tileId) {
    return tileId == 4504 || (tileId >= 4524 && tileId <= 4531);
}

}  // namespace ExploreMapGenerator
