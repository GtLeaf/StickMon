#pragma once

#include <algorithm>
#include <cstdint>

#include "game/GameState.h"
#include "game/Species.h"

namespace Game {
namespace EffortService {

inline uint8_t* statField(StatLine& effort, uint8_t statIndex) {
    switch (statIndex) {
    case 0: return &effort.hp;
    case 1: return &effort.atk;
    case 2: return &effort.def;
    case 3: return &effort.spa;
    case 4: return &effort.spd;
    case 5: return &effort.spe;
    default: return nullptr;
    }
}

inline bool grant(MonsterRuntime& monster,
                  const Species& defeatedSpecies,
                  const Species& monsterSpecies) {
    bool changed = false;
    for (uint8_t statIndex = 0; statIndex < STAT_COUNT; ++statIndex) {
        const uint8_t amount = evYieldAt(defeatedSpecies, statIndex);
        if (amount == 0) continue;

        uint8_t* value = statField(monster.ev, statIndex);
        if (!value || *value >= EV_MAX) continue;
        const uint16_t total = evTotal(monster.ev);
        if (total >= EV_TOTAL_MAX) break;

        const uint8_t roomByStat = static_cast<uint8_t>(EV_MAX - *value);
        const uint16_t roomByTotal = EV_TOTAL_MAX - total;
        const uint8_t gain = static_cast<uint8_t>(std::min<uint16_t>(
            amount, std::min<uint16_t>(roomByStat, roomByTotal)));
        if (gain == 0) continue;
        *value = static_cast<uint8_t>(*value + gain);
        changed = true;
    }

    if (!changed) return false;
    const uint16_t oldMax = monster.hpMax;
    monster.hpMax = maxHpFor(monsterSpecies, monster);
    if (monster.hpMax > oldMax) {
        monster.hpCur = static_cast<uint16_t>(std::min<uint32_t>(
            monster.hpMax,
            static_cast<uint32_t>(monster.hpCur) + monster.hpMax - oldMax));
    }
    return true;
}

}  // namespace EffortService
}  // namespace Game
