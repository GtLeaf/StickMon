#include "app/BattleSession.h"
#include "game/GameRandom.h"
#include "game/MonsterFactory.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <type_traits>
#include <utility>

namespace AmoledV1 {
namespace {
uint32_t currentMs = 0, draws = 0;
uint32_t clockNow() { return currentMs; }
uint32_t minimumRoll(uint32_t minimum, uint32_t) { ++draws; return minimum; }
Game::GameState team() {
    Game::GameState state{};
    state.teamCount = 2;
    state.team[0] = Game::MonsterFactory::create(4, 15);
    state.team[1] = Game::MonsterFactory::create(7, 15);
    return state;
}
void expectNoEffects(BattleSession& session) {
    const auto effects = session.takeEffects();
    assert(!effects.close && !effects.save && !effects.openProgression &&
           !effects.preloadSwitch && !effects.syncTeam && !effects.toast);
    assert(effects.rowBegin >= effects.rowEnd);
    assert(effects.timeline.rowBegin >= effects.timeline.rowEnd && effects.timeline.sfx == 0xFF);
}
}

// Only the allocation-failure fixture needs access; all transitions below use
// public operations and const snapshots.
struct AmoledBattleTestAccess {
    static void failQueueAllocation(BattleTimeline& timeline) {
        timeline.queue_.reset();
        timeline.allocationAttempted_ = true;
    }
};

void checkBattleCommitAndAudio() {
    static_assert(std::is_same_v<decltype(std::declval<BattleSession&>().state()),
                                 const BattleSession::State&>);
    static_assert(std::is_same_v<decltype(std::declval<BattleTimeline&>().state()),
                                 const BattleTimeline::State&>);
    for (bool wildSide : {false, true}) {
        auto state = team();
        auto wild = Game::MonsterFactory::create(7, 15);
        BattleSession session;
        session.bindClock(clockNow);
        currentMs = 1000;
        session.begin(state, wild, 0, false, 100, ExploreSpecial::Kind::NONE, 0, 100);
        const uint16_t hpBefore = wildSide ? state.team[0].hpCur : wild.hpCur;
        const uint16_t hpMax = wildSide ? state.team[0].hpMax : wild.hpMax;
        const auto side = wildSide ? BattleTurnController::Side::WILD : BattleTurnController::Side::PLAYER;
        session.performBattleAction(state, {side, BattleSystem::SPECIAL_SLOT_NONE, 33}, 100);
        const uint16_t hpAfter = wildSide ? state.team[0].hpCur : session.state().battleWild.hpCur;
        assert(hpAfter < hpBefore); // Commit belongs to action start, before presentation.
        const auto& timeline = session.timeline();
        assert(timeline.state().battleAnimationStartedMs == currentMs);
        assert(timeline.hpPercent(!wildSide, hpAfter, hpMax, currentMs) == hpBefore * 100U / hpMax);
        assert(timeline.hpPercent(!wildSide, hpAfter, hpMax, currentMs + 279) == hpBefore * 100U / hpMax);
        const auto drawsAfterCommit = draws;
        session.takeEffects();
        assert(session.takeAudio().crySpecies == 0);
        session.markPresented();
        const auto audio = session.takeAudio();
        assert(audio.crySpecies == (wildSide ? wild.speciesId : state.team[0].speciesId));
        assert(session.takeAudio().crySpecies == 0);
        session.markPresented();
        assert(session.takeAudio().crySpecies == 0);
        session.updateAttack(currentMs + 279);
        assert(session.takeEffects().timeline.sfx == 0xFF);
        session.updateAttack(currentMs + 280);
        assert(session.takeEffects().timeline.sfx != 0xFF);
        session.updateAttack(currentMs + 281);
        assert(session.takeEffects().timeline.sfx == 0xFF);
        session.updateGauges(currentMs + 700);
        session.updateAttack(currentMs + 740);
        assert(!timeline.state().battleHpAnimationActive && !timeline.state().battleAnimationActive);
        assert(timeline.hpPercent(!wildSide, hpAfter, hpMax, currentMs + 740) == hpAfter * 100U / hpMax);
        assert(draws == drawsAfterCommit);
        assert((wildSide ? state.team[0].hpCur : session.state().battleWild.hpCur) == hpAfter);
        session.takeEffects(); expectNoEffects(session);
    }
    std::puts("battle commit/audio: immediate HP commit, delayed display, fresh clock, presentation ack and impact once");
}

void checkBattleLogsAndClockWrap() {
    BattleTimeline a, b;
    a.reset(); b.reset();
    a.pushLog("A", 10, false, 650);
    b.pushLog("B", 10, false, 700);
    assert(a.serviceLog(10) && b.serviceLog(10));
    assert(std::strcmp(a.state().battleLogLines[0], "A") == 0);
    assert(std::strcmp(b.state().battleLogLines[0], "B") == 0);
    a.reset();
    assert(std::strcmp(b.state().battleLogLines[0], "B") == 0);
    assert(!b.serviceLog(709) && b.serviceLog(710) && !b.logBusy());
    for (int index = 0; index < 28; ++index) {
        char message[16]; std::snprintf(message, sizeof(message), "%d", index);
        a.pushLog(message, 100, false, 650);
    }
    assert(a.state().battleLogQueueCount == 24);
    assert(a.serviceLog(100));
    assert(std::strcmp(a.state().battleLogLines[0], "4") == 0);
    assert(!a.serviceLog(749) && a.serviceLog(750));
    assert(std::strcmp(a.state().battleLogLines[1], "5") == 0);
    a.reset();
    const uint32_t start = UINT32_MAX - 200;
    a.pushLog("wrap", start, false, 700);
    assert(a.serviceLog(start));
    assert(!a.serviceLog(start + 699));
    assert(a.serviceLog(start + 700) && !a.logBusy());
    a.startHp(false, 100, 0, 100, start + 280);
    assert(a.hpPercent(false, 0, 100, start) == 100);
    assert(a.hpPercent(false, 0, 100, start + 490) == 50);
    a.updateGauges(start + 700);
    assert(!a.state().battleHpAnimationActive);
    a.startExperience(100, 1000, start);
    assert(a.takeEffects().sfx == static_cast<uint8_t>(SfxCue::EXP_GAIN));
    assert(a.experience(start + 450) == 550);
    a.updateGauges(start + 900);
    assert(a.experience(start + 900) == 1000);
    assert(a.takeEffects().sfx == static_cast<uint8_t>(SfxCue::EXP_FULL));
    a.updateGauges(start + 901);
    assert(a.takeEffects().sfx == 0xFF);
    a.reset(); AmoledBattleTestAccess::failQueueAllocation(a);
    a.pushLog("fallback", 100);
    assert(a.logBusy() && a.state().battleLogVisibleCount == 1);
    assert(a.serviceLog(800) && !a.logBusy());
    std::puts("battle timeline: isolated bounded FIFO, per-line deadlines, allocation fallback and uint32 wrap");
}

void checkBattleSwitchAcknowledgement() {
    auto state = team();
    const auto original = state;
    auto wild = Game::MonsterFactory::create(1, 15);
    BattleSession session; session.bindClock(clockNow);
    currentMs = 100;
    session.begin(state, wild, 0, false, 100, ExploreSpecial::Kind::NONE, 0, 100);
    session.performBattleSwitch(state, 1, true, 100);
    session.takeEffects();
    session.updateBattleSwitch(state, 459);
    assert(session.state().battlePlayerSlot == 0 && !session.takeEffects().preloadSwitch);
    session.updateBattleSwitch(state, 460);
    const auto preload = session.takeEffects();
    assert(preload.preloadSwitch && session.state().battlePlayerSlot == 1);
    expectNoEffects(session);
    session.updateBattleSwitch(state, 10000); // Cannot finish while preloading.
    assert(session.timeline().state().battleSwitchStage == BattleSwitchStage::RETREATING);
    expectNoEffects(session);
    session.switchPreloaded(10000);
    session.takeEffects();
    session.switchPreloaded(11000); // Duplicate acknowledgements cannot restart entry.
    assert(session.timeline().state().battleSwitchStartedMs == 10000);
    session.updateBattleSwitch(state, 10359);
    assert(session.state().battleContinuation == BattleSession::BattleContinuation::NONE);
    session.updateBattleSwitch(state, 10360);
    assert(session.timeline().state().battleSwitchStage == BattleSwitchStage::NONE);
    assert(session.state().battleContinuation == BattleSession::BattleContinuation::WILD_TURN);
    assert(state.activeSlot == original.activeSlot && state.team[0].speciesId == original.team[0].speciesId &&
           state.team[1].speciesId == original.team[1].speciesId);
    std::puts("battle switch: retreat, preload ack, entry and counterattack preserve permanent roster order");
}

void checkBattleSettlementAndProgression() {
    auto state = team();
    for (uint8_t slot = 0; slot < 2; ++slot) {
        const auto* species = findSpecies(state.team[slot].speciesId);
        state.team[slot].exp = minimumExpForLevel(species->growthRate, 16) - 1;
    }
    auto wild = Game::MonsterFactory::create(1, 25);
    wild.hpCur = 0;
    BattleSession session;
    session.begin(state, wild, 0, true, 200, ExploreSpecial::Kind::FIRST_LATIAS, 0, 100);
    assert(session.resolveBattleFaint(state, 100));
    session.takeEffects();
    const auto coins = state.coins;
    session.finishBattleVictory(state, 1000);
    const auto settled = state;
    const auto randomAfterSettlement = draws;
    assert(state.coins > coins && state.team[0].level > 15 && state.team[1].level > 15);
    assert(session.state().battleProgressionCount == 2 && session.state().battleVictoryFinalizePending);
    session.takeEffects();
    for (int repeat = 0; repeat < 3; ++repeat) {
        session.finishBattleVictory(state, 1001 + repeat);
        assert(std::memcmp(&state, &settled, sizeof(state)) == 0);
        assert(draws == randomAfterSettlement);
        expectNoEffects(session);
    }
    session.clearLog();
    session.finishBattleAfterFriendship(state, 2000);
    const auto first = session.takeEffects();
    assert(first.openProgression && first.progressionSlot == 0 && first.progressionOldLevel == 15 && first.save);
    expectNoEffects(session);
    assert(session.startNextBattleProgression(state, 2001));
    const auto second = session.takeEffects();
    assert(second.openProgression && second.progressionSlot == 1 && second.progressionOldLevel == 15);
    assert(!session.startNextBattleProgression(state, 2002));
    expectNoEffects(session);
    assert(!session.state().battleProgressionSequenceActive);
    std::puts("battle settlement: XP/effort/coins/boss progress once and two-member progression queue");
}

void checkBattleFriendshipPrompts() {
    using Prompt = BattleViewModel::FriendshipPrompt;
    for (int scenario = 0; scenario < 4; ++scenario) {
        auto state = team();
        if (scenario == 0) state.teamCount = 1;
        const auto members = state.teamCount;
        auto wild = Game::MonsterFactory::create(1, 5); wild.hpCur = 0;
        BattleSession session;
        session.begin(state, wild, 0, false, 100, ExploreSpecial::Kind::NONE, 0, 100);
        assert(session.resolveBattleFaint(state, 100));
        session.finishBattleVictory(state, 1000);
        assert(session.state().battlePhase == BattleViewModel::Phase::FRIENDSHIP);
        assert(session.state().battleFriendshipPrompt == Prompt::OFFER);
        session.takeEffects();
        if (scenario == 2) { // A full box while deciding must not record a contact.
            state.storageCount = Game::STORAGE_CAP;
            session.resolveBattleFriendship(state, 0, 1100);
            assert(session.state().battleFriendshipPrompt == Prompt::FULL);
            assert(state.teamCount == members && state.storageCount == Game::STORAGE_CAP);
        } else if (scenario == 3) {
            session.resolveBattleFriendship(state, 1, 1100);
            assert(state.storageCount == 0 && state.teamCount == members);
            assert(session.takeEffects().close);
            continue;
        } else {
            session.resolveBattleFriendship(state, 0, 1100);
            assert(state.storageCount == 1);
            assert(session.state().battleFriendshipPrompt == (scenario == 0 ? Prompt::TEAM : Prompt::ACQUIRED));
            if (scenario == 0) {
                session.takeEffects();
                session.resolveBattleFriendship(state, 0, 1200);
                assert(session.takeEffects().syncTeam);
                assert(session.state().battleFriendshipPrompt == Prompt::ACQUIRED);
                assert(state.teamCount == members + 1);
            }
        }
        session.takeEffects();
        const auto contacts = state.storageCount, teamCount = state.teamCount;
        session.resolveBattleFriendship(state, 0, 1300);
        assert(session.takeEffects().close);
        assert(state.storageCount == contacts && state.teamCount == teamCount);
    }
    std::puts("battle friendship: offer, decline, full box, contact and optional team invitation");
}

void checkBattleFlowContracts(const char* scenario) {
    GameRandom::setRangeProvider(minimumRoll);
    if (std::strcmp(scenario, "battle-flow-commit") == 0) checkBattleCommitAndAudio();
    else if (std::strcmp(scenario, "battle-flow-logs") == 0) checkBattleLogsAndClockWrap();
    else if (std::strcmp(scenario, "battle-flow-switch") == 0) checkBattleSwitchAcknowledgement();
    else if (std::strcmp(scenario, "battle-flow-settlement") == 0) checkBattleSettlementAndProgression();
    else if (std::strcmp(scenario, "battle-flow-friendship") == 0) checkBattleFriendshipPrompts();
    else assert(false);
    GameRandom::setRangeProvider(nullptr);
}
} // namespace AmoledV1
