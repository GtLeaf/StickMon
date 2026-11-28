// 栖息地轮换活跃池主机端测试。
// 固件与测试共用 ExploreEncounters.h，避免区域表发生漂移。
#include <cstdint>
#include <cstdio>
#include <vector>

#include "game/ExploreAreaCatalog.h"
#include "game/ExploreEncounterRules.h"
#include "game/ExploreEncounters.h"
#include "game/ExplorePool.h"

namespace {

using Entry = ExploreEncounters::Entry;
using Rarity = ExplorePool::Rarity;

struct TestTable {
    const Entry* entries;
    uint8_t count;
    uint8_t minLevel;
    uint8_t maxLevel;
    uint8_t availableHabitat;
    uint8_t minimumHabitat = 0;
};

#define TABLE_COUNT(t) static_cast<uint8_t>(sizeof(t) / sizeof(t[0]))
const TestTable TABLES[] = {
    {ExploreEncounters::GRASS_PATH,
     TABLE_COUNT(ExploreEncounters::GRASS_PATH), 1, 9, 0},
    {ExploreEncounters::CREEK_SLOPE,
     TABLE_COUNT(ExploreEncounters::CREEK_SLOPE), 7, 17, ExploreHabitat::OPEN_WATER},
    {ExploreEncounters::TALL_GRASS_PARK,
     TABLE_COUNT(ExploreEncounters::TALL_GRASS_PARK), 17, 27, ExploreHabitat::OPEN_WATER},
    {ExploreEncounters::FROST_CRYSTAL_CAVE,
     TABLE_COUNT(ExploreEncounters::FROST_CRYSTAL_CAVE), 28, 42,
     ExploreHabitat::CAVE, ExploreHabitat::CAVE},
    {ExploreEncounters::MIST_FOREST_PATH,
     TABLE_COUNT(ExploreEncounters::MIST_FOREST_PATH), 41, 53, 0},
    {ExploreEncounters::ANCIENT_WATERFALL_VALLEY,
     TABLE_COUNT(ExploreEncounters::ANCIENT_WATERFALL_VALLEY), 53, 67,
     ExploreHabitat::OPEN_WATER | ExploreHabitat::CAVE},
};
#undef TABLE_COUNT

constexpr uint8_t AREA_COUNT =
    static_cast<uint8_t>(sizeof(TABLES) / sizeof(TABLES[0]));

const uint16_t EXPANSION_SPECIES[] = {
    124, 214, 215, 220, 221, 225, 238, 263, 264, 265, 266, 267, 268, 269,
    270, 271, 272, 273, 274, 275, 299, 302, 339, 340, 341, 342, 355, 356, 359,
};

struct EvolutionRange {
    uint16_t speciesId;
    uint8_t firstLevel;
    uint8_t lastLevel;
};

constexpr EvolutionRange EXPANSION_EVOLUTION_RANGES[] = {
    {124, 30, 100}, {220, 1, 32}, {221, 33, 100}, {238, 1, 29},
    {263, 1, 19}, {264, 20, 100}, {265, 1, 6}, {266, 7, 9},
    {267, 10, 100}, {268, 7, 9}, {269, 10, 100}, {270, 1, 13},
    {271, 14, 100}, {272, 14, 100}, {273, 1, 13}, {274, 14, 100},
    {275, 14, 100}, {339, 1, 29}, {340, 30, 100}, {341, 1, 29},
    {342, 30, 100}, {355, 1, 36}, {356, 37, 100},
};

uint8_t toSource(const TestTable& table, ExplorePool::SourceEntry* out) {
    for (uint8_t i = 0; i < table.count; ++i) {
        out[i] = ExplorePool::SourceEntry{
            table.entries[i].speciesId,
            table.entries[i].weight(),
            table.entries[i].rarity(),
            table.entries[i].habitat,
        };
    }
    return table.count;
}

int fail(int code, const char* what, uint32_t detail = 0) {
    std::printf("[explore_pool_host] FAIL %d: %s (%lu)\n", code, what,
                static_cast<unsigned long>(detail));
    return code;
}

bool samePool(const ExplorePool::Pool& a, const ExplorePool::Pool& b) {
    if (a.count != b.count) return false;
    for (uint8_t i = 0; i < a.count; ++i) {
        if (a.entries[i].speciesId != b.entries[i].speciesId ||
            a.entries[i].weight != b.entries[i].weight ||
            a.entries[i].rarity != b.entries[i].rarity ||
            a.entries[i].habitat != b.entries[i].habitat) {
            return false;
        }
    }
    return true;
}

bool checkEncounterRecords() {
    const uint8_t legacyFish[] = {129, 0, 12, 7, 17, uint8_t(Rarity::NORMAL)};
    ContentData::Reader legacy{legacyFish, sizeof(legacyFish)};
    Entry fish;
    if (!legacy(fish) || legacy.position != sizeof(legacyFish) ||
        fish.speciesId != 129 || fish.minLevel != 7 || fish.maxLevel != 17 ||
        fish.habitat != ExploreHabitat::OPEN_WATER || fish.weight() != 8) return false;

    Entry cave{41, 10, 20, ExploreHabitat::CAVE | ExploreHabitat::OPEN_WATER};
    std::vector<uint8_t> bytes;
    ContentData::Writer writer{&bytes, [](void* context, uint8_t value) {
        static_cast<std::vector<uint8_t>*>(context)->push_back(value);
    }};
    if (!writer(cave) || bytes.size() != sizeof(legacyFish)) return false;
    ContentData::Reader reader{bytes.data(), bytes.size()};
    Entry decoded;
    if (!reader(decoded) || reader.position != bytes.size() ||
        decoded.speciesId != cave.speciesId || decoded.minLevel != cave.minLevel ||
        decoded.maxLevel != cave.maxLevel || decoded.habitat != cave.habitat) return false;

    bytes.clear();
    fish.habitat = 0;
    if (!writer(fish) || bytes.size() != sizeof(legacyFish)) return false;
    ContentData::Reader withoutHabitat{bytes.data(), bytes.size()};
    if (!withoutHabitat(decoded) || decoded.habitat) return false;

    bytes[5] |= 0x40;
    ContentData::Reader invalid{bytes.data(), bytes.size()};
    return !invalid(decoded);
}

} // namespace

