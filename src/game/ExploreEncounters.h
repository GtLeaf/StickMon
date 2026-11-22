#pragma once
// CONTENT_TABLES_ADAPTED
#include "game/ContentData.h"

#include <cstddef>
#include <cstdint>

#include "game/ExplorePool.h"
#include "game/GameState.h"

namespace ExploreEncounters {

using Rarity = ExplorePool::Rarity;

// The species tier is shared by all regions. Species without a tier use NORMAL.
constexpr Rarity rarityForSpecies(uint16_t speciesId) {
    switch (speciesId) {
    case 10: case 12: case 161: case 194: case 195: case 220:
    case 263: case 265: case 270: case 273: case 285: case 361:
        return Rarity::COMMON;
    case 17: case 18: case 26: case 41: case 92: case 172:
    case 184: case 215: case 221: case 225: case 238: case 264:
    case 267: case 269: case 272: case 279: case 280: case 282:
    case 299: case 322: case 340: case 342: case 356: case 362:
        return Rarity::UNCOMMON;
    case 123: case 124: case 130: case 133: case 134: case 135:
    case 136: case 147: case 148: case 149: case 196: case 197:
    case 214: case 275: case 281: case 302: case 359:
        return Rarity::RARE;
    case 25:
        return Rarity::VERY_RARE;
    case 1: case 2: case 3: case 4: case 5: case 6:
    case 7: case 8: case 9:
        return Rarity::ULTRA_RARE;
    default: return Rarity::NORMAL;
    }
}

constexpr uint8_t baseWeight(Rarity rarity) {
    switch (rarity) {
    case Rarity::COMMON: return 12;
    case Rarity::NORMAL: return 8;
    case Rarity::UNCOMMON: return 5;
    case Rarity::RARE: return 3;
    case Rarity::VERY_RARE: return 2;
    case Rarity::ULTRA_RARE: return 1;
    }
    return 0;
}

struct Entry {
    uint16_t speciesId = 0;
    uint8_t minLevel = 0;
    uint8_t maxLevel = 0;
    uint8_t habitat = 0;

