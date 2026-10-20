#include "app/BattleSession.h"
#include "core/UiStrings.h"
#include "game/CareTicker.h"
#include "game/CareExperience.h"
#include "game/EffortService.h"
#include "game/ExperienceService.h"
#include "game/ExploreBoss.h"
#include "game/ExploreBossPity.h"
#include "game/FriendshipService.h"
#include "game/FriendshipSystem.h"
#include "game/GameRandom.h"
#include "game/ItemInventory.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace AmoledV1 {
using namespace BattleTiming;
namespace {
const char* battleActionBlockedFormat(BattleSystem::ActionBlockReason reason) {
    switch (reason) {
    case BattleSystem::ActionBlockReason::FLINCH: return Ui::Explore::FLINCHED_FMT;
    case BattleSystem::ActionBlockReason::SLEEP: return Ui::Explore::ASLEEP_FMT;
    case BattleSystem::ActionBlockReason::FREEZE: return Ui::Explore::FROZEN_FMT;
    case BattleSystem::ActionBlockReason::PARALYSIS: return Ui::Explore::PARALYZED_FMT;
    case BattleSystem::ActionBlockReason::CONFUSION_SELF_HIT: return Ui::Explore::CONFUSION_HIT_FMT;
    case BattleSystem::ActionBlockReason::RECHARGE: return Ui::Explore::RECHARGING_FMT;
    case BattleSystem::ActionBlockReason::MOVE_FAILED: return Ui::Explore::MOVE_FAILED;
    default: return Ui::Amoled::ACTION_BLOCKED;
    }
}

void formatBattleOutcome(char* output, size_t outputSize,
                         const BattleSystem::DamageResult& damage,
                         uint16_t dealt,
                         const BattleSystem::EffectResolution& effects,
                         const MoveInfo* move, bool attackerWild) {
    if (damage.failed) {
        std::snprintf(output, outputSize, "%s", Ui::Explore::MOVE_FAILED);
    } else if (damage.missed) {
        std::snprintf(output, outputSize, "%s", Ui::Explore::MOVE_MISSED);
    } else if (damage.effectiveness == 0) {
        std::snprintf(output, outputSize, "%s", Ui::Explore::NO_EFFECT);
    } else if (dealt > 0) {
        std::snprintf(output, outputSize,
                      attackerWild ? Ui::Explore::WILD_DAMAGE_FMT
                                   : Ui::Explore::DAMAGE_FMT,
                      dealt);
    } else if (move && effects.count > 0) {
        // Effect outcomes already report success or failure. In particular,
        // STATUS_FAILED owns the no-effect line for moves such as Splash.
        output[0] = '\0';
    } else {
        std::snprintf(output, outputSize, "%s", Ui::Explore::NO_EFFECT);
    }
}

void formatBattleMoveUsed(char* output, size_t outputSize,
                          const Species& attackerSpecies,
                          const MoveInfo* move, bool attackerWild) {
    const char* moveName = move ? move->name : Ui::Status::MOVE_UNKNOWN;
    if (attackerWild) {
        std::snprintf(output, outputSize, Ui::Explore::WILD_MOVE_USED_FMT,
                      moveName);
    } else {
        std::snprintf(output, outputSize, Ui::Explore::MOVE_USED_FMT,
                      attackerSpecies.name, moveName);
    }
}
}

void BattleSession::enqueueBattleDamageLogs(
    const BattleSystem::DamageResult& damage, uint16_t dealt,
    uint32_t nowMs) {
    if (damage.failed || damage.missed || damage.effectiveness == 0 ||
        dealt == 0) return;
    auto enqueue = [&](const char* message) {
        std::snprintf(battleMessage, sizeof(battleMessage), "%s", message);
        pushBattleLog(nowMs, false);
    };
    if (damage.critical) enqueue(Ui::Explore::CRITICAL_HIT);
    if (damage.effectiveness > 100) {
        enqueue(Ui::Explore::SUPER_EFFECTIVE);
    } else if (damage.effectiveness < 100) {
        enqueue(Ui::Explore::NOT_VERY_EFFECTIVE);
    }
}

