#pragma once
#include <cstdint>
#include "game/MarketMapSize.h"

namespace Market {
#if defined(STICKMON_MARKET_RENDER_WIDTH)
constexpr int CELL_WIDTH=STICKMON_MARKET_RENDER_WIDTH, CELL_HEIGHT=STICKMON_MARKET_RENDER_HEIGHT;
#elif defined(STICKMON_EDITOR_MAP_WIDTH)
constexpr int CELL_WIDTH=STICKMON_EDITOR_MAP_WIDTH, CELL_HEIGHT=STICKMON_EDITOR_MAP_HEIGHT;
#else
constexpr int CELL_WIDTH=MarketMapSize::WIDTH, CELL_HEIGHT=MarketMapSize::HEIGHT;
#endif
constexpr int CELLS=CELL_WIDTH*CELL_HEIGHT;
}
#include "game/AuthoredNpc.h"

namespace Market {
constexpr int WIDTH = CELL_WIDTH*2+1, HEIGHT = CELL_HEIGHT*2+1, NODES = WIDTH * HEIGHT;
constexpr int MASK_BYTES = (NODES + 7) / 8, CAPACITY = 16, STOP_CAPACITY = 48;
constexpr uint16_t NONE = 0xffff;
constexpr int GROUP_CAPACITY=3, STALL_CAPACITY=12, CUSTOMER_CAPACITY=8;
struct Mask {
    uint8_t bytes[MASK_BYTES]{};
    bool has(int node) const { return node >= 0 && node < NODES && (bytes[node/8] & (1U << (node%8))); }
    void set(int node) { if (node >= 0 && node < NODES) bytes[node/8] |= uint8_t(1U << (node%8)); }
};
constexpr uint16_t node(int x, int y) { return uint16_t(y*WIDTH+x); }
constexpr uint16_t cellNode(int cell) { return node((cell%CELL_WIDTH)*2+1,(cell/CELL_WIDTH)*2+1); }
struct Activity {
    uint16_t spawn=0, patrol[32]{};
    uint8_t direction=0, stepTicks=6, patrolCount=0;
    AuthoredNpc::Behavior behavior=AuthoredNpc::Behavior::IDLE;
    uint16_t waitMin=10, waitMax=30;
    uint32_t seed=1;
};
struct Resident {
    Activity activity;
    uint16_t species = 0;
    Mask navigation;
    // A missing reception keeps legacy authored movement unchanged.
    uint8_t reception=255, lookMask=15;
};
struct Stall {
    uint8_t resident=0, pointCount=0;
    uint16_t points[CUSTOMER_CAPACITY]{};
};
// A zero juvenile denotes a solo visitor. Existing pair records stay valid.
struct Family { uint16_t adult = 0, juvenile = 0; };
struct Data {
    uint8_t version = 1;
    struct { uint16_t layers[3][CELLS]{}; } map;
    Mask roads;
    Mask pedestrian;
    Resident residents[CAPACITY-4]{};
    uint8_t residentCount = 0;
    uint16_t stops[STOP_CAPACITY]{};
    uint8_t stopCount = 0;
    Family families[64]{};
    uint8_t familyCount = 0;
    uint16_t intervalMin = 80, intervalMax = 150, stayMin = 100, stayMax = 200;
    uint16_t entry = 0;
    uint8_t outward = 0;
    uint8_t maxGroups=1, maxVisitors=2, soloPercent=60, stallCount=0;
    bool performances=false;
    uint16_t firstMin=0, firstMax=0;
    Stall stalls[STALL_CAPACITY]{};
};
} // namespace Market