    constexpr Rarity rarity() const { return rarityForSpecies(speciesId); }
    constexpr uint8_t weight() const { return baseWeight(rarity()); }
};

template <size_t N>
constexpr bool levelRangesValid(const Entry (&entries)[N], size_t index = 0) {
    return index == N ||
           (entries[index].minLevel >= 1 &&
            entries[index].minLevel <= entries[index].maxLevel &&
            entries[index].maxLevel <= Game::LEVEL_MAX &&
            levelRangesValid(entries, index + 1));
}

template<class V> inline void contentFields(V& v, Entry& entry) {
    // The v1 record retains its original width and offsets. The old rarity
    // byte uses only three bits. The marker distinguishes an explicitly empty
    // habitat in a new record from a legacy record written before habitats.
    constexpr uint8_t HAS_HABITAT = 0x80;
    uint8_t exportedWeight = entry.weight();
    uint8_t tierAndHabitat = HAS_HABITAT | uint8_t(entry.rarity()) | (entry.habitat << 3);
    v(entry.speciesId, exportedWeight, entry.minLevel, entry.maxLevel, tierAndHabitat);
    if constexpr (std::is_same_v<V, ContentData::Reader>) {
        const bool hasHabitat = (tierAndHabitat & HAS_HABITAT) != 0;
        const uint8_t storedHabitat = (tierAndHabitat & ~HAS_HABITAT) >> 3;
        if (!exportedWeight || (tierAndHabitat & 7U) > uint8_t(Rarity::ULTRA_RARE) ||
            (storedHabitat & ~ExploreHabitat::KNOWN) || (!hasHabitat && storedHabitat)) {
            v.good = false;
        }
        entry.habitat = storedHabitat;
        if (!hasHabitat && (entry.speciesId == 129 || entry.speciesId == 130)) {
            entry.habitat = ExploreHabitat::OPEN_WATER;
        }
    }
}
#if STICKMON_EXTERNAL_CONTENT
using GRASS_PATH_ContentItem = Entry;
inline const ContentData::Table<GRASS_PATH_ContentItem,ExplorePool::MAX_SOURCE_ENTRIES,false> GRASS_PATH{30};
#else
static constexpr Entry GRASS_PATH[] = {
    {10, 1, 6},
    {161, 1, 9},
    {16, 2, 9},
    {261, 2, 9},
    {280, 3, 9},
    {172, 3, 9},
    {11, 7, 9},
    {133, 3, 9},
    {1, 3, 9},
    {4, 3, 9},
    {263, 2, 9},
    {265, 1, 6},
    {273, 3, 9},
    {270, 3, 9},
    {285, 4, 9},
    {298, 4, 9},
    {266, 7, 9},
    {268, 7, 9},
};
#endif

#if STICKMON_EXTERNAL_CONTENT
using CREEK_SLOPE_ContentItem = Entry;
inline const ContentData::Table<CREEK_SLOPE_ContentItem,ExplorePool::MAX_SOURCE_ENTRIES,false> CREEK_SLOPE{31};
#else
static constexpr Entry CREEK_SLOPE[] = {
    {194, 7, 17},
    {298, 7, 15},
    {183, 8, 17},
    {278, 7, 17},
    {129, 7, 17, ExploreHabitat::OPEN_WATER},
    {74, 8, 17},
    {322, 8, 17},
    {41, 8, 17},
    {161, 7, 14},
    {261, 7, 17},
    {16, 7, 17},
    {280, 7, 17},
    {147, 10, 17},
    {7, 7, 15},
    {5, 16, 17},
    {263, 7, 17},
    {270, 7, 13},
    {271, 14, 17},
    {339, 9, 17, ExploreHabitat::OPEN_WATER},
    {341, 9, 17, ExploreHabitat::OPEN_WATER},
    {267, 10, 17},
    {269, 10, 17},
};
#endif

#if STICKMON_EXTERNAL_CONTENT
using TALL_GRASS_PARK_ContentItem = Entry;
inline const ContentData::Table<TALL_GRASS_PARK_ContentItem,ExplorePool::MAX_SOURCE_ENTRIES,false> TALL_GRASS_PARK{32};
#else
static constexpr Entry TALL_GRASS_PARK[] = {
    {12, 17, 27},
    {285, 17, 22},
    {25, 17, 27},
    {162, 17, 27},
    {281, 20, 27},
    {17, 18, 27},
    {184, 18, 27},
    {279, 25, 27},
    {130, 20, 27, ExploreHabitat::OPEN_WATER},
    {278, 17, 24},
    {322, 17, 27},
    {286, 23, 27},
    {26, 17, 27},
    {92, 17, 24},
    {133, 17, 27},
    {123, 17, 27},
    {2, 17, 27},
    {147, 17, 27},
    {8, 17, 27},
    {263, 17, 19},
    {264, 20, 27},
    {267, 17, 27},
    {269, 17, 27},
    {274, 17, 27},
};
#endif

#if STICKMON_EXTERNAL_CONTENT
using FROST_CRYSTAL_CAVE_ContentItem = Entry;
inline const ContentData::Table<FROST_CRYSTAL_CAVE_ContentItem,ExplorePool::MAX_SOURCE_ENTRIES,false> FROST_CRYSTAL_CAVE{33};
#else
static constexpr Entry FROST_CRYSTAL_CAVE[] = {
    {361, 28, 41},
    {42, 28, 41},
    {75, 28, 41},
    {93, 28, 41},
    {282, 30, 42},
    {92, 28, 41},
    {148, 30, 41},
    {41, 28, 41},
    {362, 42, 42},
    {18, 36, 41},
    {17, 28, 35},
    {281, 28, 29},
    {9, 36, 41},
    {220, 28, 32},
    {221, 33, 42},
    {215, 31, 42},
    {225, 29, 39},
    {238, 28, 29},
    {124, 30, 42},
    {299, 29, 42, ExploreHabitat::CAVE},
    {302, 34, 42, ExploreHabitat::CAVE},
    {355, 29, 36},
    {356, 37, 42},
    {359, 37, 42},
};
#endif

#if STICKMON_EXTERNAL_CONTENT
using MIST_FOREST_PATH_ContentItem = Entry;
inline const ContentData::Table<MIST_FOREST_PATH_ContentItem,ExplorePool::MAX_SOURCE_ENTRIES,false> MIST_FOREST_PATH{34};
#else
static constexpr Entry MIST_FOREST_PATH[] = {
    {42, 41, 53},
    {286, 41, 53},
    {262, 41, 53},
    {282, 41, 53},
    {362, 42, 53},
    {93, 41, 53},
    {134, 41, 53},
    {135, 41, 53},
    {136, 41, 53},
    {196, 41, 53},
    {197, 41, 53},
    {148, 41, 49},
    {3, 41, 53},
    {214, 43, 53},
    {275, 47, 53},
    {356, 41, 53},
    {359, 45, 53},
    {123, 41, 50},
    {264, 41, 49},
    {267, 41, 51},
    {269, 41, 51},
    {274, 41, 47},
    {272, 42, 53},
    {215, 41, 47},
};
#endif

#if STICKMON_EXTERNAL_CONTENT
using ANCIENT_WATERFALL_VALLEY_ContentItem = Entry;
inline const ContentData::Table<ANCIENT_WATERFALL_VALLEY_ContentItem,ExplorePool::MAX_SOURCE_ENTRIES,false> ANCIENT_WATERFALL_VALLEY{35};
#else
static constexpr Entry ANCIENT_WATERFALL_VALLEY[] = {
    {42, 53, 67},
    {323, 53, 67},
    {195, 53, 67},
    {130, 53, 67, ExploreHabitat::OPEN_WATER},
    {75, 53, 67},
    {279, 53, 67},
    {184, 53, 67},
    {362, 53, 67},
    {149, 53, 67},
    {262, 53, 67},
    {93, 53, 67},
    {6, 53, 67},
    {9, 53, 67},
    {3, 53, 67},
    {299, 53, 64, ExploreHabitat::CAVE},
    {302, 55, 67, ExploreHabitat::CAVE},
    {340, 53, 67, ExploreHabitat::OPEN_WATER},
    {342, 53, 67, ExploreHabitat::OPEN_WATER},
    {272, 53, 63},
    {214, 53, 61},
    {359, 57, 67},
    {356, 55, 67},
    {275, 53, 64},
    {123, 53, 63},
};
#endif

#if !STICKMON_EXTERNAL_CONTENT
static_assert(levelRangesValid(GRASS_PATH) &&
                  levelRangesValid(CREEK_SLOPE) &&
                  levelRangesValid(TALL_GRASS_PARK) &&
                  levelRangesValid(FROST_CRYSTAL_CAVE) &&
                  levelRangesValid(MIST_FOREST_PATH) &&
                  levelRangesValid(ANCIENT_WATERFALL_VALLEY),
              "explore encounter level range");
#endif

struct Table {
    const Entry* entries = nullptr;
    uint8_t count = 0;
};

template <size_t N>
constexpr Table table(const Entry (&entries)[N]) {
    static_assert(N <= ExplorePool::MAX_SOURCE_ENTRIES, "explore encounter table capacity");
    return {entries, static_cast<uint8_t>(N)};
}

template<size_t N,bool Exact> inline Table table(const ContentData::Table<Entry,N,Exact>& entries) {
    return {entries.data(), static_cast<uint8_t>(entries.size())};
}

inline Table tableForArea(uint8_t area) {
    switch (area) {
    case 0: return table(GRASS_PATH);
    case 1: return table(CREEK_SLOPE);
    case 2: return table(TALL_GRASS_PARK);
    case 3: return table(FROST_CRYSTAL_CAVE);
    case 4: return table(MIST_FOREST_PATH);
    case 5: return table(ANCIENT_WATERFALL_VALLEY);
    default: return {};
    }
}

struct BattleCandidates {
    struct Candidate {
        const Entry* entry;
        uint8_t weight;
    };
    Candidate entries[ExplorePool::MAX_SOURCE_ENTRIES]{};
    uint8_t count = 0;
    uint16_t total = 0;

