#pragma once

#include <cstdint>
#include "game/GameState.h"
#include "game/Species.h"

namespace BattleSystem {

static constexpr uint8_t SPECIAL_SLOT_NONE = 0xFF;

struct DamageResult {
    uint16_t damage = 0;
    uint16_t effectiveness = 100; // percent
    bool critical = false;
    bool special = false;
    uint8_t specialSlot = SPECIAL_SLOT_NONE;
    bool statusBlocked = false;
};

uint16_t typeEffectiveness(TypeId attack, TypeId defend1, TypeId defend2);
uint8_t specialTriggerChance(const Game::MonsterRuntime& attacker);
bool rollSpecialMove(const Game::MonsterRuntime& attacker);
uint8_t rollSpecialMoveSlot(const Game::MonsterRuntime& attacker);
DamageResult calcBasicDamage(const Game::MonsterRuntime& attacker,
                             const Species& attackerSpecies,
                             const Game::MonsterRuntime& defender,
                             const Species& defenderSpecies,
                             uint8_t specialSlot);

} // namespace BattleSystem