void BattleSession::enqueueBattleEffectLogs(
    const Game::GameState& gameState,
    const BattleSystem::EffectResolution& effects, bool attackerWild,
    uint32_t nowMs) {
    const Species* playerSpecies = state_.battlePlayerSlot < gameState.teamCount
        ? findSpecies(gameState.team[state_.battlePlayerSlot].speciesId) : nullptr;
    const Species* wildSpecies = findSpecies(state_.battleWild.speciesId);
    if (!playerSpecies || !wildSpecies) return;
    auto statusLabel = [](Game::MajorStatus status) -> const char* {
        switch (status) {
        case Game::MajorStatus::POISON: return Ui::Status::STATUS_POISON;
        case Game::MajorStatus::TOXIC: return Ui::Status::STATUS_TOXIC;
        case Game::MajorStatus::PARALYSIS: return Ui::Status::STATUS_PARALYSIS;
        case Game::MajorStatus::SLEEP: return Ui::Status::STATUS_SLEEP;
        case Game::MajorStatus::BURN: return Ui::Status::STATUS_BURN;
        case Game::MajorStatus::FREEZE: return Ui::Status::STATUS_FREEZE;
        default: return Ui::Status::STATUS_OK;
        }
    };
    for (uint8_t index = 0; index < effects.count; ++index) {
        const auto& outcome = effects.outcomes[index];
        const bool targetWild = outcome.target == MoveEffectTarget::ATTACKER
            ? attackerWild : !attackerWild;
        const char* targetName = targetWild
            ? wildSpecies->name : playerSpecies->name;
        const char* attackerName = attackerWild
            ? wildSpecies->name : playerSpecies->name;
        const char* defenderName = attackerWild
            ? playerSpecies->name : wildSpecies->name;
        const bool defenderOwnsAbility = outcome.ability == AbilityId::STATIC ||
            outcome.ability == AbilityId::EFFECT_SPORE ||
            outcome.ability == AbilityId::SYNCHRONIZE;
        if (outcome.ability != AbilityId::NONE) {
            std::snprintf(battleMessage, sizeof(battleMessage),
                          Ui::Explore::ABILITY_ACTIVATED_FMT,
                          defenderOwnsAbility ? defenderName
                              : outcome.ability == AbilityId::INTIMIDATE
                                  ? attackerName : targetName,
                          abilityName(outcome.ability));
            pushBattleLog(nowMs, false);
        }
        switch (outcome.kind) {
        case BattleSystem::EffectOutcomeKind::STATUS_APPLIED:
        case BattleSystem::EffectOutcomeKind::YAWN_SLEEP:
            std::snprintf(battleMessage, sizeof(battleMessage),
                          Ui::Explore::STATUS_APPLIED_FMT, targetName,
                          statusLabel(outcome.status));
            break;
        case BattleSystem::EffectOutcomeKind::STATUS_FAILED:
            std::snprintf(battleMessage, sizeof(battleMessage), "%s",
                          Ui::Explore::NO_EFFECT);
            break;
        case BattleSystem::EffectOutcomeKind::CONFUSED:
            std::snprintf(battleMessage, sizeof(battleMessage),
                          Ui::Explore::CONFUSED_FMT, targetName);
            break;
        case BattleSystem::EffectOutcomeKind::BOUND:
            std::snprintf(battleMessage, sizeof(battleMessage),
                          Ui::Explore::BOUND_FMT, targetName);
            break;
        case BattleSystem::EffectOutcomeKind::STAT_CHANGED: {
            const uint8_t stat = static_cast<uint8_t>(outcome.stat);
            if (stat >= static_cast<uint8_t>(BattleStat::COUNT)) continue;
            std::snprintf(battleMessage, sizeof(battleMessage),
                          outcome.stageDelta > 0 ? Ui::Explore::STAT_ROSE_FMT
                                                 : Ui::Explore::STAT_FELL_FMT,
                          targetName, Ui::Explore::STAT_NAMES[stat]);
            break;
        }
        case BattleSystem::EffectOutcomeKind::DRAINED:
            std::snprintf(battleMessage, sizeof(battleMessage),
                          Ui::Explore::DRAINED_FMT, outcome.amount);
            break;
        case BattleSystem::EffectOutcomeKind::RECOIL:
            std::snprintf(battleMessage, sizeof(battleMessage),
                          Ui::Explore::RECOIL_FMT, targetName, outcome.amount);
            break;
        case BattleSystem::EffectOutcomeKind::HEALED:
            std::snprintf(battleMessage, sizeof(battleMessage),
                          Ui::Explore::HEALED_FMT, targetName, outcome.amount);
            break;
        case BattleSystem::EffectOutcomeKind::CURED:
            std::snprintf(battleMessage, sizeof(battleMessage),
                          Ui::Explore::CURED_FMT, targetName);
            break;
        case BattleSystem::EffectOutcomeKind::BIND_CLEARED:
        case BattleSystem::EffectOutcomeKind::BIND_ENDED:
            std::snprintf(battleMessage, sizeof(battleMessage),
                          Ui::Explore::BIND_ENDED_FMT, targetName);
            break;
        case BattleSystem::EffectOutcomeKind::YAWNED:
            std::snprintf(battleMessage, sizeof(battleMessage),
                          Ui::Explore::YAWNED_FMT, targetName);
            break;
        case BattleSystem::EffectOutcomeKind::STATUS_DAMAGE:
            std::snprintf(battleMessage, sizeof(battleMessage),
                          Ui::Explore::STATUS_DAMAGE_FMT, targetName,
                          outcome.amount);
            break;
        case BattleSystem::EffectOutcomeKind::BIND_DAMAGE:
            std::snprintf(battleMessage, sizeof(battleMessage),
                          Ui::Explore::BIND_DAMAGE_FMT, targetName,
                          outcome.amount);
            break;
        case BattleSystem::EffectOutcomeKind::ABILITY_ACTIVATED:
            if (outcome.amount == 0) continue;
            std::snprintf(battleMessage, sizeof(battleMessage),
                          Ui::Explore::ABSORB_HEAL_FMT, targetName,
                          outcome.amount);
            break;
        case BattleSystem::EffectOutcomeKind::FLINCHED:
            continue;
        }
        pushBattleLog(nowMs, false);
    }
}

void BattleSession::performBattleAttack(Game::GameState& gameState, uint32_t nowMs) {
    if (state_.battlePhase != BattleViewModel::Phase::ACTION) return;
    if (state_.battlePlayerSlot >= gameState.teamCount ||
        state_.battlePlayerSlot >= Game::TEAM_CAP) return;
    Game::MonsterRuntime& player = gameState.team[state_.battlePlayerSlot];
    const Species* playerSpecies = findSpecies(player.speciesId);
    const Species* wildSpecies = findSpecies(state_.battleWild.speciesId);
    if (!playerSpecies || !wildSpecies) return;

    state_.battleTurnPlan = battleTurnController.planAiTurn(
        player, *playerSpecies, state_.battlePlayerState,
        state_.battleWild, *wildSpecies, state_.battleWildState);
    state_.battleTurnActionIndex = 0;
    state_.battleTurnDamaged[0] = false;
    state_.battleTurnDamaged[1] = false;
    performBattlePlannedAction(gameState, nowMs);
}

void BattleSession::performBattlePlayerAction(Game::GameState& gameState,
    const BattleTurnController::Action& action, uint32_t nowMs) {
    if (action.side == BattleTurnController::Side::PLAYER)
        performBattleAction(gameState, action, nowMs);
}

void BattleSession::performBattleWildAction(Game::GameState& gameState,
    const BattleTurnController::Action& action, uint32_t nowMs) {
    if (action.side == BattleTurnController::Side::WILD)
        performBattleAction(gameState, action, nowMs);
}

