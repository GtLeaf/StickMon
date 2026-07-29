// 栖息地轮换活跃池主机端测试。
// 固件与测试共用 ExploreEncounters.h，避免区域表发生漂移。
#include <cstdint>
#include <cstdio>

#include "game/ExploreEncounters.h"
#include "game/ExplorePool.h"

namespace {

using Entry = ExploreEncounters::Entry;
using Rarity = ExplorePool::Rarity;

struct TestTable {
    const Entry* entries;
    uint8_t count;
    uint8_t expectedCommon;
    uint8_t expectedRare;
};

#define TABLE_COUNT(t) static_cast<uint8_t>(sizeof(t) / sizeof(t[0]))
const TestTable TABLES[] = {
    {ExploreEncounters::GRASS_PATH,
     TABLE_COUNT(ExploreEncounters::GRASS_PATH), 7, 3},
    {ExploreEncounters::CREEK_SLOPE,
     TABLE_COUNT(ExploreEncounters::CREEK_SLOPE), 12, 3},
    {ExploreEncounters::TALL_GRASS_PARK,
     TABLE_COUNT(ExploreEncounters::TALL_GRASS_PARK), 11, 8},
    {ExploreEncounters::FROST_CRYSTAL_CAVE,
     TABLE_COUNT(ExploreEncounters::FROST_CRYSTAL_CAVE), 10, 3},
    {ExploreEncounters::MIST_FOREST_PATH,
     TABLE_COUNT(ExploreEncounters::MIST_FOREST_PATH), 6, 7},
    {ExploreEncounters::ANCIENT_WATERFALL_VALLEY,
     TABLE_COUNT(ExploreEncounters::ANCIENT_WATERFALL_VALLEY), 9, 5},
};
#undef TABLE_COUNT

constexpr uint8_t AREA_COUNT =
    static_cast<uint8_t>(sizeof(TABLES) / sizeof(TABLES[0]));

uint8_t toSource(const TestTable& table, ExplorePool::SourceEntry* out) {
    for (uint8_t i = 0; i < table.count; ++i) {
        out[i] = ExplorePool::SourceEntry{
            table.entries[i].speciesId,
            table.entries[i].weight,
            table.entries[i].rarity,
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
            a.entries[i].rarity != b.entries[i].rarity) {
            return false;
        }
    }
    return true;
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

    // 3. 六区数据约束：权重、等级、分池容量、跨区稀有度和特殊终段隔离。
    uint16_t seenSpecies[128] = {};
    Rarity seenRarity[128] = {};
    uint8_t seenCount = 0;
    const uint16_t forbidden[] = {76, 94, 169, 212};
    for (uint8_t area = 0; area < AREA_COUNT; ++area) {
        uint32_t weightTotal = 0;
        uint8_t commons = 0;
        uint8_t rares = 0;
        for (uint8_t i = 0; i < TABLES[area].count; ++i) {
            const Entry& entry = TABLES[area].entries[i];
            weightTotal += entry.weight;
            if (entry.minLevel < 1 || entry.minLevel > entry.maxLevel ||
                entry.maxLevel > Game::LEVEL_MAX) {
                return fail(3, "invalid encounter level range", entry.speciesId);
            }
            ExplorePool::isRare(entry.rarity) ? ++rares : ++commons;

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
                if (seenRarity[j] != entry.rarity) {
                    return fail(5, "cross-area rarity mismatch",
                                entry.speciesId);
                }
                break;
            }
            if (!known) {
                seenSpecies[seenCount] = entry.speciesId;
                seenRarity[seenCount] = entry.rarity;
                ++seenCount;
            }
        }
        if (weightTotal != 100) {
            return fail(6, "table weight total != 100", area);
        }
        if (commons != TABLES[area].expectedCommon ||
            rares != TABLES[area].expectedRare) {
            return fail(7, "pool sizes mismatch v2.3 document", area);
        }
    }

    // 低权重尾立仍是普通池，高权重皮卡丘仍是稀有池。
    if (ExplorePool::isRare(ExploreEncounters::CREEK_SLOPE[8].rarity) ||
        !ExplorePool::isRare(
            ExploreEncounters::TALL_GRASS_PARK[2].rarity)) {
        return fail(8, "rarity must not be derived from weight");
    }

    // 4. 活跃池结构、唯一性、来源、排序、权重和与确定性。
    for (uint8_t area = 0; area < AREA_COUNT; ++area) {
        ExplorePool::SourceEntry source[ExplorePool::MAX_SOURCE_ENTRIES];
        uint8_t sourceCount = toSource(TABLES[area], source);
        for (uint32_t seedIndex = 0; seedIndex < 2000; ++seedIndex) {
            uint32_t seed = ExplorePool::mixSeed(seedIndex, area, 0);
            ExplorePool::Pool pool =
                ExplorePool::buildPool(source, sourceCount, seed);
            if (pool.count < 4 || pool.count > ExplorePool::POOL_CAP) {
                return fail(9, "pool size out of 4..6", area);
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
                        source[s].rarity == entry.rarity) {
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

    std::printf("[explore_pool_host] all tests passed\n");
    return 0;
}
