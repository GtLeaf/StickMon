// 栖息地轮换活跃池（src/game/ExplorePool.h）主机端测试。
// 验证：池大小 4~6、稀有槽 0/1/2 分布、同种子同池（确定性）、普通/稀有分池阈值、
// 池内归一化权重和（含稀有 ×3 加成）。规则见 doc/探索模式精灵分布方案_v1.0.md §七。
#include <cstdint>
#include <cstdio>

#include "game/ExplorePool.h"

namespace {

// 与固件 ExploreScene.cpp 的六张遭遇表一致（文档 §五）
struct TestEntry {
    uint16_t speciesId;
    uint8_t weight;
};

const TestEntry GRASS_PATH[] = {
    {10, 18}, {161, 19}, {16, 17}, {261, 13}, {280, 8},
    {172, 6}, {11, 15}, {133, 2}, {1, 1}, {4, 1},
};
const TestEntry CREEK_SLOPE[] = {
    {194, 20}, {298, 13}, {183, 8}, {278, 16}, {129, 14}, {74, 10},
    {322, 6}, {41, 7}, {16, 2}, {147, 1}, {7, 1}, {5, 1}, {8, 1},
};
const TestEntry TALL_GRASS_PARK[] = {
    {12, 25}, {285, 19}, {25, 11}, {162, 9}, {281, 6}, {17, 6}, {184, 5},
    {279, 5}, {130, 4}, {26, 3}, {92, 3}, {133, 2}, {123, 1}, {2, 1},
};
const TestEntry FROST_CRYSTAL_CAVE[] = {
    {361, 50}, {42, 16}, {75, 12}, {93, 7}, {148, 5}, {18, 3}, {9, 1}, {282, 6},
};
const TestEntry MIST_FOREST_PATH[] = {
    {94, 22}, {169, 20}, {286, 15}, {262, 12}, {282, 7}, {362, 5}, {134, 3},
    {135, 3}, {136, 3}, {196, 3}, {212, 3}, {197, 3}, {3, 1},
};
const TestEntry ANCIENT_WATERFALL_VALLEY[] = {
    {76, 29}, {169, 22}, {323, 21}, {94, 13}, {195, 8}, {212, 4}, {149, 2}, {6, 1},
};

struct TestTable {
    const TestEntry* entries;
    uint8_t count;
    uint8_t expectedCommon; // 文档 §7.3 池容量校验
    uint8_t expectedRare;
};

#define TABLE_COUNT(t) static_cast<uint8_t>(sizeof(t) / sizeof(t[0]))
const TestTable TABLES[] = {
    {GRASS_PATH, TABLE_COUNT(GRASS_PATH), 7, 3},
    {CREEK_SLOPE, TABLE_COUNT(CREEK_SLOPE), 8, 5},
    {TALL_GRASS_PARK, TABLE_COUNT(TALL_GRASS_PARK), 9, 5},
    {FROST_CRYSTAL_CAVE, TABLE_COUNT(FROST_CRYSTAL_CAVE), 6, 2},
    {MIST_FOREST_PATH, TABLE_COUNT(MIST_FOREST_PATH), 6, 7},
    {ANCIENT_WATERFALL_VALLEY, TABLE_COUNT(ANCIENT_WATERFALL_VALLEY), 6, 2},
};
#undef TABLE_COUNT
constexpr uint8_t AREA_COUNT = sizeof(TABLES) / sizeof(TABLES[0]);

uint8_t toSource(const TestTable& table, ExplorePool::SourceEntry* out) {
    for (uint8_t i = 0; i < table.count; ++i) {
        out[i] = ExplorePool::SourceEntry{table.entries[i].speciesId,
                                          table.entries[i].weight};
    }
    return table.count;
}

int fail(int code, const char* what, uint32_t detail = 0) {
    std::printf("[explore_pool_host] FAIL %d: %s (%lu)\n", code, what,
                static_cast<unsigned long>(detail));
    return code;
}

bool poolContains(const ExplorePool::Pool& pool, uint16_t speciesId) {
    for (uint8_t i = 0; i < pool.count; ++i) {
        if (pool.entries[i].speciesId == speciesId) return true;
    }
    return false;
}

bool samePool(const ExplorePool::Pool& a, const ExplorePool::Pool& b) {
    if (a.count != b.count) return false;
    for (uint8_t i = 0; i < a.count; ++i) {
        if (a.entries[i].speciesId != b.entries[i].speciesId ||
            a.entries[i].weight != b.entries[i].weight ||
            a.entries[i].rare != b.entries[i].rare) {
            return false;
        }
    }
    return true;
}

} // namespace

