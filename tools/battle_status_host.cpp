#include <cstdint>
#include <deque>

#include "game/BattleSystem.h"
#include "game/GameRandom.h"
#include "game/Species.h"

namespace {
std::deque<long> randomValues;

void setRandom(std::initializer_list<long> values) {
    randomValues.assign(values.begin(), values.end());
}

long nextRandom(long minimum, long maximum) {
    if (maximum <= minimum) return minimum;
    long value = randomValues.empty() ? minimum : randomValues.front();
    if (!randomValues.empty()) randomValues.pop_front();
    long range = maximum - minimum;
    value %= range;
    if (value < 0) value += range;
    return minimum + value;
}

uint32_t testRandomRange(uint32_t minimum, uint32_t maximum) {
    return static_cast<uint32_t>(nextRandom(minimum, maximum));
}

Game::MonsterRuntime monster(uint16_t speciesId, uint8_t level = 20) {
    const Species* species = findSpecies(speciesId);
    Game::MonsterRuntime value;
    value.speciesId = speciesId;
    value.level = level;
    value.hpMax = maxHpFor(*species, value);
    value.hpCur = value.hpMax;
    resetMovesForLevel(value, *species);
    return value;
}

bool hasOutcome(const BattleSystem::EffectResolution& result,
                BattleSystem::EffectOutcomeKind kind) {
    for (uint8_t index = 0; index < result.count; ++index) {
        if (result.outcomes[index].kind == kind) return true;
    }
    return false;
}
} // namespace

long random(long maximum) {
    return nextRandom(0, maximum);
}

long random(long minimum, long maximum) {
    return nextRandom(minimum, maximum);
}