void BattleSession::performBattleAction(Game::GameState& gameState,
    const BattleTurnController::Action& action, uint32_t nowMs) {
    if (state_.battlePlayerSlot >= gameState.teamCount ||
        state_.battlePlayerSlot >= Game::TEAM_CAP) return;
    Game::MonsterRuntime& player = gameState.team[state_.battlePlayerSlot];
    const Species* playerSpecies = findSpecies(player.speciesId);
    const Species* wildSpecies = findSpecies(state_.battleWild.speciesId);
    if (!playerSpecies || !wildSpecies) return;

    const bool attackerWild = action.side == BattleTurnController::Side::WILD;
    auto& attacker = attackerWild ? state_.battleWild : player;
    auto& defender = attackerWild ? player : state_.battleWild;
    const Species& attackerSpecies = attackerWild ? *wildSpecies : *playerSpecies;
    const Species& defenderSpecies = attackerWild ? *playerSpecies : *wildSpecies;
    auto& attackerState = attackerWild ? state_.battleWildState : state_.battlePlayerState;
    auto& defenderState = attackerWild ? state_.battlePlayerState : state_.battleWildState;
    const auto defenderSide = attackerWild ? BattleTurnController::Side::PLAYER
                                           : BattleTurnController::Side::WILD;
    const auto* opponentAction = state_.battleTurnPlan.actionFor(defenderSide);
    const MoveInfo* opponentMove = findMove(opponentAction ? opponentAction->moveId : 0);
    BattleSystem::DamageContext context;
    context.attackerMovesSecond = state_.battleTurnActionIndex > 0;
    context.defenderDamagedThisTurn = state_.battleTurnDamaged[attackerWild ? 0 : 1];
    context.defenderMoveIsDamaging = opponentMove && opponentMove->power > 0 &&
        opponentMove->damageClass != DamageClass::STATUS;
    context.allowForceWildEnd = !attackerWild;

    BattleActionService::PendingAction pending;
    BattleActionService::prepare(pending, attacker, attackerSpecies, attackerState,
        defender, defenderSpecies, defenderState, action.specialSlot, action.moveId, context);
    BattleActionService::commit(pending, attacker, attackerSpecies, attackerState,
        defender, defenderSpecies, defenderState,
        state_.battleTurnPlan.hasActionAfter(state_.battleTurnActionIndex, defenderSide), !attackerWild);
    const auto& check = pending.check;
    const auto& damage = pending.damage;
    const auto& result = pending.result;
    const auto& moveEffects = result.moveEffects;
    if (result.selfDamage > 0) state_.battleTurnDamaged[attackerWild ? 1 : 0] = true;
    if (result.actualDamage > 0) state_.battleTurnDamaged[attackerWild ? 0 : 1] = true;
    auto recoveryLog = [&](bool changed, const char* format) {
        if (!changed) return;
        std::snprintf(battleMessage, sizeof(battleMessage), format, attackerSpecies.name);
        pushBattleLog(nowMs);
    };
    recoveryLog(check.wokeUp, Ui::Explore::WOKE_UP_FMT);
    recoveryLog(check.thawed, Ui::Explore::THAWED_FMT);
    recoveryLog(check.confusionEnded, Ui::Explore::CONFUSION_ENDED_FMT);
    state_.battleContinuation = BattleContinuation::ADVANCE_TURN;
    if (pending.kind == BattleActionService::Kind::BLOCKED) {
        if (result.selfDamage > 0)
            timeline_.startHp(attackerWild, result.attackerHpBefore,
                result.attackerHpAfter, attacker.hpMax, (clock_ ? clock_() : nowMs));
        std::snprintf(battleMessage, sizeof(battleMessage),
                      battleActionBlockedFormat(check.blockReason), attackerSpecies.name);
        pushBattleLog(nowMs);
        return;
    }
    if (pending.kind == BattleActionService::Kind::CHARGING) {
        std::snprintf(battleMessage, sizeof(battleMessage), "%s",
                      attackerWild ? Ui::Amoled::WILD_CHARGE : Ui::Amoled::CHARGING);
        pushBattleLog(nowMs);
        if (pending.defenseRaised) {
            std::snprintf(battleMessage, sizeof(battleMessage), Ui::Explore::STAT_ROSE_FMT,
                attackerSpecies.name, Ui::Explore::STAT_NAMES[static_cast<uint8_t>(BattleStat::DEFENSE)]);
            pushBattleLog(nowMs, false);
        }
        return;
    }

    const MoveInfo* move = findMove(damage.moveId);
    const uint16_t dealt = result.actualDamage;
    formatBattleMoveUsed(battleMessage, sizeof(battleMessage), attackerSpecies, move, attackerWild);
    timeline_.holdAttackLog();
    pushBattleLog(nowMs, true, BATTLE_ATTACK_LOG_MS);
    if (damage.effectiveness == 0 && !damage.absorbed &&
        damage.activatedAbility != AbilityId::NONE) {
        std::snprintf(battleMessage, sizeof(battleMessage), Ui::Explore::ABILITY_ACTIVATED_FMT,
            defenderSpecies.name, abilityName(damage.activatedAbility));
        pushBattleLog(nowMs, false);
    }
    enqueueBattleEffectLogs(gameState, result.absorbEffects, attackerWild, nowMs);
    formatBattleOutcome(battleMessage, sizeof(battleMessage), damage,
                        dealt, moveEffects, move, attackerWild);
    // Hold the move line until impact, then play ordered result/effect logs.
    pushBattleLog(nowMs, false, BATTLE_RESULT_LOG_MS);
    enqueueBattleDamageLogs(damage, dealt, nowMs);
    enqueueBattleEffectLogs(gameState, moveEffects, attackerWild, nowMs);
    enqueueBattleEffectLogs(gameState, result.abilityEffects, attackerWild, nowMs);
    if (result.rampageEnded) {
        std::snprintf(battleMessage, sizeof(battleMessage), Ui::Explore::RAMPAGE_CONFUSED_FMT,
                      attackerSpecies.name);
        pushBattleLog(nowMs, false);
    }
    if (damage.sturdyActivated) {
        std::snprintf(battleMessage, sizeof(battleMessage), Ui::Explore::ABILITY_ACTIVATED_FMT,
                      defenderSpecies.name, abilityName(AbilityId::STURDY));
        pushBattleLog(nowMs, false);
    }
    if (result.forceWildEnd) {
        state_.battleForceWildEndPending = true;
        std::snprintf(battleMessage, sizeof(battleMessage), "%s", Ui::Explore::DRAGON_TAIL_END);
        pushBattleLog(nowMs, false);
    }
    timeline_.startAttack(pending, attackerWild, attacker.speciesId,
                          attacker.hpMax, defender.hpMax, (clock_ ? clock_() : nowMs));
    damageRows(0, BATTLE_ANIMATION_RENDER_END);
}

void BattleSession::performBattlePlannedAction(Game::GameState& gameState, uint32_t nowMs) {
    if (state_.battleTurnActionIndex >= state_.battleTurnPlan.count) {
        advanceBattleTurn(gameState, nowMs);
        return;
    }
    const BattleTurnController::Action& action =
        state_.battleTurnPlan.actions[state_.battleTurnActionIndex];
    if (action.side == BattleTurnController::Side::WILD) {
        performBattleWildAction(gameState, action, nowMs);
    } else {
        performBattlePlayerAction(gameState, action, nowMs);
    }
}

void BattleSession::advanceBattleTurn(Game::GameState& gameState, uint32_t nowMs) {
    if (state_.battlePlayerSlot >= gameState.teamCount ||
        state_.battlePlayerSlot >= Game::TEAM_CAP) return;
    Game::MonsterRuntime& player = gameState.team[state_.battlePlayerSlot];
    const Species* playerSpecies = findSpecies(player.speciesId);
    const Species* wildSpecies = findSpecies(state_.battleWild.speciesId);
    if (!playerSpecies || !wildSpecies) return;

    if (resolveBattleFaint(gameState, nowMs)) return;
    if (state_.battleForceWildEndPending) {
        closeBattle(nowMs);
        return;
    }

    ++state_.battleTurnActionIndex;
    if (state_.battleTurnActionIndex < state_.battleTurnPlan.count) {
        performBattlePlannedAction(gameState, nowMs);
        return;
    }

    auto playerEffects = BattleSystem::resolveEndTurn(player, *playerSpecies, state_.battlePlayerState);
    for (uint8_t i = 0; i < playerEffects.count; ++i)
        playerEffects.outcomes[i].target = MoveEffectTarget::ATTACKER;
    enqueueBattleEffectLogs(gameState, playerEffects, false, nowMs);
    auto wildEffects = BattleSystem::resolveEndTurn(state_.battleWild, *wildSpecies, state_.battleWildState);
    for (uint8_t i = 0; i < wildEffects.count; ++i)
        wildEffects.outcomes[i].target = MoveEffectTarget::ATTACKER;
    enqueueBattleEffectLogs(gameState, wildEffects, true, nowMs);
    state_.battleTurnPlan = BattleTurnController::TurnPlan{};
    state_.battleTurnActionIndex = 0;
    state_.battleTurnDamaged[0] = false;
    state_.battleTurnDamaged[1] = false;
    state_.battleContinuation = BattleContinuation::FINISH_END_TURN;
    damageRows(0, 448);
}

