#pragma once

#include <cstdint>
#include "game/GameState.h"
#include "game/Species.h"

namespace BattleSystem {

static constexpr uint8_t SPECIAL_SLOT_NONE = 0xFF;
static constexpr uint8_t EFFECT_OUTCOME_CAP = 12;
static constexpr uint8_t RESERVE_EXP_PERCENT = 50;

struct ExperienceAwards {
    uint16_t active = 0;
    uint16_t reserve = 0;
};

struct BattleActorState {
    int8_t statStages[static_cast<uint8_t>(BattleStat::COUNT)] = {};
    uint8_t confusionTurns = 0;
    uint8_t bindTurns = 0;
    uint8_t toxicCounter = 0;
    uint8_t yawnTurns = 0;
    bool flinched = false;
};

struct BattleAiMemory {
    Game::MoveId lastMoveId = 0;
    uint8_t consecutiveStatusMoves = 0;
};

struct BattleMoveWeights {
    uint16_t basic = 0;
    uint16_t special[SPECIAL_MOVE_SLOT_COUNT] = {};
};

enum class ActionBlockReason : uint8_t {
    NONE = 0,
    FLINCH,
    SLEEP,
    FREEZE,
    PARALYSIS,
    CONFUSION_SELF_HIT,
};

struct ActionCheckResult {
    ActionBlockReason blockReason = ActionBlockReason::NONE;
    uint16_t selfDamage = 0;
    bool wokeUp = false;
    bool thawed = false;
    bool confusionEnded = false;

    bool canAct() const { return blockReason == ActionBlockReason::NONE; }
};

enum class EffectOutcomeKind : uint8_t {
    STATUS_APPLIED = 0,
    STATUS_FAILED,
    CONFUSED,
    FLINCHED,
    BOUND,
    STAT_CHANGED,
    DRAINED,
    RECOIL,
    HEALED,
    CURED,
    BIND_CLEARED,
    YAWNED,
    STATUS_DAMAGE,
    BIND_DAMAGE,
    BIND_ENDED,
    YAWN_SLEEP,
};

struct EffectOutcome {
    EffectOutcomeKind kind = EffectOutcomeKind::STATUS_FAILED;
    MoveEffectTarget target = MoveEffectTarget::DEFENDER;
    Game::MajorStatus status = Game::MajorStatus::NONE;
    BattleStat stat = BattleStat::ATTACK;
    int8_t stageDelta = 0;
    uint16_t amount = 0;
};

struct EffectResolution {
    EffectOutcome outcomes[EFFECT_OUTCOME_CAP] = {};
    uint8_t count = 0;

    void add(const EffectOutcome& outcome) {
        if (count < EFFECT_OUTCOME_CAP) outcomes[count++] = outcome;
    }
};

struct DamageResult {
    uint16_t damage = 0;
    uint16_t effectiveness = 100; // percent
    bool critical = false;
    bool special = false;
    uint8_t specialSlot = SPECIAL_SLOT_NONE;
    Game::MoveId moveId = 0;
    uint8_t hitCount = 1;
    bool missed = false;
};

inline uint16_t experienceReward(const Species& defeatedSpecies, uint8_t defeatedLevel) {
    uint32_t reward = static_cast<uint32_t>(defeatedSpecies.baseExp) * defeatedLevel / 7;
    return static_cast<uint16_t>(reward > 0 ? reward : 1);
}

inline uint16_t scaledExperienceReward(uint16_t reward, uint16_t percent) {
    uint32_t scaled = static_cast<uint32_t>(reward) * percent / 100;
    if (scaled == 0 && reward > 0) scaled = 1;
    return static_cast<uint16_t>(scaled > 0xFFFFU ? 0xFFFFU : scaled);
}

inline ExperienceAwards experienceAwards(uint16_t reward, bool hasHealthyReserve) {
    ExperienceAwards awards;
    awards.active = reward;
    if (hasHealthyReserve) {
        awards.reserve = static_cast<uint32_t>(reward) * RESERVE_EXP_PERCENT / 100;
    }
    return awards;
}

uint16_t typeEffectiveness(TypeId attack, TypeId defend1, TypeId defend2);
uint8_t specialTriggerChance(const Game::MonsterRuntime& attacker, uint8_t specialSlot);
bool rollSpecialMove(const Game::MonsterRuntime& attacker);
uint8_t rollSpecialMoveSlot(const Game::MonsterRuntime& attacker);
BattleMoveWeights aiMoveWeights(
    const Game::MonsterRuntime& attacker,
    const Species& attackerSpecies,
    const BattleActorState& attackerState,
    const Game::MonsterRuntime& defender,
    const Species& defenderSpecies,
    const BattleActorState& defenderState,
    const BattleAiMemory& memory);
uint8_t chooseAiMoveSlot(
    const Game::MonsterRuntime& attacker,
    const Species& attackerSpecies,
    const BattleActorState& attackerState,
    const Game::MonsterRuntime& defender,
    const Species& defenderSpecies,
    const BattleActorState& defenderState,
    BattleAiMemory& memory);
Game::MoveId moveIdForAction(const Game::MonsterRuntime& attacker,
                             const Species& attackerSpecies,
                             uint8_t specialSlot);
int8_t movePriority(Game::MoveId moveId);
uint16_t effectiveSpeed(const Game::MonsterRuntime& monster,
                        const Species& species,
                        const BattleActorState& battleState);
ActionCheckResult checkAction(Game::MonsterRuntime& attacker,
                              const Species& attackerSpecies,
                              BattleActorState& battleState,
                              Game::MoveId moveId);
DamageResult calcBasicDamage(const Game::MonsterRuntime& attacker,
                             const Species& attackerSpecies,
                             const Game::MonsterRuntime& defender,
                             const Species& defenderSpecies,
                             uint8_t specialSlot,
                             const BattleActorState& attackerState,
                             const BattleActorState& defenderState);
EffectResolution applyMoveEffects(const MoveInfo& move,
                                  Game::MonsterRuntime& attacker,
                                  const Species& attackerSpecies,
                                  BattleActorState& attackerState,
                                  Game::MonsterRuntime& defender,
                                  const Species& defenderSpecies,
                                  BattleActorState& defenderState,
                                  uint16_t damageDealt,
                                  bool allowFlinch);
EffectResolution resolveEndTurn(Game::MonsterRuntime& monster,
                                const Species& species,
                                BattleActorState& battleState);
bool canFlee(const BattleActorState& battleState);
void resetVolatile(BattleActorState& battleState);

} // namespace BattleSystem
