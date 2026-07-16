#include "game/BattleSystem.h"
#include <Arduino.h>

namespace {

static constexpr uint8_t N = 100;
static constexpr uint8_t H = 50;
static constexpr uint8_t D = 200;
static constexpr uint8_t Z = 0;

static constexpr uint8_t TYPE_EFFECT[18][18] = {
//   Nor Fir Wat Gra Ele Ice Fig Poi Gro Fly Psy Bug Roc Gho Dra Dar Ste Fai
    {N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  H,  Z,  N,  N,  H,  N},
    {N,  H,  H,  D,  N,  D,  N,  N,  N,  N,  N,  D,  H,  N,  H,  N,  D,  N},
    {N,  D,  H,  H,  N,  N,  N,  N,  D,  N,  N,  N,  D,  N,  H,  N,  N,  N},
    {N,  H,  D,  H,  N,  N,  N,  H,  D,  H,  N,  H,  D,  N,  H,  N,  H,  N},
    {N,  N,  D,  H,  H,  N,  N,  N,  Z,  D,  N,  N,  N,  N,  H,  N,  N,  N},
    {N,  H,  H,  D,  N,  H,  N,  N,  D,  D,  N,  N,  N,  N,  D,  N,  H,  N},
    {D,  N,  N,  N,  N,  D,  N,  H,  N,  H,  H,  H,  D,  Z,  N,  D,  D,  H},
    {N,  N,  N,  D,  N,  N,  N,  H,  H,  N,  N,  N,  H,  H,  N,  N,  Z,  D},
    {N,  D,  N,  H,  D,  N,  N,  D,  N,  Z,  N,  H,  D,  N,  N,  N,  D,  N},
    {N,  N,  N,  D,  H,  N,  D,  N,  N,  N,  N,  D,  H,  N,  N,  N,  H,  N},
    {N,  N,  N,  N,  N,  N,  D,  D,  N,  N,  H,  N,  N,  N,  N,  Z,  H,  N},
    {N,  H,  N,  D,  N,  N,  H,  H,  N,  H,  D,  N,  N,  H,  N,  D,  H,  H},
    {N,  D,  N,  N,  N,  D,  H,  N,  H,  D,  N,  D,  N,  N,  N,  N,  H,  N},
    {Z,  N,  N,  N,  N,  N,  N,  N,  N,  N,  D,  N,  N,  D,  N,  H,  N,  N},
    {N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  D,  N,  H,  Z},
    {N,  N,  N,  N,  N,  N,  H,  N,  N,  N,  D,  N,  N,  D,  N,  H,  N,  H},
    {N,  H,  H,  N,  H,  D,  N,  N,  N,  N,  N,  N,  D,  N,  N,  N,  H,  D},
    {N,  H,  N,  N,  N,  N,  D,  H,  N,  N,  N,  N,  N,  N,  D,  D,  H,  N},
};

uint8_t typeIndex(TypeId type) {
    uint8_t idx = static_cast<uint8_t>(type);
    return idx < 18 ? idx : 0xFF;
}

uint8_t singleEffect(TypeId attack, TypeId defend) {
    if (defend == TypeId::NONE) return 100;
    uint8_t attackIdx = typeIndex(attack);
    uint8_t defendIdx = typeIndex(defend);
    if (attackIdx == 0xFF || defendIdx == 0xFF) return 100;
    return TYPE_EFFECT[attackIdx][defendIdx];
}

int8_t clampStage(int8_t stage) {
    if (stage < -6) return -6;
    if (stage > 6) return 6;
    return stage;
}

uint16_t applyStatStage(uint16_t value, int8_t stage) {
    stage = clampStage(stage);
    uint16_t numerator = stage >= 0 ? static_cast<uint16_t>(2 + stage) : 2;
    uint16_t denominator = stage >= 0 ? 2 : static_cast<uint16_t>(2 - stage);
    uint32_t adjusted = static_cast<uint32_t>(value) * numerator / denominator;
    return static_cast<uint16_t>(adjusted > 0 ? adjusted : 1);
}

uint8_t adjustedAccuracy(uint8_t base, int8_t accuracyStage, int8_t evasionStage) {
    if (base == 0) return 100;
    accuracyStage = clampStage(accuracyStage);
    evasionStage = clampStage(evasionStage);
    uint16_t accNum = accuracyStage >= 0 ? 3 + accuracyStage : 3;
    uint16_t accDen = accuracyStage >= 0 ? 3 : 3 - accuracyStage;
    uint16_t evaNum = evasionStage >= 0 ? 3 + evasionStage : 3;
    uint16_t evaDen = evasionStage >= 0 ? 3 : 3 - evasionStage;
    uint32_t chance = static_cast<uint32_t>(base) * accNum * evaDen / accDen / evaNum;
    if (chance < 1) return 1;
    return static_cast<uint8_t>(chance > 100 ? 100 : chance);
}

bool hasType(const Species& species, TypeId type) {
    return species.type1 == type || species.type2 == type;
}

bool majorStatusImmune(Game::MajorStatus status, const Species& species, TypeId moveType) {
    switch (status) {
    case Game::MajorStatus::POISON:
    case Game::MajorStatus::TOXIC:
        return hasType(species, TypeId::POISON) || hasType(species, TypeId::STEEL);
    case Game::MajorStatus::BURN:
        return hasType(species, TypeId::FIRE);
    case Game::MajorStatus::FREEZE:
        return hasType(species, TypeId::ICE);
    case Game::MajorStatus::PARALYSIS:
        return moveType == TypeId::ELECTRIC && hasType(species, TypeId::GROUND);
    default:
        return false;
    }
}

uint8_t randomTurns(uint8_t minimum, uint8_t maximum) {
    if (minimum == 0) minimum = 1;
    if (maximum < minimum) maximum = minimum;
    return static_cast<uint8_t>(random(minimum, static_cast<long>(maximum) + 1L));
}

bool rollChance(uint8_t chance) {
    return chance >= 100 || (chance > 0 && random(0, 100) < chance);
}

uint8_t rollHitCount(uint8_t minimum, uint8_t maximum) {
    if (minimum == 0) minimum = 1;
    if (maximum < minimum) maximum = minimum;
    if (minimum == 2 && maximum == 5) {
        uint8_t roll = static_cast<uint8_t>(random(0, 8));
        if (roll < 3) return 2;
        if (roll < 6) return 3;
        return roll == 6 ? 4 : 5;
    }
    return randomTurns(minimum, maximum);
}

uint8_t criticalDenominator(uint8_t stage) {
    static constexpr uint8_t DENOMINATORS[] = {16, 8, 4, 3, 2};
    return DENOMINATORS[stage < 4 ? stage : 4];
}

uint16_t confusionSelfDamage(const Game::MonsterRuntime& monster,
                             const Species& species,
                             const BattleSystem::BattleActorState& state) {
    uint16_t atk = applyStatStage(statFor(species, monster, 1),
                                  state.statStages[static_cast<uint8_t>(BattleStat::ATTACK)]);
    uint16_t def = applyStatStage(statFor(species, monster, 2),
                                  state.statStages[static_cast<uint8_t>(BattleStat::DEFENSE)]);
    if (monster.majorStatus == Game::MajorStatus::BURN) atk = max<uint16_t>(1, atk / 2);
    uint32_t base = (((2UL * monster.level / 5 + 2) * 40UL * atk / max<uint16_t>(1, def)) / 50) + 2;
    base = base * static_cast<uint8_t>(random(85, 101)) / 100;
    return static_cast<uint16_t>(base > 0 ? base : 1);
}

void addHpOutcome(BattleSystem::EffectResolution& result,
                  BattleSystem::EffectOutcomeKind kind,
                  MoveEffectTarget target,
                  uint16_t amount) {
    if (amount == 0) return;
    BattleSystem::EffectOutcome outcome;
    outcome.kind = kind;
    outcome.target = target;
    outcome.amount = amount;
    result.add(outcome);
}

} // namespace