void BattleSession::finishBattleEndTurn(Game::GameState& gameState, uint32_t nowMs) {
    if (resolveBattleFaint(gameState, nowMs)) return;
    if (BattleSystem::isChargingMove(state_.battlePlayerState) ||
        state_.battlePlayerState.lockedMoveId != 0) {
        performBattleAttack(gameState, nowMs);
        return;
    }
    damageRows(0, 448);
}

bool BattleSession::resolveBattleFaint(Game::GameState& gameState, uint32_t nowMs) {
    if (state_.battlePlayerSlot >= gameState.teamCount ||
        state_.battlePlayerSlot >= Game::TEAM_CAP) return false;
    Game::MonsterRuntime& player = gameState.team[state_.battlePlayerSlot];
    const Species* wildSpecies = findSpecies(state_.battleWild.speciesId);
    if (!wildSpecies) return false;
    state_.battlePhase = BattleViewModel::Phase::ACTION;
    if (player.hpCur == 0) {
        player.fainted = true;
        player.lastSeenAt = Game::gameSecondsForMinutes(
            gameState.gameMinutesTotal);
        const int8_t switchSlot = availableBattleSwitchSlot(gameState);
        const bool hasSwitch = switchSlot >= 0;
        state_.battlePhase = hasSwitch ? BattleViewModel::Phase::ACTION
                                : BattleViewModel::Phase::DEFEAT;
        std::snprintf(battleMessage, sizeof(battleMessage), "%s",
                      Ui::Amoled::PET_FAINTED);
        pushBattleLog(nowMs);
        state_.battleTurnPlan = BattleTurnController::TurnPlan{};
        state_.battleTurnActionIndex = 0;
        if (hasSwitch) {
            performBattleSwitch(gameState, static_cast<uint8_t>(switchSlot), false,
                                nowMs);
            return true;
        }
        damageRows(0, 448);
        return true;
    }
    if (state_.battleWild.hpCur == 0) {
        state_.battleWild.fainted = true;
        state_.battleRewardExp = BattleSystem::scaledExperienceReward(
            BattleSystem::experienceReward(*wildSpecies, state_.battleWild.level),
            state_.battleExperiencePercent);
        state_.battleRewardCoins = state_.battleIsBoss
            ? ExploreBoss::victoryCoinReward(true)
            : 8 + static_cast<uint32_t>(state_.battleWild.level) * 2;
        state_.battlePhase = BattleViewModel::Phase::VICTORY;
        startBattleExperienceAnimation(gameState, nowMs);
        std::snprintf(battleMessage, sizeof(battleMessage),
                      Ui::Amoled::VICTORY_FMT,
                      state_.battleRewardExp);
        pushBattleLog(nowMs);
        state_.battleTurnPlan = BattleTurnController::TurnPlan{};
        state_.battleTurnActionIndex = 0;
        damageRows(0, 448);
        return true;
    }
    return false;
}

void BattleSession::performBattleSwitch(Game::GameState& gameState, uint8_t teamSlot, bool consumesTurn,
                                    uint32_t nowMs) {
    if (teamSlot >= gameState.teamCount || teamSlot >= Game::TEAM_CAP ||
        teamSlot == state_.battlePlayerSlot) {
        effects_.toast = Ui::Amoled::CHOOSE_OTHER;
        return;
    }
    Game::MonsterRuntime& candidate = gameState.team[teamSlot];
    if (candidate.fainted || candidate.hpCur == 0) {
        effects_.toast = Ui::Amoled::CANNOT_SWITCH;
        return;
    }
    const Species* candidateSpecies = findSpecies(candidate.speciesId);
    const Species* wildSpecies = findSpecies(state_.battleWild.speciesId);
    if (!candidateSpecies || !wildSpecies) return;
    state_.pendingBattleSwitchSlot = teamSlot;
    state_.battleSwitchConsumesTurn = consumesTurn;
    timeline_.startSwitch((clock_ ? clock_() : nowMs));
    state_.battlePhase = BattleViewModel::Phase::ACTION;
    state_.battlePressedItem = 0xFF;
    damageRows(0, BATTLE_ANIMATION_RENDER_END);
}

void BattleSession::updateBattleSwitch(Game::GameState& gameState, uint32_t nowMs) {
    if (!timeline_.switchDue(nowMs)) return;
    if (timeline_.state().battleSwitchStage == BattleSwitchStage::RETREATING) {
        if (state_.pendingBattleSwitchSlot >= gameState.teamCount ||
            state_.pendingBattleSwitchSlot >= Game::TEAM_CAP ||
            gameState.team[state_.pendingBattleSwitchSlot].fainted ||
            gameState.team[state_.pendingBattleSwitchSlot].hpCur == 0) {
            timeline_.finishSwitch();
            state_.pendingBattleSwitchSlot = 0xFF;
            state_.battleSwitchConsumesTurn = false;
            damageRows(0, BATTLE_ANIMATION_RENDER_END);
            return;
        }
        state_.battlePlayerSlot = state_.pendingBattleSwitchSlot;
        state_.pendingBattleSwitchSlot = 0xFF;
        const Species* candidateSpecies = findSpecies(
            gameState.team[state_.battlePlayerSlot].speciesId);
        const Species* wildSpecies = findSpecies(state_.battleWild.speciesId);
        if (!candidateSpecies || !wildSpecies) {
            timeline_.finishSwitch();
            state_.battleSwitchConsumesTurn = false;
            return;
        }
        BattleSystem::resetVolatile(state_.battlePlayerState);
        battleTurnController.resetPlayerAi();
        BattleSystem::EffectResolution effects;
        BattleSystem::applyEntryAbility(
            *candidateSpecies, state_.battlePlayerState, *wildSpecies,
            state_.battleWildState, effects);
        enqueueBattleEffectLogs(gameState, effects, false, nowMs);
        timeline_.awaitSwitchPreload();
        effects_.preloadSwitch = true;
        damageRows(0, BATTLE_ANIMATION_RENDER_END);
        return;
    }
    timeline_.finishSwitch();
    const Species* candidateSpecies = findSpecies(
        gameState.team[state_.battlePlayerSlot].speciesId);
    if (candidateSpecies) {
        std::snprintf(battleMessage, sizeof(battleMessage),
                      Ui::Explore::SWITCH_IN_FMT, candidateSpecies->name);
        pushBattleLog(nowMs);
    }
    if (state_.battleSwitchConsumesTurn) {
        state_.battleContinuation = BattleContinuation::WILD_TURN;
    }
    state_.battleSwitchConsumesTurn = false;
    damageRows(0, BATTLE_ANIMATION_RENDER_END);
}

