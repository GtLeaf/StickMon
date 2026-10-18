#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

#include "game/BattleActionService.h"
#include "game/GameRandom.h"
#include "game/Species.h"

namespace {
struct Draw {
    uint32_t minimum, maximum, value;
    bool operator==(const Draw& other) const {
        return minimum == other.minimum && maximum == other.maximum && value == other.value;
    }
};
std::vector<Draw> draws;
uint32_t randomState = 1;
bool forceMinimum = false, forceMaximum = false;

uint32_t randomRange(uint32_t minimum, uint32_t maximum) {
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    const uint32_t value = forceMinimum ? minimum : forceMaximum ? maximum - 1
        : minimum + randomState % (maximum - minimum);
    draws.push_back({minimum, maximum, value});
    return value;
}

Game::MonsterRuntime monster(uint16_t speciesId, uint8_t level = 35) {
    Game::MonsterRuntime result;
    result.speciesId = speciesId;
    result.level = level;
    result.hpMax = maxHpFor(*findSpecies(speciesId), result);
    result.hpCur = result.hpMax;
    resetMovesForLevel(result, *findSpecies(speciesId));
    return result;
}

// Explicit logical snapshots avoid padding and cover persistent and volatile state.
std::vector<int64_t> snapshot(const Game::MonsterRuntime& m,
                              const BattleSystem::BattleActorState& s) {
    std::vector<int64_t> values = {
        m.speciesId, m.level, m.exp, m.hpCur, m.hpMax,
        m.move1Id, m.move2Id, m.move3Id, m.ivPacked,
        m.ev.hp, m.ev.atk, m.ev.def, m.ev.spa, m.ev.spd, m.ev.spe,
        m.nature, m.affection, m.mood, m.satiety,
        m.moveProficiency[0], m.moveProficiency[1], m.moveProficiency[2],
        static_cast<uint8_t>(m.majorStatus), m.majorStatusTurns,
        m.metArea, m.petCountToday, static_cast<uint8_t>(m.origin),
        m.statusFlags, m.bond, m.metAt, m.lastSeenAt, m.lastPettedAt,
        m.lastExploredAt, m.lastWindowGazeAt,
        s.confusionTurns, s.bindTurns, s.toxicCounter, s.yawnTurns,
        s.chargingMoveId, s.chargingSpecialSlot,
        s.lockedMoveId, s.lockedSpecialSlot, s.lockedTurns,
        s.consecutiveMoveId, s.consecutiveHits, s.usedMoveMask, s.stockpileCount,
        s.flinched, s.recharging, s.flashFireBoost,
    };
    for (int8_t stage : s.statStages) values.push_back(stage);
    return values;
}

bool has(const BattleSystem::EffectResolution& resolution,
         BattleSystem::EffectOutcomeKind kind) {
    for (uint8_t i = 0; i < resolution.count; ++i) {
        if (resolution.outcomes[i].kind == kind) return true;
    }
    return false;
}

struct Fixture {
    Game::MonsterRuntime attacker = monster(4), defender = monster(7);
    BattleSystem::BattleActorState attackerState, defenderState;
    BattleSystem::DamageContext context;
    Game::MoveId move = 52;
    bool allowFlinch = true, awardProficiency = true;
};

Fixture fixture(const std::string& name) {
    Fixture f;
    if (name == "sleep" || name == "wake") {
        f.attacker.majorStatus = Game::MajorStatus::SLEEP;
        f.attacker.majorStatusTurns = name == "sleep" ? 2 : 1;
    } else if (name == "paralysis") {
        f.attacker.majorStatus = Game::MajorStatus::PARALYSIS;
    } else if (name == "freeze" || name == "thaw") {
        f.attacker.majorStatus = Game::MajorStatus::FREEZE;
        f.move = 33;
    } else if (name == "confusion" || name == "confusion-ko") {
        f.attackerState.confusionTurns = 3;
        if (name == "confusion-ko") f.attacker.hpCur = 1;
    } else if (name == "confusion-end") {
        f.attackerState.confusionTurns = 1;
    } else if (name == "recharge") {
        f.attackerState.recharging = true;
    } else if (name == "flinch") {
        f.attackerState.flinched = true;
    } else if (name == "charge" || name == "release" || name == "charge-blocked") {
        f.move = 130;
        if (name != "charge") BattleSystem::beginChargingMove(f.attackerState, 130, 0);
        if (name == "charge-blocked") f.attackerState.flinched = true;
    } else if (name == "miss") {
        f.move = 517;
    } else if (name == "immune") {
        f.move = 10;
        f.defender = monster(92);
    } else if (name == "levitate") {
        f.move = 89;
        f.defender = monster(92);
    } else if (name == "absorb") {
        f.move = 55;
        f.defender = monster(134);
        f.defender.hpCur = 1;
    } else if (name == "flash-fire") {
        f.move = 52;
        f.defender = monster(136);
    } else if (name == "contact") {
        f.move = 10;
        f.defender = monster(25);
    } else if (name == "effect-order") {
        f.move = 424;
        f.defender = monster(25);
        f.defender.hpCur = f.defender.hpMax = 500;
    } else if (name == "rampage") {
        f.move = 37;
        f.attackerState.lockedMoveId = 37;
        f.attackerState.lockedSpecialSlot = 0;
        f.attackerState.lockedTurns = 1;
    } else if (name == "swallow") {
        f.move = 256;
        f.attacker.hpCur = 1;
        f.attackerState.stockpileCount = 3;
    } else if (name == "spit-up") {
        f.move = 255;
        f.attackerState.stockpileCount = 3;
    } else if (name == "recoil") {
        f.move = 36;
    } else if (name == "drain") {
        f.move = 71;
        f.attacker.hpCur = 1;
    } else if (name == "self-faint") {
        f.move = 120;
    } else if (name == "forced-end" || name == "wild-forced-end") {
        f.move = 525;
        f.context.allowForceWildEnd = name == "forced-end";
    } else if (name == "sturdy") {
        f.attacker = monster(7, 80);
        f.defender = monster(74, 5);
        f.move = 55;
    } else if (name == "defender-ko") {
        f.defender.hpCur = 1;
        f.move = 424;
    } else if (name == "flinch-after-action") {
        f.move = 424;
        f.allowFlinch = false;
    } else if (name == "wild-proficiency") {
        f.awardProficiency = false;
    } else if (name == "attacker-fainted") {
        f.attacker.hpCur = 0;
    } else if (name == "defender-fainted") {
        f.defender.hpCur = 0;
    }
    f.attacker.move2Id = f.move;
    return f;
}

struct Run {
    Fixture f;
    BattleActionService::PendingAction pending;
    std::vector<Draw> trace;
};

Run execute(const std::string& name, bool delayed, uint32_t seed) {
    Run run{fixture(name)};
    auto& f = run.f;
    const Species& a = *findSpecies(f.attacker.speciesId);
    const Species& d = *findSpecies(f.defender.speciesId);
    randomState = seed;
    forceMinimum = name != "miss" && name != "freeze";
    forceMaximum = name == "miss" || name == "freeze";
    // The random case checks arbitrary complete draw traces, not only edge rolls.
    if (name == "random") forceMinimum = forceMaximum = false;
    draws.clear();
    const auto attackerHp = f.attacker.hpCur, defenderHp = f.defender.hpCur;
    BattleActionService::prepare(run.pending, f.attacker, a, f.attackerState,
        f.defender, d, f.defenderState, 0, f.move, f.context);
    assert(f.attacker.hpCur == attackerHp && f.defender.hpCur == defenderHp);
    const auto preparedAttacker = snapshot(f.attacker, f.attackerState);
    const auto preparedDefender = snapshot(f.defender, f.defenderState);
    const auto preparedDraws = draws;
    if (delayed) {
        // Render/poll cycles may revisit preparation; they cannot repeat checks.
        for (int i = 0; i < 7; ++i) {
            BattleActionService::prepare(run.pending, f.attacker, a, f.attackerState,
                f.defender, d, f.defenderState, 0, f.move, f.context);
            assert(snapshot(f.attacker, f.attackerState) == preparedAttacker);
            assert(snapshot(f.defender, f.defenderState) == preparedDefender);
            assert(draws == preparedDraws);
        }
    }
    assert(BattleActionService::commit(run.pending, f.attacker, a, f.attackerState,
        f.defender, d, f.defenderState, f.allowFlinch, f.awardProficiency));
    const auto committedAttacker = snapshot(f.attacker, f.attackerState);
    const auto committedDefender = snapshot(f.defender, f.defenderState);
    const auto committedDraws = draws;
    for (int i = 0; i < 4; ++i) {
        assert(!BattleActionService::commit(run.pending, f.attacker, a, f.attackerState,
            f.defender, d, f.defenderState, f.allowFlinch, f.awardProficiency));
        BattleActionService::prepare(run.pending, f.attacker, a, f.attackerState,
            f.defender, d, f.defenderState, 0, f.move, f.context);
        assert(snapshot(f.attacker, f.attackerState) == committedAttacker);
        assert(snapshot(f.defender, f.defenderState) == committedDefender);
        assert(draws == committedDraws);
    }
    run.trace = draws;
    return run;
}

void check(const std::string& name) {
    for (uint32_t seed : {1U, 17U, 48271U, 0xFCA09875U}) {
        const Run direct = execute(name, false, seed);
        const Run delayed = execute(name, true, seed);
        assert(snapshot(direct.f.attacker, direct.f.attackerState) ==
               snapshot(delayed.f.attacker, delayed.f.attackerState));
        assert(snapshot(direct.f.defender, direct.f.defenderState) ==
               snapshot(delayed.f.defender, delayed.f.defenderState));
        assert(direct.trace == delayed.trace);
        const auto& f = direct.f;
        const auto& p = direct.pending;
        using Kind = BattleActionService::Kind;
        using Reason = BattleSystem::ActionBlockReason;
        using Effect = BattleSystem::EffectOutcomeKind;
        if (name == "sleep") assert(p.kind == Kind::BLOCKED && p.check.blockReason == Reason::SLEEP && f.attacker.majorStatusTurns == 1 && direct.trace.empty());
        if (name == "wake") assert(p.check.wokeUp && f.attacker.majorStatus == Game::MajorStatus::NONE);
        if (name == "paralysis") assert(p.kind == Kind::BLOCKED && p.check.blockReason == Reason::PARALYSIS);
        if (name == "freeze") assert(p.check.blockReason == Reason::FREEZE && f.attacker.majorStatus == Game::MajorStatus::FREEZE);
        if (name == "thaw") assert(p.check.thawed && f.attacker.majorStatus == Game::MajorStatus::NONE);
        if (name == "confusion" || name == "confusion-ko") {
            assert(p.check.blockReason == Reason::CONFUSION_SELF_HIT && p.result.selfDamage > 0);
            assert(f.defender.hpCur == f.defender.hpMax && f.attackerState.confusionTurns == 2);
            assert(f.attacker.moveProficiency[1] == 0);
            if (name == "confusion-ko") assert(f.attacker.hpCur == 0);
        }
        if (name == "confusion-end") assert(p.check.confusionEnded && f.attackerState.confusionTurns == 0);
        if (name == "recharge") assert(p.check.blockReason == Reason::RECHARGE && !f.attackerState.recharging);
        if (name == "flinch" || name == "charge-blocked") assert(p.check.blockReason == Reason::FLINCH && !BattleSystem::isChargingMove(f.attackerState));
        if (name == "charge") assert(p.kind == Kind::CHARGING && p.defenseRaised && f.attackerState.statStages[1] == 1 && f.attackerState.chargingMoveId == 130 && f.attacker.moveProficiency[1] == 0 && direct.trace.empty());
        if (name == "release") assert(p.kind == Kind::READY && p.releasingCharge && !BattleSystem::isChargingMove(f.attackerState) && p.result.actualDamage > 0);
        if (name == "miss") assert(p.damage.missed && p.result.actualDamage == 0 && f.defender.hpCur == f.defender.hpMax);
        if (name == "immune" || name == "levitate") assert(p.damage.effectiveness == 0 && p.result.actualDamage == 0 && f.defender.hpCur == f.defender.hpMax && p.result.moveEffects.count == 0);
        if (name == "absorb") assert(p.damage.absorbed && f.defender.hpCur == 1 + f.defender.hpMax / 4 && p.result.absorbEffects.count == 1);
        if (name == "flash-fire") assert(p.damage.absorbed && f.defenderState.flashFireBoost && p.result.absorbEffects.count == 1);
        if (name == "contact") assert(f.attacker.majorStatus == Game::MajorStatus::PARALYSIS && has(p.result.abilityEffects, Effect::STATUS_APPLIED));
        if (name == "effect-order") {
            assert(f.defender.majorStatus == Game::MajorStatus::BURN && f.defenderState.flinched);
            assert(f.attacker.majorStatus == Game::MajorStatus::PARALYSIS);
            assert(p.result.moveEffects.count == 2 && p.result.abilityEffects.count == 1);
            const std::vector<Draw> expected = {
                {0, 100, 0}, {1, 2, 1}, {0, 16, 0}, {85, 101, 85},
                {0, 100, 0}, {0, 100, 0}, {0, 100, 0},
            };
            assert(direct.trace == expected);
        }
        if (name == "rampage") assert(p.result.rampageEnded && f.attackerState.lockedMoveId == 0 && f.attackerState.confusionTurns > 0);
        if (name == "swallow") assert(f.attacker.hpCur == f.attacker.hpMax && f.attackerState.stockpileCount == 0 && has(p.result.moveEffects, Effect::HEALED));
        if (name == "spit-up") assert(p.result.actualDamage > 0 && f.attackerState.stockpileCount == 0);
        if (name == "recoil") assert(f.attacker.hpCur < f.attacker.hpMax && has(p.result.moveEffects, Effect::RECOIL));
        if (name == "drain") assert(f.attacker.hpCur > 1 && has(p.result.moveEffects, Effect::DRAINED));
        if (name == "self-faint") assert(f.attacker.hpCur == 0 && p.result.actualDamage > 0);
        if (name == "forced-end") assert(p.result.forceWildEnd);
        if (name == "wild-forced-end") assert(!p.result.forceWildEnd);
        if (name == "sturdy") assert(p.damage.sturdyActivated && f.defender.hpCur == 1);
        if (name == "defender-ko") assert(f.defender.hpCur == 0 && f.defender.majorStatus == Game::MajorStatus::NONE && !f.defenderState.flinched);
        if (name == "flinch-after-action") assert(!f.defenderState.flinched);
        if (name == "wild-proficiency") assert(f.attacker.moveProficiency[1] == 0);
        if (name == "random") assert(p.result.actualDamage > 0 && f.attacker.moveProficiency[1] == 1);
        if (name == "attacker-fainted" || name == "defender-fainted") {
            assert(p.kind == Kind::BLOCKED && direct.trace.empty());
            assert(p.result.actualDamage == 0 && p.result.selfDamage == 0);
            assert(f.attacker.moveProficiency[1] == 0);
        }
    }
    std::printf("battle action %s: direct/delayed state and random trace agree; repeats have no effects\n", name.c_str());
}

void checkResetAndRelease() {
    Fixture f = fixture("charge");
    const Species& a = *findSpecies(f.attacker.speciesId);
    const Species& d = *findSpecies(f.defender.speciesId);
    BattleActionService::PendingAction p;
    assert(!BattleActionService::commit(p, f.attacker, a, f.attackerState,
        f.defender, d, f.defenderState, true, true));
    BattleActionService::prepare(p, f.attacker, a, f.attackerState,
        f.defender, d, f.defenderState, 0, f.move);
    assert(p.kind == BattleActionService::Kind::CHARGING);
    assert(BattleActionService::commit(p, f.attacker, a, f.attackerState,
        f.defender, d, f.defenderState, true, true));
    const auto defenderHp = f.defender.hpCur;
    p.reset();
    BattleActionService::prepare(p, f.attacker, a, f.attackerState,
        f.defender, d, f.defenderState, 0, f.move);
    assert(p.releasingCharge && p.kind == BattleActionService::Kind::READY);
    assert(BattleActionService::commit(p, f.attacker, a, f.attackerState,
        f.defender, d, f.defenderState, true, true));
    assert(f.defender.hpCur < defenderHp && f.attacker.moveProficiency[1] == 1);
}
} // namespace

int main(int argc, char** argv) {
    GameRandom::setRangeProvider(randomRange);
    assert(argc == 2);
    if (std::string(argv[1]) == "reset-release") checkResetAndRelease();
    else check(argv[1]);
    return 0;
}