    const Entry* at(uint16_t roll) const {
        for (uint8_t i = 0; i < count; ++i) {
            if (roll < entries[i].weight) return entries[i].entry;
            roll -= entries[i].weight;
        }
        return nullptr;
    }
};

inline BattleCandidates battleCandidates(Table table, const ExplorePool::Pool& pool,
                                          uint8_t mapHabitat) {
    BattleCandidates candidates;
    if (!table.entries) return candidates;
    const auto add = [&](const Entry& entry, uint8_t weight) {
        if (!ExploreHabitat::matches(entry.habitat, mapHabitat) ||
            candidates.count == ExplorePool::MAX_SOURCE_ENTRIES) return;
        candidates.entries[candidates.count++] = {&entry, weight};
        candidates.total += weight;
    };
    if (pool.count == 0) {
        for (uint8_t i = 0; i < table.count; ++i) add(table.entries[i], table.entries[i].weight());
    } else {
        for (uint8_t i = 0; i < pool.count && i < ExplorePool::POOL_CAP; ++i) {
            const auto& member = pool.entries[i];
            for (uint8_t j = 0; j < table.count; ++j) {
                const auto& entry = table.entries[j];
                if (entry.speciesId != member.speciesId) continue;
                add(entry, ExplorePool::rollWeightOf(member));
                break;
            }
        }
    }
    return candidates;
}

inline ExplorePool::Pool poolForArea(uint8_t area, uint32_t gameMinutesTotal,
                                     uint8_t rerollCount) {
    const Table encounterTable = tableForArea(area);
    if (!encounterTable.entries || encounterTable.count == 0) return {};
    ExplorePool::SourceEntry source[ExplorePool::MAX_SOURCE_ENTRIES] = {};
    const uint8_t count = encounterTable.count < ExplorePool::MAX_SOURCE_ENTRIES
        ? encounterTable.count : ExplorePool::MAX_SOURCE_ENTRIES;
    for (uint8_t index = 0; index < count; ++index) {
        const Entry& entry = encounterTable.entries[index];
        source[index] = {entry.speciesId, entry.weight(), entry.rarity(), entry.habitat};
    }
    return ExplorePool::buildPool(
        source, count,
        ExplorePool::mixSeed(ExplorePool::slotIndexFor(gameMinutesTotal),
                             area, rerollCount));
}

} // namespace ExploreEncounters