void BattleSession::performBattleWildTurn(Game::GameState& gameState, uint32_t nowMs) {
    if (state_.battlePhase != BattleViewModel::Phase::ACTION) return;
    if (state_.battlePlayerSlot >= gameState.teamCount ||
        state_.battlePlayerSlot >= Game::TEAM_CAP) return;
    Game::MonsterRuntime& player = gameState.team[state_.battlePlayerSlot];
    const Species* playerSpecies = findSpecies(player.speciesId);
    const Species* wildSpecies = findSpecies(state_.battleWild.speciesId);
    if (!playerSpecies || !wildSpecies) return;

    state_.battleTurnPlan = battleTurnController.planWildOnly(
        player, *playerSpecies, state_.battlePlayerState,
        state_.battleWild, *wildSpecies, state_.battleWildState);
    state_.battleTurnActionIndex = 0;
    state_.battleTurnDamaged[0] = false;
    state_.battleTurnDamaged[1] = false;
    performBattlePlannedAction(gameState, nowMs);
}

int8_t BattleSession::availableBattleSwitchSlot(const Game::GameState& gameState) const {
    for (uint8_t slot = 0;
         slot < gameState.teamCount && slot < Game::TEAM_CAP; ++slot) {
        if (slot == state_.battlePlayerSlot) continue;
        const Game::MonsterRuntime& candidate = gameState.team[slot];
        if (!candidate.fainted && candidate.hpCur > 0) {
            return static_cast<int8_t>(slot);
        }
    }
    return -1;
}

void BattleSession::startBattleExperienceAnimation(Game::GameState& gameState, uint32_t nowMs) {
    if (state_.battlePlayerSlot >= gameState.teamCount ||
        state_.battlePlayerSlot >= Game::TEAM_CAP) {
        return;
    }
    const Game::MonsterRuntime& player = gameState.team[state_.battlePlayerSlot];
    const Species* species = findSpecies(player.speciesId);
    if (!species) return;
    bool hasReserve = false;
    for (uint8_t slot = 0; slot < gameState.teamCount &&
         slot < Game::TEAM_CAP; ++slot) {
        if (slot == state_.battlePlayerSlot) continue;
        const Game::MonsterRuntime& reserve = gameState.team[slot];
        if (!reserve.fainted && reserve.hpCur > 0) {
            hasReserve = true;
            break;
        }
    }
    const BattleSystem::ExperienceAwards awards =
        BattleSystem::experienceAwards(state_.battleRewardExp, hasReserve);
    Game::MonsterRuntime preview = player;
    Game::ExperienceService::add(preview, *species, awards.active);
    timeline_.startExperience(player.exp, preview.exp, nowMs);
}

void BattleSession::performBattleFlee(Game::GameState& gameState, uint32_t nowMs) {
    if (BattleSystem::canFlee(state_.battlePlayerState) &&
        GameRandom::range(0, 100) < 60) {
        closeBattle(nowMs);
        effects_.toast = Ui::Amoled::GOT_AWAY;
        return;
    }
    std::snprintf(battleMessage, sizeof(battleMessage), "%s",
                  Ui::Amoled::CANNOT_ESCAPE);
    pushBattleLog(nowMs);
    state_.battleContinuation = BattleContinuation::WILD_TURN;
}

void BattleSession::finishBattleVictory(Game::GameState& gameState, uint32_t nowMs) {
    if (state_.battlePhase != BattleViewModel::Phase::VICTORY || state_.battleVictorySettled) return;
    state_.battleVictorySettled = true;
    if (state_.battlePlayerSlot >= gameState.teamCount ||
        state_.battlePlayerSlot >= Game::TEAM_CAP) {
        closeBattle(nowMs);
        return;
    }
    Game::MonsterRuntime& player = gameState.team[state_.battlePlayerSlot];
    const Species* species = findSpecies(player.speciesId);
    const Species* wildSpecies = findSpecies(state_.battleWild.speciesId);
    if (!species || !wildSpecies) {
        closeBattle(nowMs);
        return;
    }
    uint8_t reserveSlot = 0xFF;
    for (uint8_t slot = 0; slot < gameState.teamCount &&
         slot < Game::TEAM_CAP; ++slot) {
        if (slot == state_.battlePlayerSlot) continue;
        const Game::MonsterRuntime& reserve = gameState.team[slot];
        if (!reserve.fainted && reserve.hpCur > 0) {
            reserveSlot = slot;
            break;
        }
    }
    const BattleSystem::ExperienceAwards awards =
        BattleSystem::experienceAwards(
            state_.battleRewardExp, reserveSlot != 0xFF);

    Game::EffortService::grant(player, *wildSpecies, *species);
    Game::CareExperience::prepareDay(gameState);
    Game::CareExperience::recordFor(gameState, player);
    const Game::ExperienceService::Result playerExperience =
        Game::ExperienceService::add(player, *species, awards.active);
    queueBattleProgression(gameState, state_.battlePlayerSlot, playerExperience.oldLevel,
                           playerExperience.leveledUp);
    if (playerExperience.leveledUp) {
        std::snprintf(battleMessage, sizeof(battleMessage),
                      Ui::Explore::LEVEL_UP_LOG_FMT, species->name,
                      playerExperience.newLevel);
        pushBattleLog(nowMs);
    }

    if (reserveSlot != 0xFF && awards.reserve > 0) {
        Game::MonsterRuntime& reserve = gameState.team[reserveSlot];
        const Species* reserveSpecies = findSpecies(reserve.speciesId);
        if (reserveSpecies) {
            Game::CareExperience::recordFor(gameState, reserve);
            Game::EffortService::grant(
                reserve, *wildSpecies, *reserveSpecies);
            const Game::ExperienceService::Result reserveExperience =
                Game::ExperienceService::add(
                    reserve, *reserveSpecies, awards.reserve);
            if (reserveExperience.awarded > 0) {
                std::snprintf(battleMessage, sizeof(battleMessage),
                              Ui::Explore::SHARED_EXP_GAIN_FMT,
                              reserveSpecies->name,
                              static_cast<unsigned>(
                                  reserveExperience.awarded));
                pushBattleLog(nowMs);
            }
            queueBattleProgression(gameState,
                reserveSlot, reserveExperience.oldLevel,
                reserveExperience.leveledUp);
            if (reserveExperience.leveledUp) {
                std::snprintf(battleMessage, sizeof(battleMessage),
                              Ui::Explore::LEVEL_UP_LOG_FMT,
                              reserveSpecies->name,
                              reserveExperience.newLevel);
                pushBattleLog(nowMs);
            }
        }
    }
    gameState.coins += state_.battleRewardCoins;
    if (state_.battleIsBoss) {
        uint8_t area = area_;
        if (area < Game::EXPLORE_AREA_COUNT) {
            if (gameState.explorePoolRerollCounts[area] < UINT8_MAX) {
                ++gameState.explorePoolRerollCounts[area];
            }
            if (state_.battleSpecialKind == ExploreSpecial::Kind::NONE) {
                ExploreBossPity::resetArea(gameState, area);
            }
            uint8_t defeated = ExploreSpecial::defeatedBit(state_.battleSpecialKind);
            if (defeated != 0) {
                gameState.specialBossDefeatedMask = static_cast<uint8_t>(
                    gameState.specialBossDefeatedMask | defeated);
            }
        }
    }
    bool allowsFriendship = state_.battleSpecialKind == ExploreSpecial::Kind::NONE ||
        ExploreSpecial::configFor(state_.battleSpecialKind).allowsFriendship;
    Game::FriendshipService::OfferResult offer =
        Game::FriendshipService::evaluateOffer(
            gameState, *wildSpecies, state_.battleWild,
            state_.battleIsBoss, allowsFriendship, state_.battleFoodBond);
    if (offer.offered) {
        state_.battleFriendshipPrompt = BattleViewModel::FriendshipPrompt::OFFER;
        state_.battleFriendshipContactSlot = 0xFF;
        state_.battlePhase = BattleViewModel::Phase::FRIENDSHIP;
        state_.battlePressedItem = 0xFF;
        std::snprintf(battleMessage, sizeof(battleMessage),
                      Ui::Explore::FRIEND_RECOGNIZES_FMT,
                      wildSpecies->name);
        pushBattleLog(nowMs);
        std::snprintf(battleMessage, sizeof(battleMessage), "%s",
                      Ui::Explore::FRIEND_CONTACT_QUESTION);
        pushBattleLog(nowMs);
        damageRows(0, 448);
        return;
    }
    if (offer.eligible) {
        Game::FriendshipService::recordFailure(
            gameState, state_.battleWild.speciesId);
    }
    if (timeline_.logBusy()) {
        state_.battleVictoryFinalizePending = true;
        damageRows(352, 448);
        return;
    }
    finishBattleAfterFriendship(gameState, nowMs);
}

