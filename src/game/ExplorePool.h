#pragma once

#include <cstdint>

// 栖息地轮换（大量出现）活跃池逻辑，规则见 doc/探索模式精灵分布方案_v1.0.md §七。
// 纯数据 + inline 实现，不依赖 Arduino / 硬件，固件与主机端测试（tools/explore_pool_host.cpp）共用。
namespace ExplorePool {

static constexpr uint8_t POOL_CAP = 6;           // 活跃池容量（预览 2x3 图标阵）
static constexpr uint8_t MAX_SOURCE_ENTRIES = 24; // 单区域遭遇表条目上限
static constexpr uint8_t COMMON_WEIGHT_MIN = 4;  // weight >= 4 进普通池（§7.3）
static constexpr uint8_t RARE_WEIGHT_MAX = 3;    // weight <= 3 进稀有池（§7.3）
static constexpr uint8_t RARE_ROLL_BONUS = 3;    // 稀有成员池内滚点权重倍率（§7.9.3）
static constexpr uint8_t BASE_SLOTS_MIN = 4;     // 基础槽 4~5 只普通（§7.3）
static constexpr uint8_t BASE_SLOTS_MAX = 5;
static constexpr uint32_t SLOT_PERIOD_MINUTES = 480; // 8 小时一个时段（§7.2）

// 池抽取的源数据视图：只需遭遇表的 {speciesId, weight} 两列
struct SourceEntry {
    uint16_t speciesId;
    uint8_t weight;
};

struct PoolEntry {
    uint16_t speciesId;
    uint8_t weight; // 原表权重（滚点时稀有成员按 RARE_ROLL_BONUS 倍率计）
    bool rare;
};

struct Pool {
    PoolEntry entries[POOL_CAP];
    uint8_t count = 0;
};

// xorshift32：本地确定性 RNG，池重建不依赖 Arduino random()
struct Xorshift32 {
    uint32_t state;
    uint32_t next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    uint32_t below(uint32_t bound) { return bound == 0 ? 0 : next() % bound; }
};

inline bool isCommonWeight(uint8_t weight) { return weight >= COMMON_WEIGHT_MIN; }
inline bool isRareWeight(uint8_t weight) { return weight <= RARE_WEIGHT_MAX; }

// 时段序号 = 游戏内总分钟 / 480（§7.2）
inline uint32_t slotIndexFor(uint32_t gameMinutesTotal) {
    return gameMinutesTotal / SLOT_PERIOD_MINUTES;
}

// 池种子 = mix(时段序号, 区域ID, 重抽计数)（§7.6）
inline uint32_t mixSeed(uint32_t slotIndex, uint8_t areaId, uint8_t rerollCount) {
    uint32_t seed = slotIndex * 0x9E3779B9UL ^
                    static_cast<uint32_t>(areaId + 1) * 0x85EBCA6BUL ^
                    static_cast<uint32_t>(rerollCount + 1) * 0xC2B2AE35UL;
    seed ^= seed >> 16;
    seed *= 0x7FEB352DUL;
    seed ^= seed >> 15;
    return seed == 0 ? 1 : seed;
}

// 稀有槽数量：0 / 1 / 2 = 40% / 40% / 20%（§7.3），roll 取 [0, 100)
inline uint8_t rareSlotsForRoll(uint32_t roll) {
    return roll < 40 ? 0 : (roll < 80 ? 1 : 2);
}

// 池内滚点权重：稀有成员 ×3（§7.4 / §7.9.3）
inline uint8_t rollWeightOf(const PoolEntry& entry) {
    return entry.rare ? static_cast<uint8_t>(entry.weight * RARE_ROLL_BONUS)
                      : entry.weight;
}

inline uint32_t poolWeightTotal(const Pool& pool) {
    uint32_t total = 0;
    for (uint8_t i = 0; i < pool.count; ++i) {
        total += rollWeightOf(pool.entries[i]);
    }
    return total;
}

inline bool poolHasRare(const Pool& pool) {
    for (uint8_t i = 0; i < pool.count; ++i) {
        if (pool.entries[i].rare) return true;
    }
    return false;
}

// 由种子确定性重建活跃池：基础槽 4~5 只普通（均匀不放回）+ 0~2 只稀有（§7.3）。
// 兜底：普通池不足 4 只全取；稀有池为空则稀有槽恒 0；总容量不超过 POOL_CAP。
inline Pool buildPool(const SourceEntry* source, uint8_t sourceCount, uint32_t seed) {
    Pool pool{};
    if (!source || sourceCount == 0) return pool;
    if (sourceCount > MAX_SOURCE_ENTRIES) sourceCount = MAX_SOURCE_ENTRIES;

    uint8_t commonIndices[MAX_SOURCE_ENTRIES] = {};
    uint8_t rareIndices[MAX_SOURCE_ENTRIES] = {};
    uint8_t commonCount = 0;
    uint8_t rareCount = 0;
    for (uint8_t i = 0; i < sourceCount; ++i) {
        if (isCommonWeight(source[i].weight)) {
            commonIndices[commonCount++] = i;
        } else if (isRareWeight(source[i].weight)) {
            rareIndices[rareCount++] = i;
        }
    }

    Xorshift32 rng{seed};
    uint8_t baseSlots = static_cast<uint8_t>(
        BASE_SLOTS_MIN + rng.below(BASE_SLOTS_MAX - BASE_SLOTS_MIN + 1));
    if (baseSlots > commonCount) baseSlots = commonCount;
    uint8_t rareSlots = rareSlotsForRoll(rng.below(100));
    if (rareSlots > rareCount) rareSlots = rareCount;
    if (rareSlots > POOL_CAP - baseSlots) {
        rareSlots = static_cast<uint8_t>(POOL_CAP - baseSlots);
    }

    // Fisher-Yates 部分洗牌：均匀不放回抽取
    for (uint8_t i = 0; i < baseSlots; ++i) {
        uint8_t j = static_cast<uint8_t>(i + rng.below(commonCount - i));
        uint8_t tmp = commonIndices[i];
        commonIndices[i] = commonIndices[j];
        commonIndices[j] = tmp;
    }
    for (uint8_t i = 0; i < rareSlots; ++i) {
        uint8_t j = static_cast<uint8_t>(i + rng.below(rareCount - i));
        uint8_t tmp = rareIndices[i];
        rareIndices[i] = rareIndices[j];
        rareIndices[j] = tmp;
    }

    for (uint8_t i = 0; i < baseSlots; ++i) {
        const SourceEntry& entry = source[commonIndices[i]];
        pool.entries[pool.count++] = PoolEntry{entry.speciesId, entry.weight, false};
    }
    for (uint8_t i = 0; i < rareSlots; ++i) {
        const SourceEntry& entry = source[rareIndices[i]];
        pool.entries[pool.count++] = PoolEntry{entry.speciesId, entry.weight, true};
    }
    return pool;
}

} // namespace ExplorePool
