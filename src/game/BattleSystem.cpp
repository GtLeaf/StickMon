#include "game/BattleSystem.h"
#include <algorithm>
#include "game/GameRandom.h"

namespace {

static constexpr uint8_t N = 100;
static constexpr uint8_t H = 50;
static constexpr uint8_t D = 200;
static constexpr uint8_t Z = 0;

static constexpr uint8_t SPECIAL_CHANCE_MIN = 15;
static constexpr uint8_t SPECIAL_CHANCE_MAX = 40;
static constexpr uint8_t SPECIAL_AFFECTION_BONUS_MAX = 10;
static constexpr uint8_t SPECIAL_LOW_HP_BONUS = 5;
static constexpr uint32_t PROFICIENCY_CURVE_DENOMINATOR = 1000000UL;

constexpr uint32_t clampedProficiency(uint8_t proficiency) {
    return proficiency > Game::MOVE_PROFICIENCY_MAX
        ? Game::MOVE_PROFICIENCY_MAX
        : proficiency;
}

constexpr uint32_t proficiencySmoothstep(uint8_t proficiency) {
    return clampedProficiency(proficiency) * clampedProficiency(proficiency) *
           (300UL - 2UL * clampedProficiency(proficiency));
}

constexpr uint8_t specialProficiencyChance(uint8_t proficiency) {
    return static_cast<uint8_t>(
        SPECIAL_CHANCE_MIN +
        ((SPECIAL_CHANCE_MAX - SPECIAL_CHANCE_MIN) *
             proficiencySmoothstep(proficiency) +
         PROFICIENCY_CURVE_DENOMINATOR / 2) /
            PROFICIENCY_CURVE_DENOMINATOR);
}

static_assert(specialProficiencyChance(0) == 15 &&
                  specialProficiencyChance(25) == 19 &&
                  specialProficiencyChance(50) == 28 &&
                  specialProficiencyChance(60) == 31 &&
                  specialProficiencyChance(75) == 36 &&
                  specialProficiencyChance(90) == 39 &&
                  specialProficiencyChance(100) == 40,
              "special move proficiency curve must keep its tuning anchors");

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

int8_t abilityAdjustedStage(int8_t delta, AbilityId ability) {
    if (ability != AbilityId::SIMPLE) return delta;
    return clampStage(static_cast<int8_t>(delta * 2));
}

bool hasMajorStatus(const Game::MonsterRuntime& monster) {
    return monster.majorStatus != Game::MajorStatus::NONE;
}

uint16_t scaledMovePower(const MoveInfo& move,
                         const Game::MonsterRuntime& attacker,
                         const BattleSystem::BattleActorState& attackerState,
                         const Game::MonsterRuntime& defender,
                         const BattleSystem::DamageContext& context) {
    uint32_t power = move.power;
    if ((move.flags & MOVE_FLAG_SPIT_UP) != 0) power = 100;
    if ((move.flags & MOVE_FLAG_DOUBLE_POISONED) &&
        (defender.majorStatus == Game::MajorStatus::POISON ||
         defender.majorStatus == Game::MajorStatus::TOXIC)) {
        power *= 2;
    }
    if ((move.flags & MOVE_FLAG_DOUBLE_STATUS) && hasMajorStatus(defender)) power *= 2;
    if ((move.flags & MOVE_FLAG_STORED_POWER) != 0) {
        uint8_t positiveStages = 0;
        for (uint8_t index = 0; index < static_cast<uint8_t>(BattleStat::COUNT); ++index) {
            if (attackerState.statStages[index] > 0) {
                positiveStages += static_cast<uint8_t>(attackerState.statStages[index]);
            }
        }
        power = 20UL * (1UL + positiveStages);
    }
    if ((move.flags & MOVE_FLAG_ERUPTION_POWER) && attacker.hpMax > 0) {
        power = std::max<uint32_t>(1, power * attacker.hpCur / attacker.hpMax);
    }
    if ((move.flags & MOVE_FLAG_BRINE) && defender.hpMax > 0 &&
        defender.hpCur * 2UL <= defender.hpMax) {
        power *= 2;
    }
    if ((move.flags & MOVE_FLAG_PAYBACK) && context.attackerMovesSecond) power *= 2;
    if ((move.flags & MOVE_FLAG_ASSURANCE) && context.defenderDamagedThisTurn) power *= 2;
    if ((move.flags & MOVE_FLAG_ROLLOUT) != 0) {
        power <<= std::min<uint8_t>(attackerState.consecutiveHits, 4);
    }
    if ((move.flags & MOVE_FLAG_FURY_CUTTER) != 0) {
        power <<= std::min<uint8_t>(attackerState.consecutiveHits, 3);
    }
    if ((move.flags & MOVE_FLAG_SPIT_UP) != 0) {
        power *= std::max<uint8_t>(1, attackerState.stockpileCount);
    }
    return static_cast<uint16_t>(std::min<uint32_t>(power, 1000));
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
        return hasType(species, TypeId::ELECTRIC) ||
               (moveType == TypeId::ELECTRIC && hasType(species, TypeId::GROUND));
    default:
        return false;
    }
}

uint8_t randomTurns(uint8_t minimum, uint8_t maximum) {
    if (minimum == 0) minimum = 1;
    if (maximum < minimum) maximum = minimum;
    return static_cast<uint8_t>(
        GameRandom::range(minimum, static_cast<uint32_t>(maximum) + 1U));
}