void BattleSession::resolveBattleFriendship(Game::GameState& gameState, uint8_t choice, uint32_t nowMs) {
    if (state_.battlePhase != BattleViewModel::Phase::FRIENDSHIP) return;
    if (state_.battleFriendshipPrompt == BattleViewModel::FriendshipPrompt::OFFER) {
        if (choice != 0) {
            finishBattleAfterFriendship(gameState, nowMs);
            return;
        }
        uint8_t contactSlot = 0xFF;
        if (gameState.storageCount >= Game::STORAGE_CAP) {
            state_.battleFriendshipPrompt = BattleViewModel::FriendshipPrompt::FULL;
            std::snprintf(battleMessage, sizeof(battleMessage), "%s",
                          Ui::Amoled::BOX_FULL);
            pushBattleLog(nowMs);
            state_.battlePressedItem = 0xFF;
            damageRows(56, 448);
            return;
        }
        if (!Game::FriendshipService::recordContact(
                gameState, state_.battleWild, area_,
                gameState.gameMinutesTotal * 60UL, &contactSlot)) {
            state_.battleFriendshipPrompt = BattleViewModel::FriendshipPrompt::FULL;
            std::snprintf(battleMessage, sizeof(battleMessage), "%s",
                          Ui::Amoled::BOX_FULL);
            pushBattleLog(nowMs);
            state_.battlePressedItem = 0xFF;
            damageRows(56, 448);
            return;
        }
        Game::FriendshipService::recordSuccess(
            gameState, state_.battleWild.speciesId);
        state_.battleFriendshipContactSlot = contactSlot;
        if (gameState.teamCount < Game::TEAM_CAP) {
            state_.battleFriendshipPrompt = BattleViewModel::FriendshipPrompt::TEAM;
            std::snprintf(battleMessage, sizeof(battleMessage), "%s",
                          Ui::Amoled::ADD_TO_TEAM);
        } else {
            state_.battleFriendshipPrompt = BattleViewModel::FriendshipPrompt::ACQUIRED;
            std::snprintf(battleMessage, sizeof(battleMessage), "%s",
                          Ui::Amoled::FRIEND_ADDED);
        }
        pushBattleLog(nowMs);
        state_.battlePressedItem = 0xFF;
        damageRows(56, 448);
        return;
    }
    if (state_.battleFriendshipPrompt == BattleViewModel::FriendshipPrompt::TEAM) {
        if (choice == 0 && state_.battleFriendshipContactSlot != 0xFF) {
            Game::FriendshipService::InviteResult result =
                Game::FriendshipService::inviteContact(
                    gameState, state_.battleFriendshipContactSlot,
                    gameState.gameMinutesTotal);
            if (result == Game::FriendshipService::InviteResult::JOINED) {
                effects_.syncTeam = true;
                state_.battleFriendshipPrompt = BattleViewModel::FriendshipPrompt::ACQUIRED;
                std::snprintf(battleMessage, sizeof(battleMessage), "%s",
                              Ui::Amoled::TEAM_MEMBER_ADDED);
                pushBattleLog(nowMs);
                damageRows(56, 448);
                return;
            }
        }
        finishBattleAfterFriendship(gameState, nowMs);
        return;
    }
    finishBattleAfterFriendship(gameState, nowMs);
}

void BattleSession::finishBattleAfterFriendship(Game::GameState& gameState, uint32_t nowMs) {
    if (state_.battleProgressionCount > state_.battleProgressionIndex) {
        effects_.stopAutonomous = true;
        state_.battleProgressionSequenceActive = true;
        if (startNextBattleProgression(gameState, nowMs)) {
            // The battle screen has handed off to level/evolution choices.
            effects_.save = true;
            return;
        }
    }
    closeBattle(nowMs);
    effects_.resumeAutonomous = true;
}

void BattleSession::resetBattleProgressionQueue() {
    for (uint8_t slot = 0; slot < Game::TEAM_CAP; ++slot) {
        state_.battleProgressionSlots[slot] = 0;
        state_.battleProgressionOldLevels[slot] = 0;
    }
    state_.battleProgressionCount = 0;
    state_.battleProgressionIndex = 0;
    state_.battleProgressionSequenceActive = false;
    state_.battleVictoryFinalizePending = false;
}