int main() {
    // 1. 稀有槽概率分档边界：0 / 1 / 2 = 40% / 40% / 20%。
    if (ExplorePool::rareSlotsForRoll(0) != 0 ||
        ExplorePool::rareSlotsForRoll(39) != 0 ||
        ExplorePool::rareSlotsForRoll(40) != 1 ||
        ExplorePool::rareSlotsForRoll(79) != 1 ||
        ExplorePool::rareSlotsForRoll(80) != 2 ||
        ExplorePool::rareSlotsForRoll(99) != 2) {
        return fail(1, "rare slot roll bands");
    }

    // 2. 显式稀有度决定分池，权重不参与身份判断。
    if (ExplorePool::isRare(Rarity::COMMON) ||
        ExplorePool::isRare(Rarity::NORMAL) ||
        ExplorePool::isRare(Rarity::UNCOMMON) ||
        !ExplorePool::isRare(Rarity::RARE) ||
        !ExplorePool::isRare(Rarity::VERY_RARE) ||
        !ExplorePool::isRare(Rarity::ULTRA_RARE)) {
        return fail(2, "explicit rarity split");
    }

    if (ExploreEncounters::GRASS_PATH[0].weight() != 12 ||
        ExploreEncounters::CREEK_SLOPE[2].weight() != 8 ||
        ExploreEncounters::TALL_GRASS_PARK[2].weight() != 2 ||
        ExploreEncounters::FROST_CRYSTAL_CAVE[4].weight() != 5 ||
        ExploreEncounters::MIST_FOREST_PATH[6].weight() != 3 ||
        ExploreEncounters::ANCIENT_WATERFALL_VALLEY[11].weight() != 1) {
        return fail(25, "six rarity tiers supply automatic base weights");
    }

    const auto creek = ExploreEncounters::tableForArea(1);
    const ExplorePool::Pool fishAndBird{{{129, 8, Rarity::NORMAL},
                                         {278, 8, Rarity::NORMAL}}, 2};
    const auto dry = ExploreEncounters::battleCandidates(creek, fishAndBird, 0);
    const auto wet = ExploreEncounters::battleCandidates(creek, fishAndBird, ExploreHabitat::OPEN_WATER);
    if (dry.total != 8 || dry.at(0)->speciesId != 278 ||
        wet.total != 16 || wet.at(0)->speciesId != 129 || wet.at(8)->speciesId != 278) {
        return fail(26, "fish are restricted to maps with open water");
    }
    const ExplorePool::Pool fishOnly{{{129, 8, Rarity::NORMAL}}, 1};
    if (ExploreEncounters::battleCandidates(creek, fishOnly, 0).total != 0)
        return fail(27, "filtered active pool does not fall back to the region table");
    const Entry pondAndCave[] = {{130, 50, 60, ExploreHabitat::OPEN_WATER},
                                 {41, 50, 60, ExploreHabitat::CAVE}};
    const ExplorePool::Pool mixed{{{130, 3, Rarity::RARE}, {41, 5, Rarity::UNCOMMON}}, 2};
    const auto table = ExploreEncounters::table(pondAndCave);
    const auto pond = ExploreEncounters::battleCandidates(table, mixed, ExploreHabitat::OPEN_WATER);
    const auto cave = ExploreEncounters::battleCandidates(table, mixed, ExploreHabitat::CAVE);
    const auto fallback = ExploreEncounters::battleCandidates(table, {}, ExploreHabitat::OPEN_WATER);
    if (pond.total != 9 || pond.at(8) != &pondAndCave[0] || pond.at(9) ||
        cave.total != 5 || cave.at(4) != &pondAndCave[1] ||
        fallback.total != 3 || fallback.at(2) != &pondAndCave[0])
        return fail(28, "cave gate, rare bonus, and empty snapshot fallback");
    if (!checkEncounterRecords())
        return fail(29, "legacy and current serialized encounter rows");

    // 3. 六区数据约束：权重、等级、分池容量、跨区稀有度和特殊终段隔离。
    constexpr uint8_t weights[] = {12, 8, 5, 3, 2, 1};
    uint16_t seenSpecies[AREA_COUNT * ExplorePool::MAX_SOURCE_ENTRIES] = {};
    Rarity seenRarity[AREA_COUNT * ExplorePool::MAX_SOURCE_ENTRIES] = {};
    uint8_t seenCount = 0;
    const uint16_t forbidden[] = {76, 94, 169, 212};
    for (uint8_t area = 0; area < AREA_COUNT; ++area) {
        const auto& testTable = TABLES[area];
        if (testTable.count > ExplorePool::MAX_SOURCE_ENTRIES)
            return fail(39, "area exceeds encounter table capacity", area);
        if (ExploreEncounters::battleCandidates(ExploreEncounters::tableForArea(area),
                                               {}, testTable.minimumHabitat).count < 15) {
            return fail(40, "at least 15 species even without optional terrain", area);
        }
        uint8_t commons = 0;
        uint8_t rares = 0;
        for (uint8_t i = 0; i < testTable.count; ++i) {
            const Entry& entry = testTable.entries[i];
            if (entry.weight() != weights[static_cast<uint8_t>(entry.rarity())])
                return fail(6, "entry weight must derive from its rarity", entry.speciesId);
            if (entry.minLevel < 1 || entry.minLevel > entry.maxLevel ||
                entry.maxLevel > Game::LEVEL_MAX) {
                return fail(3, "invalid encounter level range", entry.speciesId);
            }
            if (entry.minLevel < testTable.minLevel || entry.maxLevel > testTable.maxLevel)
                return fail(41, "encounter level outside its area band", entry.speciesId);
            if (!ExploreHabitat::matches(entry.habitat, testTable.availableHabitat))
                return fail(42, "encounter habitat unavailable in its area", entry.speciesId);
            for (const auto& range : EXPANSION_EVOLUTION_RANGES) {
                if (entry.speciesId == range.speciesId &&
                    (entry.minLevel < range.firstLevel || entry.maxLevel > range.lastLevel)) {
                    return fail(43, "encounter level outside its evolution stage", entry.speciesId);
                }
            }
            ExplorePool::isRare(entry.rarity()) ? ++rares : ++commons;

            for (uint8_t j = 0;
                 j < static_cast<uint8_t>(sizeof(forbidden) /
                                          sizeof(forbidden[0]));
                 ++j) {
                if (entry.speciesId == forbidden[j]) {
                    return fail(4, "special evolution final in wild table",
                                entry.speciesId);
                }
            }

            bool known = false;
            for (uint8_t j = 0; j < seenCount; ++j) {
                if (seenSpecies[j] != entry.speciesId) continue;
                known = true;
                if (seenRarity[j] != entry.rarity()) {
                    return fail(5, "cross-area rarity mismatch",
                                entry.speciesId);
                }
                break;
            }
            if (!known) {
                seenSpecies[seenCount] = entry.speciesId;
                seenRarity[seenCount] = entry.rarity();
                ++seenCount;
            }
        }
        if (commons < ExplorePool::BASE_SLOTS_MAX || rares < 2) {
            return fail(7, "area must support full ordinary and rare slots", area);
        }
    }
    for (uint16_t species : EXPANSION_SPECIES) {
        bool present = false;
        for (uint8_t i = 0; i < seenCount; ++i) present |= seenSpecies[i] == species;
        if (!present) return fail(44, "expansion species missing from all areas", species);
    }

    // 区域条目复用物种稀有度。
    if (ExplorePool::isRare(ExploreEncounters::CREEK_SLOPE[8].rarity()) ||
        !ExplorePool::isRare(
            ExploreEncounters::TALL_GRASS_PARK[2].rarity())) {
        return fail(8, "rarity must not be derived from weight");
    }

    // 4. 活跃池结构、唯一性、来源、排序、权重和与确定性。
    for (uint8_t area = 0; area < AREA_COUNT; ++area) {
        ExplorePool::SourceEntry source[ExplorePool::MAX_SOURCE_ENTRIES];
        uint8_t sourceCount = toSource(TABLES[area], source);
        const ExploreEncounters::Table encounterTable =
            ExploreEncounters::tableForArea(area);
        if (encounterTable.entries != TABLES[area].entries ||
            encounterTable.count != sourceCount) {
            return fail(36, "shared encounter table selection", area);
        }
        bool reachable[ExplorePool::MAX_SOURCE_ENTRIES] = {};
        for (uint32_t seedIndex = 0; seedIndex < 2000; ++seedIndex) {
            uint32_t seed = ExplorePool::mixSeed(seedIndex, area, 0);
            ExplorePool::Pool pool =
                ExplorePool::buildPool(source, sourceCount, seed);
            if (!samePool(pool, ExploreEncounters::poolForArea(
                    area, seedIndex * ExplorePool::SLOT_PERIOD_MINUTES, 0))) {
                return fail(37, "shared area pool selection", area);
            }
            if (pool.count < 4 || pool.count > ExplorePool::POOL_CAP) {
                return fail(9, "pool size out of 4..6", area);
            }
            if (!ExploreEncounters::battleCandidates(
                    encounterTable, pool, TABLES[area].minimumHabitat).total) {
                return fail(45, "habitat filters eliminated the whole active pool", area);
            }
            const auto eligible = ExploreEncounters::battleCandidates(
                encounterTable, pool, TABLES[area].availableHabitat);
            for (uint8_t i = 0; i < eligible.count; ++i) {
                const auto index = eligible.entries[i].entry - encounterTable.entries;
                reachable[index] = true;
            }

            uint8_t commonSlots = 0;
            uint8_t rareSlots = 0;
            bool seenRare = false;
            uint32_t expectedWeightTotal = 0;
            for (uint8_t i = 0; i < pool.count; ++i) {
                const ExplorePool::PoolEntry& entry = pool.entries[i];
                bool inSource = false;
                for (uint8_t s = 0; s < sourceCount; ++s) {
                    if (source[s].speciesId == entry.speciesId &&
                        source[s].weight == entry.weight &&
                        source[s].rarity == entry.rarity &&
                        source[s].habitat == entry.habitat) {
                        inSource = true;
                        break;
                    }
                }
                if (!inSource) {
                    return fail(10, "pool member not in source", area);
                }
                for (uint8_t j = static_cast<uint8_t>(i + 1);
                     j < pool.count; ++j) {
                    if (pool.entries[j].speciesId == entry.speciesId) {
                        return fail(11, "duplicate pool member", area);
                    }
                }

                if (ExplorePool::isRare(entry.rarity)) {
                    seenRare = true;
                    ++rareSlots;
                } else {
                    if (seenRare) {
                        return fail(12, "commons must precede rares", area);
                    }
                    ++commonSlots;
                }
                expectedWeightTotal += ExplorePool::rollWeightOf(entry);
            }
            if (commonSlots < 4 || commonSlots > 5) {
                return fail(13, "base slots out of 4..5", area);
            }
            if (rareSlots > 2 || (rareSlots == 2 && commonSlots != 4)) {
                return fail(14, "rare slot capacity contract", area);
            }
            if (ExplorePool::poolWeightTotal(pool) != expectedWeightTotal) {
                return fail(15, "pool weight total mismatch", area);
            }
            if (ExplorePool::poolHasRare(pool) != (rareSlots > 0)) {
                return fail(16, "poolHasRare mismatch", area);
            }

            ExplorePool::Pool rebuilt =
                ExplorePool::buildPool(source, sourceCount, seed);
            if (!samePool(pool, rebuilt)) {
                return fail(17, "same seed must rebuild same pool", area);
            }
        }
        for (uint8_t i = 0; i < sourceCount; ++i) {
            if (!reachable[i]) return fail(46, "entry never reached by rotation", source[i].speciesId);
        }
    }

    // 5. 实际稀有槽长期分布必须保持 40% / 40% / 20%。
    {
        ExplorePool::SourceEntry source[ExplorePool::MAX_SOURCE_ENTRIES];
        uint8_t sourceCount = toSource(TABLES[4], source);
        uint32_t histogram[3] = {0, 0, 0};
        constexpr uint32_t SAMPLES = 30000;
        for (uint32_t i = 0; i < SAMPLES; ++i) {
            ExplorePool::Pool pool = ExplorePool::buildPool(
                source, sourceCount, ExplorePool::mixSeed(i, 4, 0));
            uint8_t rareSlots = 0;
            for (uint8_t j = 0; j < pool.count; ++j) {
                if (ExplorePool::isRare(pool.entries[j].rarity)) ++rareSlots;
            }
            ++histogram[rareSlots];
        }
        uint32_t p0 = histogram[0] * 100 / SAMPLES;
        uint32_t p1 = histogram[1] * 100 / SAMPLES;
        uint32_t p2 = histogram[2] * 100 / SAMPLES;
        if (p0 < 35 || p0 > 45) return fail(18, "rare slot 0 rate", p0);
        if (p1 < 35 || p1 > 45) return fail(19, "rare slot 1 rate", p1);
        if (p2 < 15 || p2 > 25) return fail(20, "rare slot 2 rate", p2);
    }

    // 6. 兜底：普通池不足时全取；稀有池为空时没有稀有成员。
    {
        const ExplorePool::SourceEntry fewCommons[] = {
            {1, 10, Rarity::COMMON},
            {2, 20, Rarity::NORMAL},
            {3, 30, Rarity::UNCOMMON},
            {4, 1, Rarity::RARE},
            {5, 2, Rarity::ULTRA_RARE},
        };
        for (uint32_t i = 0; i < 500; ++i) {
            ExplorePool::Pool pool = ExplorePool::buildPool(
                fewCommons, 5, ExplorePool::mixSeed(i, 0, 7));
            uint8_t commons = 0;
            uint8_t rares = 0;
            for (uint8_t j = 0; j < pool.count; ++j) {
                ExplorePool::isRare(pool.entries[j].rarity)
                    ? ++rares : ++commons;
            }
            if (commons != 3) {
                return fail(21, "small common pool must be taken whole");
            }
            if (rares > 2 || pool.count != commons + rares) {
                return fail(22, "rare slots must fit capacity");
            }
        }

        const ExplorePool::SourceEntry noRares[] = {
            {1, 10, Rarity::COMMON},
            {2, 20, Rarity::NORMAL},
            {3, 30, Rarity::UNCOMMON},
            {4, 15, Rarity::COMMON},
            {5, 25, Rarity::NORMAL},
        };
        for (uint32_t i = 0; i < 500; ++i) {
            ExplorePool::Pool pool = ExplorePool::buildPool(
                noRares, 5, ExplorePool::mixSeed(i, 1, 3));
            if (ExplorePool::poolHasRare(pool)) {
                return fail(23, "empty rare pool must yield zero rare slots");
            }
            if (pool.count < 4 || pool.count > 5) {
                return fail(24, "common-only pool size out of 4..5");
            }
        }
    }

    // 7. 种子输入与 8 小时时段。
    {
        uint32_t base = ExplorePool::mixSeed(100, 2, 0);
        if (ExplorePool::mixSeed(101, 2, 0) == base ||
            ExplorePool::mixSeed(100, 3, 0) == base ||
            ExplorePool::mixSeed(100, 2, 1) == base) {
            return fail(25, "mixSeed must react to every input");
        }
        if (ExplorePool::slotIndexFor(0) != 0 ||
            ExplorePool::slotIndexFor(479) != 0 ||
            ExplorePool::slotIndexFor(480) != 1 ||
            ExplorePool::slotIndexFor(1440) != 3) {
            return fail(26, "slot index period");
        }
    }

    // 8. 多区域缓存清单去重并保持优先区域顺序。
    {
        ExplorePool::SourceEntry firstSource[ExplorePool::MAX_SOURCE_ENTRIES];
        ExplorePool::SourceEntry secondSource[ExplorePool::MAX_SOURCE_ENTRIES];
        ExplorePool::Pool first = ExplorePool::buildPool(
            firstSource, toSource(TABLES[0], firstSource),
            ExplorePool::mixSeed(0, 0, 0));
        ExplorePool::Pool second = ExplorePool::buildPool(
            secondSource, toSource(TABLES[1], secondSource),
            ExplorePool::mixSeed(0, 1, 0));
        uint16_t speciesIds[ExplorePool::POOL_CAP * 2] = {};
        uint8_t capacity =
            static_cast<uint8_t>(sizeof(speciesIds) / sizeof(speciesIds[0]));
        uint8_t count = ExplorePool::appendUniqueSpecies(
            first, speciesIds, 0, capacity);
        count = ExplorePool::appendUniqueSpecies(
            first, speciesIds, count, capacity);
        if (count != first.count) {
            return fail(27, "cache manifest must deduplicate pools", count);
        }
        uint8_t combined = ExplorePool::appendUniqueSpecies(
            second, speciesIds, count, capacity);
        if (combined < count || combined > capacity) {
            return fail(28, "cache manifest count out of range", combined);
        }
        for (uint8_t i = 0; i < first.count; ++i) {
            if (speciesIds[i] != first.entries[i].speciesId) {
                return fail(29, "cache manifest priority order", i);
            }
        }
    }

    // 9. 稀有滚点权重 ×3 且封顶 10。
    {
        ExplorePool::PoolEntry rareLow{4, 1, Rarity::RARE};
        ExplorePool::PoolEntry rareMid{130, 4, Rarity::RARE};
        ExplorePool::PoolEntry rareHigh{25, 10, Rarity::VERY_RARE};
        ExplorePool::PoolEntry common{10, 18, Rarity::COMMON};
        if (ExplorePool::rollWeightOf(rareLow) != 3 ||
            ExplorePool::rollWeightOf(rareMid) != 10 ||
            ExplorePool::rollWeightOf(rareHigh) != 10 ||
            ExplorePool::rollWeightOf(common) != 18) {
            return fail(30, "rare roll weight bonus/cap");
        }
    }

    for (uint8_t area = 0; area < AREA_COUNT; ++area) {
        const uint8_t average = ExploreAreaCatalog::recommendedLevel(area);
        const uint8_t spread = ExploreAreaCatalog::depthSpread(area);
        if (ExploreEncounterRules::targetLevel(
                average, spread, 0, 3, 0, 11) != average - spread ||
            ExploreEncounterRules::targetLevel(
                average, spread, 1, 3, 0, 11) != average ||
            ExploreEncounterRules::targetLevel(
                average, spread, 2, 3, 10, 11) != average + spread) {
            return fail(31, "wild target must follow route depth", area);
        }
    }
    if (ExploreEncounterRules::depthLevelOffset(332, 4) != -4 ||
        ExploreEncounterRules::depthLevelOffset(333, 4) != 0 ||
        ExploreEncounterRules::depthLevelOffset(666, 4) != 0 ||
        ExploreEncounterRules::depthLevelOffset(667, 4) != 4) {
        return fail(32, "route depth band boundaries");
    }
    if (ExploreEncounterRules::levelForRoll(1, 100, 10, 9) != 8 ||
        ExploreEncounterRules::levelForRoll(1, 100, 10, 10) != 9 ||
        ExploreEncounterRules::levelForRoll(1, 100, 10, 29) != 9 ||
        ExploreEncounterRules::levelForRoll(1, 100, 10, 30) != 10 ||
        ExploreEncounterRules::levelForRoll(1, 100, 10, 69) != 10 ||
        ExploreEncounterRules::levelForRoll(1, 100, 10, 70) != 11 ||
        ExploreEncounterRules::levelForRoll(1, 100, 10, 89) != 11 ||
        ExploreEncounterRules::levelForRoll(1, 100, 10, 90) != 12 ||
        ExploreEncounterRules::levelForRoll(1, 6, 50, 90) != 6 ||
        ExploreEncounterRules::levelForRoll(28, 29, 5, 0) != 28) {
        return fail(33, "wild level probabilities and species bounds");
    }
    ExplorePool::Pool rollPool{};
    rollPool.entries[0] = {10, 2, Rarity::COMMON};
    rollPool.entries[1] = {4, 1, Rarity::RARE};
    rollPool.count = 2;
    for (uint32_t roll = 0; roll < 5; ++roll) {
        const ExplorePool::PoolEntry* picked =
            ExplorePool::entryForRoll(rollPool, roll);
        if (!picked || picked->speciesId != (roll < 2 ? 10 : 4)) {
            return fail(34, "active pool weighted selection", roll);
        }
    }
    if (ExplorePool::entryForRoll(rollPool, 5) ||
        ExplorePool::entryForRoll(ExplorePool::Pool{}, 0)) {
        return fail(35, "active pool roll must reject empty weights");
    }
    if (ExploreEncounters::tableForArea(AREA_COUNT).entries ||
        ExploreEncounters::poolForArea(AREA_COUNT, 0, 0).count != 0) {
        return fail(38, "unknown area has no encounter pool");
    }

    std::printf("[explore_pool_host] all tests passed\n");
    return 0;
}