namespace BattleSystem {

uint16_t typeEffectiveness(TypeId attack, TypeId defend1, TypeId defend2) {
    uint16_t value = singleEffect(attack, defend1);
    return static_cast<uint16_t>(value * singleEffect(attack, defend2) / 100);
}

uint8_t specialTriggerChance(const Game::MonsterRuntime& attacker, uint8_t specialSlot) {
    if (specialMoveIdForMonster(attacker, specialSlot) == 0) return 0;
    uint8_t moveSlot = specialSlot + 1;
    if (moveSlot >= Game::MOVE_SLOT_COUNT) return 0;
    uint16_t chance = 5;
    chance += min<uint8_t>(attacker.moveProficiency[moveSlot],
                           Game::MOVE_PROFICIENCY_MAX) * 20 / 100;
    chance += attacker.affection * 25 / 255;
    if (attacker.hpMax > 0 && attacker.hpCur * 100UL < attacker.hpMax * 30UL) chance += 15;
    return chance > 85 ? 85 : static_cast<uint8_t>(chance);
}

bool rollSpecialMove(const Game::MonsterRuntime& attacker) {
    return rollSpecialMoveSlot(attacker) != SPECIAL_SLOT_NONE;
}

uint8_t rollSpecialMoveSlot(const Game::MonsterRuntime& attacker) {
    uint8_t slots[SPECIAL_MOVE_SLOT_COUNT];
    uint8_t count = 0;
    if (specialMoveIdForMonster(attacker, 0) != 0) slots[count++] = 0;
    if (specialMoveIdForMonster(attacker, 1) != 0) slots[count++] = 1;
    if (count == 0) return SPECIAL_SLOT_NONE;
    uint8_t specialSlot = slots[random(0, count)];
    return random(0, 100) < specialTriggerChance(attacker, specialSlot)
        ? specialSlot
        : SPECIAL_SLOT_NONE;
}

Game::MoveId moveIdForAction(const Game::MonsterRuntime& attacker,
                             const Species& attackerSpecies,
                             uint8_t specialSlot) {
    if (specialSlot != SPECIAL_SLOT_NONE) {
        Game::MoveId special = specialMoveIdForMonster(attacker, specialSlot);
        if (special != 0) return special;
    }
    return moveIdForMonster(attackerSpecies, attacker, false);
}

int8_t movePriority(Game::MoveId moveId) {
    const MoveInfo* move = findMove(moveId);
    return move ? move->priority : 0;
}

uint16_t effectiveSpeed(const Game::MonsterRuntime& monster,
                        const Species& species,
                        const BattleActorState& battleState) {
    uint16_t speed = applyStatStage(
        statFor(species, monster, 5),
        battleState.statStages[static_cast<uint8_t>(BattleStat::SPEED)]);
    if (monster.majorStatus == Game::MajorStatus::PARALYSIS) {
        speed = max<uint16_t>(1, speed / 4);
    }
    return speed;
}

ActionCheckResult checkAction(Game::MonsterRuntime& attacker,
                              const Species& attackerSpecies,
                              BattleActorState& battleState,
                              Game::MoveId moveId) {
    ActionCheckResult result;
    const MoveInfo* move = findMove(moveId);

    if (battleState.flinched) {
        battleState.flinched = false;
        result.blockReason = ActionBlockReason::FLINCH;
        return result;
    }

    if (attacker.majorStatus == Game::MajorStatus::SLEEP) {
        if (attacker.majorStatusTurns == 0) attacker.majorStatusTurns = 1;
        attacker.majorStatusTurns--;
        if (attacker.majorStatusTurns == 0) {
            attacker.majorStatus = Game::MajorStatus::NONE;
            result.wokeUp = true;
        } else if (!move || !(move->flags & MOVE_FLAG_USABLE_ASLEEP)) {
            result.blockReason = ActionBlockReason::SLEEP;
            return result;
        }
    }

    if (attacker.majorStatus == Game::MajorStatus::FREEZE) {
        if (random(0, 100) < 20) {
            attacker.majorStatus = Game::MajorStatus::NONE;
            result.thawed = true;
        } else {
            result.blockReason = ActionBlockReason::FREEZE;
            return result;
        }
    }

    if (battleState.confusionTurns > 0) {
        if (battleState.confusionTurns == 1) {
            battleState.confusionTurns = 0;
            result.confusionEnded = true;
        } else {
            battleState.confusionTurns--;
            if (random(0, 100) < 50) {
                result.blockReason = ActionBlockReason::CONFUSION_SELF_HIT;
                result.selfDamage = confusionSelfDamage(attacker, attackerSpecies, battleState);
                return result;
            }
        }
    }

    if (attacker.majorStatus == Game::MajorStatus::PARALYSIS && random(0, 100) < 25) {
        result.blockReason = ActionBlockReason::PARALYSIS;
    }
    return result;
}

DamageResult calcBasicDamage(const Game::MonsterRuntime& attacker,
                             const Species& attackerSpecies,
                             const Game::MonsterRuntime& defender,
                             const Species& defenderSpecies,
                             uint8_t specialSlot,
                             const BattleActorState& attackerState,
                             const BattleActorState& defenderState) {
    DamageResult result;
    result.special = specialSlot != SPECIAL_SLOT_NONE &&
                     specialMoveIdForMonster(attacker, specialSlot) != 0;
    result.specialSlot = result.special ? specialSlot : SPECIAL_SLOT_NONE;
    result.moveId = moveIdForAction(attacker, attackerSpecies, specialSlot);
    const MoveInfo* move = findMove(result.moveId);
    if (!move) return result;

    uint8_t accuracy = adjustedAccuracy(
        move->accuracy,
        attackerState.statStages[static_cast<uint8_t>(BattleStat::ACCURACY)],
        defenderState.statStages[static_cast<uint8_t>(BattleStat::EVASION)]);
    if (move->accuracy > 0 && random(0, 100) >= accuracy) {
        result.missed = true;
        return result;
    }

    if (move->power == 0 || move->damageClass == DamageClass::STATUS) {
        result.effectiveness = 100;
        result.hitCount = 0;
        return result;
    }

    bool usesSpecialStat = move->damageClass == DamageClass::SPECIAL;
    uint16_t atk = statFor(attackerSpecies, attacker, usesSpecialStat ? 3 : 1);
    uint16_t def = statFor(defenderSpecies, defender, usesSpecialStat ? 4 : 2);
    BattleStat attackStat = usesSpecialStat ? BattleStat::SP_ATTACK : BattleStat::ATTACK;
    BattleStat defenseStat = usesSpecialStat ? BattleStat::SP_DEFENSE : BattleStat::DEFENSE;
    atk = applyStatStage(atk, attackerState.statStages[static_cast<uint8_t>(attackStat)]);
    def = applyStatStage(def, defenderState.statStages[static_cast<uint8_t>(defenseStat)]);
    if (!usesSpecialStat && attacker.majorStatus == Game::MajorStatus::BURN) {
        atk = max<uint16_t>(1, atk / 2);
    }

    uint16_t effectiveness = typeEffectiveness(
        move->type, defenderSpecies.type1, defenderSpecies.type2);
    result.effectiveness = effectiveness;
    if (effectiveness == 0) return result;

    uint8_t hits = rollHitCount(move->minHits, move->maxHits);
    result.hitCount = hits;
    uint32_t totalDamage = 0;
    for (uint8_t hit = 0; hit < hits; ++hit) {
        uint32_t base = (((2UL * attacker.level / 5 + 2) * move->power *
                          max<uint16_t>(1, atk) / max<uint16_t>(1, def)) / 50) + 2;
        uint8_t stab = (move->type == attackerSpecies.type1 ||
                        move->type == attackerSpecies.type2) ? 150 : 100;
        uint32_t damage = base * stab / 100;
        damage = damage * effectiveness / 100;
        damage = damage * static_cast<uint8_t>(random(85, 101)) / 100;
        bool critical = random(0, criticalDenominator(move->criticalStage)) == 0;
        if (critical) {
            damage = damage * 3 / 2;
            result.critical = true;
        }
        if (damage == 0) damage = 1;
        totalDamage += damage;
    }
    result.damage = static_cast<uint16_t>(totalDamage > 65535 ? 65535 : totalDamage);
    return result;
}

EffectResolution applyMoveEffects(const MoveInfo& move,
                                  Game::MonsterRuntime& attacker,
                                  const Species& attackerSpecies,
                                  BattleActorState& attackerState,
                                  Game::MonsterRuntime& defender,
                                  const Species& defenderSpecies,
                                  BattleActorState& defenderState,
                                  uint16_t damageDealt,
                                  bool allowFlinch) {
    EffectResolution result;
    bool statRollInitialized = false;
    bool statRollPassed = false;
    uint8_t statRollChance = 0;
    MoveEffectTarget statRollTarget = MoveEffectTarget::DEFENDER;

    for (uint8_t index = 0; index < move.effectCount; ++index) {
        const MoveEffectSpec* effect = moveEffectFor(move, index);
        if (!effect) continue;

        bool passed = false;
        if (effect->kind == MoveEffectKind::STAT_STAGE) {
            if (!statRollInitialized || statRollChance != effect->chance ||
                statRollTarget != effect->target) {
                statRollInitialized = true;
                statRollChance = effect->chance;
                statRollTarget = effect->target;
                statRollPassed = rollChance(effect->chance);
            }
            passed = statRollPassed;
        } else {
            passed = rollChance(effect->chance);
        }
        if (!passed) continue;

        Game::MonsterRuntime& targetMon = effect->target == MoveEffectTarget::ATTACKER
            ? attacker : defender;
        const Species& targetSpecies = effect->target == MoveEffectTarget::ATTACKER
            ? attackerSpecies : defenderSpecies;
        BattleActorState& targetState = effect->target == MoveEffectTarget::ATTACKER
            ? attackerState : defenderState;
        if (effect->target == MoveEffectTarget::DEFENDER && defender.hpCur == 0) continue;
        EffectOutcome outcome;
        outcome.target = effect->target;

        switch (effect->kind) {
        case MoveEffectKind::MAJOR_STATUS: {
            Game::MajorStatus status = static_cast<Game::MajorStatus>(effect->value);
            if (targetMon.majorStatus != Game::MajorStatus::NONE ||
                majorStatusImmune(status, targetSpecies, move.type)) {
                if (move.damageClass == DamageClass::STATUS) {
                    outcome.kind = EffectOutcomeKind::STATUS_FAILED;
                    outcome.status = status;
                    result.add(outcome);
                }
                break;
            }
            targetMon.majorStatus = status;
            targetMon.majorStatusTurns = status == Game::MajorStatus::SLEEP
                ? static_cast<uint8_t>(randomTurns(2, 5) + 1)
                : 0;
            targetState.toxicCounter = 0;
            outcome.kind = EffectOutcomeKind::STATUS_APPLIED;
            outcome.status = status;
            result.add(outcome);
            break;
        }
        case MoveEffectKind::CONFUSION:
            if (targetState.confusionTurns == 0) {
                targetState.confusionTurns = static_cast<uint8_t>(randomTurns(2, 5) + 1);
                outcome.kind = EffectOutcomeKind::CONFUSED;
                result.add(outcome);
            } else if (move.damageClass == DamageClass::STATUS) {
                outcome.kind = EffectOutcomeKind::STATUS_FAILED;
                result.add(outcome);
            }
            break;
        case MoveEffectKind::FLINCH:
            if (allowFlinch) {
                targetState.flinched = true;
                outcome.kind = EffectOutcomeKind::FLINCHED;
                result.add(outcome);
            }
            break;
        case MoveEffectKind::BIND:
            if (targetState.bindTurns == 0) {
                targetState.bindTurns = randomTurns(effect->minTurns, effect->maxTurns);
                outcome.kind = EffectOutcomeKind::BOUND;
                result.add(outcome);
            }
            break;
        case MoveEffectKind::STAT_STAGE: {
            uint8_t statIndex = effect->aux;
            if (statIndex >= static_cast<uint8_t>(BattleStat::COUNT)) break;
            int8_t before = targetState.statStages[statIndex];
            int8_t after = clampStage(static_cast<int8_t>(before + effect->value));
            if (after == before) break;
            targetState.statStages[statIndex] = after;
            outcome.kind = EffectOutcomeKind::STAT_CHANGED;
            outcome.stat = static_cast<BattleStat>(statIndex);
            outcome.stageDelta = static_cast<int8_t>(after - before);
            result.add(outcome);
            break;
        }
        case MoveEffectKind::DRAIN: {
            uint16_t amount = static_cast<uint32_t>(damageDealt) * effect->value / 100;
            if (amount == 0 && damageDealt > 0) amount = 1;
            amount = min<uint16_t>(amount, attacker.hpMax - attacker.hpCur);
            attacker.hpCur += amount;
            addHpOutcome(result, EffectOutcomeKind::DRAINED,
                         MoveEffectTarget::ATTACKER, amount);
            break;
        }
        case MoveEffectKind::RECOIL: {
            uint16_t amount = static_cast<uint32_t>(damageDealt) * effect->value / 100;
            if (amount == 0 && damageDealt > 0) amount = 1;
            amount = min<uint16_t>(amount, attacker.hpCur);
            attacker.hpCur -= amount;
            addHpOutcome(result, EffectOutcomeKind::RECOIL,
                         MoveEffectTarget::ATTACKER, amount);
            break;
        }
        case MoveEffectKind::HEAL: {
            uint16_t amount = max<uint16_t>(1,
                static_cast<uint32_t>(targetMon.hpMax) * effect->value / 100);
            amount = min<uint16_t>(amount, targetMon.hpMax - targetMon.hpCur);
            targetMon.hpCur += amount;
            addHpOutcome(result, EffectOutcomeKind::HEALED, effect->target, amount);
            break;
        }
        case MoveEffectKind::REST:
            if (attacker.hpCur < attacker.hpMax) {
                uint16_t amount = attacker.hpMax - attacker.hpCur;
                attacker.hpCur = attacker.hpMax;
                attacker.majorStatus = Game::MajorStatus::SLEEP;
                attacker.majorStatusTurns = 3;
                attackerState.toxicCounter = 0;
                addHpOutcome(result, EffectOutcomeKind::HEALED,
                             MoveEffectTarget::ATTACKER, amount);
                outcome.kind = EffectOutcomeKind::STATUS_APPLIED;
                outcome.target = MoveEffectTarget::ATTACKER;
                outcome.status = Game::MajorStatus::SLEEP;
                result.add(outcome);
            } else {
                outcome.kind = EffectOutcomeKind::STATUS_FAILED;
                result.add(outcome);
            }
            break;
        case MoveEffectKind::CURE_STATUS:
            if (targetMon.majorStatus != Game::MajorStatus::NONE) {
                targetMon.majorStatus = Game::MajorStatus::NONE;
                targetMon.majorStatusTurns = 0;
                targetState.toxicCounter = 0;
                outcome.kind = EffectOutcomeKind::CURED;
                result.add(outcome);
            }
            break;
        case MoveEffectKind::CLEAR_BIND:
            if (targetState.bindTurns > 0) {
                targetState.bindTurns = 0;
                outcome.kind = EffectOutcomeKind::BIND_CLEARED;
                result.add(outcome);
            }
            break;
        case MoveEffectKind::YAWN:
            if (targetMon.majorStatus == Game::MajorStatus::NONE &&
                targetState.yawnTurns == 0) {
                targetState.yawnTurns = 2;
                outcome.kind = EffectOutcomeKind::YAWNED;
                result.add(outcome);
            } else if (move.damageClass == DamageClass::STATUS) {
                outcome.kind = EffectOutcomeKind::STATUS_FAILED;
                result.add(outcome);
            }
            break;
        }
    }
    if (move.damageClass == DamageClass::STATUS && result.count == 0) {
        EffectOutcome outcome;
        outcome.kind = EffectOutcomeKind::STATUS_FAILED;
        result.add(outcome);
    }
    return result;
}

EffectResolution resolveEndTurn(Game::MonsterRuntime& monster,
                                const Species& species,
                                BattleActorState& battleState) {
    EffectResolution result;
    if (monster.hpCur == 0) return result;

    uint16_t statusDamage = 0;
    if (monster.majorStatus == Game::MajorStatus::POISON ||
        monster.majorStatus == Game::MajorStatus::BURN) {
        statusDamage = max<uint16_t>(1, monster.hpMax / 8);
    } else if (monster.majorStatus == Game::MajorStatus::TOXIC) {
        if (battleState.toxicCounter < 15) battleState.toxicCounter++;
        statusDamage = max<uint16_t>(1,
            static_cast<uint32_t>(monster.hpMax) * battleState.toxicCounter / 16);
    }
    if (statusDamage > 0) {
        statusDamage = min<uint16_t>(statusDamage, monster.hpCur);
        monster.hpCur -= statusDamage;
        EffectOutcome outcome;
        outcome.kind = EffectOutcomeKind::STATUS_DAMAGE;
        outcome.status = monster.majorStatus;
        outcome.amount = statusDamage;
        result.add(outcome);
    }

    if (monster.hpCur > 0 && battleState.bindTurns > 0) {
        uint16_t damage = min<uint16_t>(max<uint16_t>(1, monster.hpMax / 16), monster.hpCur);
        monster.hpCur -= damage;
        EffectOutcome outcome;
        outcome.kind = EffectOutcomeKind::BIND_DAMAGE;
        outcome.amount = damage;
        result.add(outcome);
        battleState.bindTurns--;
        if (battleState.bindTurns == 0) {
            outcome = EffectOutcome{};
            outcome.kind = EffectOutcomeKind::BIND_ENDED;
            result.add(outcome);
        }
    }

    if (monster.hpCur > 0 && battleState.yawnTurns > 0) {
        battleState.yawnTurns--;
        if (battleState.yawnTurns == 0 && monster.majorStatus == Game::MajorStatus::NONE &&
            !majorStatusImmune(Game::MajorStatus::SLEEP, species, TypeId::NORMAL)) {
            monster.majorStatus = Game::MajorStatus::SLEEP;
            monster.majorStatusTurns = static_cast<uint8_t>(randomTurns(2, 5) + 1);
            EffectOutcome outcome;
            outcome.kind = EffectOutcomeKind::YAWN_SLEEP;
            outcome.status = Game::MajorStatus::SLEEP;
            result.add(outcome);
        }
    }
    return result;
}

bool canFlee(const BattleActorState& battleState) {
    return battleState.bindTurns == 0;
}

void resetVolatile(BattleActorState& battleState) {
    battleState = BattleActorState{};
}

} // namespace BattleSystem
