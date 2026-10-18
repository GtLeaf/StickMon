#include "game/BattleActionService.h"

#include <algorithm>

namespace BattleActionService {

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
    const BattleSystem::DamageContext& context) {
    if (pending.kind != Kind::EMPTY) return;

    pending.moveId = moveId;
    pending.specialSlot = specialSlot;
    pending.releasingCharge = BattleSystem::isChargingMove(attackerState);
    if (attacker.hpCur == 0 || defender.hpCur == 0) {
        pending.kind = Kind::BLOCKED;
        pending.check.blockReason = BattleSystem::ActionBlockReason::MOVE_FAILED;
        return;
    }

    pending.check = BattleSystem::checkAction(
        attacker, attackerSpecies, attackerState, moveId,
        context.defenderMoveIsDamaging);
    if (!pending.check.canAct()) {
        pending.kind = Kind::BLOCKED;
        if (pending.releasingCharge) {
            BattleSystem::clearChargingMove(attackerState);
        }
        return;
    }

    const MoveInfo* move = findMove(moveId);
    if (move && BattleSystem::moveRequiresCharge(moveId) &&
        !pending.releasingCharge) {
        BattleSystem::beginChargingMove(attackerState, moveId, specialSlot);
        if ((move->flags & MOVE_FLAG_CHARGE_DEFENSE) != 0) {
            const uint8_t defense = static_cast<uint8_t>(BattleStat::DEFENSE);
            const int8_t before = attackerState.statStages[defense];
            attackerState.statStages[defense] = std::min<int8_t>(6, before + 1);
            pending.defenseRaised = attackerState.statStages[defense] != before;
        }
        pending.kind = Kind::CHARGING;
        return;
    }

    if (pending.releasingCharge) BattleSystem::clearChargingMove(attackerState);
    pending.damage = BattleSystem::calcBasicDamage(
        attacker, attackerSpecies, defender, defenderSpecies, specialSlot,
        attackerState, defenderState, context);
    pending.kind = Kind::READY;
}

bool commit(
    PendingAction& pending,
    Game::MonsterRuntime& attacker,
    const Species& attackerSpecies,
    BattleSystem::BattleActorState& attackerState,
    Game::MonsterRuntime& defender,
    const Species& defenderSpecies,
    BattleSystem::BattleActorState& defenderState,
    bool allowFlinch,
    bool awardProficiency) {
    if (pending.kind == Kind::EMPTY || pending.committed) return false;
    pending.committed = true;

    CommitResult& result = pending.result;
    result.attackerHpBefore = attacker.hpCur;
    result.defenderHpBefore = defender.hpCur;
    result.defenderHpAfterDamage = defender.hpCur;

    if (pending.kind == Kind::BLOCKED) {
        result.selfDamage = std::min<uint16_t>(
            pending.check.selfDamage, attacker.hpCur);
        attacker.hpCur = static_cast<uint16_t>(attacker.hpCur - result.selfDamage);
    } else if (pending.kind == Kind::READY) {
        const BattleSystem::DamageResult& damage = pending.damage;
        const MoveInfo* move = findMove(damage.moveId);
        const bool hit = !damage.failed && !damage.missed;
        if (!hit || damage.effectiveness == 0) {
            if (move) {
                result.rampageEnded = BattleSystem::recordMoveResult(
                    attackerState, attacker, attackerSpecies, *move, hit,
                    pending.specialSlot);
            }
            if (hit && damage.effectiveness == 0 && damage.absorbed) {
                result.absorbEffects = BattleSystem::applyAbsorbAbility(
                    damage, defender, defenderSpecies, defenderState);
            }
        } else {
            result.actualDamage = std::min<uint16_t>(damage.damage, defender.hpCur);
            defender.hpCur = static_cast<uint16_t>(defender.hpCur - result.actualDamage);
            result.defenderHpAfterDamage = defender.hpCur;
            if (move) {
                result.moveEffects = BattleSystem::applyMoveEffects(
                    *move, attacker, attackerSpecies, attackerState,
                    defender, defenderSpecies, defenderState,
                    result.actualDamage, allowFlinch);
                result.abilityEffects = BattleSystem::applyPostDamageAbilities(
                    *move, attacker, attackerSpecies, attackerState,
                    defender, defenderSpecies, defenderState,
                    result.actualDamage);
                result.rampageEnded = BattleSystem::recordMoveResult(
                    attackerState, attacker, attackerSpecies, *move, true,
                    damage.specialSlot);
                result.forceWildEnd = damage.forceWildEnd && defender.hpCur > 0;
                if (awardProficiency && damage.special) {
                    const uint8_t moveSlot = static_cast<uint8_t>(
                        damage.specialSlot + 1);
                    if (moveSlot > 0 && moveSlot < Game::MOVE_SLOT_COUNT &&
                        attacker.moveProficiency[moveSlot] < Game::MOVE_PROFICIENCY_MAX) {
                        ++attacker.moveProficiency[moveSlot];
                    }
                }
            }
        }
    }

    result.attackerHpAfter = attacker.hpCur;
    result.defenderHpAfter = defender.hpCur;
    return true;
}

} // namespace BattleActionService
