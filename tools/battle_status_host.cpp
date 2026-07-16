#include <cstdint>
#include <deque>

#include "game/BattleSystem.h"
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
    const Species& bulbasaur = *findSpecies(1);
    const Species& charmander = *findSpecies(4);
    const Species& squirtle = *findSpecies(7);

    Game::MonsterRuntime attacker = monster(4);
    Game::MonsterRuntime defender = monster(7);
    BattleSystem::BattleActorState attackerState;
    BattleSystem::BattleActorState defenderState;

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
    if (attacker.hpCur != 150 || attackerState.bindTurns != 0 ||
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

    return 0;
}
