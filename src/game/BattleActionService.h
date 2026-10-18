#pragma once

#include <cstdint>

#include "game/BattleSystem.h"

namespace BattleActionService {

enum class Kind : uint8_t {
    EMPTY = 0,
    BLOCKED,
    CHARGING,
    READY,
};

struct CommitResult {
    uint16_t attackerHpBefore = 0;
    uint16_t attackerHpAfter = 0;
    uint16_t defenderHpBefore = 0;
    uint16_t defenderHpAfterDamage = 0;
    uint16_t defenderHpAfter = 0;
    uint16_t actualDamage = 0;
    uint16_t selfDamage = 0;
    BattleSystem::EffectResolution moveEffects;
    BattleSystem::EffectResolution abilityEffects;
    BattleSystem::EffectResolution absorbEffects;
    bool rampageEnded = false;
    bool forceWildEnd = false;
};

struct PendingAction {
    Kind kind = Kind::EMPTY;
    BattleSystem::ActionCheckResult check;
    BattleSystem::DamageResult damage;
    CommitResult result;
    Game::MoveId moveId = 0;
    uint8_t specialSlot = BattleSystem::SPECIAL_SLOT_NONE;
    bool releasingCharge = false;
    bool defenseRaised = false;
    bool committed = false;

    void reset() { *this = PendingAction{}; }
};

// Preparation owns action checks and charge transitions, but never changes HP.
// Repeat calls return the prepared action without consuming more randomness.
void prepare(
    PendingAction& pending,
    Game::MonsterRuntime& attacker,
    const Species& attackerSpecies,
    BattleSystem::BattleActorState& attackerState,
    const Game::MonsterRuntime& defender,
    const Species& defenderSpecies,
    const BattleSystem::BattleActorState& defenderState,
    uint8_t specialSlot,
    Game::MoveId moveId,
    const BattleSystem::DamageContext& context = {});

// Commit at the presentation's impact node. Only its first call returns true;
// later calls leave state and randomness untouched and retain the result.
bool commit(
    PendingAction& pending,
    Game::MonsterRuntime& attacker,
    const Species& attackerSpecies,
    BattleSystem::BattleActorState& attackerState,
    Game::MonsterRuntime& defender,
    const Species& defenderSpecies,
    BattleSystem::BattleActorState& defenderState,
    bool allowFlinch,
    bool awardProficiency);

} // namespace BattleActionService