void BattleSession::queueBattleProgression(Game::GameState& gameState, uint8_t teamSlot, uint8_t oldLevel,
                                       bool leveledUp) {
    if (!leveledUp || teamSlot >= gameState.teamCount ||
        teamSlot >= Game::TEAM_CAP ||
        state_.battleProgressionCount >= Game::TEAM_CAP) {
        return;
    }
    state_.battleProgressionSlots[state_.battleProgressionCount] = teamSlot;
    state_.battleProgressionOldLevels[state_.battleProgressionCount] = oldLevel;
    ++state_.battleProgressionCount;
}

bool BattleSession::startNextBattleProgression(Game::GameState& gameState, uint32_t nowMs) {
    while (state_.battleProgressionIndex < state_.battleProgressionCount) {
        const uint8_t index = state_.battleProgressionIndex++;
        const uint8_t teamSlot = state_.battleProgressionSlots[index];
        const uint8_t oldLevel = state_.battleProgressionOldLevels[index];
        if (teamSlot >= gameState.teamCount || teamSlot >= Game::TEAM_CAP ||
            gameState.team[teamSlot].level <= oldLevel) {
            continue;
        }
        effects_.openProgression = true;
        effects_.progressionSlot = teamSlot;
        effects_.progressionOldLevel = oldLevel;
        return true;
    }
    state_.battleProgressionSequenceActive = false;
    return false;
}

void BattleSession::reset() {
    state_ = State{};
    effects_ = Effects{};
    battleTurnController.reset();
    timeline_.reset();
    battleMessage[0] = '\0';
}

void BattleSession::begin(Game::GameState& gameState, const Game::MonsterRuntime& wild,
    uint8_t leaderSlot, bool boss, uint16_t experiencePercent,
    ExploreSpecial::Kind special, uint8_t area, uint32_t nowMs) {
    reset();
    area_ = area;
    state_.battleWild = wild;
    state_.battlePlayerSlot = leaderSlot;
    state_.battleIsBoss = boss;
    state_.battleExperiencePercent = experiencePercent;
    state_.battleSpecialKind = special;
    BattleSystem::resetVolatile(state_.battlePlayerState);
    BattleSystem::resetVolatile(state_.battleWildState);
    const auto* wildSpecies = findSpecies(wild.speciesId);
    const auto* playerSpecies = leaderSlot < gameState.teamCount && leaderSlot < Game::TEAM_CAP
        ? findSpecies(gameState.team[leaderSlot].speciesId) : nullptr;
    if (playerSpecies && wildSpecies) {
        BattleSystem::EffectResolution effects;
        BattleSystem::applyEntryAbility(*playerSpecies, state_.battlePlayerState,
            *wildSpecies, state_.battleWildState, effects);
        effects = BattleSystem::EffectResolution{};
        BattleSystem::applyEntryAbility(*wildSpecies, state_.battleWildState,
            *playerSpecies, state_.battlePlayerState, effects);
    }
    (void)nowMs;
}

void BattleSession::updateFlow(Game::GameState& gameState, uint32_t nowMs) {
    if (timeline_.serviceLog(nowMs)) damageRows(352, 448);
    updateBattleSwitch(gameState, clock_ ? clock_() : nowMs);
    if (state_.battleContinuation != BattleContinuation::NONE && !timeline_.actionBusy()) {
        const auto continuation = state_.battleContinuation;
        state_.battleContinuation = BattleContinuation::NONE;
        if (continuation == BattleContinuation::WILD_TURN)
            performBattleWildTurn(gameState, nowMs);
        else if (continuation == BattleContinuation::FINISH_END_TURN)
            finishBattleEndTurn(gameState, nowMs);
        else advanceBattleTurn(gameState, nowMs);
    }
    if (effects_.close || effects_.openProgression) return;
    if (state_.battlePhase == BattleViewModel::Phase::VICTORY &&
        !timeline_.state().battleExperienceAnimationActive && !timeline_.logBusy() &&
        !state_.battleVictoryFinalizePending && state_.battleContinuation == BattleContinuation::NONE)
        finishBattleVictory(gameState, nowMs);
    if (state_.battleVictoryFinalizePending && !timeline_.logBusy()) {
        state_.battleVictoryFinalizePending = false;
        finishBattleAfterFriendship(gameState, nowMs);
    }
}

bool BattleSession::inputBusy() const {
    return timeline_.inputBusy() || state_.battleContinuation != BattleContinuation::NONE;
}

void BattleSession::pushLog(const char* message, uint32_t nowMs, bool invalidate, uint16_t durationMs) {
    timeline_.pushLog(message, nowMs, invalidate, durationMs);
}
void BattleSession::pushBattleLog(uint32_t nowMs, bool invalidate, uint16_t durationMs) {
    pushLog(battleMessage, nowMs, invalidate, durationMs);
}
void BattleSession::damageRows(int begin, int end) {
    effects_.rowBegin = std::min(effects_.rowBegin, begin);
    effects_.rowEnd = std::max(effects_.rowEnd, end);
}
void BattleSession::closeBattle(uint32_t nowMs) {
    (void)nowMs;
    effects_.close = true;
}
BattleSession::Effects BattleSession::takeEffects() {
    auto result = effects_;
    effects_ = Effects{};
    result.timeline = timeline_.takeEffects();
    return result;
}