bool rollChance(uint8_t chance) {
    return chance >= 100 ||
           (chance > 0 && GameRandom::range(0, 100) < chance);
}

uint8_t rollHitCount(uint8_t minimum, uint8_t maximum) {
    if (minimum == 0) minimum = 1;
    if (maximum < minimum) maximum = minimum;
    if (minimum == 2 && maximum == 5) {
        uint8_t roll = static_cast<uint8_t>(GameRandom::range(0, 8));
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
    if (monster.majorStatus == Game::MajorStatus::BURN) {
        atk = std::max<uint16_t>(1, atk / 2);
    }
    uint32_t base =
        (((2UL * monster.level / 5 + 2) * 40UL * atk /
          std::max<uint16_t>(1, def)) /
         50) +
        2;
    base = base * static_cast<uint8_t>(GameRandom::range(85, 101)) / 100;
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

bool aiEffectUseful(
    const MoveInfo& move,
    const MoveEffectSpec& effect,
    const Game::MonsterRuntime& attacker,
    const Species& attackerSpecies,
    const BattleSystem::BattleActorState& attackerState,
    const Game::MonsterRuntime& defender,
    const Species& defenderSpecies,
    const BattleSystem::BattleActorState& defenderState) {
    const Game::MonsterRuntime& targetMon =
        effect.target == MoveEffectTarget::ATTACKER ? attacker : defender;
    const Species& targetSpecies =
        effect.target == MoveEffectTarget::ATTACKER
            ? attackerSpecies
            : defenderSpecies;
    const BattleSystem::BattleActorState& targetState =
        effect.target == MoveEffectTarget::ATTACKER
            ? attackerState
            : defenderState;
    switch (effect.kind) {
    case MoveEffectKind::MAJOR_STATUS:
        return targetMon.majorStatus == Game::MajorStatus::NONE &&
               !majorStatusImmune(
                   static_cast<Game::MajorStatus>(effect.value),
                   targetSpecies, move.type);
    case MoveEffectKind::CONFUSION:
        return targetState.confusionTurns == 0;
    case MoveEffectKind::FLINCH:
        return abilityForSpecies(targetSpecies) != AbilityId::INNER_FOCUS;
    case MoveEffectKind::BIND:
        return targetState.bindTurns == 0;
    case MoveEffectKind::STAT_STAGE: {
        if (effect.aux >= static_cast<uint8_t>(BattleStat::COUNT)) return false;
        int8_t before = targetState.statStages[effect.aux];
        return clampStage(static_cast<int8_t>(before + effect.value)) != before;
    }
    case MoveEffectKind::DRAIN:
    case MoveEffectKind::HEAL:
        return targetMon.hpCur < targetMon.hpMax;
    case MoveEffectKind::RECOIL:
        return true;
    case MoveEffectKind::REST:
        return attacker.hpCur < attacker.hpMax;
    case MoveEffectKind::CURE_STATUS:
        return targetMon.majorStatus != Game::MajorStatus::NONE;
    case MoveEffectKind::CLEAR_BIND:
        return targetState.bindTurns > 0;
    case MoveEffectKind::YAWN:
        return targetMon.majorStatus == Game::MajorStatus::NONE &&
               targetState.yawnTurns == 0;
    }
    return false;
}

bool aiStatusMoveUseful(
    const MoveInfo& move,
    const Game::MonsterRuntime& attacker,
    const Species& attackerSpecies,
    const BattleSystem::BattleActorState& attackerState,
    const Game::MonsterRuntime& defender,
    const Species& defenderSpecies,
    const BattleSystem::BattleActorState& defenderState) {
    if ((move.flags & MOVE_FLAG_STOCKPILE) && attackerState.stockpileCount >= 3) {
        return false;
    }
    if ((move.flags & MOVE_FLAG_SWALLOW) &&
        (attackerState.stockpileCount == 0 || attacker.hpCur >= attacker.hpMax)) {
        return false;
    }
    for (uint8_t index = 0; index < move.effectCount; ++index) {
        const MoveEffectSpec* effect = moveEffectFor(move, index);
        if (effect && aiEffectUseful(
                move, *effect, attacker, attackerSpecies, attackerState,
                defender, defenderSpecies, defenderState)) {
            return true;
        }
    }
    return false;
}

bool isDamagingMove(const MoveInfo* move) {
    return move && move->battleSupported && move->power > 0 &&
           move->damageClass != DamageClass::STATUS;
}

uint16_t scaleAiMoveWeight(uint16_t weight, uint16_t numerator,
                           uint16_t denominator) {
    if (weight == 0 || denominator == 0) return 0;
    uint32_t scaled = static_cast<uint32_t>(weight) * numerator / denominator;
    return static_cast<uint16_t>(std::max<uint32_t>(1, scaled));
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
    uint16_t chance = specialProficiencyChance(attacker.moveProficiency[moveSlot]);
    chance += attacker.affection * SPECIAL_AFFECTION_BONUS_MAX / 255;
    if (attacker.hpMax > 0 && attacker.hpCur * 100UL < attacker.hpMax * 30UL) {
        chance += SPECIAL_LOW_HP_BONUS;
    }
    return std::min<uint16_t>(chance, SPECIAL_CHANCE_MAX);
}

bool rollSpecialMove(const Game::MonsterRuntime& attacker) {
    return rollSpecialMoveSlot(attacker) != SPECIAL_SLOT_NONE;
}

uint8_t rollSpecialMoveSlot(const Game::MonsterRuntime& attacker) {
    uint8_t chances[SPECIAL_MOVE_SLOT_COUNT] = {};
    uint8_t totalChance = 0;
    for (uint8_t specialSlot = 0; specialSlot < SPECIAL_MOVE_SLOT_COUNT; ++specialSlot) {
        chances[specialSlot] = specialTriggerChance(attacker, specialSlot);
        totalChance += chances[specialSlot];
    }
    if (totalChance == 0) return SPECIAL_SLOT_NONE;

    uint8_t roll = static_cast<uint8_t>(GameRandom::range(0, 100));
    uint8_t threshold = 0;
    for (uint8_t specialSlot = 0; specialSlot < SPECIAL_MOVE_SLOT_COUNT; ++specialSlot) {
        threshold += chances[specialSlot];
        if (roll < threshold) return specialSlot;
    }
    return SPECIAL_SLOT_NONE;
}

BattleMoveWeights aiMoveWeights(
    const Game::MonsterRuntime& attacker,
    const Species& attackerSpecies,
    const BattleActorState& attackerState,
    const Game::MonsterRuntime& defender,
    const Species& defenderSpecies,
    const BattleActorState& defenderState,
    const BattleAiMemory& memory) {
    BattleMoveWeights result;
    uint16_t specialChanceTotal = 0;
    for (uint8_t slot = 0; slot < SPECIAL_MOVE_SLOT_COUNT; ++slot) {
        result.special[slot] = specialTriggerChance(attacker, slot);
        specialChanceTotal += result.special[slot];
    }
    result.basic = specialChanceTotal < 80
        ? static_cast<uint16_t>(100 - specialChanceTotal)
        : 20;

    const MoveInfo* moves[Game::MOVE_SLOT_COUNT] = {
        findMove(moveIdForAction(attacker, attackerSpecies, SPECIAL_SLOT_NONE)),
        findMove(specialMoveIdForMonster(attacker, 0)),
        findMove(specialMoveIdForMonster(attacker, 1)),
    };
    uint16_t* weights[Game::MOVE_SLOT_COUNT] = {
        &result.basic,
        &result.special[0],
        &result.special[1],
    };
    bool defenderIsGhost = hasType(defenderSpecies, TypeId::GHOST);
    bool hasUsableDamage = false;

    for (uint8_t index = 0; index < Game::MOVE_SLOT_COUNT; ++index) {
        const MoveInfo* move = moves[index];
        uint16_t& weight = *weights[index];
        if (!move || !move->battleSupported || weight == 0) {
            weight = 0;
            continue;
        }
        if (isDamagingMove(move)) {
            uint16_t effectiveness = typeEffectiveness(
                move->type, defenderSpecies.type1, defenderSpecies.type2);
            if (effectiveness == 0) {
                weight = 0;
                continue;
            }
            hasUsableDamage = true;
            if (effectiveness >= 200) {
                weight = scaleAiMoveWeight(weight, 3, 2);
            } else if (effectiveness < 100) {
                weight = scaleAiMoveWeight(weight, 1, 2);
            }
            if (defenderIsGhost && index > 0) {
                weight = scaleAiMoveWeight(weight, 2, 1);
            }
            continue;
        }

        if (move->damageClass != DamageClass::STATUS ||
            !aiStatusMoveUseful(
                *move, attacker, attackerSpecies, attackerState,
                defender, defenderSpecies, defenderState)) {
            weight = 0;
            continue;
        }
        if (memory.consecutiveStatusMoves > 0) {
            weight = scaleAiMoveWeight(weight, 1, 4);
        }
    }

    if (hasUsableDamage) {
        for (uint8_t index = 0; index < Game::MOVE_SLOT_COUNT; ++index) {
            const MoveInfo* move = moves[index];
            if (!move || move->damageClass != DamageClass::STATUS) continue;
            if (memory.consecutiveStatusMoves >= 2 ||
                move->id == memory.lastMoveId) {
                *weights[index] = 0;
            }
        }
    }
    return result;
}

uint8_t chooseAiMoveSlot(
    const Game::MonsterRuntime& attacker,
    const Species& attackerSpecies,
    const BattleActorState& attackerState,
    const Game::MonsterRuntime& defender,
    const Species& defenderSpecies,
    const BattleActorState& defenderState,
    BattleAiMemory& memory) {
    BattleMoveWeights weights = aiMoveWeights(
        attacker, attackerSpecies, attackerState,
        defender, defenderSpecies, defenderState, memory);
    uint32_t total = weights.basic;
    for (uint8_t slot = 0; slot < SPECIAL_MOVE_SLOT_COUNT; ++slot) {
        total += weights.special[slot];
    }

    uint8_t selected = SPECIAL_SLOT_NONE;
    if (total > 0) {
        uint32_t roll = GameRandom::range(0, total);
        if (roll >= weights.basic) {
            roll -= weights.basic;
            for (uint8_t slot = 0; slot < SPECIAL_MOVE_SLOT_COUNT; ++slot) {
                if (roll < weights.special[slot]) {
                    selected = slot;
                    break;
                }
                roll -= weights.special[slot];
            }
        }
    }

    Game::MoveId selectedMoveId = moveIdForAction(
        attacker, attackerSpecies, selected);
    const MoveInfo* selectedMove = findMove(selectedMoveId);
    memory.lastMoveId = selectedMoveId;
    if (selectedMove && selectedMove->damageClass == DamageClass::STATUS) {
        if (memory.consecutiveStatusMoves < 0xFF) {
            ++memory.consecutiveStatusMoves;
        }
    } else {
        memory.consecutiveStatusMoves = 0;
    }
    return selected;
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

bool moveRequiresCharge(Game::MoveId moveId) {
    const MoveInfo* move = findMove(moveId);
    return move && (move->flags & MOVE_FLAG_TWO_TURN_CHARGE) != 0;
}

bool moveLocksUser(Game::MoveId moveId) {
    const MoveInfo* move = findMove(moveId);
    return move && (move->flags & MOVE_FLAG_RAMPAGE) != 0;
}

bool moveCausesRecharge(Game::MoveId moveId) {
    const MoveInfo* move = findMove(moveId);
    return move && (move->flags & MOVE_FLAG_RECHARGE) != 0;
}

bool moveForcesWildBattleEnd(Game::MoveId moveId) {
    const MoveInfo* move = findMove(moveId);
    return move && (move->flags & MOVE_FLAG_FORCE_WILD_END) != 0;
}

uint8_t moveSlotIndex(const Game::MonsterRuntime& monster,
                      const Species& species,
                      Game::MoveId moveId) {
    if (moveId == moveIdForMonster(species, monster, false)) return 0;
    if (moveId == specialMoveIdForMonster(monster, 0)) return 1;
    if (moveId == specialMoveIdForMonster(monster, 1)) return 2;
    return 0xFF;
}

bool canUseLastResort(const Game::MonsterRuntime& monster,
                      const Species& species,
                      const BattleActorState& state,
                      Game::MoveId moveId) {
    const MoveInfo* move = findMove(moveId);
    if (!move || move->id != 387) return true;
    uint8_t knownMask = 0;
    for (uint8_t slot = 0; slot < Game::MOVE_SLOT_COUNT; ++slot) {
        Game::MoveId known = slot == 0
            ? moveIdForMonster(species, monster, false)
            : specialMoveIdForMonster(monster, slot - 1);
        if (known != 0 && known != moveId) knownMask |= static_cast<uint8_t>(1U << slot);
    }
    return knownMask != 0 && (state.usedMoveMask & knownMask) == knownMask;
}

bool isChargingMove(const BattleActorState& battleState) {
    return battleState.chargingMoveId != 0;
}

void beginChargingMove(BattleActorState& battleState,
                       Game::MoveId moveId,
                       uint8_t specialSlot) {
    battleState.chargingMoveId = moveId;
    battleState.chargingSpecialSlot = specialSlot;
}

void clearChargingMove(BattleActorState& battleState) {
    battleState.chargingMoveId = 0;
    battleState.chargingSpecialSlot = SPECIAL_SLOT_NONE;
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
        speed = std::max<uint16_t>(1, speed / 4);
    }
    return speed;
}

void applyEntryAbility(const Species& entrantSpecies,
                       BattleActorState& entrantState,
                       const Species& opponentSpecies,
                       BattleActorState& opponentState,
                       EffectResolution& result) {
    (void)entrantState;
    if (abilityForSpecies(entrantSpecies) != AbilityId::INTIMIDATE) return;
    uint8_t index = static_cast<uint8_t>(BattleStat::ATTACK);
    int8_t before = opponentState.statStages[index];
    int8_t delta = abilityAdjustedStage(-1, abilityForSpecies(opponentSpecies));
    int8_t after = clampStage(static_cast<int8_t>(before + delta));
    if (after == before) return;
    opponentState.statStages[index] = after;
    EffectOutcome outcome;
    outcome.kind = EffectOutcomeKind::STAT_CHANGED;
    outcome.target = MoveEffectTarget::DEFENDER;
    outcome.stat = BattleStat::ATTACK;
    outcome.stageDelta = static_cast<int8_t>(after - before);
    outcome.ability = AbilityId::INTIMIDATE;
    result.add(outcome);
}

ActionCheckResult checkAction(Game::MonsterRuntime& attacker,
                              const Species& attackerSpecies,
                              BattleActorState& battleState,
                              Game::MoveId moveId,
                              bool defenderMoveIsDamaging) {
    ActionCheckResult result;
    const MoveInfo* move = findMove(moveId);

    if (battleState.recharging) {
        battleState.recharging = false;
        result.blockReason = ActionBlockReason::RECHARGE;
        return result;
    }
    if (move && (move->flags & MOVE_FLAG_SUCKER_PUNCH) && !defenderMoveIsDamaging) {
        result.blockReason = ActionBlockReason::MOVE_FAILED;
        return result;
    }
    if (move && (move->flags & MOVE_FLAG_SWALLOW) && battleState.stockpileCount == 0) {
        result.blockReason = ActionBlockReason::MOVE_FAILED;
        return result;
    }
    if (move && (move->flags & MOVE_FLAG_SPIT_UP) && battleState.stockpileCount == 0) {
        result.blockReason = ActionBlockReason::MOVE_FAILED;
        return result;
    }
    if (move && (move->flags & MOVE_FLAG_STOCKPILE) && battleState.stockpileCount >= 3) {
        result.blockReason = ActionBlockReason::MOVE_FAILED;
        return result;
    }
    if (move && (move->flags & MOVE_FLAG_REQUIRES_ASLEEP_USER) &&
        attacker.majorStatus != Game::MajorStatus::SLEEP) {
        result.blockReason = ActionBlockReason::MOVE_FAILED;
        return result;
    }
    if (!canUseLastResort(attacker, attackerSpecies, battleState, moveId)) {
        result.blockReason = ActionBlockReason::MOVE_FAILED;
        return result;
    }

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
        if ((move && move->type == TypeId::FIRE) ||
            GameRandom::range(0, 100) < 20) {
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
            if (GameRandom::range(0, 100) < 50) {
                result.blockReason = ActionBlockReason::CONFUSION_SELF_HIT;
                result.selfDamage = confusionSelfDamage(attacker, attackerSpecies, battleState);
                return result;
            }
        }
    }

    if (attacker.majorStatus == Game::MajorStatus::PARALYSIS &&
        GameRandom::range(0, 100) < 25) {
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
                             const BattleActorState& defenderState,
                             const DamageContext& context) {
    DamageResult result;
    result.special = specialSlot != SPECIAL_SLOT_NONE &&
                     specialMoveIdForMonster(attacker, specialSlot) != 0;
    result.specialSlot = result.special ? specialSlot : SPECIAL_SLOT_NONE;
    result.moveId = moveIdForAction(attacker, attackerSpecies, specialSlot);
    const MoveInfo* move = findMove(result.moveId);
    if (!move) return result;
    if ((move->flags & MOVE_FLAG_REQUIRES_SLEEPING_TARGET) &&
        defender.majorStatus != Game::MajorStatus::SLEEP) {
        result.failed = true;
        return result;
    }

    AbilityId attackerAbility = abilityForSpecies(attackerSpecies);
    AbilityId defenderAbility = abilityForSpecies(defenderSpecies);
    uint8_t accuracy = adjustedAccuracy(
        move->accuracy,
        attackerState.statStages[static_cast<uint8_t>(BattleStat::ACCURACY)],
        defenderState.statStages[static_cast<uint8_t>(BattleStat::EVASION)]);
    if (attackerAbility == AbilityId::COMPOUND_EYES && move->accuracy > 0) {
        accuracy = static_cast<uint8_t>(std::min<uint16_t>(100, accuracy * 13 / 10));
    }
    if (move->accuracy > 0 && GameRandom::range(0, 100) >= accuracy) {
        result.missed = true;
        return result;
    }

    if (move->type == TypeId::GROUND && defenderAbility == AbilityId::LEVITATE) {
        result.effectiveness = 0;
        result.activatedAbility = defenderAbility;
        return result;
    }
    if ((move->type == TypeId::WATER && defenderAbility == AbilityId::WATER_ABSORB) ||
        (move->type == TypeId::ELECTRIC && defenderAbility == AbilityId::VOLT_ABSORB) ||
        (move->type == TypeId::FIRE && defenderAbility == AbilityId::FLASH_FIRE)) {
        result.absorbed = true;
        result.effectiveness = 0;
        result.activatedAbility = defenderAbility;
        if (defenderAbility == AbilityId::WATER_ABSORB ||
            defenderAbility == AbilityId::VOLT_ABSORB) {
            result.absorbedAmount = std::min<uint16_t>(
                std::max<uint16_t>(1, defender.hpMax / 4),
                defender.hpMax - defender.hpCur);
        }
        return result;
    }

    if ((move->power == 0 && (move->flags & MOVE_FLAG_SPIT_UP) == 0) ||
        move->damageClass == DamageClass::STATUS) {
        result.effectiveness = 100;
        result.hitCount = 0;
        return result;
    }

    bool usesSpecialStat = move->damageClass == DamageClass::SPECIAL;
    uint16_t atk = statFor(attackerSpecies, attacker, usesSpecialStat ? 3 : 1);
    uint16_t def = statFor(defenderSpecies, defender, usesSpecialStat ? 4 : 2);
    BattleStat attackStat = usesSpecialStat ? BattleStat::SP_ATTACK : BattleStat::ATTACK;
    BattleStat defenseStat = usesSpecialStat ? BattleStat::SP_DEFENSE : BattleStat::DEFENSE;
    int8_t attackerStage = attackerState.statStages[static_cast<uint8_t>(attackStat)];
    int8_t defenderStage = defenderState.statStages[static_cast<uint8_t>(defenseStat)];
    bool ignoreDefenderStages = (move->flags & MOVE_FLAG_IGNORE_DEFENDER_STAGES) != 0;
    if (ignoreDefenderStages) defenderStage = 0;
    atk = applyStatStage(atk, attackerStage);
    def = applyStatStage(def, defenderStage);
    if (!usesSpecialStat && attackerAbility == AbilityId::HUGE_POWER) atk *= 2;
    if (!usesSpecialStat && attacker.majorStatus == Game::MajorStatus::BURN) {
        atk = std::max<uint16_t>(1, atk / 2);
    }

    uint16_t effectiveness = typeEffectiveness(
        move->type, defenderSpecies.type1, defenderSpecies.type2);
    if ((move->flags & MOVE_FLAG_FREEZE_DRY) && hasType(defenderSpecies, TypeId::WATER)) {
        effectiveness = static_cast<uint16_t>(effectiveness * 4);
    }
    result.effectiveness = effectiveness;
    if (effectiveness == 0) return result;

    uint8_t hits = rollHitCount(move->minHits, move->maxHits);
    result.hitCount = hits;
    uint32_t totalDamage = 0;
    uint16_t power = scaledMovePower(*move, attacker, attackerState, defender, context);
    if (attackerAbility == AbilityId::TECHNICIAN && power <= 60) power = power * 3 / 2;
    if (attacker.hpMax > 0 && attacker.hpCur * 3UL <= attacker.hpMax) {
        if ((attackerAbility == AbilityId::OVERGROW && move->type == TypeId::GRASS) ||
            (attackerAbility == AbilityId::BLAZE && move->type == TypeId::FIRE) ||
            (attackerAbility == AbilityId::TORRENT && move->type == TypeId::WATER)) {
            power = power * 3 / 2;
        }
    }
    if (attackerState.flashFireBoost && move->type == TypeId::FIRE) power = power * 3 / 2;
    if (defenderAbility == AbilityId::THICK_FAT &&
        (move->type == TypeId::FIRE || move->type == TypeId::ICE)) {
        power = std::max<uint16_t>(1, power / 2);
    }

    for (uint8_t hit = 0; hit < hits; ++hit) {
        bool critical = (move->flags & MOVE_FLAG_ALWAYS_CRITICAL) != 0 ||
            GameRandom::range(0, criticalDenominator(move->criticalStage)) == 0;
        uint16_t hitAtk = atk;
        uint16_t hitDef = def;
        if (critical) {
            if (attackerStage < 0) {
                hitAtk = statFor(attackerSpecies, attacker, usesSpecialStat ? 3 : 1);
                if (!usesSpecialStat && attackerAbility == AbilityId::HUGE_POWER) hitAtk *= 2;
            }
            if (!ignoreDefenderStages && defenderStage > 0) {
                hitDef = statFor(defenderSpecies, defender, usesSpecialStat ? 4 : 2);
            }
        }
        uint32_t base = (((2UL * attacker.level / 5 + 2) * power *
                          std::max<uint16_t>(1, hitAtk) /
                          std::max<uint16_t>(1, hitDef)) /
                         50) +
                        2;
        uint8_t stab = (move->type == attackerSpecies.type1 ||
                        move->type == attackerSpecies.type2)
            ? (attackerAbility == AbilityId::ADAPTABILITY ? 200 : 150) : 100;
        uint32_t damage = base * stab / 100;
        damage = damage * effectiveness / 100;
        damage =
            damage * static_cast<uint8_t>(GameRandom::range(85, 101)) / 100;
        if (critical) {
            damage = damage * 3 / 2;
            result.critical = true;
        }
        if (damage == 0) damage = 1;
        totalDamage += damage;
    }
    uint16_t damage = static_cast<uint16_t>(totalDamage > 65535 ? 65535 : totalDamage);
    if ((move->flags & MOVE_FLAG_FALSE_SWIPE) && defender.hpCur > 1) {
        damage = std::min<uint16_t>(damage, defender.hpCur - 1);
    }
    if (defenderAbility == AbilityId::SOLID_ROCK && effectiveness > 100) {
        damage = damage * 3 / 4;
    }
    if (defenderAbility == AbilityId::STURDY && hits == 1 &&
        defender.hpCur == defender.hpMax &&
        damage >= defender.hpCur && defender.hpCur > 1) {
        damage = defender.hpCur - 1;
        result.sturdyActivated = true;
        result.activatedAbility = defenderAbility;
    }
    result.forceWildEnd = context.allowForceWildEnd &&
        (move->flags & MOVE_FLAG_FORCE_WILD_END) != 0;
    result.damage = damage;
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
                majorStatusImmune(status, targetSpecies, move.type) ||
                ((move.flags & MOVE_FLAG_POWDER) &&
                 hasType(targetSpecies, TypeId::GRASS)) ||
                (move.damageClass != DamageClass::STATUS &&
                 abilityForSpecies(targetSpecies) == AbilityId::SHIELD_DUST)) {
                if (move.damageClass == DamageClass::STATUS) {
                    outcome.kind = EffectOutcomeKind::STATUS_FAILED;
                    outcome.status = status;
                    result.add(outcome);
                }
                break;
            }
            targetMon.majorStatus = status;
            targetMon.majorStatusTurns = status == Game::MajorStatus::SLEEP
                ? static_cast<uint8_t>(randomTurns(1, 3) + 1)
                : 0;
            targetState.toxicCounter = 0;
            outcome.kind = EffectOutcomeKind::STATUS_APPLIED;
            outcome.status = status;
            result.add(outcome);
            if (effect->target == MoveEffectTarget::DEFENDER &&
                abilityForSpecies(targetSpecies) == AbilityId::SYNCHRONIZE &&
                attacker.majorStatus == Game::MajorStatus::NONE &&
                (status == Game::MajorStatus::POISON ||
                 status == Game::MajorStatus::TOXIC ||
                 status == Game::MajorStatus::PARALYSIS ||
                 status == Game::MajorStatus::BURN) &&
                !majorStatusImmune(status, attackerSpecies, move.type)) {
                attacker.majorStatus = status;
                attacker.majorStatusTurns = 0;
                attackerState.toxicCounter = 0;
                outcome = EffectOutcome{};
                outcome.kind = EffectOutcomeKind::STATUS_APPLIED;
                outcome.target = MoveEffectTarget::ATTACKER;
                outcome.status = status;
                outcome.ability = AbilityId::SYNCHRONIZE;
                result.add(outcome);
            }
            break;
        }
        case MoveEffectKind::CONFUSION:
            if (move.damageClass != DamageClass::STATUS &&
                abilityForSpecies(targetSpecies) == AbilityId::SHIELD_DUST) break;
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
            if (allowFlinch &&
                abilityForSpecies(targetSpecies) != AbilityId::INNER_FOCUS &&
                abilityForSpecies(targetSpecies) != AbilityId::SHIELD_DUST) {
                targetState.flinched = true;
                outcome.kind = EffectOutcomeKind::FLINCHED;
                result.add(outcome);
            }
            break;
        case MoveEffectKind::BIND:
            if (move.damageClass != DamageClass::STATUS &&
                abilityForSpecies(targetSpecies) == AbilityId::SHIELD_DUST) break;
            if (targetState.bindTurns == 0) {
                targetState.bindTurns = randomTurns(effect->minTurns, effect->maxTurns);
                outcome.kind = EffectOutcomeKind::BOUND;
                result.add(outcome);
            }
            break;
        case MoveEffectKind::STAT_STAGE: {
            if (move.damageClass != DamageClass::STATUS &&
                effect->target == MoveEffectTarget::DEFENDER &&
                abilityForSpecies(targetSpecies) == AbilityId::SHIELD_DUST) break;
            uint8_t statIndex = effect->aux;
            if (statIndex >= static_cast<uint8_t>(BattleStat::COUNT)) break;
            int8_t before = targetState.statStages[statIndex];
            int8_t delta = abilityAdjustedStage(
                effect->value, abilityForSpecies(targetSpecies));
            int8_t after = clampStage(static_cast<int8_t>(before + delta));
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
            amount = std::min<uint16_t>(
                amount, attacker.hpMax - attacker.hpCur);
            attacker.hpCur += amount;
            addHpOutcome(result, EffectOutcomeKind::DRAINED,
                         MoveEffectTarget::ATTACKER, amount);
            break;
        }
        case MoveEffectKind::RECOIL: {
            uint16_t amount = static_cast<uint32_t>(damageDealt) * effect->value / 100;
            if (amount == 0 && damageDealt > 0) amount = 1;
            amount = std::min<uint16_t>(amount, attacker.hpCur);
            attacker.hpCur -= amount;
            addHpOutcome(result, EffectOutcomeKind::RECOIL,
                         MoveEffectTarget::ATTACKER, amount);
            break;
        }
        case MoveEffectKind::HEAL: {
            uint8_t healPercent = effect->value;
            if ((move.flags & MOVE_FLAG_SWALLOW) != 0) {
                healPercent = targetState.stockpileCount >= 3 ? 100
                    : (targetState.stockpileCount == 2 ? 50 : 25);
            }
            uint16_t amount = std::max<uint16_t>(1,
                static_cast<uint32_t>(targetMon.hpMax) * healPercent / 100);
            amount = std::min<uint16_t>(
                amount, targetMon.hpMax - targetMon.hpCur);
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

EffectResolution applyPostDamageAbilities(
    const MoveInfo& move,
    Game::MonsterRuntime& attacker,
    const Species& attackerSpecies,
    BattleActorState& attackerState,
    Game::MonsterRuntime& defender,
    const Species& defenderSpecies,
    BattleActorState& defenderState,
    uint16_t damageDealt) {
    EffectResolution result;
    if (damageDealt == 0) return result;

    AbilityId defenderAbility = abilityForSpecies(defenderSpecies);
    if ((move.flags & MOVE_FLAG_CONTACT) != 0 &&
        attacker.majorStatus == Game::MajorStatus::NONE) {
        Game::MajorStatus status = Game::MajorStatus::NONE;
        if (defenderAbility == AbilityId::STATIC && rollChance(30)) {
            status = Game::MajorStatus::PARALYSIS;
        } else if (defenderAbility == AbilityId::EFFECT_SPORE && rollChance(30)) {
            uint8_t roll = static_cast<uint8_t>(GameRandom::range(0, 3));
            status = roll == 0 ? Game::MajorStatus::POISON
                : (roll == 1 ? Game::MajorStatus::PARALYSIS
                             : Game::MajorStatus::SLEEP);
        }
        if (status != Game::MajorStatus::NONE &&
            !majorStatusImmune(status, attackerSpecies, TypeId::NORMAL)) {
            attacker.majorStatus = status;
            attacker.majorStatusTurns = status == Game::MajorStatus::SLEEP
                ? static_cast<uint8_t>(randomTurns(1, 3) + 1) : 0;
            attackerState.toxicCounter = 0;
            EffectOutcome outcome;
            outcome.kind = EffectOutcomeKind::STATUS_APPLIED;
            outcome.target = MoveEffectTarget::ATTACKER;
            outcome.status = status;
            outcome.ability = defenderAbility;
            result.add(outcome);
        }
    }
    return result;
}

EffectResolution applyAbsorbAbility(const DamageResult& damage,
                                    Game::MonsterRuntime& defender,
                                    const Species& defenderSpecies,
                                    BattleActorState& defenderState) {
    EffectResolution result;
    if (!damage.absorbed || damage.activatedAbility == AbilityId::NONE) return result;
    EffectOutcome outcome;
    outcome.kind = EffectOutcomeKind::ABILITY_ACTIVATED;
    outcome.target = MoveEffectTarget::DEFENDER;
    outcome.ability = damage.activatedAbility;
    if (damage.activatedAbility == AbilityId::WATER_ABSORB ||
        damage.activatedAbility == AbilityId::VOLT_ABSORB) {
        uint16_t amount = damage.absorbedAmount;
        defender.hpCur += amount;
        outcome.amount = amount;
    } else if (damage.activatedAbility == AbilityId::FLASH_FIRE) {
        defenderState.flashFireBoost = true;
    }
    (void)defenderSpecies;
    result.add(outcome);
    return result;
}

bool recordMoveResult(BattleActorState& state,
                      Game::MonsterRuntime& monster,
                      const Species& species,
                      const MoveInfo& move,
                      bool hit,
                      uint8_t specialSlot) {
    uint8_t slot = moveSlotIndex(monster, species, move.id);
    if (slot < Game::MOVE_SLOT_COUNT) state.usedMoveMask |= static_cast<uint8_t>(1U << slot);

    bool sequenceMove = (move.flags & (MOVE_FLAG_ROLLOUT | MOVE_FLAG_FURY_CUTTER)) != 0;
    if (hit && sequenceMove) {
        if (state.consecutiveMoveId == move.id) {
            state.consecutiveHits = std::min<uint8_t>(state.consecutiveHits + 1, 5);
        } else {
            state.consecutiveMoveId = move.id;
            state.consecutiveHits = 1;
        }
    } else if (!sequenceMove || !hit) {
        state.consecutiveMoveId = 0;
        state.consecutiveHits = 0;
    }

    if (!hit) return false;
    if ((move.flags & MOVE_FLAG_RECHARGE) != 0) state.recharging = true;
    if ((move.flags & MOVE_FLAG_SELF_FAINT) != 0) monster.hpCur = 0;
    if ((move.flags & MOVE_FLAG_STOCKPILE) != 0 && state.stockpileCount < 3) {
        ++state.stockpileCount;
    }
    if ((move.flags & (MOVE_FLAG_SWALLOW | MOVE_FLAG_SPIT_UP)) != 0) {
        state.stockpileCount = 0;
    }
    if ((move.flags & MOVE_FLAG_RAMPAGE) != 0) {
        if (state.lockedMoveId == 0) {
            state.lockedMoveId = move.id;
            state.lockedSpecialSlot = specialSlot;
            state.lockedTurns = randomTurns(2, 3);
        }
        if (state.lockedTurns > 0) --state.lockedTurns;
        if (state.lockedTurns == 0) {
            state.lockedMoveId = 0;
            state.lockedSpecialSlot = SPECIAL_SLOT_NONE;
            state.confusionTurns = static_cast<uint8_t>(randomTurns(2, 5) + 1);
            return true;
        }
    }
    return false;
}

EffectResolution resolveEndTurn(Game::MonsterRuntime& monster,
                                const Species& species,
                                BattleActorState& battleState) {
    EffectResolution result;
    if (monster.hpCur == 0) return result;

    if (abilityForSpecies(species) == AbilityId::SHED_SKIN &&
        monster.majorStatus != Game::MajorStatus::NONE && rollChance(30)) {
        monster.majorStatus = Game::MajorStatus::NONE;
        monster.majorStatusTurns = 0;
        battleState.toxicCounter = 0;
        EffectOutcome outcome;
        outcome.kind = EffectOutcomeKind::CURED;
        outcome.target = MoveEffectTarget::ATTACKER;
        outcome.ability = AbilityId::SHED_SKIN;
        result.add(outcome);
    }

    uint16_t statusDamage = 0;
    if (monster.majorStatus == Game::MajorStatus::POISON ||
        monster.majorStatus == Game::MajorStatus::BURN) {
        statusDamage = std::max<uint16_t>(1, monster.hpMax / 8);
    } else if (monster.majorStatus == Game::MajorStatus::TOXIC) {
        if (battleState.toxicCounter < 15) battleState.toxicCounter++;
        statusDamage = std::max<uint16_t>(1,
            static_cast<uint32_t>(monster.hpMax) * battleState.toxicCounter / 16);
    }
    if (statusDamage > 0) {
        statusDamage = std::min<uint16_t>(statusDamage, monster.hpCur);
        monster.hpCur -= statusDamage;
        EffectOutcome outcome;
        outcome.kind = EffectOutcomeKind::STATUS_DAMAGE;
        outcome.status = monster.majorStatus;
        outcome.amount = statusDamage;
        result.add(outcome);
    }

    if (monster.hpCur > 0 && battleState.bindTurns > 0) {
        uint16_t damage = std::min<uint16_t>(
            std::max<uint16_t>(1, monster.hpMax / 8), monster.hpCur);
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
