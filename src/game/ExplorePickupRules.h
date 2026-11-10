#pragma once

#include <cstdint>
#include "game/ContentData.h"

#include "game/GameState.h"

namespace ExplorePickupRules {

enum Id : uint8_t {
    NONE = 0,
    COIN,
    POTION,
    SUPER_POTION,
    ANTIDOTE,
    RARE_CANDY,
    MAX_POTION,
    FULL_RESTORE,
    FULL_HEAL,
    REVIVE,
    MAX_REPEL,
    HONEY,
    NUGGET,
    BIG_PEARL,
    STAR_PIECE,
    HEART_SCALE,
};

struct Entry {
    uint8_t id;
    uint16_t weight;
};

template<class V> inline void contentFields(V& v, Entry& x) { v(x.id,x.weight); }
struct CoinRange { uint8_t minCoin,maxCoin; };
template<class V> inline void contentFields(V& v, CoinRange& x) { v(x.minCoin,x.maxCoin); }
#if !STICKMON_EXTERNAL_CONTENT
inline constexpr Entry GRASS_PATH_PICKUPS[] = {
    {COIN, 40}, {POTION, 25}, {ANTIDOTE, 10},
    {HONEY, 10}, {NUGGET, 3}, {RARE_CANDY, 2},
};
inline constexpr Entry CREEK_SLOPE_PICKUPS[] = {
    {COIN, 40}, {POTION, 20}, {ANTIDOTE, 10},
    {MAX_REPEL, 8}, {HONEY, 8}, {NUGGET, 5}, {RARE_CANDY, 2},
};
inline constexpr Entry TALL_GRASS_PARK_PICKUPS[] = {
    {COIN, 38}, {SUPER_POTION, 20}, {ANTIDOTE, 8}, {REVIVE, 5},
    {MAX_REPEL, 8}, {NUGGET, 6}, {BIG_PEARL, 3}, {RARE_CANDY, 2},
};
inline constexpr Entry FROST_CRYSTAL_CAVE_PICKUPS[] = {
    {COIN, 36}, {SUPER_POTION, 18}, {FULL_HEAL, 8}, {REVIVE, 6},
    {MAX_REPEL, 6}, {NUGGET, 4}, {BIG_PEARL, 6}, {HEART_SCALE, 2}, {RARE_CANDY, 2},
};
inline constexpr Entry MIST_FOREST_PATH_PICKUPS[] = {
    {COIN, 34}, {MAX_POTION, 15}, {FULL_HEAL, 8}, {REVIVE, 7},
    {BIG_PEARL, 7}, {STAR_PIECE, 4}, {HEART_SCALE, 2}, {RARE_CANDY, 2},
};
inline constexpr Entry ANCIENT_WATERFALL_VALLEY_PICKUPS[] = {
    {COIN, 32}, {MAX_POTION, 15}, {FULL_RESTORE, 6}, {REVIVE, 8},
    {BIG_PEARL, 5}, {STAR_PIECE, 7}, {HEART_SCALE, 2}, {RARE_CANDY, 2},
};

#endif
struct Table {
    const Entry* entries;
    uint8_t count;
    uint8_t minCoin;
    uint8_t maxCoin;
};

template <unsigned N>
constexpr Table makeTable(const Entry (&entries)[N], uint8_t minCoin, uint8_t maxCoin) {
    return {entries, static_cast<uint8_t>(N), minCoin, maxCoin};
}

#if STICKMON_EXTERNAL_CONTENT
inline const Table& tableForArea(uint8_t area) {
    static const ContentData::Table<Entry,32,false> entries[]={
        ContentData::Table<Entry,32,false>(40),ContentData::Table<Entry,32,false>(41),
        ContentData::Table<Entry,32,false>(42),ContentData::Table<Entry,32,false>(43),
        ContentData::Table<Entry,32,false>(44),ContentData::Table<Entry,32,false>(45)};
    static const ContentData::Table<CoinRange,6> coins{46};
    static Table tables[6]{};
    const unsigned i=area<6 ? area : 5;
    tables[i]={entries[i].data(),uint8_t(entries[i].size()),coins[i].minCoin,coins[i].maxCoin};
    return tables[i];
}
#else
inline constexpr Table TABLES[] = {
    makeTable(GRASS_PATH_PICKUPS, 10, 30),
    makeTable(CREEK_SLOPE_PICKUPS, 15, 40),
    makeTable(TALL_GRASS_PARK_PICKUPS, 20, 60),
    makeTable(FROST_CRYSTAL_CAVE_PICKUPS, 30, 80),
    makeTable(MIST_FOREST_PATH_PICKUPS, 40, 110),
    makeTable(ANCIENT_WATERFALL_VALLEY_PICKUPS, 50, 150),
};

constexpr const Table& tableForArea(uint8_t area) {
    return TABLES[area < Game::EXPLORE_AREA_COUNT ? area : Game::EXPLORE_AREA_COUNT - 1];
}

#endif

constexpr bool available(uint8_t id, uint16_t stepsToday) {
    return id != RARE_CANDY || stepsToday >= 5000;
}

constexpr uint16_t weightTotal(const Table& table, uint16_t stepsToday) {
    uint16_t total = 0;
    for (uint8_t i = 0; i < table.count; ++i) {
        if (available(table.entries[i].id, stepsToday)) total += table.entries[i].weight;
    }
    return total;
}

// The caller owns the random draw and its ordering within the session.
constexpr uint8_t forRoll(const Table& table, uint16_t stepsToday, uint16_t roll) {
    for (uint8_t i = 0; i < table.count; ++i) {
        if (!available(table.entries[i].id, stepsToday)) continue;
        if (roll < table.entries[i].weight) return table.entries[i].id;
        roll -= table.entries[i].weight;
    }
    return NONE;
}

constexpr Game::ItemId itemId(uint8_t id) {
    switch (id) {
    case POTION: return Game::ItemId::POTION;
    case SUPER_POTION: return Game::ItemId::SUPER_POTION;
    case ANTIDOTE: return Game::ItemId::ANTIDOTE;
    case RARE_CANDY: return Game::ItemId::CANDY;
    case MAX_POTION: return Game::ItemId::MAX_POTION;
    case FULL_RESTORE: return Game::ItemId::FULL_RESTORE;
    case FULL_HEAL: return Game::ItemId::FULL_HEAL;
    case REVIVE: return Game::ItemId::REVIVE;
    case MAX_REPEL: return Game::ItemId::MAX_REPEL;
    case HONEY: return Game::ItemId::HONEY;
    case NUGGET: return Game::ItemId::NUGGET;
    case BIG_PEARL: return Game::ItemId::BIG_PEARL;
    case STAR_PIECE: return Game::ItemId::STAR_PIECE;
    case HEART_SCALE: return Game::ItemId::HEART_SCALE;
    default: return Game::ItemId::COUNT;
    }
}

#if !STICKMON_EXTERNAL_CONTENT
static_assert(sizeof(TABLES) / sizeof(TABLES[0]) == Game::EXPLORE_AREA_COUNT,
              "pickup tables must follow the six exploration areas");
#endif

}  // namespace ExplorePickupRules
