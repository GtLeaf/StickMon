#pragma once

#include <cstdint>
#include "game/BondSystem.h"
#include "game/GameState.h"
#include "game/Species.h"
#include "game/SpeciesBehavior.h"

// 照护逻辑（HP 恢复 / 饥饿衰减 / 濒死休息 / 日计数重置）的纯逻辑实现。
// 由 GameEngine::tickCare（清醒时逐真实分钟）与深度睡眠定时静默唤醒路径
// （main.cpp → GameEngine::runSilentCareWake）共用；不依赖 Hal/显示/Arduino
// min/max，可直接对 GameState 调用，也可被 host 测试包含。
namespace Game {

constexpr uint16_t GAME_MINUTES_PER_DAY = 24U * 60U;
constexpr uint16_t HP_RECOVERY_INTERVAL_MIN = 5;
constexpr uint8_t HP_RECOVERY_PERCENT_PER_TICK = 10;
constexpr uint8_t HP_RECOVERY_EMPTY_GAIN_PER_TICK = 1;
constexpr uint32_t FAINT_REST_SECONDS = 60UL * 60UL;
constexpr uint8_t SATIETY_DECAY_AWAKE_INTERVAL_MIN = 1;
constexpr uint8_t SATIETY_DECAY_SLEEP_INTERVAL_MIN = 3;
constexpr uint8_t SATIETY_DECAY_MAX_DROP_PER_TICK = 4;
constexpr uint16_t BASE_SLEEP_START_MINUTE = 22U * 60U;
constexpr uint16_t BASE_SLEEP_END_MINUTE = 6U * 60U;
constexpr int16_t NATURE_SLEEP_OFFSET_MINUTE = 30;

struct MonsterSleepSchedule {
    uint16_t startMinute;
    uint16_t endMinute;
};

constexpr bool isMinuteInSleepWindow(uint16_t minutesOfDay,
                                     uint16_t startMinute,
                                     uint16_t endMinute) {
    return minutesOfDay < endMinute || minutesOfDay >= startMinute;
}

static_assert(!isMinuteInSleepWindow(21U * 60U + 59U,
                                    BASE_SLEEP_START_MINUTE, BASE_SLEEP_END_MINUTE),
              "baseline 21:59 must be awake time");
static_assert(isMinuteInSleepWindow(22U * 60U,
                                   BASE_SLEEP_START_MINUTE, BASE_SLEEP_END_MINUTE),
              "baseline 22:00 must be sleep time");
static_assert(isMinuteInSleepWindow(5U * 60U + 59U,
                                   BASE_SLEEP_START_MINUTE, BASE_SLEEP_END_MINUTE),
              "baseline 05:59 must be sleep time");
static_assert(!isMinuteInSleepWindow(6U * 60U,
                                    BASE_SLEEP_START_MINUTE, BASE_SLEEP_END_MINUTE),
              "baseline 06:00 must be awake time");

inline MonsterSleepSchedule sleepScheduleForNature(uint8_t nature) {
    uint8_t boosted = natureBoostStat(nature);
    uint8_t lowered = natureLowerStat(nature);
    if (boosted == 5 && lowered != 5) {
        return {
            static_cast<uint16_t>(BASE_SLEEP_START_MINUTE + NATURE_SLEEP_OFFSET_MINUTE),
            static_cast<uint16_t>(BASE_SLEEP_END_MINUTE - NATURE_SLEEP_OFFSET_MINUTE),
        };
    }
    if (lowered == 5 && boosted != 5) {
        return {
            static_cast<uint16_t>(BASE_SLEEP_START_MINUTE - NATURE_SLEEP_OFFSET_MINUTE),
            static_cast<uint16_t>(BASE_SLEEP_END_MINUTE + NATURE_SLEEP_OFFSET_MINUTE),
        };
    }
    return {BASE_SLEEP_START_MINUTE, BASE_SLEEP_END_MINUTE};
}

inline bool isScheduledSleepMinute(uint16_t minutesOfDay, uint8_t nature) {
    MonsterSleepSchedule schedule = sleepScheduleForNature(nature);
    return isMinuteInSleepWindow(minutesOfDay, schedule.startMinute, schedule.endMinute);
}

inline bool isSleepCareTime(uint32_t gameMinutesTotal, uint8_t nature) {
    uint16_t minutesOfDay = (uint16_t)(gameMinutesTotal % GAME_MINUTES_PER_DAY);
    return isScheduledSleepMinute(minutesOfDay, nature);
}

inline uint32_t gameSecondsForMinutes(uint32_t minutes) {
    uint64_t seconds = static_cast<uint64_t>(minutes) * 60ULL;
    return seconds > 0xFFFFFFFFULL ? 0xFFFFFFFFUL : static_cast<uint32_t>(seconds);
}

// 跨 tickCare 调用保留的会话型累加器（不持久化）。
// 静默唤醒路径每次从 0 开始：单块内的间隔取整会有轻微近似，但不影响长期行为。
struct CareTickAccumulators {
    uint16_t hpRecoveryMinuteAcc = 0;
    uint16_t satietyDecayMinuteAcc[TEAM_CAP] = {};
    bool satietyDecayWasSleeping[TEAM_CAP] = {};
};

// 日计数重置（careDay/stepsToday 等）；返回是否有字段变化。
inline bool resetDailyCareCounters(GameState& state) {
    uint32_t day = state.gameMinutesTotal / GAME_MINUTES_PER_DAY;
    if (day > 0xFFFF) day = 0xFFFF;
    if (state.careDay == (uint16_t)day) return false;

    state.careDay = (uint16_t)day;
    state.careExpToday = 0;
    state.stepsToday = 0;
    state.walkExpToday = 0;
    state.pairMoodRewardsToday = 0;
    for (uint8_t i = 0; i < state.teamCount && i < Game::TEAM_CAP; ++i) {
        state.team[i].petCountToday = 0;
    }
    for (uint8_t i = 0; i < state.storageCount && i < Game::STORAGE_CAP; ++i) {
        if ((state.storage[i].petCountToday & Bond::INVITE_LOCK_FLAG) == 0) {
            state.storage[i].petCountToday = 0;
        }
    }
    return true;
}

// 对 state 应用 elapsedMin（真实流逝分钟）的照护推进。
// homeRecovery=true 时进行居家 HP 恢复与濒死休息结算；false 时濒死精灵只刷新
// lastSeenAt（探索中不休息）。gameSpeed 用于 HP 恢复的游戏时间缩放（与清醒
// tickCare 一致）。返回本次濒死休息完成（复活）的精灵数，供调用方记日志。
inline uint8_t applyCareMinutes(GameState& state, CareTickAccumulators& acc,
                                uint32_t elapsedMin, float gameSpeed,
                                bool homeRecovery) {
    uint16_t hpRecoveryTicks = 0;
    if (homeRecovery) {
        uint32_t scaledHpRecoveryMin = (uint32_t)((float)elapsedMin * gameSpeed);
        uint32_t total = (uint32_t)acc.hpRecoveryMinuteAcc + scaledHpRecoveryMin;
        if (total > 60000UL) total = 60000UL;
        hpRecoveryTicks = (uint16_t)(total / HP_RECOVERY_INTERVAL_MIN);
        acc.hpRecoveryMinuteAcc = (uint16_t)(total % HP_RECOVERY_INTERVAL_MIN);
    }
    uint32_t nowGameSec = gameSecondsForMinutes(state.gameMinutesTotal);
    uint8_t faintRevivals = 0;

    auto updateHealthRecovery = [&](MonsterRuntime& mon) {
        if (!homeRecovery) {
            if (mon.fainted) mon.lastSeenAt = nowGameSec;
            return;
        }
        if (mon.fainted) {
            if (mon.lastSeenAt == 0 || mon.lastSeenAt > nowGameSec) {
                mon.lastSeenAt = nowGameSec;
            }
            uint32_t elapsedFaintSec = nowGameSec - mon.lastSeenAt;
            if (elapsedFaintSec >= FAINT_REST_SECONDS) {
                mon.fainted = false;
                mon.majorStatus = MajorStatus::NONE;
                mon.majorStatusTurns = 0;
                uint16_t restHp =
                    (uint16_t)((mon.hpMax * HP_RECOVERY_PERCENT_PER_TICK + 99) / 100);
                mon.hpCur = restHp > 1 ? restHp : 1;
                mon.lastSeenAt = nowGameSec;
                ++faintRevivals;
            }
            return;
        }

        if (hpRecoveryTicks > 0 && mon.hpCur < mon.hpMax) {
            uint16_t gainPerTick = HP_RECOVERY_EMPTY_GAIN_PER_TICK;
            if (mon.satiety != 0) {
                gainPerTick =
                    (uint16_t)((mon.hpMax * HP_RECOVERY_PERCENT_PER_TICK + 99) / 100);
                if (gainPerTick < 1) gainPerTick = 1;
            }
            uint32_t gain = (uint32_t)gainPerTick * hpRecoveryTicks;
            uint32_t healed = (uint32_t)mon.hpCur + gain;
            mon.hpCur = healed > mon.hpMax ? mon.hpMax : (uint16_t)healed;
        }
    };

    for (uint8_t i = 0; i < state.teamCount && i < Game::TEAM_CAP; ++i) {
        MonsterRuntime& mon = state.team[i];
        const SpeciesCareProfile careProfile = speciesCareProfileFor(mon.speciesId);
        if (mon.origin != Origin::VISITOR) {
            if (!careProfile.satietyDecays) {
                acc.satietyDecayMinuteAcc[i] = 0;
                acc.satietyDecayWasSleeping[i] = false;
                mon.satiety = 100;
            } else {
                bool sleeping =
                    mon.majorStatus == MajorStatus::SLEEP ||
                    isSleepCareTime(state.gameMinutesTotal, mon.nature);
                uint8_t decayInterval =
                    sleeping ? SATIETY_DECAY_SLEEP_INTERVAL_MIN
                             : SATIETY_DECAY_AWAKE_INTERVAL_MIN;
                if (sleeping != acc.satietyDecayWasSleeping[i]) {
                    acc.satietyDecayMinuteAcc[i] = 0;
                    acc.satietyDecayWasSleeping[i] = sleeping;
                }
                uint32_t decayTotal =
                    (uint32_t)acc.satietyDecayMinuteAcc[i] + elapsedMin;
                if (decayTotal > 60000UL) decayTotal = 60000UL;
                uint32_t drop = decayTotal / decayInterval;
                if (drop > SATIETY_DECAY_MAX_DROP_PER_TICK) {
                    drop = SATIETY_DECAY_MAX_DROP_PER_TICK;
                }
                acc.satietyDecayMinuteAcc[i] =
                    (uint16_t)(decayTotal % decayInterval);
                mon.satiety = mon.satiety > drop
                    ? (uint8_t)(mon.satiety - drop)
                    : 0;
            }
        }
        uint8_t targetMood = mon.satiety > 60 ? 75 : (mon.satiety > 25 ? 55 : 35);
        if (mon.fainted) targetMood = 20;
        if (mon.mood < targetMood) mon.mood++;
        else if (mon.mood > targetMood) mon.mood--;
        updateHealthRecovery(mon);
    }
    for (uint8_t i = 0; i < state.storageCount && i < Game::STORAGE_CAP; ++i) {
        updateHealthRecovery(state.storage[i]);
    }
    return faintRevivals;
}

}  // namespace Game
