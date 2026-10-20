#include "app/BattleTimeline.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <new>

namespace AmoledV1 {
using namespace BattleTiming;
namespace {
bool statusEffectSucceeded(
    GameAssets::BattleFx fx, const BattleSystem::DamageResult& damage,
    const BattleSystem::EffectResolution& effects) {
    if (!GameAssets::battleFxIsStatus(fx) || damage.missed || damage.failed ||
        damage.effectiveness == 0) return false;
    for (uint8_t i = 0; i < effects.count; ++i) {
        const auto& outcome = effects.outcomes[i];
        if (outcome.kind == BattleSystem::EffectOutcomeKind::STAT_CHANGED &&
            outcome.target == MoveEffectTarget::DEFENDER &&
            outcome.stageDelta != 0) return true;
    }
    return false;
}
}

void BattleTimeline::pushLog(const char* message, uint32_t nowMs, bool invalidate,
                              uint16_t durationMs) {
    if (!message || !message[0]) return;
    if (!queue_ && !allocationAttempted_) {
        allocationAttempted_ = true;
        queue_.reset(new (std::nothrow) LogQueue{});
    }
    if (!queue_) {
        // Allocation failure must not strand battle continuations behind a
        // nonexistent FIFO. Display the latest line for its normal duration.
        std::snprintf(state_.battleLogLines[0], BATTLE_LOG_LEN, "%s", message);
        state_.battleLogVisibleCount = 1;
        state_.battleLogActive = true;
        state_.battleLogUntil = nowMs + (durationMs ? durationMs : BATTLE_LOG_DEFAULT_MS);
        damageRows(352, 448);
        return;
    }
    if (state_.battleLogQueueCount >= BATTLE_LOG_QUEUE_CAP) {
        queue_->durations[state_.battleLogHead] = 0;
        state_.battleLogHead = static_cast<uint8_t>(
            (state_.battleLogHead + 1) % BATTLE_LOG_QUEUE_CAP);
        --state_.battleLogQueueCount;
    }
    const uint8_t tail = static_cast<uint8_t>(
        (state_.battleLogHead + state_.battleLogQueueCount) % BATTLE_LOG_QUEUE_CAP);
    std::snprintf(queue_->lines[tail], sizeof(queue_->lines[tail]), "%s",
                  message);
    queue_->durations[tail] = durationMs == 0
        ? BATTLE_LOG_DEFAULT_MS : durationMs;
    ++state_.battleLogQueueCount;
    if (invalidate && serviceLog(nowMs)) {
        damageRows(352, 448);
    }
}

bool BattleTimeline::serviceLog(uint32_t nowMs) {
    if (state_.battleThrownItem != Game::ItemId::COUNT ||
        (state_.battleAttackLogHeld && state_.battleAnimationActive)) return false;
    if (state_.battleLogActive &&
        static_cast<int32_t>(nowMs - state_.battleLogUntil) < 0) {
        return false;
    }
    if (state_.battleLogQueueCount == 0) {
        const bool changed = state_.battleLogActive || state_.battleLogVisibleCount > 0;
        state_.battleLogActive = false;
        state_.battleLogVisibleCount = 0;
        state_.battleLogUntil = 0;
        for (auto& line : state_.battleLogLines) line[0] = '\0';
        return changed;
    }

    uint8_t line = 0;
    if (state_.battleLogVisibleCount < BATTLE_LOG_VISIBLE_CAP) {
        line = state_.battleLogVisibleCount++;
    } else {
        for (uint8_t index = 1; index < BATTLE_LOG_VISIBLE_CAP; ++index) {
            std::memcpy(state_.battleLogLines[index - 1], state_.battleLogLines[index],
                        BATTLE_LOG_LEN);
        }
        line = BATTLE_LOG_VISIBLE_CAP - 1;
    }
    std::memcpy(state_.battleLogLines[line], queue_->lines[state_.battleLogHead],
                BATTLE_LOG_LEN);
    state_.battleLogLines[line][BATTLE_LOG_LEN - 1] = '\0';
    queue_->lines[state_.battleLogHead][0] = '\0';
    const uint16_t durationMs = queue_->durations[state_.battleLogHead] == 0
        ? BATTLE_LOG_DEFAULT_MS : queue_->durations[state_.battleLogHead];
    queue_->durations[state_.battleLogHead] = 0;
    state_.battleLogHead = static_cast<uint8_t>(
        (state_.battleLogHead + 1) % BATTLE_LOG_QUEUE_CAP);
    --state_.battleLogQueueCount;
    state_.battleLogActive = true;
    state_.battleLogUntil = nowMs + durationMs;
    return true;
}

bool BattleTimeline::logBusy() const {
    return state_.battleLogActive || state_.battleLogQueueCount > 0;
}

void BattleTimeline::clearLog() {
    state_.battleLogHead = 0;
    state_.battleLogQueueCount = 0;
    state_.battleLogVisibleCount = 0;
    state_.battleLogUntil = 0;
    state_.battleLogActive = false;
    if (queue_) for (auto& line : queue_->lines) line[0] = '\0';
    if (queue_) for (auto& duration : queue_->durations) duration = 0;
    for (auto& line : state_.battleLogLines) line[0] = '\0';
}

void BattleTimeline::startHp(bool wildSide, uint16_t fromHp,
                                       uint16_t toHp, uint16_t maxHp,
                                       uint32_t startedMs) {
    state_.battleHpAnimationWild = wildSide;
    state_.battleHpAnimationFrom = fromHp;
    state_.battleHpAnimationTo = toHp;
    state_.battleHpAnimationMax = std::max<uint16_t>(1, maxHp);
    state_.battleHpAnimationStartedMs = startedMs;
    state_.nextBattleHpAnimationFrameMs = startedMs;
    state_.battleHpAnimationActive = fromHp != toHp;
}

uint8_t BattleTimeline::hpPercent(
    bool wildSide, uint16_t currentHp, uint16_t maxHp,
    uint32_t nowMs) const {
    uint32_t shownHp = currentHp;
    uint32_t shownMax = std::max<uint16_t>(1, maxHp);
    if (state_.battleHpAnimationActive &&
        state_.battleHpAnimationWild == wildSide) {
        shownMax = state_.battleHpAnimationMax;
        if (static_cast<int32_t>(nowMs - state_.battleHpAnimationStartedMs) <= 0) {
            shownHp = state_.battleHpAnimationFrom;
        } else {
            uint32_t elapsed = std::min<uint32_t>(
                BATTLE_HP_ANIMATION_MS,
                nowMs - state_.battleHpAnimationStartedMs);
            if (state_.battleHpAnimationFrom >= state_.battleHpAnimationTo) {
                const uint32_t distance =
                    state_.battleHpAnimationFrom - state_.battleHpAnimationTo;
                shownHp = state_.battleHpAnimationFrom -
                    distance * elapsed / BATTLE_HP_ANIMATION_MS;
            } else {
                const uint32_t distance =
                    state_.battleHpAnimationTo - state_.battleHpAnimationFrom;
                shownHp = state_.battleHpAnimationFrom +
                    distance * elapsed / BATTLE_HP_ANIMATION_MS;
            }
        }
    }
    return static_cast<uint8_t>(std::min<uint32_t>(
        100, shownHp * 100U / shownMax));
}

uint32_t BattleTimeline::experience(uint32_t nowMs) const {
    if (!state_.battleExperienceVisible ||
        state_.battleExperienceAnimationTo <= state_.battleExperienceAnimationFrom) {
        return state_.battleExperienceAnimationFrom;
    }
    if (!state_.battleExperienceAnimationActive) {
        return state_.battleExperienceAnimationTo;
    }
    if (static_cast<int32_t>(nowMs - state_.battleExperienceAnimationStartedMs) <= 0) {
        return state_.battleExperienceAnimationFrom;
    }
    const uint32_t elapsed = std::min<uint32_t>(
        BATTLE_EXP_ANIMATION_MS,
        nowMs - state_.battleExperienceAnimationStartedMs);
    const uint64_t distance = static_cast<uint64_t>(
        state_.battleExperienceAnimationTo - state_.battleExperienceAnimationFrom) *
        elapsed;
    return state_.battleExperienceAnimationFrom +
        static_cast<uint32_t>(distance / BATTLE_EXP_ANIMATION_MS);
}

void BattleTimeline::startAttack(const BattleActionService::PendingAction& action,
    bool attackerWild, uint16_t attackerSpecies, uint16_t attackerMaxHp,
    uint16_t defenderMaxHp, uint32_t startedMs) {
    const auto& damage = action.damage;
    const auto& result = action.result;
    const auto& moveEffects = result.moveEffects;
    const auto* move = findMove(damage.moveId);
    const uint16_t dealt = result.actualDamage;
    state_.battleAnimationActive = true;
    state_.battleAnimationAttackerWild = attackerWild;
    state_.battleAnimationHit = !damage.missed && !damage.failed && dealt > 0;
    state_.battleAnimationFx = move ? GameAssets::battleFxForMove(move->id, move->type)
                             : GameAssets::BattleFx::HIT;
    state_.battleAnimationStatusEffect = statusEffectSucceeded(state_.battleAnimationFx, damage, moveEffects);
    state_.battleAnimationDamage = dealt;
    state_.battleAnimationDurationMs = BATTLE_HP_DAMAGE_DELAY_MS + BATTLE_HP_ANIMATION_MS + BATTLE_GAUGE_FRAME_MS;
    state_.battleAnimationFrame = 1;
    state_.battleAnimationStartedMs = startedMs;
    if (result.defenderHpBefore != result.defenderHpAfter) {
        startHp(!attackerWild, result.defenderHpBefore,
            result.defenderHpAfter, defenderMaxHp, state_.battleAnimationStartedMs + BATTLE_HP_DAMAGE_DELAY_MS);
    } else if (result.attackerHpBefore != result.attackerHpAfter) {
        startHp(attackerWild, result.attackerHpBefore,
            result.attackerHpAfter, attackerMaxHp, state_.battleAnimationStartedMs + BATTLE_HP_DAMAGE_DELAY_MS);
    }
    state_.battleAudioPending = true;
    state_.battleAudioReady = false;
    state_.battlePendingSfx = 0xFF;
    state_.battleImpactAudioPlayed = false;
    state_.battleImpactSfx = static_cast<uint8_t>(state_.battleAnimationHit
        ? (damage.effectiveness > 100 ? SfxCue::DAMAGE_SUPER
            : damage.effectiveness < 100 ? SfxCue::DAMAGE_WEAK : SfxCue::DAMAGE_NORMAL)
        : state_.battleAnimationStatusEffect ? SfxCue::UI_CONFIRM : SfxCue::UI_CANCEL);
    state_.battlePendingCrySpecies = attackerSpecies;
    damageRows(0, BATTLE_ANIMATION_RENDER_END);
}

void BattleTimeline::updateGauges(uint32_t nowMs) {
    if (state_.battleHpAnimationActive) {
        const uint32_t gaugeNowMs = nowMs;
        if (static_cast<int32_t>(gaugeNowMs -
                                 state_.nextBattleHpAnimationFrameMs) >= 0) {
            state_.nextBattleHpAnimationFrameMs =
                gaugeNowMs + BATTLE_GAUGE_FRAME_MS;
            damageRows(state_.battleHpAnimationWild ? 48 : 266,
                              state_.battleHpAnimationWild ? 88 : 306);
        }
        if (static_cast<int32_t>(
                gaugeNowMs - state_.battleHpAnimationStartedMs) >= 0 &&
            gaugeNowMs - state_.battleHpAnimationStartedMs >=
                BATTLE_HP_ANIMATION_MS) {
            state_.battleHpAnimationActive = false;
            damageRows(state_.battleHpAnimationWild ? 48 : 266,
                              state_.battleHpAnimationWild ? 88 : 306);
        }
    }
    if (state_.battleExperienceAnimationActive) {
        const uint32_t gaugeNowMs = nowMs;
        if (static_cast<int32_t>(
                gaugeNowMs - state_.nextBattleExperienceAnimationFrameMs) >= 0) {
            state_.nextBattleExperienceAnimationFrameMs =
                gaugeNowMs + BATTLE_GAUGE_FRAME_MS;
            damageRows(232, 308);
        }
        if (gaugeNowMs - state_.battleExperienceAnimationStartedMs >=
            BATTLE_EXP_ANIMATION_MS) {
            state_.battleExperienceAnimationActive = false;
            effects_.sfx = static_cast<uint8_t>(SfxCue::EXP_FULL);
            damageRows(232, 308);
        }
    }

    if (state_.battleThrownItem != Game::ItemId::COUNT) {
        const uint32_t elapsed = nowMs - state_.battleItemThrowStartedMs;
        const uint8_t frame = static_cast<uint8_t>(std::min<uint32_t>(24, elapsed / 33));
        if (frame != state_.battleItemThrowFrame) {
            state_.battleItemThrowFrame = frame;
            damageRows(0, BATTLE_ANIMATION_RENDER_END);
        }
        if (elapsed >= 800) {
            state_.battleThrownItem = Game::ItemId::COUNT;
            damageRows(0, BATTLE_ANIMATION_RENDER_END);
        }
    }
}

void BattleTimeline::updateAttack(uint32_t nowMs) {
    if (state_.battleAnimationActive) {
        // The caller samples this clock after consuming earlier audio effects;
        // reusing the loop timestamp can underflow and skip the first frame.
        uint32_t animationNowMs = nowMs;
        uint32_t elapsed = animationNowMs - state_.battleAnimationStartedMs;
        if (!state_.battleImpactAudioPlayed &&
            elapsed >= BATTLE_HP_DAMAGE_DELAY_MS) {
            state_.battleImpactAudioPlayed = true;
            if (state_.battleImpactSfx != 0xFF) {
                effects_.sfx = state_.battleImpactSfx;
            }
        }
        uint8_t frame = elapsed < state_.battleAnimationDurationMs
            ? static_cast<uint8_t>(std::min<uint32_t>(
                  6, elapsed / 80U + 1U))
            : 0;
        if (frame != state_.battleAnimationFrame) {
            state_.battleAnimationFrame = frame;
            damageRows(0, BATTLE_ANIMATION_RENDER_END);
        }
        if (elapsed >= state_.battleAnimationDurationMs) {
            state_.battleAnimationActive = false;
            state_.battleAnimationStatusEffect = false;
            state_.battleAnimationFrame = 0;
            state_.battleAnimationDamage = 0;
            state_.battleAttackLogHeld = false;
            state_.battleImpactSfx = 0xFF;
            damageRows(0, BATTLE_ANIMATION_RENDER_END);
        }
    }
}

void BattleTimeline::reset() {
    state_ = State{};
    effects_ = Effects{};
    switchAwaitingPreload_ = false;
    if (!queue_) allocationAttempted_ = false;
    clearLog();
}
void BattleTimeline::startExperience(uint32_t from, uint32_t to, uint32_t nowMs) {
    state_.battleExperienceVisible = true;
    state_.battleExperienceAnimationFrom = from;
    state_.battleExperienceAnimationTo = to;
    state_.battleExperienceAnimationStartedMs = nowMs;
    state_.nextBattleExperienceAnimationFrameMs = nowMs;
    state_.battleExperienceAnimationActive = to > from;
    if (to > from) effects_.sfx = static_cast<uint8_t>(SfxCue::EXP_GAIN);
}
void BattleTimeline::startThrow(Game::ItemId item, uint32_t startedMs) {
    state_.battleThrownItem = item;
    state_.battleItemThrowFrame = 0;
    state_.battleItemThrowStartedMs = startedMs;
    damageRows(0, BATTLE_ANIMATION_RENDER_END);
}
void BattleTimeline::startSwitch(uint32_t nowMs) {
    state_.battleSwitchStage = BattleSwitchStage::RETREATING;
    state_.battleSwitchStartedMs = nowMs;
    state_.nextBattleSwitchFrameMs = nowMs;
    switchAwaitingPreload_ = false;
    damageRows(0, BATTLE_ANIMATION_RENDER_END);
}
bool BattleTimeline::switchDue(uint32_t nowMs) {
    if (state_.battleSwitchStage == BattleSwitchStage::NONE || switchAwaitingPreload_) return false;
    if (static_cast<int32_t>(nowMs - state_.nextBattleSwitchFrameMs) >= 0) {
        state_.nextBattleSwitchFrameMs = nowMs + BATTLE_SWITCH_FRAME_MS;
        damageRows(0, BATTLE_ANIMATION_RENDER_END);
    }
    return nowMs - state_.battleSwitchStartedMs >= BATTLE_SWITCH_PHASE_MS;
}
void BattleTimeline::switchPreloaded(uint32_t nowMs) {
    if (!switchAwaitingPreload_) return;
    switchAwaitingPreload_ = false;
    state_.battleSwitchStage = BattleSwitchStage::ENTERING;
    state_.battleSwitchStartedMs = nowMs;
    state_.nextBattleSwitchFrameMs = nowMs;
    damageRows(0, BATTLE_ANIMATION_RENDER_END);
}
void BattleTimeline::finishSwitch() {
    state_.battleSwitchStage = BattleSwitchStage::NONE;
    state_.battleSwitchStartedMs = 0;
    switchAwaitingPreload_ = false;
    damageRows(0, BATTLE_ANIMATION_RENDER_END);
}
int16_t BattleTimeline::switchOffset(uint32_t nowMs) const {
    if (state_.battleSwitchStage == BattleSwitchStage::NONE) return 0;
    const uint32_t elapsed = std::min<uint32_t>(BATTLE_SWITCH_PHASE_MS,
        nowMs - state_.battleSwitchStartedMs);
    const uint32_t progress = state_.battleSwitchStage == BattleSwitchStage::RETREATING
        ? elapsed : BATTLE_SWITCH_PHASE_MS - elapsed;
    return static_cast<int16_t>(-static_cast<int32_t>(BATTLE_SWITCH_TRAVEL_X * progress * progress /
        (BATTLE_SWITCH_PHASE_MS * BATTLE_SWITCH_PHASE_MS)));
}
bool BattleTimeline::actionBusy() const {
    return state_.battleSwitchStage != BattleSwitchStage::NONE ||
        state_.battleThrownItem != Game::ItemId::COUNT ||
        state_.battleAnimationActive || state_.battleHpAnimationActive || logBusy();
}
bool BattleTimeline::inputBusy() const {
    return actionBusy() || state_.battleExperienceAnimationActive;
}
void BattleTimeline::markPresented() {
    if (state_.battleAnimationActive && state_.battleAudioPending && !state_.battleAudioReady)
        state_.battleAudioReady = true;
}
BattleTimeline::Audio BattleTimeline::takeAudio() {
    Audio audio;
    if (!state_.battleAudioPending || !state_.battleAudioReady) return audio;
    audio.sfx = state_.battlePendingSfx;
    audio.crySpecies = state_.battlePendingCrySpecies;
    state_.battleAudioPending = state_.battleAudioReady = false;
    state_.battlePendingSfx = 0xFF;
    state_.battlePendingCrySpecies = 0;
    return audio;
}
void BattleTimeline::damageRows(int begin, int end) {
    effects_.rowBegin = std::min(effects_.rowBegin, begin);
    effects_.rowEnd = std::max(effects_.rowEnd, end);
}
BattleTimeline::Effects BattleTimeline::takeEffects() {
    const auto result = effects_;
    effects_ = Effects{};
    return result;
}

}  // namespace AmoledV1
