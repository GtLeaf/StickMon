#pragma once

#include <cstdint>

// 栖息地轮换（大量出现）活跃池逻辑，规则见 doc/探索模式精灵分布方案_v1.0.md §七。
// 纯数据 + inline 实现，不依赖 Arduino / 硬件，固件与主机端测试（tools/explore_pool_host.cpp）共用。
namespace ExplorePool {

static constexpr uint8_t POOL_CAP = 6;           // 活跃池容量（预览 2x3 图标阵）
static constexpr uint8_t MAX_SOURCE_ENTRIES = 24; // 单区域遭遇表条目上限
static constexpr uint8_t RARE_ROLL_BONUS = 3;    // 稀有成员池内滚点权重倍率（§7.9.3）
static constexpr uint8_t RARE_ROLL_WEIGHT_CAP = 10;
static constexpr uint8_t BASE_SLOTS_MIN = 4;     // 基础槽 4~5 只普通（§7.3）
static constexpr uint8_t BASE_SLOTS_MAX = 5;
static constexpr uint32_t SLOT_PERIOD_MINUTES = 480; // 8 小时一个时段（§7.2）

enum class Rarity : uint8_t {
    COMMON = 0,
    NORMAL,
    UNCOMMON,
    RARE,
    VERY_RARE,
    ULTRA_RARE,
};

constexpr bool isRare(Rarity rarity) {
    return static_cast<uint8_t>(rarity) >=
           static_cast<uint8_t>(Rarity::RARE);
}

// 稀有度决定分池，权重只影响池内遭遇滚点。
struct SourceEntry {
    uint16_t speciesId;
    uint8_t weight;
    Rarity rarity;
};

struct PoolEntry {
    uint16_t speciesId;
    uint8_t weight;
    Rarity rarity;
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

// 池内滚点权重：稀有成员 ×3，最高 10（§7.4 / §7.9.3）
inline uint8_t rollWeightOf(const PoolEntry& entry) {
    if (!isRare(entry.rarity)) return entry.weight;
    uint16_t boosted = static_cast<uint16_t>(entry.weight) * RARE_ROLL_BONUS;
    return static_cast<uint8_t>(
        boosted > RARE_ROLL_WEIGHT_CAP ? RARE_ROLL_WEIGHT_CAP : boosted);
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
        if (isRare(pool.entries[i].rarity)) return true;
    }
    return false;
}

inline uint8_t appendUniqueSpecies(const Pool& pool, uint16_t* speciesIds,
                                   uint8_t count, uint8_t capacity) {
    if (!speciesIds || count >= capacity) return count;
    for (uint8_t entryIndex = 0;
         entryIndex < pool.count && count < capacity;
         ++entryIndex) {
        uint16_t speciesId = pool.entries[entryIndex].speciesId;
        if (speciesId == 0) continue;
        bool duplicate = false;
        for (uint8_t i = 0; i < count; ++i) {
            if (speciesIds[i] == speciesId) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) speciesIds[count++] = speciesId;
    }
    return count;
}

// 由种子确定性重建活跃池：先决定 0~2 个稀有槽，再决定 4~5 个基础槽（§7.3）。
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
        if (isRare(source[i].rarity)) {
            rareIndices[rareCount++] = i;
        } else {
            commonIndices[commonCount++] = i;
        }
    }

    Xorshift32 rng{seed};
    uint8_t rareSlots = rareSlotsForRoll(rng.below(100));
    if (rareSlots > rareCount) rareSlots = rareCount;
    uint8_t baseSlots = rareSlots >= 2
        ? BASE_SLOTS_MIN
        : static_cast<uint8_t>(
              BASE_SLOTS_MIN +
              rng.below(BASE_SLOTS_MAX - BASE_SLOTS_MIN + 1));
    if (baseSlots > commonCount) baseSlots = commonCount;
    if (baseSlots > POOL_CAP - rareSlots) {
        baseSlots = static_cast<uint8_t>(POOL_CAP - rareSlots);
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
        pool.entries[pool.count++] =
            PoolEntry{entry.speciesId, entry.weight, entry.rarity};
    }
    for (uint8_t i = 0; i < rareSlots; ++i) {
        const SourceEntry& entry = source[rareIndices[i]];
        pool.entries[pool.count++] =
            PoolEntry{entry.speciesId, entry.weight, entry.rarity};
    }
    return pool;
}

} // namespace ExplorePool
