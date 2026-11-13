// SPDX-License-Identifier: Apache-2.0
#include "scenes/MenuScene.h"
#if STICKMON_UI_ROUND_TOUCH
#include <algorithm>
#include <cstdio>
#include "core/GameEngine.h"
#include "core/UiStrings.h"
#include "game/MarketItemCatalog.h"
#include "hardware/Hal.h"
#include "ui/SceneSurface.h"
#include "ui/round_touch/ItemFeedbackUi.h"

namespace {
Features::Bag::BattleResult battleResult(Game::ItemId item) {
    using Item = Game::ItemId;
    using Result = Features::Bag::BattleResult;
    if (Game::MarketItemCatalog::contains(item) && Game::ItemInventory::usableInBattle(item))
        return Result::MARKET_ITEM;
    switch (item) {
    case Item::POTION: return Result::POTION;
    case Item::SUPER_POTION: return Result::SUPER_POTION;
    case Item::ANTIDOTE: return Result::ANTIDOTE;
    case Item::PARALYZE_HEAL: return Result::PARALYZE_HEAL;
    case Item::AWAKENING: return Result::AWAKENING;
    case Item::BURN_HEAL: return Result::BURN_HEAL;
    case Item::ICE_HEAL: return Result::ICE_HEAL;
    case Item::MAX_POTION: return Result::MAX_POTION;
    case Item::FULL_RESTORE: return Result::FULL_RESTORE;
    case Item::FULL_HEAL: return Result::FULL_HEAL;
    case Item::REVIVE: return Result::REVIVE;
    default: return Result::NONE;
    }
}
const char* failureNotice(Game::ItemInventory::UseResult result) {
    using Result = Game::ItemInventory::UseResult;
    switch (result) {
    case Result::NO_STOCK: return Ui::Amoled::NO_STOCK;
    case Result::INVALID_TARGET: return Ui::Amoled::NO_TARGET;
    case Result::FAINTED: return Ui::Amoled::MON_FAINTED;
    case Result::HP_FULL: return Ui::Amoled::HP_FULL;
    case Result::STATUS_NORMAL: return Ui::Amoled::STATUS_NORMAL;
    case Result::NO_FAINTED_TARGET: return Ui::Amoled::NO_FAINTED_MON;
    default: return Ui::Amoled::NO_EFFECT;
    }
}
}

bool MenuScene::selectingTeamItem() const {
    return viewMode == ViewMode::TEAM && pendingTeamItem != Game::ItemId::COUNT;
}

void MenuScene::useTeamItemOn(uint8_t slot) {
    if (!selectingTeamItem() || itemFeedback || itemProgression != ItemProgression::NONE) return;
    auto& engine = GameEngine::ins();
    const auto& state = engine.gameState();
    if (slot >= state.teamCount || slot >= Game::TEAM_CAP) return;
    // Recheck context at execution, not only when opening the detail page.
    if (battleBagMode && !Game::ItemInventory::usableInBattle(pendingTeamItem)) {
        pageNotice(Ui::Amoled::CANNOT_USE);
        return;
    }
    const uint16_t hpBefore = state.team[slot].hpCur;
    const auto result = engine.useTeamItem(pendingTeamItem, slot);
    if (result != Game::ItemInventory::UseResult::USED) {
        pageNotice(failureNotice(result));
        return;
    }
    itemTargetSlot = slot;
    itemHpFrom = hpBefore;
    itemHpTo = state.team[slot].hpCur;
    itemFeedbackStarted = Hal::ins().millis();
    itemFeedback = true;
    cancelTouch();
    pageNotice(Ui::Amoled::ITEM_USED);
}

uint16_t MenuScene::teamItemHp(uint8_t slot) const {
    const auto& mon = pageState().team[slot];
    if (!itemFeedback || slot != itemTargetSlot) return mon.hpCur;
    const uint32_t elapsed = std::min<uint32_t>(500, Hal::ins().millis() - itemFeedbackStarted);
    const int32_t distance = int32_t(itemHpTo) - itemHpFrom;
    return uint16_t(int32_t(itemHpFrom) + distance * int32_t(elapsed) / 500);
}

void MenuScene::finishTeamItem() {
    const auto item = pendingTeamItem;
    pendingTeamItem = Game::ItemId::COUNT;
    itemFeedback = false;
    itemProgression = ItemProgression::NONE;
    toast = nullptr;
    cancelTouch();
    if (battleBagMode) {
        battleBagResult = battleResult(item);
        resetNavigation();
    } else {
        popView();
        bagUi.afterUse(pageState(), bagContext());
    }
}

void MenuScene::advanceItemProgression() {
    auto& engine = GameEngine::ins();
    const auto before = itemProgression;
    if (engine.hasPendingLevelUp()) itemProgression = ItemProgression::LEVEL_UP;
    else if (engine.hasPendingEvolution()) itemProgression = ItemProgression::EVOLUTION;
    else if (engine.hasPendingMoveReplacement()) itemProgression = ItemProgression::REPLACED;
    else if (engine.hasPendingMoveLearn()) itemProgression = ItemProgression::MOVE;
    else { finishTeamItem(); return; }
    if (itemProgression == ItemProgression::MOVE && before != itemProgression)
        ProgressionUi::resetMoveLearnState(itemMoveLearn);
    if (itemProgression == ItemProgression::EVOLUTION && before != itemProgression)
        ProgressionUi::resetEvolutionAnimation();
    cancelTouch();
    Ui::SceneSurface::mainMenu(false, true);
}

