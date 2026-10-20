#pragma once

#include "app/BattleTimeline.h"
#include "game/BattleTurnController.h"
#include "game/ExploreSpecialEncounter.h"
#include "ui/models/ScreenModels.h"

namespace AmoledV1 {

// Owns combat transitions and settlement. External actions are consumed by
// the caller; the game and the monotonic clock remain explicit dependencies.
class BattleSession {
public:
    using Clock = uint32_t (*)();
    enum class BattleContinuation : uint8_t { NONE, ADVANCE_TURN, WILD_TURN, FINISH_END_TURN };
    struct State {
        Game::MonsterRuntime battleWild;
        uint8_t battlePlayerSlot = 0;
        BattleSystem::BattleActorState battlePlayerState;
        BattleSystem::BattleActorState battleWildState;
        BattleTurnController::TurnPlan battleTurnPlan;
        uint8_t battleTurnActionIndex = 0;
        bool battleTurnDamaged[2] = {};
        bool battleForceWildEndPending = false;
        BattleContinuation battleContinuation = BattleContinuation::NONE;
        uint8_t pendingBattleSwitchSlot = 0xFF;
        bool battleSwitchConsumesTurn = false;
        BattleViewModel::Phase battlePhase = BattleViewModel::Phase::ACTION;
        uint8_t battlePressedItem = 0xFF;
        uint16_t battleRewardExp = 0;
        uint32_t battleRewardCoins = 0;
        bool battleIsBoss = false;
        uint16_t battleExperiencePercent = 100;
        ExploreSpecial::Kind battleSpecialKind = ExploreSpecial::Kind::NONE;
        BattleViewModel::FriendshipPrompt battleFriendshipPrompt =
            BattleViewModel::FriendshipPrompt::OFFER;
        uint8_t battleFriendshipContactSlot = 0xFF;
        uint8_t battleProgressionSlots[Game::TEAM_CAP] = {};
        uint8_t battleProgressionOldLevels[Game::TEAM_CAP] = {};
        uint8_t battleProgressionCount = 0;
        uint8_t battleProgressionIndex = 0;
        bool battleProgressionSequenceActive = false;
        bool battleVictoryFinalizePending = false;
        uint8_t battleFoodBond = 0;
        bool battleVictorySettled = false;
    };
    struct Effects {
        int rowBegin = 448, rowEnd = 0;
        const char* toast = nullptr;
        bool close = false, resumeAutonomous = false;
        bool stopAutonomous = false, save = false, syncTeam = false;
        bool preloadSwitch = false, openProgression = false;
        uint8_t progressionSlot = 0, progressionOldLevel = 0;
        BattleTimeline::Effects timeline;
    };
    void bindClock(Clock clock) { clock_ = clock; }
    const State& state() const { return state_; }
    BattleViewModel model(const Game::GameState& gameState, uint32_t nowMs) const;
    const BattleTimeline& timeline() const { return timeline_; }
    void begin(Game::GameState& gameState, const Game::MonsterRuntime& wild,
               uint8_t leaderSlot, bool boss, uint16_t experiencePercent,
               ExploreSpecial::Kind special, uint8_t area, uint32_t nowMs);
    void reset();
    void updateGauges(uint32_t nowMs) { timeline_.updateGauges(nowMs); }
    void updateAttack(uint32_t nowMs) { timeline_.updateAttack(nowMs); }
    void updateFlow(Game::GameState& gameState, uint32_t nowMs);
    void switchPreloaded(uint32_t nowMs) { timeline_.switchPreloaded(nowMs); }
    void markPresented() { timeline_.markPresented(); }
    BattleTimeline::Audio takeAudio() { return timeline_.takeAudio(); }
    bool inputBusy() const;
    void press(uint8_t item) { state_.battlePressedItem = item; }
    void showAction() { state_.battlePhase = BattleViewModel::Phase::ACTION; press(0xFF); }
    bool throwFood(Game::GameState& gameState, Game::ItemId item, uint32_t nowMs);
    void throwAudioPlayed(uint32_t nowMs);
    void itemUsed(uint32_t nowMs);
    bool finishDefeat(Game::GameState& gameState);
    void pushLog(const char* message, uint32_t nowMs, bool invalidate = true,
                 uint16_t durationMs = BattleTiming::BATTLE_LOG_DEFAULT_MS);
    bool serviceLog(uint32_t nowMs) { return timeline_.serviceLog(nowMs); }
    bool logBusy() const { return timeline_.logBusy(); }
    void clearLog() { battleMessage[0] = '\0'; timeline_.clearLog(); }
    Effects takeEffects();
#if STICKMON_ENABLE_DEBUG_FEATURES
    void configureDiagnostic(Game::GameState& gameState, uint8_t scenario, Game::MoveId moveId,
                             uint8_t specialSlot);
#endif
    void performBattleAttack(Game::GameState& gameState, uint32_t nowMs);
    void performBattlePlayerAction(Game::GameState& gameState,
                                  const BattleTurnController::Action& action, uint32_t nowMs);
    void performBattleWildAction(Game::GameState& gameState,
                                  const BattleTurnController::Action& action, uint32_t nowMs);
    void performBattleAction(Game::GameState& gameState,
                                  const BattleTurnController::Action& action, uint32_t nowMs);
    void performBattlePlannedAction(Game::GameState& gameState, uint32_t nowMs);
    void advanceBattleTurn(Game::GameState& gameState, uint32_t nowMs);
    void finishBattleEndTurn(Game::GameState& gameState, uint32_t nowMs);
    bool resolveBattleFaint(Game::GameState& gameState, uint32_t nowMs);
    void performBattleSwitch(Game::GameState& gameState, uint8_t teamSlot, bool consumesTurn,
                                    uint32_t nowMs);
    void updateBattleSwitch(Game::GameState& gameState, uint32_t nowMs);
    void performBattleWildTurn(Game::GameState& gameState, uint32_t nowMs);
    int8_t availableBattleSwitchSlot(const Game::GameState& gameState) const;
    void startBattleExperienceAnimation(Game::GameState& gameState, uint32_t nowMs);
    void performBattleFlee(Game::GameState& gameState, uint32_t nowMs);
    void finishBattleVictory(Game::GameState& gameState, uint32_t nowMs);
    void resolveBattleFriendship(Game::GameState& gameState, uint8_t choice, uint32_t nowMs);
    void finishBattleAfterFriendship(Game::GameState& gameState, uint32_t nowMs);
    void resetBattleProgressionQueue();
    void queueBattleProgression(Game::GameState& gameState, uint8_t teamSlot, uint8_t oldLevel,
                                       bool leveledUp);
    bool startNextBattleProgression(Game::GameState& gameState, uint32_t nowMs);

private:
    friend struct AmoledAppTestAccess;
    void pushBattleLog(uint32_t nowMs, bool invalidate = true,
                       uint16_t durationMs = BattleTiming::BATTLE_LOG_DEFAULT_MS);
    void enqueueBattleEffectLogs(const Game::GameState& gameState,
        const BattleSystem::EffectResolution& effects, bool attackerWild, uint32_t nowMs);
    void enqueueBattleDamageLogs(const BattleSystem::DamageResult& damage,
                                 uint16_t dealt, uint32_t nowMs);
    void damageRows(int begin, int end);
    void closeBattle(uint32_t nowMs);
    State state_{};
    BattleTimeline timeline_;
    BattleTurnController battleTurnController;
    Effects effects_{};
    Clock clock_ = nullptr;
    uint8_t area_ = 0;
    char battleMessage[64] = {};
};

}  // namespace AmoledV1