int main() {
    // 1. 稀有槽概率分档边界（40%/40%/20%）
    if (ExplorePool::rareSlotsForRoll(0) != 0 ||
        ExplorePool::rareSlotsForRoll(39) != 0 ||
        ExplorePool::rareSlotsForRoll(40) != 1 ||
        ExplorePool::rareSlotsForRoll(79) != 1 ||
        ExplorePool::rareSlotsForRoll(80) != 2 ||
        ExplorePool::rareSlotsForRoll(99) != 2) {
        return fail(1, "rare slot roll bands");
    }

    // 2. 分池阈值：weight >= 4 普通池，weight <= 3 稀有池
    if (!ExplorePool::isCommonWeight(4) || ExplorePool::isCommonWeight(3) ||
        !ExplorePool::isRareWeight(3) || ExplorePool::isRareWeight(4)) {
        return fail(2, "common/rare weight thresholds");
    }

    // 3. 各区域表：权重和 = 100，普通/稀有池容量与文档 §7.3 一致
    for (uint8_t area = 0; area < AREA_COUNT; ++area) {
        uint32_t weightTotal = 0;
        uint8_t commons = 0;
        uint8_t rares = 0;
        for (uint8_t i = 0; i < TABLES[area].count; ++i) {
            weightTotal += TABLES[area].entries[i].weight;
            if (ExplorePool::isCommonWeight(TABLES[area].entries[i].weight)) ++commons;
            if (ExplorePool::isRareWeight(TABLES[area].entries[i].weight)) ++rares;
        }
        if (weightTotal != 100) return fail(3, "table weight total != 100", area);
        if (commons != TABLES[area].expectedCommon ||
            rares != TABLES[area].expectedRare) {
            return fail(4, "common/rare pool sizes mismatch doc 7.3", area);
        }
    }

    // 4. 池结构不变式：大小 4~6、成员唯一且来自源表、普通在前稀有在后、
    //    基础槽 4~5、稀有槽 0~2、权重和含稀有 ×3
    for (uint8_t area = 0; area < AREA_COUNT; ++area) {
        ExplorePool::SourceEntry source[ExplorePool::MAX_SOURCE_ENTRIES];
        uint8_t sourceCount = toSource(TABLES[area], source);
        for (uint32_t seedIndex = 0; seedIndex < 2000; ++seedIndex) {
            uint32_t seed = ExplorePool::mixSeed(seedIndex, area, 0);
            ExplorePool::Pool pool = ExplorePool::buildPool(source, sourceCount, seed);
            if (pool.count < 4 || pool.count > ExplorePool::POOL_CAP) {
                return fail(5, "pool size out of 4..6", area);
            }
            uint32_t expectedWeightTotal = 0;
            uint8_t commonSlots = 0;
            uint8_t rareSlots = 0;
            bool seenRare = false;
            for (uint8_t i = 0; i < pool.count; ++i) {
                const ExplorePool::PoolEntry& entry = pool.entries[i];
                bool inSource = false;
                for (uint8_t s = 0; s < sourceCount; ++s) {
                    if (source[s].speciesId == entry.speciesId &&
                        source[s].weight == entry.weight) {
                        inSource = true;
                        break;
                    }
                }
                if (!inSource) return fail(6, "pool member not in source", area);
                for (uint8_t j = (uint8_t)(i + 1); j < pool.count; ++j) {
                    if (pool.entries[j].speciesId == entry.speciesId) {
                        return fail(7, "duplicate pool member", area);
                    }
                }
                if (entry.rare) {
                    seenRare = true;
                    ++rareSlots;
                    if (!ExplorePool::isRareWeight(entry.weight)) {
                        return fail(8, "rare member weight > 3", area);
                    }
                } else {
                    if (seenRare) return fail(9, "commons must precede rares", area);
                    ++commonSlots;
                    if (!ExplorePool::isCommonWeight(entry.weight)) {
                        return fail(10, "common member weight < 4", area);
                    }
                }
                expectedWeightTotal += ExplorePool::rollWeightOf(entry);
            }
            if (commonSlots < 4 || commonSlots > 5) {
                return fail(11, "base slots out of 4..5", area);
            }
            if (rareSlots > 2) return fail(12, "rare slots > 2", area);
            if (ExplorePool::poolWeightTotal(pool) != expectedWeightTotal) {
                return fail(13, "pool weight total mismatch", area);
            }
            if (ExplorePool::poolHasRare(pool) != (rareSlots > 0)) {
                return fail(14, "poolHasRare mismatch", area);
            }

            // 5. 确定性：同种子同池
            ExplorePool::Pool rebuilt =
                ExplorePool::buildPool(source, sourceCount, seed);
            if (!samePool(pool, rebuilt)) {
                return fail(15, "same seed must rebuild the same pool", area);
            }
        }
    }

    // 6. 稀有槽分布：roll 40/40/20，但基础槽为 5 时稀有槽 2 被容量兜底压成 1，
    //    长期期望约 40% / 50% / 10%（基础槽 4/5 各半）
    {
        ExplorePool::SourceEntry source[ExplorePool::MAX_SOURCE_ENTRIES];
        uint8_t sourceCount = toSource(TABLES[4], source); // 迷雾森道：稀有池 7 只
        uint32_t histogram[3] = {0, 0, 0};
        constexpr uint32_t SAMPLES = 30000;
        for (uint32_t i = 0; i < SAMPLES; ++i) {
            ExplorePool::Pool pool = ExplorePool::buildPool(
                source, sourceCount, ExplorePool::mixSeed(i, 4, 0));
            uint8_t rareSlots = 0;
            for (uint8_t j = 0; j < pool.count; ++j) {
                if (pool.entries[j].rare) ++rareSlots;
            }
            ++histogram[rareSlots];
        }
        uint32_t p0 = histogram[0] * 100 / SAMPLES;
        uint32_t p1 = histogram[1] * 100 / SAMPLES;
        uint32_t p2 = histogram[2] * 100 / SAMPLES;
        if (p0 < 35 || p0 > 45) return fail(16, "rare slot 0 rate", p0);
        if (p1 < 44 || p1 > 56) return fail(17, "rare slot 1 rate", p1);
        if (p2 < 5 || p2 > 15) return fail(18, "rare slot 2 rate", p2);
    }

    // 7. 兜底：普通池不足 4 只全取；稀有池为空则稀有槽恒 0
    {
        const ExplorePool::SourceEntry fewCommons[] = {
            {1, 10}, {2, 20}, {3, 30}, {4, 1}, {5, 2},
        };
        for (uint32_t i = 0; i < 500; ++i) {
            ExplorePool::Pool pool = ExplorePool::buildPool(
                fewCommons, 5, ExplorePool::mixSeed(i, 0, 7));
            uint8_t commons = 0;
            uint8_t rares = 0;
            for (uint8_t j = 0; j < pool.count; ++j) {
                pool.entries[j].rare ? ++rares : ++commons;
            }
            if (commons != 3) return fail(19, "small common pool must be taken whole");
            if (rares > 2 || pool.count != commons + rares) {
                return fail(20, "rare slots must fit capacity");
            }
        }

        const ExplorePool::SourceEntry noRares[] = {
            {1, 10}, {2, 20}, {3, 30}, {4, 15}, {5, 25},
        };
        for (uint32_t i = 0; i < 500; ++i) {
            ExplorePool::Pool pool = ExplorePool::buildPool(
                noRares, 5, ExplorePool::mixSeed(i, 1, 3));
            if (ExplorePool::poolHasRare(pool)) {
                return fail(21, "empty rare pool must yield zero rare slots");
            }
            if (pool.count < 4 || pool.count > 5) {
                return fail(22, "common-only pool size out of 4..5");
            }
        }
    }

    // 8. 种子混合：时段序号 / 区域ID / 重抽计数任一变化都改变种子
    {
        uint32_t base = ExplorePool::mixSeed(100, 2, 0);
        if (ExplorePool::mixSeed(101, 2, 0) == base ||
            ExplorePool::mixSeed(100, 3, 0) == base ||
            ExplorePool::mixSeed(100, 2, 1) == base) {
            return fail(23, "mixSeed must react to every input");
        }
        // 时段序号 = 游戏内总分钟 / 480（§7.2）
        if (ExplorePool::slotIndexFor(0) != 0 ||
            ExplorePool::slotIndexFor(479) != 0 ||
            ExplorePool::slotIndexFor(480) != 1 ||
            ExplorePool::slotIndexFor(1440) != 3) {
            return fail(24, "slot index period");
        }
    }

    // 9. 稀有滚点权重 ×3（§7.9.3）
    {
        ExplorePool::PoolEntry rareEntry{4, 1, true};
        ExplorePool::PoolEntry commonEntry{10, 18, false};
        if (ExplorePool::rollWeightOf(rareEntry) != 3 ||
            ExplorePool::rollWeightOf(commonEntry) != 18) {
            return fail(25, "rare roll weight bonus x3");
        }
    }

    std::printf("[explore_pool_host] all tests passed\n");
    return 0;
}
