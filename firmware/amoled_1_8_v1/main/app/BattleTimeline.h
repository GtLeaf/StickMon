#pragma once

#include <memory>
#include "assets/GameAssets.h"
#include "core/AudioCues.h"
#include "game/BattleActionService.h"

namespace AmoledV1 {

namespace BattleTiming {
constexpr uint16_t BATTLE_ANIMATION_RENDER_END = 384;
constexpr uint16_t BATTLE_LOG_DEFAULT_MS = 700;
constexpr uint16_t BATTLE_ATTACK_LOG_MS = 700;
constexpr uint16_t BATTLE_RESULT_LOG_MS = 650;
constexpr uint32_t BATTLE_HP_ANIMATION_MS = 420;
constexpr uint32_t BATTLE_HP_DAMAGE_DELAY_MS = 280;
constexpr uint32_t BATTLE_EXP_ANIMATION_MS = 900;
constexpr uint32_t BATTLE_GAUGE_FRAME_MS = 40;
constexpr uint32_t BATTLE_SWITCH_PHASE_MS = 360;
constexpr uint32_t BATTLE_SWITCH_FRAME_MS = 40;
constexpr int BATTLE_SWITCH_TRAVEL_X = 180;
}

enum class BattleSwitchStage : uint8_t { NONE, RETREATING, ENTERING };

// Owns bounded presentation state, with no device or navigation ownership.
class BattleTimeline {
public:
    static constexpr uint8_t BATTLE_LOG_QUEUE_CAP = 24;
    static constexpr uint8_t BATTLE_LOG_VISIBLE_CAP = 2;
    static constexpr uint8_t BATTLE_LOG_LEN = 64;
    struct State {
        Game::ItemId battleThrownItem = Game::ItemId::COUNT;
        uint32_t battleItemThrowStartedMs = 0;
        uint8_t battleItemThrowFrame = 0;
        bool battleAnimationActive = false;
        bool battleAnimationAttackerWild = false;
        bool battleAnimationHit = false;
        bool battleAnimationStatusEffect = false;
        GameAssets::BattleFx battleAnimationFx = GameAssets::BattleFx::HIT;
        uint16_t battleAnimationDamage = 0;
        uint32_t battleAnimationStartedMs = 0;
        uint32_t battleAnimationDurationMs = 240;
        uint8_t battleAnimationFrame = 0;
        bool battleAttackLogHeld = false;
        bool battleImpactAudioPlayed = false;
        uint8_t battleImpactSfx = 0xFF;
        bool battleHpAnimationActive = false;
        bool battleHpAnimationWild = false;
        uint16_t battleHpAnimationFrom = 0;
        uint16_t battleHpAnimationTo = 0;
        uint16_t battleHpAnimationMax = 1;
        uint32_t battleHpAnimationStartedMs = 0;
        uint32_t nextBattleHpAnimationFrameMs = 0;
        bool battleExperienceVisible = false;
        bool battleExperienceAnimationActive = false;
        uint32_t battleExperienceAnimationFrom = 0;
        uint32_t battleExperienceAnimationTo = 0;
        uint32_t battleExperienceAnimationStartedMs = 0;
        uint32_t nextBattleExperienceAnimationFrameMs = 0;
        bool battleAudioPending = false;
        bool battleAudioReady = false;
        uint8_t battlePendingSfx = 0xFF;
        uint16_t battlePendingCrySpecies = 0;
        char battleLogLines[BATTLE_LOG_VISIBLE_CAP][BATTLE_LOG_LEN] = {};
        uint8_t battleLogHead = 0;
        uint8_t battleLogQueueCount = 0;
        uint8_t battleLogVisibleCount = 0;
        uint32_t battleLogUntil = 0;
        bool battleLogActive = false;
        BattleSwitchStage battleSwitchStage = BattleSwitchStage::NONE;
        uint32_t battleSwitchStartedMs = 0;
        uint32_t nextBattleSwitchFrameMs = 0;
    };
    struct Effects {
        int rowBegin = 448, rowEnd = 0;
        uint8_t sfx = 0xFF;
    };
    struct Audio { uint8_t sfx = 0xFF; uint16_t crySpecies = 0; };
    const State& state() const { return state_; }
    void reset();
    void pushLog(const char* message, uint32_t nowMs, bool invalidate = true,
                 uint16_t durationMs = BattleTiming::BATTLE_LOG_DEFAULT_MS);
    bool serviceLog(uint32_t nowMs);
    bool logBusy() const;
    void clearLog();
    void startAttack(const BattleActionService::PendingAction& action,
                     bool attackerWild, uint16_t attackerSpecies,
                     uint16_t attackerMaxHp, uint16_t defenderMaxHp,
                     uint32_t startedMs);
    void holdAttackLog() { state_.battleAttackLogHeld = true; }
    void startHp(bool wild, uint16_t from, uint16_t to, uint16_t max,
                 uint32_t startedMs);
    uint8_t hpPercent(bool wild, uint16_t hp, uint16_t max, uint32_t nowMs) const;
    void startExperience(uint32_t from, uint32_t to, uint32_t nowMs);
    uint32_t experience(uint32_t nowMs) const;
    void startThrow(Game::ItemId item, uint32_t startedMs);
    void updateGauges(uint32_t nowMs);
    void updateAttack(uint32_t nowMs);
    void startSwitch(uint32_t nowMs);
    bool switchDue(uint32_t nowMs);
    void awaitSwitchPreload() { switchAwaitingPreload_ = true; }
    void switchPreloaded(uint32_t nowMs);
    void finishSwitch();
    int16_t switchOffset(uint32_t nowMs) const;
    bool actionBusy() const;
    bool inputBusy() const;
    void markPresented();
    Audio takeAudio();
    Effects takeEffects();

private:
    friend struct AmoledAppTestAccess;
    friend struct AmoledBattleTestAccess;
    struct LogQueue {
        char lines[BATTLE_LOG_QUEUE_CAP][BATTLE_LOG_LEN] = {};
        uint16_t durations[BATTLE_LOG_QUEUE_CAP] = {};
    };
    void damageRows(int begin, int end);
    State state_{};
    Effects effects_{};
    // Allocate once on entering battle, never on a frame update. Keeping the
    // bounded FIFO indirect avoids adding 1.5 KiB to temporary app stack frames.
    std::unique_ptr<LogQueue> queue_;
    bool switchAwaitingPreload_ = false;
    bool allocationAttempted_ = false;
};

}  // namespace AmoledV1
