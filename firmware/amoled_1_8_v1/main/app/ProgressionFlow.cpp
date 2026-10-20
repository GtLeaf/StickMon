#include "ProgressionFlow.h"

#include <algorithm>
#include <cmath>

#include "HomeScreen.h"
#include "game/CareExperience.h"
#include "game/Species.h"
#include "ui/UiMetrics.h"

namespace AmoledV1 {
namespace {
constexpr int MENU_HEADER_HEIGHT = 56;
constexpr uint32_t TEAM_MOVES_DETAIL_ANIM_MS = 240;
constexpr uint32_t PROGRESSION_ANIMATION_FRAME_MS = 50;
constexpr uint32_t LEVEL_UP_ANIMATION_MS = 800;
constexpr uint32_t EVOLUTION_CANCEL_HOLD_MS = 600;
constexpr uint32_t EVOLUTION_TOUCH_GAP_MS = 50;
}

bool ProgressionFlow::openLevel(Game::GameState& gameState, AppSceneFlow::Scene returnScene,
                               uint8_t teamSlot, uint8_t oldLevel, uint32_t nowMs, bool fromCare) {
    if (teamSlot >= gameState.teamCount || teamSlot >= Game::TEAM_CAP) return false;
    Game::MonsterRuntime& monster = gameState.team[teamSlot];
    if (monster.level <= oldLevel) return false;
    active_ = true;
    state_.progressionFromCare = fromCare;

    state_.progressionReturnScene = returnScene;
    state_.progressionTeamSlot = teamSlot;
    state_.progressionOldLevel = oldLevel;
    state_.progressionLevel = monster.level;
    state_.progressionLevelUpStartedMs = nowMs;
    state_.progressionLevelUpTouchAllowed = false;
    state_.progressionFromSpeciesId = monster.speciesId;
    state_.progressionToSpeciesId = 0;
    state_.progressionMoveId = 0;
    state_.progressionOldMove2 = 0;
    state_.progressionOldMove3 = 0;
    state_.progressionMoveCursor = 0;
    state_.progressionPressedItem = 0xFF;
    state_.progressionEvolution.reset();
    state_.progressionEvolutionItem = Game::ItemId::COUNT;
    state_.nextProgressionAnimationFrameMs = nowMs + PROGRESSION_ANIMATION_FRAME_MS;
    state_.progressionEvolutionTouchStartedMs = 0;
    state_.progressionEvolutionTouchResumeUntilMs = 0;
    state_.progressionEvolutionTouchHeld = false;
    state_.progressionEvolutionTouchConsumed = false;
    state_.progressionMode = ProgressionViewModel::Mode::LEVEL_UP;
    damageRows(0, 448);
    return true;
}

void ProgressionFlow::beginEvolution(uint16_t fromSpeciesId, uint16_t toSpeciesId,
                                     uint32_t nowMs, Game::ItemId consumedItem) {
    state_.progressionFromSpeciesId = fromSpeciesId;
    state_.progressionToSpeciesId = toSpeciesId;
    state_.progressionEvolutionItem = consumedItem;
    state_.progressionEvolution.begin(fromSpeciesId, toSpeciesId, nowMs);
    state_.nextProgressionAnimationFrameMs = nowMs;
    state_.progressionEvolutionTouchStartedMs = 0;
    state_.progressionEvolutionTouchResumeUntilMs = 0;
    state_.progressionEvolutionTouchHeld = false;
    state_.progressionEvolutionTouchConsumed = false;
    effects_.preloadFrom = fromSpeciesId;
    effects_.preloadTo = toSpeciesId;
}

bool ProgressionFlow::openEvolution(const Game::GameState& gameState, AppSceneFlow::Scene returnScene,
                                   uint8_t teamSlot, uint16_t fromSpeciesId, uint16_t toSpeciesId,
                                   uint32_t nowMs, Game::ItemId consumedItem) {
    if (teamSlot >= gameState.teamCount || teamSlot >= Game::TEAM_CAP ||
        fromSpeciesId == 0 || toSpeciesId == 0 ||
        fromSpeciesId == toSpeciesId) return false;
    active_ = true;
    state_.progressionReturnScene = returnScene;
    state_.progressionTeamSlot = teamSlot;
    state_.progressionOldLevel = gameState.team[teamSlot].level;
    state_.progressionLevel = gameState.team[teamSlot].level;
    state_.progressionMoveId = 0;
    state_.progressionOldMove2 = 0;
    state_.progressionOldMove3 = 0;
    state_.progressionMoveCursor = 0;
    state_.progressionPressedItem = 0xFF;
    state_.progressionMode = ProgressionViewModel::Mode::EVOLUTION;
    beginEvolution(fromSpeciesId, toSpeciesId, nowMs, consumedItem);
    state_.progressionFromCare = false;
    damageRows(0, 448);
    return true;
}

bool ProgressionFlow::findNextMove(const Game::GameState& gameState, uint8_t teamSlot,
                                   uint8_t oldLevel, uint16_t& cursor,
                                   Game::MoveId& moveId) const {
    if (teamSlot >= gameState.teamCount || teamSlot >= Game::TEAM_CAP) {
        return false;
    }
    const Game::MonsterRuntime& monster = gameState.team[teamSlot];
    const Species* species = findSpecies(monster.speciesId);
    if (!species) return false;

    uint16_t count = learnsetEntryCountForSpecies(*species);
    while (cursor < count) {
        uint16_t index = cursor++;
        const LearnsetEntry* entry = learnsetEntryForSpecies(*species, index);
        if (!entry) continue;
        if (entry->level > monster.level) break;
        if (entry->level <= oldLevel ||
            !canLearnAsSpecialMove(*species, entry->moveId) ||
            entry->moveId == monster.move1Id ||
            entry->moveId == monster.move2Id ||
            entry->moveId == monster.move3Id) {
            continue;
        }
        moveId = entry->moveId;
        return true;
    }
    return false;
}

void ProgressionFlow::advance(Game::GameState& gameState, uint32_t nowMs) {
    if (!active_ || state_.progressionTeamSlot >= gameState.teamCount ||
        state_.progressionTeamSlot >= Game::TEAM_CAP) {
        return;
    }

    Game::MonsterRuntime& monster = gameState.team[state_.progressionTeamSlot];
    const Species* species = findSpecies(monster.speciesId);
    if (!species) {
        complete(gameState);
        return;
    }

    if (state_.progressionMode == ProgressionViewModel::Mode::LEVEL_UP) {
        gameState.pendingLevelUp = false;
        gameState.pendingLevelUpLevel = 0;
        const Species* target = levelUpEvolutionTarget(
            *species, monster, gameState.gameMinutesTotal);
        if (target) {
            state_.progressionMode = ProgressionViewModel::Mode::EVOLUTION;
            state_.progressionPressedItem = 0xFF;
            beginEvolution(monster.speciesId, target->id, nowMs);
            effects_.save = true;
            damageRows(0, 448);
            return;
        }
        state_.progressionMode = ProgressionViewModel::Mode::MOVE_LEARN;
        state_.progressionMoveCursor = 0;
    } else if (state_.progressionMode == ProgressionViewModel::Mode::EVOLUTION) {
        if (!state_.progressionEvolution.animationComplete(nowMs)) return;
        const Species* target = findSpecies(state_.progressionToSpeciesId);
        if (target && target->id != monster.speciesId) {
            if (state_.progressionEvolutionItem != Game::ItemId::COUNT &&
                !Game::ItemInventory::remove(
                    gameState, state_.progressionEvolutionItem, 1)) {
                effects_.noStock = true;
                return;
            }
            uint16_t oldHpMax = monster.hpMax;
            monster.speciesId = target->id;
            monster.hpMax = maxHpFor(*target, monster);
            if (monster.hpMax > oldHpMax) {
                monster.hpCur = static_cast<uint16_t>(std::min<uint32_t>(
                    monster.hpMax,
                    static_cast<uint32_t>(monster.hpCur) +
                        monster.hpMax - oldHpMax));
            } else {
                monster.hpCur = std::min(monster.hpCur, monster.hpMax);
            }
            if (!canRetainSpecialMove(*target, monster.move2Id,
                                       monster.level)) {
                monster.move2Id = 0;
                monster.moveProficiency[1] = 0;
            }
            if (!canRetainSpecialMove(*target, monster.move3Id,
                                       monster.level)) {
                monster.move3Id = 0;
                monster.moveProficiency[2] = 0;
            }
            state_.progressionFromSpeciesId = target->id;
            state_.progressionToSpeciesId = 0;
            state_.progressionMoveCursor = 0;
            effects_.syncTeam = true;
            effects_.save = true;

            const Species* nextTarget =
                state_.progressionEvolutionItem == Game::ItemId::COUNT
                    ? levelUpEvolutionTarget(
                          *target, monster, gameState.gameMinutesTotal) : nullptr;
            if (nextTarget && nextTarget->id != monster.speciesId) {
                state_.progressionPressedItem = 0xFF;
                beginEvolution(
                    monster.speciesId, nextTarget->id, nowMs);
                damageRows(0, 448);
                return;
            }
        }
        state_.progressionEvolution.reset();
        state_.progressionEvolutionItem = Game::ItemId::COUNT;
        state_.nextProgressionAnimationFrameMs = 0;
        state_.progressionMode = ProgressionViewModel::Mode::MOVE_LEARN;
    } else if (state_.progressionMode == ProgressionViewModel::Mode::MOVE_LEARN) {
        // The button is also the skip action when a move is not wanted.
    }

    Game::MoveId nextMove = 0;
    if (!findNextMove(gameState, state_.progressionTeamSlot, state_.progressionOldLevel,
                      state_.progressionMoveCursor, nextMove)) {
        complete(gameState);
        return;
    }
    state_.progressionMoveId = nextMove;
    if (monster.move2Id == 0) {
        monster.move2Id = nextMove;
        monster.moveProficiency[1] = 0;
        effects_.save = true;
        advance(gameState, nowMs);
        return;
    }
    if (monster.move3Id == 0) {
        monster.move3Id = nextMove;
        monster.moveProficiency[2] = 0;
        effects_.save = true;
        advance(gameState, nowMs);
        return;
    }
    state_.progressionOldMove2 = monster.move2Id;
    state_.progressionOldMove3 = monster.move3Id;
    state_.progressionMode = ProgressionViewModel::Mode::MOVE_REPLACE;
    state_.progressionPressedItem = 0xFF;
    state_.progressionSelectedItem = 0xFF;
    state_.progressionScroll = 0;
    state_.progressionDetailProgress = 0.0f;
    state_.progressionDetailAnimating = false;
    state_.progressionDetailTargetVisible = false;
    effects_.save = true;
    damageRows(0, 448);
}

void ProgressionFlow::complete(Game::GameState& gameState) {
    if (!active_) return;
    active_ = false;
    if (state_.progressionFromCare && state_.progressionTeamSlot < gameState.teamCount) {
        if (auto* record = Game::CareExperience::recordFor(
                gameState, gameState.team[state_.progressionTeamSlot])) {
            record->pendingOldLevel = 0;
        }
        state_.progressionFromCare = false;
    }
    gameState.pendingLevelUp = false;
    gameState.pendingLevelUpLevel = 0;
    gameState.pendingMoveLearn = false;
    gameState.pendingMoveSlot = 0;
    gameState.pendingMoveId = 0;
    gameState.pendingMoveCursor = 0;
    state_.progressionPressedItem = 0xFF;
    state_.progressionEvolution.reset();
    state_.progressionEvolutionItem = Game::ItemId::COUNT;
    state_.nextProgressionAnimationFrameMs = 0;
    state_.progressionEvolutionTouchStartedMs = 0;
    state_.progressionEvolutionTouchResumeUntilMs = 0;
    state_.progressionEvolutionTouchHeld = false;
    state_.progressionEvolutionTouchConsumed = false;
    effects_.save = true;
    effects_.completed = true;
}

void ProgressionFlow::tap(Game::GameState& gameState, int x, int y, uint32_t nowMs) {
    if (!active_) return;
    if (state_.progressionMode == ProgressionViewModel::Mode::MOVE_REPLACE) {
        if (x >= 300 && x < 356 && y >= 10 && y < 66) {
            if (state_.progressionSelectedItem >= 3) return;
            if (state_.progressionSelectedItem == 0) {
                state_.progressionMode = ProgressionViewModel::Mode::MOVE_LEARN;
            } else {
                Game::MonsterRuntime& monster =
                    gameState.team[state_.progressionTeamSlot];
                Game::MoveId& slot = state_.progressionSelectedItem == 1
                    ? monster.move2Id : monster.move3Id;
                slot = state_.progressionMoveId;
                if (state_.progressionSelectedItem == 1) {
                    monster.moveProficiency[1] = 0;
                } else {
                    monster.moveProficiency[2] = 0;
                }
                effects_.save = true;
            }
            state_.progressionPressedItem = 0xFF;
            advance(gameState, nowMs);
            return;
        }
        const int choice = progressionReplaceItemAt(
            x, y, state_.progressionScroll, state_.progressionSelectedItem,
            state_.progressionDetailProgress);
        if (choice >= 0) {
            if (state_.progressionSelectedItem == choice &&
                !state_.progressionDetailAnimating &&
                state_.progressionDetailProgress >= 0.99f) {
                state_.progressionDetailTargetVisible = false;
                state_.progressionDetailAnimStartMs = nowMs;
                state_.progressionDetailAnimating = true;
            } else if (state_.progressionSelectedItem != choice) {
                state_.progressionSelectedItem = static_cast<uint8_t>(choice);
                if (state_.progressionDetailProgress > 0.0f) {
                    state_.progressionDetailProgress = 1.0f;
                    state_.progressionDetailAnimating = false;
                    state_.progressionDetailTargetVisible = true;
                } else {
                    state_.progressionDetailTargetVisible = true;
                    state_.progressionDetailAnimStartMs = nowMs;
                    state_.progressionDetailAnimating = true;
                }
            }
            damageRows(0, 448);
        }
        return;
    }
    if (state_.progressionMode == ProgressionViewModel::Mode::EVOLUTION) {
        if (state_.progressionEvolution.cancelling()) {
            if (!state_.progressionEvolution.cancellationComplete(nowMs)) return;
            state_.progressionEvolution.reset();
            state_.progressionEvolutionItem = Game::ItemId::COUNT;
            state_.progressionToSpeciesId = 0;
            state_.progressionMoveCursor = 0;
            state_.progressionMode = ProgressionViewModel::Mode::MOVE_LEARN;
            advance(gameState, nowMs);
        } else if (state_.progressionEvolution.animationComplete(nowMs)) {
            advance(gameState, nowMs);
        } else cancelEvolution(nowMs);
        return;
    }
    if (state_.progressionMode == ProgressionViewModel::Mode::LEVEL_UP &&
        nowMs - state_.progressionLevelUpStartedMs < LEVEL_UP_ANIMATION_MS) return;
    if (progressionItemAt(x, y, state_.progressionMode) != 0) return;
    advance(gameState, nowMs);
}

void ProgressionFlow::beginTouch(int x, int y, uint32_t nowMs) {
    if (!active_) return;
    if (state_.progressionMode == ProgressionViewModel::Mode::EVOLUTION) {
        const bool cancellable = state_.progressionEvolution.initialized() &&
            !state_.progressionEvolution.cancelling() &&
            !state_.progressionEvolution.animationComplete(nowMs);
        const bool cancellationAnimating =
            state_.progressionEvolution.cancelling() &&
            !state_.progressionEvolution.cancellationComplete(
                nowMs);
        const bool resumeGesture =
            state_.progressionEvolutionTouchStartedMs != 0 &&
            state_.progressionEvolutionTouchResumeUntilMs != 0 &&
            static_cast<int32_t>(nowMs -
                                 state_.progressionEvolutionTouchResumeUntilMs) <= 0;
        state_.progressionEvolutionTouchHeld = cancellable;
        state_.progressionEvolutionTouchConsumed =
            resumeGesture || cancellable || cancellationAnimating;
        state_.progressionEvolutionTouchStartedMs = cancellable
            ? (resumeGesture ? state_.progressionEvolutionTouchStartedMs
                             : nowMs)
            : 0;
        state_.progressionEvolutionTouchResumeUntilMs = 0;
        if (cancellable) damageRows(0, 348);
    } else {
        if (state_.progressionMode == ProgressionViewModel::Mode::LEVEL_UP) {
            state_.progressionLevelUpTouchAllowed =
                nowMs - state_.progressionLevelUpStartedMs >=
                    LEVEL_UP_ANIMATION_MS;
        }
        const int hit = state_.progressionMode == ProgressionViewModel::Mode::MOVE_REPLACE
            ? progressionReplaceItemAt(x, y,
                  state_.progressionScroll, state_.progressionSelectedItem,
                  state_.progressionDetailProgress)
            : progressionItemAt(x, y, state_.progressionMode);
        state_.progressionPressedItem = hit < 0 ? 0xFF : static_cast<uint8_t>(hit);
        damageRows(MENU_HEADER_HEIGHT, 448);
    }
}

ProgressionFlow::Release ProgressionFlow::prepareRelease(uint32_t nowMs) {
    const bool evolutionTouchWasConsumed =
        state_.progressionMode == ProgressionViewModel::Mode::EVOLUTION &&
        state_.progressionEvolutionTouchConsumed;
    const bool evolutionCompletedAtRelease =
        evolutionTouchWasConsumed &&
        state_.progressionEvolution.animationComplete(nowMs);
    const bool evolutionHoldReached =
        evolutionTouchWasConsumed && state_.progressionEvolutionTouchHeld &&
        !evolutionCompletedAtRelease &&
        !state_.progressionEvolution.cancelling() &&
        nowMs - state_.progressionEvolutionTouchStartedMs >=
            EVOLUTION_CANCEL_HOLD_MS;
    if (evolutionHoldReached &&
        state_.progressionEvolution.beginCancellation(nowMs)) {
        effects_.stopCry = true;
        state_.nextProgressionAnimationFrameMs = nowMs;
        damageRows(0, 448);
    }
    const bool evolutionTouchCanResume =
        evolutionTouchWasConsumed && state_.progressionEvolutionTouchHeld &&
        !evolutionCompletedAtRelease && !evolutionHoldReached &&
        !state_.progressionEvolution.cancelling();
    state_.progressionEvolutionTouchHeld = false;
    return {evolutionTouchWasConsumed, evolutionTouchCanResume};
}

void ProgressionFlow::finishTouch(uint32_t nowMs, bool resume) {
    state_.progressionPressedItem = 0xFF;
    state_.progressionLevelUpTouchAllowed = false;
    state_.progressionEvolutionTouchConsumed = false;
    if (resume) {
        state_.progressionEvolutionTouchResumeUntilMs =
            nowMs + EVOLUTION_TOUCH_GAP_MS;
    } else {
        state_.progressionEvolutionTouchStartedMs = 0;
        state_.progressionEvolutionTouchResumeUntilMs = 0;
    }
}

void ProgressionFlow::update(uint32_t nowMs) {
    if (!active_) return;
    if (state_.progressionMode == ProgressionViewModel::Mode::EVOLUTION &&
        state_.progressionEvolution.initialized()) {
        if (state_.progressionEvolutionTouchHeld) {
            if (state_.progressionEvolution.animationComplete(nowMs)) {
                // Completion wins when it reaches its deadline before the
                // hold. The gesture remains consumed until the finger lifts.
                state_.progressionEvolutionTouchHeld = false;
                damageRows(330, 348);
            } else if (nowMs - state_.progressionEvolutionTouchStartedMs >=
                       EVOLUTION_CANCEL_HOLD_MS) {
                state_.progressionEvolutionTouchHeld = false;
                if (state_.progressionEvolution.beginCancellation(nowMs)) {
                    effects_.stopCry = true;
                    state_.nextProgressionAnimationFrameMs = nowMs;
                    damageRows(0, 448);
                }
            }
        }
        if (!state_.progressionEvolution.cancelling() &&
            !state_.progressionEvolution.cryPlayed() &&
            state_.progressionEvolution.elapsed(nowMs) >=
                Game::EvolutionSequence::REVEAL_END_MS) {
            state_.progressionEvolution.markCryPlayed();
            effects_.crySpecies = state_.progressionToSpeciesId;
        }
        const bool animating = state_.progressionEvolution.cancelling()
            ? !state_.progressionEvolution.cancellationComplete(nowMs)
            : !state_.progressionEvolution.animationComplete(nowMs);
        if (animating &&
            static_cast<int32_t>(nowMs - state_.nextProgressionAnimationFrameMs) >= 0) {
            state_.nextProgressionAnimationFrameMs =
                nowMs + PROGRESSION_ANIMATION_FRAME_MS;
            damageRows(0, 348);
        } else if (!animating && state_.nextProgressionAnimationFrameMs != 0) {
            state_.nextProgressionAnimationFrameMs = 0;
            damageRows(0, 448);
        }
    }
    if (state_.progressionMode == ProgressionViewModel::Mode::LEVEL_UP &&
        state_.nextProgressionAnimationFrameMs != 0 &&
        static_cast<int32_t>(nowMs - state_.nextProgressionAnimationFrameMs) >= 0) {
        if (nowMs - state_.progressionLevelUpStartedMs < LEVEL_UP_ANIMATION_MS) {
            state_.nextProgressionAnimationFrameMs =
                nowMs + PROGRESSION_ANIMATION_FRAME_MS;
        } else {
            state_.nextProgressionAnimationFrameMs = 0;
        }
        damageRows(0, 448);
    }
    if (state_.progressionMode == ProgressionViewModel::Mode::MOVE_REPLACE &&
        state_.progressionDetailAnimating) {
        const float raw = std::min(1.0f,
            static_cast<float>(nowMs - state_.progressionDetailAnimStartMs) /
            static_cast<float>(TEAM_MOVES_DETAIL_ANIM_MS));
        const float eased = state_.progressionDetailTargetVisible
            ? 1.0f - (1.0f - raw) * (1.0f - raw) : raw * raw;
        state_.progressionDetailProgress = state_.progressionDetailTargetVisible
            ? eased : 1.0f - eased;
        if (raw >= 1.0f) {
            state_.progressionDetailAnimating = false;
            state_.progressionDetailProgress = state_.progressionDetailTargetVisible ? 1.0f : 0.0f;
            if (!state_.progressionDetailTargetVisible) state_.progressionSelectedItem = 0xFF;
        }
        const int maxScroll = std::max(0,
            static_cast<int>(std::lround(
                UiMetrics::MOVE_DETAIL_HEIGHT * state_.progressionDetailProgress)) - 174);
        state_.progressionScroll = std::clamp<int16_t>(state_.progressionScroll, 0,
            static_cast<int16_t>(maxScroll));
        if (state_.progressionSelectedItem == 2 && state_.progressionDetailTargetVisible) {
            state_.progressionScroll = static_cast<int16_t>(maxScroll);
        }
        damageRows(0, 448);
    }
}

ProgressionViewModel ProgressionFlow::model(
        const Game::GameState& gameState, const char* toast, uint32_t nowMs) const {
    ProgressionViewModel model;
    model.state = &gameState;
    model.mode = state_.progressionMode;
    model.teamSlot = state_.progressionTeamSlot;
    model.oldLevel = state_.progressionOldLevel;
    model.level = state_.progressionLevel;
    model.fromSpeciesId = state_.progressionFromSpeciesId;
    model.toSpeciesId = state_.progressionToSpeciesId;
    model.levelUpElapsedMs = nowMs - state_.progressionLevelUpStartedMs;
    model.evolutionPhase = state_.progressionEvolution.phase(nowMs);
    model.evolutionElapsedMs = state_.progressionEvolution.cancelling()
        ? state_.progressionEvolution.cancellationElapsed(nowMs)
        : state_.progressionEvolution.elapsed(nowMs);
    model.evolutionReady =
        state_.progressionEvolution.animationComplete(nowMs);
    model.evolutionCancelHoldProgress = state_.progressionEvolutionTouchHeld
        ? static_cast<uint8_t>(std::clamp<uint32_t>(
              (nowMs - state_.progressionEvolutionTouchStartedMs) * 100U /
                  EVOLUTION_CANCEL_HOLD_MS, 1, 99))
        : 0;
    model.moveId = state_.progressionMoveId;
    model.oldMove2 = state_.progressionOldMove2;
    model.oldMove3 = state_.progressionOldMove3;
    model.pressedItem = state_.progressionPressedItem;
    model.selectedItem = state_.progressionSelectedItem;
    model.scrollOffsetY = state_.progressionScroll;
    model.detailProgress = state_.progressionDetailProgress;
    model.toast = toast;
    return model;
}

bool ProgressionFlow::cancelEvolution(uint32_t nowMs) {
    if (!active_ || state_.progressionMode != ProgressionViewModel::Mode::EVOLUTION ||
        !state_.progressionEvolution.beginCancellation(nowMs)) return false;
    effects_.stopCry = true;
    state_.progressionPressedItem = 0xFF;
    state_.nextProgressionAnimationFrameMs = nowMs;
    damageRows(0, 448);
    return true;
}

void ProgressionFlow::dragReplacement(int deltaY) {
    state_.progressionPressedItem = 0xFF;
    state_.progressionScroll -= static_cast<int16_t>(deltaY);
    const int maximum = state_.progressionSelectedItem < 3
        ? std::max(0, static_cast<int>(std::lround(
              UiMetrics::MOVE_DETAIL_HEIGHT * state_.progressionDetailProgress)) - 174)
        : 0;
    state_.progressionScroll = std::clamp<int16_t>(
        state_.progressionScroll, 0, static_cast<int16_t>(maximum));
    damageRows(TEAM_MOVES_HEADER_HEIGHT, 448);
}

void ProgressionFlow::cancelTouch() {
    state_.progressionPressedItem = 0xFF;
    state_.progressionLevelUpTouchAllowed = false;
    state_.progressionEvolutionTouchConsumed = state_.progressionEvolutionTouchHeld = false;
    state_.progressionEvolutionTouchStartedMs = 0;
    state_.progressionEvolutionTouchResumeUntilMs = 0;
}

void ProgressionFlow::damageRows(int top, int bottom) {
    effects_.rowBegin = std::min(effects_.rowBegin, top);
    effects_.rowEnd = std::max(effects_.rowEnd, bottom);
}

ProgressionFlow::Effects ProgressionFlow::takeEffects() {
    const Effects effects = effects_;
    effects_ = Effects{};
    return effects;
}

}  // namespace AmoledV1