int main() {
    GameRandom::setRangeProvider(testRandomRange);
    const Species& bulbasaur = *findSpecies(1);
    const Species& charmander = *findSpecies(4);
    const Species& squirtle = *findSpecies(7);

    Game::MonsterRuntime attacker = monster(4);
    Game::MonsterRuntime defender = monster(7);
    BattleSystem::BattleActorState attackerState;
    BattleSystem::BattleActorState defenderState;

    const MoveInfo& solarBeam = *findMove(76);
    if (!BattleSystem::moveRequiresCharge(solarBeam.id) ||
        (solarBeam.flags & MOVE_FLAG_TWO_TURN_CHARGE) == 0) {
        return 50;
    }
    BattleSystem::beginChargingMove(attackerState, solarBeam.id, 0);
    if (!BattleSystem::isChargingMove(attackerState) ||
        attackerState.chargingMoveId != solarBeam.id ||
        attackerState.chargingSpecialSlot != 0) {
        return 51;
    }
    BattleSystem::clearChargingMove(attackerState);
    if (BattleSystem::isChargingMove(attackerState) ||
        attackerState.chargingSpecialSlot != BattleSystem::SPECIAL_SLOT_NONE) {
        return 52;
    }

    const MoveInfo& fireFang = *findMove(424);
    setRandom({0, 0});
    auto fireEffects = BattleSystem::applyMoveEffects(
        fireFang, attacker, charmander, attackerState,
        defender, squirtle, defenderState, 20, true);
    if (defender.majorStatus != Game::MajorStatus::BURN || !defenderState.flinched ||
        !hasOutcome(fireEffects, BattleSystem::EffectOutcomeKind::STATUS_APPLIED) ||
        !hasOutcome(fireEffects, BattleSystem::EffectOutcomeKind::FLINCHED)) {
        return 1;
    }

    defender = monster(4);
    defenderState = {};
    const MoveInfo& ember = *findMove(52);
    setRandom({0});
    BattleSystem::applyMoveEffects(
        ember, attacker, charmander, attackerState,
        defender, charmander, defenderState, 10, true);
    if (defender.majorStatus != Game::MajorStatus::NONE) return 2;

    attackerState = {};
    defenderState = {};
    attacker = monster(11);
    defender = monster(7);
    const MoveInfo& harden = *findMove(106);
    setRandom({0});
    auto hardenResult = BattleSystem::applyMoveEffects(
        harden, attacker, *findSpecies(11), attackerState,
        defender, squirtle, defenderState, 0, false);
    if (attackerState.statStages[static_cast<uint8_t>(BattleStat::DEFENSE)] != 1 ||
        !hasOutcome(hardenResult, BattleSystem::EffectOutcomeKind::STAT_CHANGED)) {
        return 3;
    }

    attacker = monster(1);
    attacker.majorStatus = Game::MajorStatus::PARALYSIS;
    attackerState = {};
    setRandom({0});
    auto paralysis = BattleSystem::checkAction(attacker, bulbasaur, attackerState, 33);
    if (paralysis.blockReason != BattleSystem::ActionBlockReason::PARALYSIS) return 4;

    attacker.majorStatus = Game::MajorStatus::FREEZE;
    setRandom({0});
    auto thaw = BattleSystem::checkAction(attacker, bulbasaur, attackerState, 33);
    if (!thaw.canAct() || !thaw.thawed || attacker.majorStatus != Game::MajorStatus::NONE) {
        return 5;
    }

    attacker.majorStatus = Game::MajorStatus::SLEEP;
    attacker.majorStatusTurns = 2;
    auto sleep = BattleSystem::checkAction(attacker, bulbasaur, attackerState, 33);
    if (sleep.blockReason != BattleSystem::ActionBlockReason::SLEEP) return 6;
    auto wake = BattleSystem::checkAction(attacker, bulbasaur, attackerState, 33);
    if (!wake.canAct() || !wake.wokeUp) return 7;

    attackerState = {};
    attackerState.confusionTurns = 2;
    setRandom({0, 0});
    auto confusion = BattleSystem::checkAction(attacker, bulbasaur, attackerState, 33);
    if (confusion.blockReason != BattleSystem::ActionBlockReason::CONFUSION_SELF_HIT ||
        confusion.selfDamage == 0) {
        return 8;
    }

    attacker = monster(1);
    attacker.hpMax = 160;
    attacker.hpCur = 160;
    attacker.majorStatus = Game::MajorStatus::BURN;
    attackerState = {};
    auto burn = BattleSystem::resolveEndTurn(attacker, bulbasaur, attackerState);
    if (attacker.hpCur != 140 || !hasOutcome(burn, BattleSystem::EffectOutcomeKind::STATUS_DAMAGE)) {
        return 9;
    }

    attacker.hpCur = 160;
    attacker.majorStatus = Game::MajorStatus::TOXIC;
    attackerState = {};
    BattleSystem::resolveEndTurn(attacker, bulbasaur, attackerState);
    if (attacker.hpCur != 150 || attackerState.toxicCounter != 1) return 10;
    BattleSystem::resolveEndTurn(attacker, bulbasaur, attackerState);
    if (attacker.hpCur != 130 || attackerState.toxicCounter != 2) return 11;

    attacker.hpCur = 160;
    attacker.majorStatus = Game::MajorStatus::NONE;
    attackerState = {};
    attackerState.bindTurns = 1;
    auto bind = BattleSystem::resolveEndTurn(attacker, bulbasaur, attackerState);
    if (attacker.hpCur != 140 || attackerState.bindTurns != 0 ||
        !hasOutcome(bind, BattleSystem::EffectOutcomeKind::BIND_ENDED)) {
        return 12;
    }
    attackerState.bindTurns = 2;
    if (BattleSystem::canFlee(attackerState)) return 13;

    attacker = monster(4);
    defender = monster(1);
    attackerState = {};
    defenderState = {};
    const MoveInfo& inferno = *findMove(517);
    attacker.move2Id = inferno.id;
    setRandom({99});
    auto miss = BattleSystem::calcBasicDamage(
        attacker, charmander, defender, bulbasaur, 0,
        attackerState, defenderState);
    if (!miss.missed || miss.damage != 0) return 14;

    attacker = monster(1);
    attacker.affection = 0;
    attacker.hpCur = attacker.hpMax;
    attacker.moveProficiency[1] = 0;
    attacker.moveProficiency[2] = 0;
    if (BattleSystem::specialTriggerChance(attacker, 0) != 15 ||
        BattleSystem::specialTriggerChance(attacker, 1) != 15) {
        return 15;
    }
    attacker.moveProficiency[1] = 25;
    attacker.moveProficiency[2] = 60;
    if (BattleSystem::specialTriggerChance(attacker, 0) != 19 ||
        BattleSystem::specialTriggerChance(attacker, 1) != 31) {
        return 16;
    }
    attacker.moveProficiency[1] = Game::MOVE_PROFICIENCY_MAX;
    attacker.moveProficiency[2] = Game::MOVE_PROFICIENCY_MAX;
    if (BattleSystem::specialTriggerChance(attacker, 0) != 40 ||
        BattleSystem::specialTriggerChance(attacker, 1) != 40) {
        return 17;
    }
    setRandom({0});
    if (BattleSystem::rollSpecialMoveSlot(attacker) != 0) return 18;
    setRandom({39});
    if (BattleSystem::rollSpecialMoveSlot(attacker) != 0) return 19;
    setRandom({40});
    if (BattleSystem::rollSpecialMoveSlot(attacker) != 1) return 20;
    setRandom({79});
    if (BattleSystem::rollSpecialMoveSlot(attacker) != 1) return 21;
    setRandom({80});
    if (BattleSystem::rollSpecialMoveSlot(attacker) != BattleSystem::SPECIAL_SLOT_NONE) return 22;

    uint8_t previousChance = 0;
    attacker.affection = 0;
    attacker.hpCur = attacker.hpMax;
    for (uint8_t proficiency = 0; proficiency <= Game::MOVE_PROFICIENCY_MAX;
         ++proficiency) {
        attacker.moveProficiency[1] = proficiency;
        uint8_t chance = BattleSystem::specialTriggerChance(attacker, 0);
        if (chance < 15 || chance > 40 || chance < previousChance) return 23;
        previousChance = chance;
    }
    attacker.moveProficiency[1] = 0;
    attacker.affection = 255;
    if (BattleSystem::specialTriggerChance(attacker, 0) != 25) return 24;
    attacker.hpCur = 1;
    if (BattleSystem::specialTriggerChance(attacker, 0) != 30) return 25;

    BattleSystem::BattleAiMemory aiMemory;
    attacker = monster(1);
    attacker.move1Id = 33;   // 撞击
    attacker.move2Id = 77;   // 毒粉
    attacker.move3Id = 45;   // 叫声
    attacker.moveProficiency[1] = Game::MOVE_PROFICIENCY_MAX;
    attacker.moveProficiency[2] = Game::MOVE_PROFICIENCY_MAX;
    defender = monster(7);
    attackerState = {};
    defenderState = {};

    defender.majorStatus = Game::MajorStatus::POISON;
    auto poisonedWeights = BattleSystem::aiMoveWeights(
        attacker, bulbasaur, attackerState,
        defender, squirtle, defenderState, aiMemory);
    if (poisonedWeights.special[0] != 0 || poisonedWeights.basic == 0) return 26;

    defender.majorStatus = Game::MajorStatus::NONE;
    aiMemory.lastMoveId = 77;
    aiMemory.consecutiveStatusMoves = 1;
    auto repeatWeights = BattleSystem::aiMoveWeights(
        attacker, bulbasaur, attackerState,
        defender, squirtle, defenderState, aiMemory);
    if (repeatWeights.special[0] != 0 || repeatWeights.special[1] == 0) return 27;

    aiMemory.lastMoveId = 45;
    aiMemory.consecutiveStatusMoves = 2;
    auto statusChainWeights = BattleSystem::aiMoveWeights(
        attacker, bulbasaur, attackerState,
        defender, squirtle, defenderState, aiMemory);
    if (statusChainWeights.special[0] != 0 ||
        statusChainWeights.special[1] != 0 ||
        statusChainWeights.basic == 0) {
        return 28;
    }

    attacker.move2Id = 106;  // 变硬
    attacker.move3Id = 0;
    attackerState = {};
    attackerState.statStages[static_cast<uint8_t>(BattleStat::DEFENSE)] = 6;
    aiMemory = {};
    auto cappedBuffWeights = BattleSystem::aiMoveWeights(
        attacker, bulbasaur, attackerState,
        defender, squirtle, defenderState, aiMemory);
    if (cappedBuffWeights.special[0] != 0) return 29;

    attacker = monster(4);
    attacker.move1Id = 10;   // 抓
    attacker.move2Id = 52;   // 火花
    attacker.move3Id = 0;
    attacker.moveProficiency[1] = Game::MOVE_PROFICIENCY_MAX;
    const Species& gastly = *findSpecies(92);
    defender = monster(92);
    attackerState = {};
    defenderState = {};
    aiMemory = {};
    auto ghostWeights = BattleSystem::aiMoveWeights(
        attacker, charmander, attackerState,
        defender, gastly, defenderState, aiMemory);
    if (ghostWeights.basic != 0 || ghostWeights.special[0] == 0) return 30;
    setRandom({0});
    if (BattleSystem::chooseAiMoveSlot(
            attacker, charmander, attackerState,
            defender, gastly, defenderState, aiMemory) != 0 ||
        aiMemory.lastMoveId != 52 || aiMemory.consecutiveStatusMoves != 0) {
        return 31;
    }

    // Conditional damage and special move rules must not degrade to plain power.
    attacker = monster(92, 50);
    defender = monster(1, 50);
    attacker.move2Id = 506;  // Hex
    attackerState = {};
    defenderState = {};
    setRandom({15, 100});
    auto hexNormal = BattleSystem::calcBasicDamage(
        attacker, gastly, defender, bulbasaur, 0, attackerState, defenderState);
    defender.majorStatus = Game::MajorStatus::POISON;
    setRandom({15, 100});
    auto hexStatus = BattleSystem::calcBasicDamage(
        attacker, gastly, defender, bulbasaur, 0, attackerState, defenderState);
    if (hexStatus.damage < hexNormal.damage * 19 / 10) return 60;

    attacker = monster(123, 30);
    defender = monster(7, 30);
    attacker.move2Id = 206;  // False Swipe
    defender.hpCur = 5;
    setRandom({15, 100});
    auto falseSwipe = BattleSystem::calcBasicDamage(
        attacker, *findSpecies(123), defender, squirtle, 0,
        attackerState, defenderState);
    if (falseSwipe.damage != 4) return 61;

    attacker = monster(7, 30);
    defender = monster(92, 30);
    attacker.move2Id = 89;  // Earthquake, injected for battle-rule test
    setRandom({15, 100});
    auto levitate = BattleSystem::calcBasicDamage(
        attacker, squirtle, defender, gastly, 0, attackerState, defenderState);
    if (levitate.effectiveness != 0 || levitate.damage != 0 ||
        levitate.activatedAbility != AbilityId::LEVITATE) return 62;

    attacker = monster(7, 30);
    defender = monster(134, 30);
    attacker.move2Id = 55;  // Water Gun
    defender.hpCur = defender.hpMax / 2;
    setRandom({15, 100});
    auto absorbed = BattleSystem::calcBasicDamage(
        attacker, squirtle, defender, *findSpecies(134), 0,
        attackerState, defenderState);
    auto absorbEffect = BattleSystem::applyAbsorbAbility(
        absorbed, defender, *findSpecies(134), defenderState);
    if (!absorbed.absorbed || absorbed.damage != 0 || absorbEffect.count != 1 ||
        defender.hpCur <= defender.hpMax / 2) return 63;

    attacker = monster(4, 50);
    defender = monster(143, 50);
    attacker.move2Id = 63;  // Hyper Beam
    const MoveInfo& hyperBeam = *findMove(63);
    BattleSystem::recordMoveResult(
        attackerState, attacker, charmander, hyperBeam, true, 0);
    auto recharge = BattleSystem::checkAction(
        attacker, charmander, attackerState, 10);
    if (recharge.blockReason != BattleSystem::ActionBlockReason::RECHARGE ||
        attackerState.recharging) return 64;

    defender = monster(74, 30);
    defender.hpCur = defender.hpMax;
    attacker = monster(7, 80);
    attacker.move2Id = 55;
    setRandom({15, 100});
    auto sturdy = BattleSystem::calcBasicDamage(
        attacker, squirtle, defender, *findSpecies(74), 0,
        attackerState, defenderState);
    if (!sturdy.sturdyActivated || sturdy.damage != defender.hpCur - 1) return 65;

    if (findMove(168) || findMove(343) || findMove(450) || findMove(512)) return 66;

    attacker = monster(7, 30);
    defender = monster(134, 30);
    attacker.move2Id = 56;  // Hydro Pump can miss before Water Absorb triggers
    setRandom({99});
    auto missedAbsorb = BattleSystem::calcBasicDamage(
        attacker, squirtle, defender, *findSpecies(134), 0,
        attackerState, defenderState);
    if (!missedAbsorb.missed || missedAbsorb.absorbed) return 67;

    attacker = monster(123, 80);
    defender = monster(74, 30);
    attacker.move2Id = 42;  // Pin Missile
    setRandom({15, 7, 100, 100, 100, 100, 100});
    auto multiHitSturdy = BattleSystem::calcBasicDamage(
        attacker, *findSpecies(123), defender, *findSpecies(74), 0,
        attackerState, defenderState);
    if (multiHitSturdy.sturdyActivated) return 68;

    return 0;
}