bool BattleSession::throwFood(Game::GameState& gameState, Game::ItemId item, uint32_t nowMs) {
    const int8_t foodIndex = Game::foodIndexForItemId(item);
    const auto* wildSpecies = findSpecies(state_.battleWild.speciesId);
    if (foodIndex < 0 || !wildSpecies || !Game::ItemInventory::remove(gameState, item)) return false;
    std::snprintf(battleMessage, sizeof(battleMessage), Ui::Explore::FOOD_THROW_FMT,
                  wildSpecies->name, Ui::Room::FOOD_NAMES[foodIndex]);
    pushBattleLog(nowMs);
    state_.battlePhase = BattleViewModel::Phase::ACTION;
    timeline_.startThrow(item, clock_ ? clock_() : nowMs);
    const auto throwClass = FriendshipSystem::classifyFoodThrow(
        static_cast<uint8_t>(foodIndex), state_.battleWild.nature);
    const bool accepted = FriendshipSystem::acceptsFoodThrow(state_.battleIsBoss,
        throwClass, static_cast<uint8_t>(GameRandom::range(0, 100)));
    std::snprintf(battleMessage, sizeof(battleMessage), accepted
        ? (throwClass == FoodTuning::ThrowClass::LIKED ? Ui::Explore::FOOD_ACCEPTED_LIKED_FMT
                                                     : Ui::Explore::FOOD_ACCEPTED_FMT)
        : (throwClass == FoodTuning::ThrowClass::DISLIKED ? Ui::Explore::FOOD_REFUSED_DISLIKED_FMT
                                                        : Ui::Explore::FOOD_REFUSED_FMT), wildSpecies->name);
    pushBattleLog(nowMs);
    if (accepted) state_.battleFoodBond = FriendshipSystem::addFoodBond(
        state_.battleFoodBond, FriendshipSystem::throwBondGain(throwClass));
    state_.battleContinuation = accepted ? BattleContinuation::ADVANCE_TURN
                                        : BattleContinuation::WILD_TURN;
    return true;
}
void BattleSession::throwAudioPlayed(uint32_t nowMs) {
    if (timeline_.state().battleThrownItem != Game::ItemId::COUNT)
        timeline_.startThrow(timeline_.state().battleThrownItem, nowMs);
}
void BattleSession::itemUsed(uint32_t nowMs) {
    pushLog(Ui::Amoled::ITEM_USED, nowMs);
    state_.battlePhase = BattleViewModel::Phase::ACTION;
    state_.battleContinuation = BattleContinuation::WILD_TURN;
}
bool BattleSession::finishDefeat(Game::GameState& gameState) {
    if (state_.battlePhase != BattleViewModel::Phase::DEFEAT) return false;
    if (state_.battlePlayerSlot < gameState.teamCount && state_.battlePlayerSlot < Game::TEAM_CAP) {
        auto& player = gameState.team[state_.battlePlayerSlot];
        player.hpCur = 0;
        player.fainted = true;
        player.lastSeenAt = Game::gameSecondsForMinutes(gameState.gameMinutesTotal);
    }
    state_.battlePhase = BattleViewModel::Phase::ACTION;
    return true;
}
#if STICKMON_ENABLE_DEBUG_FEATURES
void BattleSession::configureDiagnostic(Game::GameState& gameState, uint8_t scenario,
    Game::MoveId moveId, uint8_t specialSlot) {
    state_.battleIsBoss = false;
    auto& player = gameState.team[0];
    if (scenario == 2 || scenario == 11) {
        player.majorStatus = Game::MajorStatus::SLEEP;
        player.majorStatusTurns = scenario == 2 ? 2 : 1;
    }
    if (scenario == 3) player.majorStatus = Game::MajorStatus::PARALYSIS;
    if (scenario == 4) state_.battlePlayerState.confusionTurns = 3;
    if (scenario == 6) state_.battleWild.hpCur = 1;
    if (scenario == 8) {
        player.hpCur = 1;
        state_.battlePlayerState.stockpileCount = 3;
    }
    clearLog();
    state_.battleTurnPlan = BattleTurnController::TurnPlan{};
    state_.battleTurnPlan.count = 1;
    state_.battleTurnPlan.actions[0] = {BattleTurnController::Side::PLAYER, specialSlot, moveId};
    state_.battleTurnActionIndex = 0;
}
#endif

BattleViewModel BattleSession::model(const Game::GameState& gameState, uint32_t nowMs) const {
    BattleViewModel model;
    model.state = &gameState;
    uint8_t activeSlot = state_.battlePlayerSlot < gameState.teamCount
        ? state_.battlePlayerSlot : 0;
    const Game::MonsterRuntime& active = gameState.team[activeSlot];
    model.playerSpeciesId = active.speciesId;
    model.wildSpeciesId = state_.battleWild.speciesId;
    model.playerLevel = active.level;
    model.wildLevel = state_.battleWild.level;
    model.playerHp = timeline_.hpPercent(
        false, active.hpCur, active.hpMax, nowMs);
    model.wildHp = timeline_.hpPercent(
        true, state_.battleWild.hpCur, state_.battleWild.hpMax, nowMs);
    model.showPlayerExperience = timeline_.state().battleExperienceVisible;
    if (timeline_.state().battleExperienceVisible) {
        if (const Species* activeSpecies = findSpecies(active.speciesId)) {
            const uint32_t shownExp = timeline_.experience(
                nowMs);
            model.playerLevel = levelForExp(
                activeSpecies->growthRate, shownExp);
            const uint32_t levelFloor = minimumExpForLevel(
                activeSpecies->growthRate, model.playerLevel);
            const uint32_t levelCeiling =
                model.playerLevel >= Game::LEVEL_MAX
                    ? levelFloor
                    : minimumExpForLevel(
                          activeSpecies->growthRate,
                          static_cast<uint8_t>(model.playerLevel + 1));
            model.playerExperience = model.playerLevel >= Game::LEVEL_MAX
                ? 100
                : static_cast<uint8_t>(std::min<uint32_t>(
                      100, (shownExp - levelFloor) * 100U /
                               std::max<uint32_t>(
                                   1, levelCeiling - levelFloor)));
        }
    }
    model.phase = state_.battlePhase;
    model.friendshipPrompt = state_.battleFriendshipPrompt;
    model.pressedItem = state_.battlePressedItem;
    model.activeSlot = activeSlot;
    model.teamCount = std::min<uint8_t>(gameState.teamCount, Game::TEAM_CAP);
    static constexpr Game::ItemId BATTLE_ITEMS[] = {
        Game::ItemId::POTION, Game::ItemId::SUPER_POTION,
        Game::ItemId::MAX_POTION, Game::ItemId::FULL_RESTORE,
        Game::ItemId::FULL_HEAL, Game::ItemId::REVIVE,
        Game::ItemId::ANTIDOTE, Game::ItemId::PARALYZE_HEAL,
        Game::ItemId::AWAKENING, Game::ItemId::BURN_HEAL,
        Game::ItemId::ICE_HEAL,
    };
    model.battleBagCount = 0;
    for (Game::ItemId item : BATTLE_ITEMS) {
        if (Game::ItemInventory::count(gameState, item) == 0) continue;
        if (model.battleBagCount >= 4) break;
        model.battleBagItems[model.battleBagCount++] = item;
    }
    model.thrownItem = timeline_.state().battleThrownItem;
    model.itemThrowProgress = std::min(1.0f,
        static_cast<float>(nowMs - timeline_.state().battleItemThrowStartedMs) / 800.0f);
    model.animationActive = timeline_.state().battleAnimationActive;
    model.animationAttackerWild = timeline_.state().battleAnimationAttackerWild;
    model.animationHit = timeline_.state().battleAnimationHit;
    model.animationStatusEffect = timeline_.state().battleAnimationStatusEffect;
    model.animationFx = timeline_.state().battleAnimationFx;
    model.animationDamage = timeline_.state().battleAnimationDamage;
    model.animationFrame = timeline_.state().battleAnimationFrame;
    model.playerSwitchOffsetX = timeline_.switchOffset(nowMs);
    model.playerStatus = active.majorStatus;
    model.wildStatus = state_.battleWild.majorStatus;
    model.playerBattleState = state_.battlePlayerState;
    model.wildBattleState = state_.battleWildState;
    model.logCount = timeline_.state().battleLogVisibleCount;
    for (uint8_t line = 0; line < model.logCount && line < 2; ++line) {
        model.logLines[line] = timeline_.state().battleLogLines[line];
    }
    return model;
}

}  // namespace AmoledV1
