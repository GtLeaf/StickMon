#pragma once

#include <algorithm>
#include "game/ContactRoster.h"
#include "game/ExplorePartyService.h"
#include "game/SpeciesBehavior.h"

namespace Game::ExploreCost {

constexpr uint8_t MIN_DEPARTURE_SATIETY = 20;
constexpr uint8_t STEPS_PER_FOOD = 6;

inline bool needsFood(const MonsterRuntime& monster) {
    return monster.speciesId != 0 && monster.origin != Origin::VISITOR &&
           speciesCareProfileFor(monster.speciesId).needsFood;
}

inline int8_t hungrySlot(const GameState& state) {
    for (uint8_t slot = 0; slot < state.teamCount && slot < TEAM_CAP; ++slot) {
        const auto& monster = state.team[slot];
        if (needsFood(monster) && ExplorePartyService::healthy(monster) &&
            monster.satiety < MIN_DEPARTURE_SATIETY) return slot;
    }
    return -1;
}

// Capture identities when a monster actually enters battle, before any join
// reward can reorder or replace party slots.
struct Battle {
    MonsterRuntime participants[TEAM_CAP]{};
    uint8_t count = 0;
    bool active = false, boss = false;
    void enter(const GameState& state, uint8_t slot) {
        if (!active || slot >= state.teamCount || slot >= TEAM_CAP) return;
        const auto& monster = state.team[slot];
        for (uint8_t i = 0; i < count; ++i)
            if (ContactRoster::sameMonster(participants[i], monster)) return;
        if (count < TEAM_CAP) participants[count++] = monster;
    }
    void begin(const GameState& state, uint8_t slot, bool isBoss) {
        count = 0;
        active = true;
        boss = isBoss;
        enter(state, slot);
    }
};

struct Session {
    struct Member {
        MonsterRuntime identity{};
        uint16_t pending = 0;
        uint16_t spent = 0;
        uint8_t remainder = 0;
    };
    static constexpr uint8_t CAP = TEAM_CAP * 2;
    Member members[CAP]{};
    uint8_t count = 0;
    uint32_t minutes = 0;
    bool active = false, finished = false;

    void begin() {
        for (auto& entry : members) entry = Member{};
        count = 0;
        minutes = 0;
        active = true;
        finished = false;
    }
    Member* member(const MonsterRuntime& monster) {
        for (uint8_t i = 0; i < count; ++i)
            if (ContactRoster::sameMonster(members[i].identity, monster))
                return &members[i];
        if (count == CAP) return nullptr;
        members[count].identity = monster;
        return &members[count++];
    }
    void accrue(const MonsterRuntime& monster, uint8_t amount) {
        if (!needsFood(monster)) return;
        auto* entry = member(monster);
        if (entry) entry->pending = static_cast<uint16_t>(
            std::min<uint32_t>(65535, static_cast<uint32_t>(entry->pending) + amount));
    }
    uint8_t step(GameState& state) {
        if (!active || finished) return 0;
        ++minutes;
        for (uint8_t slot = 0; slot < state.teamCount && slot < TEAM_CAP; ++slot) {
            auto& monster = state.team[slot];
            if (!needsFood(monster) || !ExplorePartyService::healthy(monster)) continue;
            auto* entry = member(monster);
            if (entry && ++entry->remainder == STEPS_PER_FOOD) {
                entry->remainder = 0;
                accrue(monster, 1);
            }
        }
        return 1;
    }
    uint8_t settleBattle(GameState& state, Battle& battle) {
        if (!active || finished || !battle.active) return 0;
        battle.active = false;
        const uint8_t time = battle.boss ? 10 : 5;
        minutes += time;
        for (uint8_t i = 0; i < battle.count; ++i) {
            for (uint8_t slot = 0; slot < state.teamCount && slot < TEAM_CAP; ++slot) {
                auto& monster = state.team[slot];
                if (ContactRoster::sameMonster(monster, battle.participants[i])) {
                    accrue(monster, battle.boss ? 2 : 1);
                    break;
                }
            }
        }
        return time;
    }
    void finish(GameState& state) {
        if (!active || finished) return;
        for (uint8_t i = 0; i < count; ++i) {
            auto& entry = members[i];
            const uint32_t amount = static_cast<uint32_t>(entry.pending) +
                                    (entry.remainder ? 1U : 0U);
            for (uint8_t slot = 0; slot < state.teamCount && slot < TEAM_CAP; ++slot) {
                auto& monster = state.team[slot];
                if (needsFood(monster) &&
                    ContactRoster::sameMonster(monster, entry.identity)) {
                    entry.spent = static_cast<uint16_t>(std::min<uint32_t>(monster.satiety, amount));
                    monster.satiety -= entry.spent;
                    break;
                }
            }
            entry.pending = 0;
            entry.remainder = 0;
        }
        finished = true;
    }
};

}  // namespace Game::ExploreCost