void MenuScene::renderItemProgression() {
    auto& engine = GameEngine::ins();
    const auto now = Hal::ins().millis();
    switch (itemProgression) {
    case ItemProgression::LEVEL_UP: {
        const auto& mon = engine.gameState().team[itemTargetSlot];
        Ui::ItemFeedback::renderLevelUp(mon, engine.speciesFor(mon).name, engine.pendingLevelUpLevel());
        break;
    }
    case ItemProgression::EVOLUTION: {
        ProgressionUi::renderEvolution(engine.pendingEvolutionFromSpeciesId(), engine.pendingEvolutionToSpeciesId(), now);
        break;
    }
    case ItemProgression::CANCELLED: {
        ProgressionUi::renderEvolutionCancelled(itemCancelledSpecies, now);
        break;
    }
    case ItemProgression::MOVE: ProgressionUi::renderMoveLearn(itemMoveLearn); break;
    case ItemProgression::REPLACED: ProgressionUi::renderMoveReplacement(); break;
    default: break;
    }
}

bool MenuScene::handleItemProgressionButton(const ButtonEvent& event) {
    if (event.action != BtnAction::PRESSED || event.btn > 1) return true;
    auto& engine = GameEngine::ins();
    const auto now = Hal::ins().millis();
    switch (itemProgression) {
    case ItemProgression::LEVEL_UP:
        if (event.btn == 0) { engine.acknowledgePendingLevelUp(); advanceItemProgression(); }
        break;
    case ItemProgression::EVOLUTION:
        if (event.btn == 1) {
            itemCancelledSpecies = engine.pendingEvolutionFromSpeciesId();
            const auto target = engine.pendingEvolutionToSpeciesId();
            if (engine.cancelPendingEvolution()) {
                ProgressionUi::beginEvolutionCancellation(itemCancelledSpecies, target, now);
                itemProgression = ItemProgression::CANCELLED;
                cancelTouch();
            }
        } else if (ProgressionUi::evolutionAnimationComplete(engine.pendingEvolutionFromSpeciesId(), engine.pendingEvolutionToSpeciesId(), now)) {
            engine.acknowledgePendingEvolution();
            ProgressionUi::resetEvolutionAnimation();
            advanceItemProgression();
        }
        break;
    case ItemProgression::CANCELLED:
        if (event.btn == 0 && ProgressionUi::evolutionCancellationComplete(now)) {
            ProgressionUi::resetEvolutionAnimation();
            advanceItemProgression();
        }
        break;
    case ItemProgression::MOVE:
        if (ProgressionUi::handleMoveLearnInput(itemMoveLearn, event.btn)) advanceItemProgression();
        break;
    case ItemProgression::REPLACED:
        if (event.btn == 0) { engine.acknowledgePendingMoveReplacement(); advanceItemProgression(); }
        break;
    default: break;
    }
    return true;
}

bool MenuScene::handleItemProgressionTouch(const Platform::IInputDevice::TouchEvent& event) {
    if (itemProgression == ItemProgression::MOVE) {
        if (ProgressionUi::handleMoveLearnTouch(itemMoveLearn, event)) advanceItemProgression();
    } else if (itemProgression == ItemProgression::LEVEL_UP) {
        if (ProgressionUi::handleLevelUpTouch(event)) handleItemProgressionButton({0, BtnAction::PRESSED});
    } else if (itemProgression == ItemProgression::REPLACED) {
        if (ProgressionUi::handleMoveReplacementTouch(event)) handleItemProgressionButton({0, BtnAction::PRESSED});
    } else {
        switch (ProgressionUi::handleEvolutionTouch(event, Hal::ins().millis())) {
        case Ui::EvolutionTouchAction::CONTINUE: handleItemProgressionButton({0, BtnAction::PRESSED}); break;
        case Ui::EvolutionTouchAction::CANCEL: handleItemProgressionButton({1, BtnAction::PRESSED}); break;
        default: break;
        }
    }
    return true;
}

void MenuScene::useDirectBagItem(Game::ItemId item) {
    auto& engine = GameEngine::ins();
    const int8_t foodIndex = Game::foodIndexForItemId(item);
    if (battleBagMode && foodIndex >= 0) {
        if (!engine.removeItem(item)) { pageNotice(Ui::Room::FOOD_NO_STOCK); return; }
        battleBagFoodIndex = foodIndex;
        battleBagResult = BattleBagResult::FOOD_THROWN;
        toast = nullptr;
        resetNavigation();
        return;
    }
    if (item != Game::ItemId::MAX_REPEL && item != Game::ItemId::HONEY) return;
    if (!exploreContextMode || battleBagMode) { pageNotice(Ui::Amoled::EXPLORE_ONLY); return; }
    const bool used = item == Game::ItemId::MAX_REPEL ? engine.useMaxRepel() : engine.useHoney();
    pageNotice(used ? Ui::Amoled::ITEM_USED : Ui::Amoled::ITEM_NOT_READY);
    if (used) bagUi.afterUse(pageState(), bagContext());
}
#endif
