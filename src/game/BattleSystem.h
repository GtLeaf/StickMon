#pragma once

#include <cstdint>
#include "game/GameState.h"
#include "game/Species.h"

namespace BattleSystem {

struct DamageResult {
    uint16_t damage = 0;
    uint8_t effectiveness = 100; // percent
    bool critical = false;
    bool special = false;
    bool statusBlocked = false;
};

uint8_t typeEffectiveness(TypeId attack, TypeId defend1, TypeId defend2);
uint8_t specialTriggerChance(const Game::MonsterRuntime& attacker);
bool rollSpecialMove(const Game::MonsterRuntime& attacker);
DamageResult calcBasicDamage(const Game::MonsterRuntime& attacker,
                             const Species& attackerSpecies,
                             const Game::MonsterRuntime& defender,
                             const Species& defenderSpecies,
                             bool specialMove);

} // namespace BattleSystem
