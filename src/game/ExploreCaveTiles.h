#pragma once

#include <cstdint>

namespace ExploreCaveTiles {

// Runtime-only IDs avoid collisions with Outside.png tiles that share the
// original RPG Maker XP IDs from Caves.png.
constexpr uint16_t ENTRANCE_LEFT = 4700;
constexpr uint16_t ENTRANCE_RIGHT = 4701;
constexpr uint16_t ENTRANCE_FRONT_TOP = 4702;
constexpr uint16_t ENTRANCE_FRONT_BOTTOM = 4703;
constexpr uint16_t ENTRANCE_BACK = 4704;
constexpr uint16_t ROCK_STEP = 4705;
constexpr uint16_t UP_LADDER[2] = {4706, 4707};
constexpr uint16_t DOWN_LADDER_OVERLAY = 4708;

constexpr uint16_t ROCK_ISLAND[3][3] = {
    {4709, 4710, 4711},
    {4712, 4713, 4714},
    {4715, 4716, 4717},
};
constexpr uint16_t CLIFF_RIGHT_TO_DOWN = 4718;
constexpr uint16_t CLIFF_LEFT_TO_DOWN = 4719;
constexpr uint16_t EDGE_TRACE[9] = {
    4720, 4721, 4722, 4723, 4724, 4725, 4726, 4727, 4728,
};

constexpr uint16_t DOWN_LADDER_ROCK_BASE[3][3] = {
    {4729, 4730, 4731},
    {4732, 4733, 4734},
    {4735, 4736, 4737},
};
constexpr uint16_t DOWN_LADDER_OPEN_BASE[3][3] = {
    {4738, 4739, 4740},
    {4732, 4733, 4734},
    {4735, 4736, 4737},
};

constexpr uint16_t FROST_EXIT[3] = {4741, 4742, 4743};
constexpr uint16_t FROST_BROKEN_ICE_HOLE = 4744;
constexpr uint16_t FROST_ROUND_WATER_BOTTOM[2] = {4745, 4746};
constexpr uint16_t FROST_DOWNWARD_STAIRS = 4747;
constexpr uint16_t FROST_CAVE_HOLE = 4748;
constexpr uint16_t FROST_EDGE_TRACE[9] = {
    4749, 4750, 4751, 4752, 4753, 4754, 4755, 4756, 4757,
};

constexpr bool isRuntimeTile(uint16_t tileId) {
    return tileId >= ENTRANCE_LEFT && tileId <= FROST_EDGE_TRACE[8];
}

constexpr bool isStatefulFrostTile(uint16_t tileId) {
    return tileId == FROST_BROKEN_ICE_HOLE ||
           tileId == FROST_CAVE_HOLE;
}

static_assert(FROST_EDGE_TRACE[8] - ENTRANCE_LEFT + 1 == 58,
              "Cave runtime tile aliases must remain contiguous");

}  // namespace ExploreCaveTiles
