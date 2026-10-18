#pragma once

#include <algorithm>
#include "game/GameState.h"
#include "game/Species.h"

namespace Game {
namespace CareExperience {

enum class Source : uint8_t { FOOD, PET, BATH, PLAY };

inline uint32_t rounded(uint32_t numerator, uint32_t denominator) {
    return (numerator + denominator / 2) / denominator;
}

inline uint32_t budgetFor(const MonsterRuntime& monster) {
    const Species* species = findSpecies(monster.speciesId);
    if (!species || monster.level == 0 || monster.level >= LEVEL_MAX ||
        monster.origin == Origin::VISITOR) return 0;
    const uint32_t gap = minimumExpForLevel(species->growthRate, monster.level + 1) -
        minimumExpForLevel(species->growthRate, monster.level);
    const uint32_t percent = monster.level <= 10 ? 40 : monster.level <= 20 ? 30 : 20;
    return (gap * percent + 99) / 100;
}

inline bool matches(const CareRewardRecord& record, const MonsterRuntime& monster) {
    return record.valid && record.ivPacked == monster.ivPacked &&
        record.metAt == monster.metAt && record.nature == monster.nature &&
        record.metArea == monster.metArea && record.origin == monster.origin;
}

inline bool stillOwned(const GameState& state, const CareRewardRecord& record) {
    for (uint8_t i = 0; i < std::min(state.teamCount, TEAM_CAP); ++i)
        if (matches(record, state.team[i])) return true;
    for (uint8_t i = 0; i < std::min(state.storageCount, STORAGE_CAP); ++i)
        if (matches(record, state.storage[i])) return true;
    return false;
}

inline CareRewardRecord* recordFor(GameState& state, const MonsterRuntime& monster) {
    if (monster.origin == Origin::VISITOR) return nullptr;
    for (auto& record : state.careRewards)
        if (matches(record, monster)) return &record;
    CareRewardRecord* slot = nullptr;
    for (auto& record : state.careRewards) {
        if (!record.valid) { slot = &record; break; }
    }
    if (!slot) {
        for (auto& record : state.careRewards) {
            if (!stillOwned(state, record)) { slot = &record; break; }
        }
    }
    if (!slot) return nullptr;
    *slot = {};
    slot->valid = true;
    slot->ivPacked = monster.ivPacked;
    slot->metAt = monster.metAt;
    slot->nature = monster.nature;
    slot->metArea = monster.metArea;
    slot->origin = monster.origin;
    slot->budget = budgetFor(monster);
    return slot;
}

// Called before level-changing rewards and at each game-day rollover. All owned
// monsters get a baseline, including contacts currently outside the room.
inline bool prepareDay(GameState& state) {
    const uint32_t day = state.gameMinutesTotal / 1440;
    if (state.careRewardDay == day) return false;
    state.careRewardDay = day;
    for (auto& record : state.careRewards) {
        if (!stillOwned(state, record)) { record = {}; continue; }
        record.budget = 0;
        for (uint32_t& amount : record.awarded) amount = 0;
        record.bathStageMask = 0;
        record.pairCount = 0;
        // Keep interrupted progression queued across midnight.
        record.valid = record.pendingOldLevel != 0;
    }
    for (uint8_t i = 0; i < std::min(state.teamCount, TEAM_CAP); ++i)
        if (auto* record = recordFor(state, state.team[i])) record->budget = budgetFor(state.team[i]);
    for (uint8_t i = 0; i < std::min(state.storageCount, STORAGE_CAP); ++i)
        if (auto* record = recordFor(state, state.storage[i])) {
            // Team copies are authoritative when the contact mirror is stale.
            bool inTeam = false;
            for (uint8_t j = 0; j < std::min(state.teamCount, TEAM_CAP); ++j)
                inTeam = inTeam || matches(*record, state.team[j]);
            if (!inTeam) record->budget = budgetFor(state.storage[i]);
        }
    return true;
}

inline uint32_t categoryCap(uint32_t budget, Source source) {
    // Allocate rounding residue to food so the four caps sum to the budget.
    const uint32_t pet = rounded(budget, 5);
    const uint32_t bath = rounded(budget * 35, 100);
    const uint32_t play = rounded(budget, 20);
    switch (source) {
    case Source::FOOD: return budget - std::min(budget, pet + bath + play);
    case Source::PET: return pet;
    case Source::BATH: return bath;
    case Source::PLAY: return play;
    }
    return 0;
}

// Reserves experience exactly once; callers apply it through their existing
// experience/progression pipeline so evolution and move learning stay intact.
inline uint32_t grant(GameState& state, uint8_t teamSlot, Source source,
                      uint8_t detail = 0) {
    prepareDay(state);
    if (teamSlot >= std::min(state.teamCount, TEAM_CAP)) return 0;
    const auto& monster = state.team[teamSlot];
    if (monster.fainted || monster.hpCur == 0 || monster.level >= LEVEL_MAX ||
        monster.origin == Origin::VISITOR) return 0;
    auto* record = recordFor(state, monster);
    if (!record || record->budget == 0) return 0;
    const uint32_t cap = categoryCap(record->budget, source);
    const uint8_t index = static_cast<uint8_t>(source);
    uint32_t request = 0;
    switch (source) {
    case Source::FOOD:
        request = std::max<uint32_t>(1, rounded(record->budget,
            detail == ROOM_NORMAL_FOOD_INDEX ? 24 : 6));
        break;
    case Source::PET:
        request = std::max<uint32_t>(1, rounded(record->budget, 20));
        break;
    case Source::BATH: {
        if (detail > 2 || (record->bathStageMask & (1U << detail))) return 0;
        record->bathStageMask |= 1U << detail;
        const uint32_t soap = rounded(cap, 5);
        const uint32_t brush = rounded(cap * 3, 10);
        request = detail == 0 ? soap : detail == 1 ? brush : cap - std::min(cap, soap + brush);
        break;
    }
    case Source::PLAY:
        if (record->pairCount >= 3) return 0;
        ++record->pairCount;
        request = (cap + 2) / 3;
        break;
    }
    uint32_t total = 0;
    for (uint32_t amount : record->awarded) total += amount;
    const uint32_t amount = std::min(request, std::min(
        cap - std::min(cap, record->awarded[index]),
        record->budget - std::min(record->budget, total)));
    record->awarded[index] += amount;
    return amount;
}

} // namespace CareExperience
} // namespace Game
