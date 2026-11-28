#pragma once

#include <algorithm>
#include <cstdio>
#include <cstring>
#include "assets/GameAssets.h"
#include "assets/PokemonSprites.h"
#include "core/TouchTap.h"
#include "core/UiStrings.h"
#include "game/BattleSystem.h"
#include "game/MarketTutorService.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TextBlock.h"
#include "ui/common/MarketStallMotion.h"
#include "ui/common/MarketUiFeedback.h"
#include "ui/common/MarketCoinHud.h"
#include "ui/common/MarketListScroll.h"
#include "ui/common/MarketTutorPerformance.h"
#include "ui/common/MarketDialogueUi.h"
#include "ui/common/MarketView.h"
#include "ui/round_touch/MarketStallUi.h"

// Native360 round Gengar pages; shared services own all gameplay mutations. The controller
// owns page state and hit geometry; the application owns saving. A request is
// returned from touch(), applied to a GameState copy and confirmed through
// committed(), so a failed save never shows success or keeps a charge. The
// menu closes by tapping the scene above it; sub-pages return one level there.
namespace Ui {
class RoundMarketTutorUi {
public:
    enum class Page : uint8_t { CLOSED, GREET, MENU, TUTOR, SLOT, CONFIRM, RESULT,
                                TRAIN_MEMBER, TRAIN_MOVE, RECALL_MEMBER, RECALL_LIST, TRAINING, TEACHING, ERROR };
    enum class Mode : uint8_t { TEACH, TRAIN, RECALL };
    struct Request {
        enum Kind : uint8_t { NONE, CLOSE, TEACH, TRAIN, RECALL } kind = NONE;
        uint8_t member = 0, slot = 0;
        Game::MoveId move = 0;
        uint32_t period=0,iv=0,metAt=0;
    };
    using Rect = RoundMarketStall::Rect;

    static constexpr int W = 360, H = 360, PANEL_Y = 132, ROW_Y = 140, ROW_H = 56, GREET_H = MarketDialogueUi::ROUND_HEIGHT;
    static constexpr int MENU_Y = PANEL_Y, MENU_ROW_H = 64;
    static constexpr uint32_t TRAIN_MS = MarketTutorPerformance::TRAINING_MS;
    static constexpr uint32_t TEACH_MS = MarketTutorPerformance::LEARNING_MS, TRAIN_SKIP_MS = 400;
    static constexpr uint32_t GREET_IN_MS = 360, OPEN_MS = 420, CLOSE_MS = 520;

    bool takeSfx(SfxCue& cue) { return feedback_.takeSfx(cue); }
    bool active() const { return page_ != Page::CLOSED; }
    Page page() const { return page_; }
    Mode mode() const { return mode_; }
    bool pending() const { return pending_; }
    uint8_t member() const { return member_; }
    int scrollOffset() const { return scroll_; }
    bool animating() const { return transition_ != Transition::NONE; }
    int panelTop() const { return page_ == Page::GREET ? H - GREET_H + panelOffset() : basePanelTop() + panelOffset(); }
    int hudOffset() const {
        if (page_ == Page::GREET) return MarketStallMotion::HUD_HIDDEN;
        if (transition_ == Transition::OPENING && page_ != Page::ERROR) return tween(MarketStallMotion::HUD_HIDDEN,0);
        return transition_==Transition::CLOSING?tween(0,MarketStallMotion::HUD_HIDDEN):0;
    }

    void open(const Game::GameState& state, const char* welcome = nullptr, bool initialized = true) {
        *this = RoundMarketTutorUi{};
        period_=state.marketTutor.periodDay;
        page_ = initialized ? Page::GREET : Page::ERROR;
        transition_ = Transition::GREET_IN;
        if (!initialized) return;
        const auto& t = state.marketTutor;
        if (welcome) greeting_.start(welcome,MarketTutorText::SPEAKER);
        else if (t.move && !t.taught) {
            char greeting[96];
            const MoveInfo* move = findMove(t.move);
            std::snprintf(greeting, sizeof(greeting), MarketTutorText::GREET_CLASS, move ? move->name : "");
            greeting_.start(greeting,MarketTutorText::SPEAKER);
        } else {
            greeting_.start(t.move ? MarketTutorText::GREET_TAUGHT : MarketTutorText::GREET_CLOSED,MarketTutorText::SPEAKER);
        }
    }
    void close() { feedback_.clear(); page_ = Page::CLOSED; transition_ = Transition::NONE; cancelTouch(); pending_ = false; }
    void cancelTouch() { tap_.cancel(); scrollMotion_.cancel(); confirmScrollMotion_.cancel(); }

    bool update(uint32_t deltaMs) {
        if (!active() || !deltaMs) return false;
        deltaMs = std::min<uint32_t>(deltaMs, 250);
        bool changed = false;
        if (errorMs_) {
            errorMs_ = deltaMs >= errorMs_ ? 0 : errorMs_ - deltaMs;
            if (!errorMs_) { error_ = nullptr; changed = true; }
        }
        if (animating()) {
            transitionMs_ = std::min(duration(), transitionMs_ + deltaMs);
            if (transitionMs_ == duration()) {
                if (transition_ == Transition::CLOSING) close();
                else transition_ = Transition::NONE;
            }
            return true;
        }
        if (page_ == Page::GREET) return greeting_.update(deltaMs) || changed;
        if (performing()) {
            const uint32_t duration = page_ == Page::TEACHING ? TEACH_MS : TRAIN_MS;
            showMs_ = std::min(duration, showMs_ + deltaMs);
            if (showMs_ == duration) finishPerformance();
            return true;
        }
        if (page_ == Page::RECALL_LIST)
            changed = scrollMotion_.update(deltaMs, scrollMaximum_, scroll_) || changed;
        else scrollMotion_.cancel();
        if (page_ == Page::CONFIRM && mode_ != Mode::TRAIN)
            changed = confirmScrollMotion_.update(deltaMs, confirmScrollMaximum(), confirmScroll_) || changed;
        else confirmScrollMotion_.cancel();
        return changed;
    }

    // Move the camera with the service panel.
    int sceneShift(const Market::Session& s, const MarketView::Viewport& v) const {
        if (page_ == Page::GREET) return 0;
        const int shift=RoundMarketStall::sceneShift(s,v);
        if(transition_==Transition::OPENING)return tween(0,shift);
        if(transition_==Transition::CLOSING)return tween(shift,0);
        return shift;
    }
    int sceneShiftX(const Market::Session& s,const MarketView::Viewport& v) const {
        if (page_ == Page::GREET) return 0;
        const int shift=RoundMarketStall::sceneShiftX(s,v);
        if(transition_==Transition::OPENING)return tween(0,shift);
        if(transition_==Transition::CLOSING)return tween(shift,0);
        return shift;
    }

    Request touch(const Platform::IInputDevice::TouchEvent& e, const Game::GameState& state, MarketCoinMotion* coins = nullptr) {
        using Type = Platform::IInputDevice::TouchType;
        Request none{};
        if ((int(e.x)-180)*(int(e.x)-180)+(int(e.y)-180)*(int(e.y)-180)>=180*180) {cancelTouch();return none;}
        if (pending_ || page_ == Page::CLOSED || animating()) { cancelTouch(); return none; }
        if (page_ != Page::GREET && MarketCoinHud::inspect(e,coins,16+hudOffset(),32,MarketCoinHud::ROUND)) return none;
        if (performing()) {
            if (showMs_ < TRAIN_SKIP_MS) { tap_.cancel(); return none; }
            if (tap_.update(e, 0) == 0) finishPerformance();
            return none;
        }
        if (page_ == Page::GREET) {
            if (greeting_.touch(e,tap_)) { feedback_.set(SfxCue::UI_CONFIRM); openPanel(); }
            return none;
        }
        if (page_ == Page::RECALL_LIST && listDrag(e, state)) return none;
        if (page_ == Page::CONFIRM && mode_ != Mode::TRAIN) {
            if (prepareConfirmBody(state)) { confirmScroll_ = 0; confirmScrollMotion_.cancel(); }
            const auto r = confirmBodyRect();
            if (confirmScrollMotion_.touch(e, {r.x, r.y, r.w, r.h}, confirmScrollMaximum(),
                                           confirmScroll_, tap_)) return none;
        }
        const int target = e.type == Type::DOWN || e.type == Type::UP ? hit(e.x, e.y, state) : -1;
        const int clicked = tap_.update(e, target);
        if (clicked < 0) return none;
        if (clicked == OUTSIDE) {
            const int backTarget = (page_ == Page::MENU || page_ == Page::ERROR) ? CLOSE : page_ == Page::CONFIRM ? BACK :
                page_ == Page::RESULT ? PRIMARY : page_ == Page::TRAIN_MOVE ? BACK : FOOTER;
            return activate(backTarget, state);
        }
        const Page previous = page_;
        const auto request = activate(clicked, state);
        if (page_ == Page::CONFIRM && previous != Page::CONFIRM && mode_ != Mode::TRAIN) {
            confirmScroll_ = 0; confirmScrollMotion_.cancel(); prepareConfirmBody(state);
        }
        return request;
    }

    // Result of the application's save for the last request.
    void committed(bool saved, const Game::GameState& state) {
        pending_ = false;
        if (!saved) { feedback_.set(SfxCue::UI_CANCEL); error_ = MarketTutorText::SAVE_FAILED; return; }
        error_ = nullptr;
        const auto& m = state.team[member_];
        const MoveInfo* move = findMove(chosenMove(state));
        const char* who = speciesName(m);
        if (mode_ == Mode::TEACH) {
            std::snprintf(result1_, sizeof(result1_), MarketTutorText::TEACH_DONE, who, move ? move->name : "");
            std::snprintf(result2_, sizeof(result2_), "%s", MarketTutorText::TEACH_DONE2);
        } else if (mode_ == Mode::TRAIN) {
            const MoveInfo* trained = findMove(slotMove(m, slot_));
            const auto after = m.moveProficiency[slot_];
            const char* message = before_ < 90 && after >= 90 ? MarketTutorText::TRAIN_MASTERED :
                before_ < 60 && after >= 60 ? MarketTutorText::TRAIN_SKILLED : MarketTutorText::TRAIN_DONE;
            std::snprintf(result1_, sizeof(result1_), message, who, trained ? trained->name : "");
            result2_[0] = 0;
        } else {
            std::snprintf(result1_, sizeof(result1_), MarketTutorText::RECALL_DONE, who, move ? move->name : "");
            result2_[0] = 0;
        }
        page_ = mode_ == Mode::TEACH ? Page::TEACHING : mode_ == Mode::TRAIN ? Page::TRAINING : Page::RESULT;
        showMs_ = 0; tap_.cancel();
        if (mode_ == Mode::RECALL) feedback_.set(SfxCue::MOVE_LEARNT);
    }
    void rejected(Game::MarketTutor::Result r) { pending_ = false; feedback_.set(SfxCue::UI_CANCEL); error_ = reason(r); }

    void render(const Game::GameState& state, const MarketCoinMotion* coins = nullptr) const {
        if (page_ == Page::CLOSED) return;
        if (page_ == Page::GREET) { greeting_.render(panelOffset()); return; }
        panel(basePanelTop());
        switch (page_) {
        case Page::ERROR: body(MarketTutorText::SAVE_FAILED,{68,204,224,54},true,red());button(wide(),"离开",true);break;
        case Page::MENU: renderMenu(state); break;
        case Page::TUTOR: renderTutor(state); break;
        case Page::SLOT: renderSlot(state); break;
        case Page::CONFIRM: renderConfirm(state); break;
        case Page::RESULT: renderResult(state); break;
        case Page::TRAIN_MEMBER: renderMembers(state, true); break;
        case Page::TRAIN_MOVE: renderTrainMoves(state); break;
        case Page::TRAINING: case Page::TEACHING: renderPerformance(state); break;
        case Page::RECALL_MEMBER: renderMembers(state, false); break;
        case Page::RECALL_LIST: renderRecallList(state); break;
        default: break;
        }
        if (page_ == Page::MENU && error_) body(error_,{100,326,160,18},true,red(),16);
        renderHud(state,coins);
    }

private:
    Page page_ = Page::CLOSED;
    enum class Transition : uint8_t { NONE, GREET_IN, OPENING, CLOSING };
    Transition transition_ = Transition::NONE;
    uint32_t period_=0;
    uint32_t transitionMs_ = 0, errorMs_ = 0, showMs_ = 0;
    Mode mode_ = Mode::TEACH;
    TouchTap tap_;
    MarketUiFeedback feedback_;
    uint8_t member_ = 0, slot_ = 1, before_ = 0;
    Game::MoveId recallMove_ = 0;
    int scroll_ = 0, scrollMaximum_ = 0;
    MarketListScroll scrollMotion_;
    mutable TextBlock bodyText_,confirmBody_;
    MarketListScroll confirmScrollMotion_;
    int confirmScroll_ = 0;
    mutable int confirmTextSize_=16;
    mutable bool confirmMessage_=false;
    bool pending_ = false;
    const char* error_ = nullptr;
    MarketDialogueUi::RoundGreeting greeting_;
    char result1_[96]{}, result2_[64]{};

    enum Target { ROW0 = 0, ROW1, ROW2, FOOTER = 10, BACK, PRIMARY, CLOSE, RECALL_ROW = 20, OUTSIDE = 1000, DISABLED };

    void finishPerformance() {
        page_ = Page::RESULT; tap_.cancel();
        feedback_.set(mode_ == Mode::TRAIN ? SfxCue::UI_CONFIRM : SfxCue::MOVE_LEARNT);
    }

    bool performing() const { return page_ == Page::TRAINING || page_ == Page::TEACHING; }

    uint32_t duration() const {
        return transition_ == Transition::GREET_IN ? GREET_IN_MS : transition_ == Transition::CLOSING ? CLOSE_MS : OPEN_MS;
    }
    // One progress curve keeps panel, camera and badge in step.
    int tween(int from, int to) const {
        return MarketStallMotion::tween(from, to, transitionMs_, duration());
    }
    // The greeting box rises from the bottom edge, then its top edge grows
    // into the panel top; closing sinks the whole panel away.
    int basePanelTop() const { return PANEL_Y; }
    int panelOffset() const {
        if (transition_ == Transition::GREET_IN) return tween(GREET_H, 0);
        if (transition_ == Transition::OPENING) return tween(H - GREET_H - basePanelTop(), 0);
        if (transition_ == Transition::CLOSING) return tween(0, H - basePanelTop());
        return 0;
    }
    void openPanel() {
        page_ = Page::MENU; transition_ = Transition::OPENING; transitionMs_ = 0;
        tap_.cancel();
    }

    // ---- geometry shared by hit testing and drawing
    static Rect row(int i, int top = ROW_Y) { return {60, top + i * ROW_H, 240, ROW_H}; }
    Rect moveRow(int i) const { return page_==Page::TRAIN_MOVE?Rect{68,184+i*48,224,48}:Rect{68,188+i*60,224,60}; }
    static Rect menuRow(int i) { return {58, ROW_Y + i * MENU_ROW_H, 244, MENU_ROW_H}; }
    static Rect wide() { return RoundMarketStall::ACTION; }
    static constexpr int TUTOR_MEMBER_TOP = 216, LIST_TOP = 140, LIST_BOTTOM = 360;

    Rect tutorMember(int i, int count) const {
        return {count == 1 ? 128 : 68 + i * 120, page_==Page::TUTOR?TUTOR_MEMBER_TOP:204, 104, 84};
    }

    static const char* speciesName(const Game::MonsterRuntime& m) {
        const Species* s = findSpecies(m.speciesId);
        return s ? s->name : "";
    }
    static Game::MoveId slotMove(const Game::MonsterRuntime& m, uint8_t slot) {
        return slot == 0 ? m.move1Id : slot == 1 ? m.move2Id : slot == 2 ? m.move3Id : 0;
    }
    Game::MoveId chosenMove(const Game::GameState& state) const {
        return mode_ == Mode::TEACH ? state.marketTutor.move : mode_ == Mode::RECALL ? recallMove_ :
            slotMove(state.team[member_], slot_);
    }
    static uint8_t members(const Game::GameState& s) { return std::min<uint8_t>(s.teamCount, Game::TEAM_CAP); }

    static const char* reason(Game::MarketTutor::Result r) {
        using R = Game::MarketTutor::Result;
        switch (r) {
        case R::NO_CLASS: return MarketTutorText::WHY_PERIOD;
        case R::ALREADY_TAUGHT: return MarketTutorText::WHY_TAUGHT;
        case R::ALREADY_TRAINED: case R::QUOTA_FULL: return MarketTutorText::WHY_TRAINED;
        case R::FAINTED: return MarketTutorText::WHY_FAINTED_SHORT;
        case R::HUNGRY: return MarketTutorText::WHY_HUNGRY_SHORT;
        case R::MAXED: return MarketTutorText::TRAIN_MAXED;
        case R::NO_HEART_SCALE: return MarketTutorText::SUB_NO_SCALE;
        default: return MarketTutorText::WHY_GENERIC;
        }
    }

    uint8_t recallList(const Game::GameState& s, Game::MoveId* out) const {
        const Species* sp = findSpecies(s.team[member_].speciesId);
        return sp ? Game::MoveManagementService::collectRecallable(*sp, s.team[member_], out,
                                                                  Game::MoveManagementService::MAX_RECALLABLE_MOVE_COUNT) : 0;
    }
    int maxScroll(const Game::GameState& s) const {
        Game::MoveId moves[Game::MoveManagementService::MAX_RECALLABLE_MOVE_COUNT];
        return std::max(0, recallList(s, moves) * ROW_H + ROW_H - (LIST_BOTTOM - LIST_TOP));
    }
    bool listDrag(const Platform::IInputDevice::TouchEvent& e, const Game::GameState& s) {
        scrollMaximum_ = maxScroll(s);
        return scrollMotion_.touch(e, {54, LIST_TOP, 252, LIST_BOTTOM - LIST_TOP},
                                   scrollMaximum_, scroll_, tap_);
    }

    // ---- availability shared by hit testing and drawing
    Game::MarketTutor::Result slotState(const Game::GameState& s, uint8_t slot) const {
        if (mode_ == Mode::TEACH) {
            auto copy = s.marketTutor; (void)copy;
            return Game::MarketTutor::checkTeach(s, member_, slot);
        }
        return Game::MarketTutor::Result::OK;
    }
    bool confirmDisabled(const Game::GameState& s, const char** why, unsigned long* shortfall) const {
        using R = Game::MarketTutor::Result;
        R r = R::OK;
        if (mode_ == Mode::TEACH) r = Game::MarketTutor::checkTeach(s, member_, slot_);
        else if (mode_ == Mode::TRAIN) r = Game::MarketTutor::checkTrain(s, member_, slot_);
        else if (!Game::ItemInventory::count(s, Game::ItemId::HEART_SCALE)) r = R::NO_HEART_SCALE;
        if (r == R::OK) { *why = nullptr; return false; }
        if (r == R::NO_COINS) {
            const unsigned long price = mode_ == Mode::TEACH ? Game::MarketTutor::lessonPrice(s.marketTutor.move) :
                Game::MarketTutor::trainStep(s.team[member_].moveProficiency[slot_]).cost;
            *shortfall = price > s.coins ? price - s.coins : 0;
            *why = MarketTutorText::NO_COINS;
        } else *why = reason(r);
        return true;
    }

    int hit(int x, int y, const Game::GameState& s) const {
        // Keep outside taps distinct from rows/buttons at both touch edges.
        if (!RoundMarketStall::PANEL.contains(x,y)) return OUTSIDE;
        const auto contains = [&](Rect r) { return r.contains(x, y); };
        switch (page_) {
        case Page::ERROR: return contains(wide()) ? CLOSE : -1;
        case Page::MENU:
            for (int i = 0; i < 3; ++i) if (contains(menuRow(i))) return i;
            return -1;
        case Page::TUTOR:
            for (int i = 0; i < members(s); ++i) {
                if (!contains(tutorMember(i, members(s)))) continue;
                return Game::MarketTutor::eligibility(s.team[i], s.marketTutor.move) ==
                    Game::MarketTutor::Eligibility::CAN_LEARN && !s.marketTutor.taught ? i : DISABLED;
            }
            return -1;
        case Page::SLOT:
            for (int i = 1; i < 3; ++i) if (contains(moveRow(i - 1))) return i;
            return -1;
        case Page::CONFIRM: {
            const char* why = nullptr; unsigned long need = 0;
            return contains(wide()) ? (!confirmDisabled(s, &why, &need) ? PRIMARY : DISABLED) : -1;
        }
        case Page::RESULT: return contains(wide()) ? PRIMARY : -1;
        case Page::TRAIN_MEMBER: case Page::RECALL_MEMBER:
            for (int i = 0; i < members(s); ++i) if (contains(tutorMember(i, members(s)))) return memberEnabled(s, i) ? i : DISABLED;
            return -1;
        case Page::TRAIN_MOVE:
            for (int i = 0; i < 2; ++i) if (contains(moveRow(i))) return trainable(s, uint8_t(i + 1)) ? i : DISABLED;
            { const char* why = nullptr; unsigned long need = 0;
              return contains(wide()) ? (trainable(s, slot_) && !confirmDisabled(s, &why, &need) ? PRIMARY : DISABLED) : -1; }
        case Page::RECALL_LIST: {
            if (y < LIST_TOP || y >= LIST_BOTTOM || x < 54 || x >= 306) return -1;
            Game::MoveId moves[Game::MoveManagementService::MAX_RECALLABLE_MOVE_COUNT];
            const int index = (y - LIST_TOP + scroll_) / ROW_H;
            return index < recallList(s, moves) ? RECALL_ROW + index : -1;
        }
        default: return -1;
        }
    }
    bool menuEnabled(const Game::GameState& s, int i) const {
        if (i == 0) return s.marketTutor.move != 0 && !s.marketTutor.taught;
        if (i == 1) return Game::ItemInventory::count(s, Game::ItemId::HEART_SCALE) > 0;
        return Game::MarketTutor::trainableMembers(s) > 0;
    }
    bool memberEnabled(const Game::GameState& s, int i) const {
        const auto& m = s.team[i];
        if (m.origin == Game::Origin::VISITOR) return false;
        if (page_ == Page::TRAIN_MEMBER) return !Game::MarketTutor::trainedThisPeriod(s, m) && (m.move2Id || m.move3Id);
        Game::MoveId moves[Game::MoveManagementService::MAX_RECALLABLE_MOVE_COUNT];
        const Species* sp = findSpecies(m.speciesId);
        return sp && Game::MoveManagementService::collectRecallable(*sp, m, moves, Game::MoveManagementService::MAX_RECALLABLE_MOVE_COUNT) > 0;
    }
    bool trainable(const Game::GameState& s, uint8_t slot) const {
        const auto& m = s.team[member_];
        return slotMove(m, slot) && m.moveProficiency[slot] < Game::MOVE_PROFICIENCY_MAX;
    }

    Request activate(int target, const Game::GameState& s) {
        Request r{};r.period=period_;
        if(member_<s.teamCount){r.iv=s.team[member_].ivPacked;r.metAt=s.team[member_].metAt;}
        scrollMotion_.cancel();
        feedback_.set(target == FOOTER || target == BACK || target == CLOSE || page_ == Page::RESULT ?
                      SfxCue::UI_CANCEL : SfxCue::UI_CONFIRM);
        if (target == DISABLED) { feedback_.set(SfxCue::UI_CANCEL); return r; }
        error_ = nullptr; errorMs_ = 0;
        switch (page_) {
        case Page::ERROR: transition_=Transition::CLOSING;transitionMs_=0;tap_.cancel();return r;
        case Page::MENU:
            if (target == CLOSE) {
                transition_ = Transition::CLOSING; transitionMs_ = 0; tap_.cancel(); return r;
            }
            if (!menuEnabled(s, target)) {
                error_ = target == 0 ? (s.marketTutor.taught ? MarketTutorText::WHY_TAUGHT : MarketTutorText::SUB_CLOSED) :
                    target == 1 ? MarketTutorText::NEED_SCALE : MarketTutorText::SUB_TRAIN_DONE;
                feedback_.set(SfxCue::UI_CANCEL); errorMs_ = 1800;
                return r;
            }
            if (target == 0) { mode_ = Mode::TEACH; page_ = Page::TUTOR; }
            if (target == 1) { mode_ = Mode::RECALL; page_ = Page::RECALL_MEMBER; }
            if (target == 2) { mode_ = Mode::TRAIN; page_ = Page::TRAIN_MEMBER; }
            return r;
        case Page::TUTOR:
            if (target == FOOTER) { page_ = Page::MENU; return r; }
            member_ = uint8_t(target); page_ = Page::SLOT; return r;
        case Page::SLOT:
            if (target == FOOTER) { page_ = mode_ == Mode::TEACH ? Page::TUTOR : Page::RECALL_LIST; return r; }
            slot_ = uint8_t(target); before_ = s.team[member_].moveProficiency[slot_]; page_ = Page::CONFIRM; return r;
        case Page::CONFIRM:
            if (target == BACK) { page_ = mode_ == Mode::TRAIN ? Page::TRAIN_MOVE : Page::SLOT; return r; }
            feedback_.clear(); pending_ = true;
            r.kind = mode_ == Mode::TEACH ? Request::TEACH : mode_ == Mode::TRAIN ? Request::TRAIN : Request::RECALL;
            r.member = member_; r.slot = slot_; r.move = chosenMove(s);
            return r;
        case Page::RESULT:
            transition_ = Transition::CLOSING; transitionMs_ = 0;
            tap_.cancel(); scroll_ = 0; return r;
        case Page::TRAIN_MEMBER: case Page::RECALL_MEMBER:
            if (target == FOOTER) { page_ = Page::MENU; return r; }
            member_ = uint8_t(target); scroll_ = 0;
            page_ = page_ == Page::TRAIN_MEMBER ? Page::TRAIN_MOVE : Page::RECALL_LIST;
            slot_ = trainable(s, 1) ? 1 : 2;
            return r;
        case Page::TRAIN_MOVE:
            if (target == BACK || target == FOOTER) { page_ = Page::TRAIN_MEMBER; return r; }
            if (target == PRIMARY) {
                before_ = s.team[member_].moveProficiency[slot_]; feedback_.clear(); pending_ = true;
                r.kind = Request::TRAIN; r.member = member_; r.slot = slot_; r.move = chosenMove(s);
                tap_.cancel(); return r;
            }
            slot_ = uint8_t(target + 1); return r;
        case Page::RECALL_LIST: {
            if (target == FOOTER) { page_ = Page::RECALL_MEMBER; return r; }
            Game::MoveId moves[Game::MoveManagementService::MAX_RECALLABLE_MOVE_COUNT];
            const uint8_t n = recallList(s, moves);
            if (target - RECALL_ROW < n) { recallMove_ = moves[target - RECALL_ROW]; page_ = Page::SLOT; }
            return r;
        }
        default: return r;
        }
    }

    // ---- drawing helpers
    static uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return PixelRenderer::rgb(r, g, b); }
    static uint16_t paper() { return rgb(248, 248, 224); }
    static uint16_t ink() { return rgb(40, 56, 80); }
    static uint16_t edge() { return rgb(48, 64, 112); }
    static uint16_t light() { return rgb(128, 152, 184); }
    static uint16_t green() { return rgb(221, 232, 192); }
    static uint16_t muted() { return rgb(89, 105, 88); }
    static uint16_t line() { return rgb(186, 196, 160); }
    static uint16_t red() { return rgb(183, 56, 56); }
    static uint16_t off() { return rgb(225, 227, 207); }

    static int textWidth(const char* s,int size) {return RoundMarketStall::width(s,size);}
    void text(int x,int y,const char* s,int size=32,uint16_t color=0) const {RoundMarketStall::text(x,y+panelOffset(),s,size,color);}
    void rtext(int x,int y,const char* s,int size=32,uint16_t color=0) const {text(x-textWidth(s,size),y,s,size,color);}
    void ctext(int y,const char* s,int size=32,uint16_t color=0) const {text((W-textWidth(s,size))/2,y,s,size,color);}
    void renderHud(const Game::GameState& s,const MarketCoinMotion* motion) const {RoundMarketStall::hud(s.coins,motion,hudOffset());}
    void panel(int) const {RoundMarketStall::panel(panelOffset());}
    void button(Rect r,const char* label,bool primary,bool disabled=false) const {RoundMarketStall::button(r,label,primary,disabled,panelOffset());}
    void rowFrame(Rect r,bool selected) const {
        r.y+=panelOffset();auto& c=PixelRenderer::canvas();
        if(selected)c.fillRect(58,r.y,244,r.h,green());
        c.drawFastHLine(68,r.y+r.h-1,224,line());
    }
    void rowText(Rect r,int x,const char* main,const char*,const char* right,bool disabled,uint16_t=0,int rightSize=16) const {
        const int available=292-x-(right&&*right?textWidth(right,rightSize)+12:0);
        const int size=textWidth(main,32)<=available?32:textWidth(main,24)<=available?24:20;
        text(x,r.y+(r.h-size)/2,main,size,disabled?muted():ink());
        if(right&&*right)rtext(292,r.y+(r.h-rightSize)/2,right,rightSize,disabled?muted():ink());
    }
    // Type-tinted disc stands in for a TM until a bundled icon is added.
    void discIcon(int x, int y, TypeId type, int size = 48) const {
        y += panelOffset();
        auto& c = PixelRenderer::canvas();
        const int cx = x + size / 2, cy = y + size / 2, r = size / 2 - 1;
        const uint16_t base = MovesDrawingColor(type);
        c.fillCircle(cx, cy, r, rgb(56, 40, 88));
        c.fillCircle(cx, cy, r - 3, base);
        c.drawCircle(cx - r / 3, cy - r / 3, r / 3, rgb(240, 232, 255));
        c.fillCircle(cx, cy, r / 4, rgb(56, 40, 88));
        c.fillCircle(cx, cy, r / 8, paper());
    }
    static uint16_t MovesDrawingColor(TypeId type) {
        switch (type) {
        case TypeId::FIRE: return rgb(239, 91, 67); case TypeId::WATER: return rgb(67, 145, 224);
        case TypeId::GRASS: return rgb(96, 186, 80); case TypeId::ELECTRIC: return rgb(240, 200, 48);
        case TypeId::ICE: return rgb(112, 208, 224); case TypeId::FIGHTING: return rgb(200, 72, 56);
        case TypeId::POISON: return rgb(168, 80, 176); case TypeId::GROUND: return rgb(208, 168, 88);
        case TypeId::FLYING: return rgb(144, 160, 232); case TypeId::PSYCHIC: return rgb(240, 96, 144);
        case TypeId::BUG: return rgb(160, 184, 48); case TypeId::ROCK: return rgb(184, 160, 72);
        case TypeId::GHOST: return rgb(138, 92, 200); case TypeId::DRAGON: return rgb(104, 80, 224);
        case TypeId::DARK: return rgb(104, 88, 80); case TypeId::STEEL: return rgb(168, 176, 192);
        case TypeId::FAIRY: return rgb(232, 144, 200); default: return rgb(168, 168, 136);
        }
    }
    void trainIcon(int x, int y) const {
        y += panelOffset();
        auto& c = PixelRenderer::canvas();
        c.fillTriangle(x + 24, y + 3, x + 6, y + 21, x + 42, y + 21, edge());
        c.fillTriangle(x + 24, y + 8, x + 12, y + 19, x + 36, y + 19, rgb(240, 168, 48));
        c.fillRect(x + 15, y + 19, 18, 15, edge()); c.fillRect(x + 18, y + 19, 12, 12, rgb(240, 168, 48));
        c.fillRect(x + 6, y + 37, 36, 6, edge()); c.fillRect(x + 9, y + 39, 30, 2, rgb(112, 176, 88));
    }
    void scaleIcon(int x, int y) const {
        y += panelOffset();
        if (!GameAssets::drawItemIcon48(Game::ItemId::HEART_SCALE, x + 24, y + 24))
            PixelRenderer::canvas().fillCircle(x + 24, y + 24, 14, rgb(240, 192, 208));
    }
    void monsterIcon(int x, int y, uint16_t species, int size = 48) const {
        y += panelOffset();
        const auto* frame = PokemonSprites::findCachedSpeciesSprite(species, PokemonSprites::SpriteKind::ICON_0);
        if (!frame) frame = PokemonSprites::findSpeciesSprite(species, PokemonSprites::SpriteKind::ICON_0);
        if (frame) {
            const int w = frame->width, h = frame->height;
            const float s = std::min(size / 32.0f, float(size) / std::max(w, h));
            if (PokemonSprites::drawFrameScaled(frame, x + (size - int(w * s)) / 2, y + (size - int(h * s)) / 2, s, false, true)) return;
        }
        PixelRenderer::canvas().fillCircle(x + size / 2, y + size / 2, size / 3, line());
    }
    static void moveInfoLine(char* out, size_t n, const MoveInfo* m, bool withAccuracy) {
        if (!m) { out[0] = 0; return; }
        const char* cls = m->damageClass == DamageClass::STATUS ? MarketTutorText::STATUS :
            m->damageClass == DamageClass::SPECIAL ? MarketTutorText::SPECIAL : MarketTutorText::PHYSICAL;
        int used = std::snprintf(out, n, "%s / %s", typeName(m->type), cls);
        if (m->power && used > 0 && size_t(used) < n) used += std::snprintf(out + used, n - used, " / ");
        if (m->power && used > 0 && size_t(used) < n) used += std::snprintf(out + used, n - used, MarketTutorText::POWER, m->power);
        if (withAccuracy && m->accuracy && used > 0 && size_t(used) < n) {
            used += std::snprintf(out + used, n - used, " / ");
            if (used > 0 && size_t(used) < n) std::snprintf(out + used, n - used, MarketTutorText::ACCURACY, m->accuracy);
        }
    }
    static void powerLabel(char* out, size_t size, const MoveInfo* move) {
        if (move && move->power) std::snprintf(out, size, MarketTutorText::POWER, move->power);
        else std::snprintf(out, size, "%s", MarketTutorText::POWER_NONE);
    }

    void body(const char* value,Rect r,bool centered=false,uint16_t color=0,int maxSize=32) const {
        r.y+=panelOffset();RoundMarketStall::body(bodyText_,value,r,centered,color,false,0,maxSize);
    }
    void renderMenu(const Game::GameState& s) const {
        const char* names[]={MarketTutorText::MENU_TUTOR,MarketTutorText::MENU_RECALL,MarketTutorText::MENU_TRAIN};
        const auto* move=findMove(s.marketTutor.move);
        for(int i=0;i<3;++i){const auto r=menuRow(i);if(i<2)rowFrame(r,false);
            if(i==0)discIcon(64,r.y+8,move?move->type:TypeId::GHOST);
            else if(i==1)scaleIcon(64,r.y+8);else trainIcon(64,r.y+8);
            text(132,r.y+16,names[i],32,menuEnabled(s,i)?ink():muted());
        }
    }
    void memberCard(const Game::GameState& s,int i,bool enabled) const {
        const auto r=tutorMember(i,members(s));auto& c=PixelRenderer::canvas();
        c.fillRoundRect(r.x,r.y+panelOffset(),r.w,r.h,12,enabled?green():off());
        c.drawRoundRect(r.x,r.y+panelOffset(),r.w,r.h,12,enabled?light():line());
        monsterIcon(r.x+12,r.y+2,s.team[i].speciesId,80);
        if(!enabled)c.blendRoundRectAlphaNative(r.x+2,r.y+2+panelOffset(),r.w-4,r.h-4,10,off(),120);
    }
    void renderTutor(const Game::GameState& s) const {
        const auto* move=findMove(s.marketTutor.move);discIcon(64,144,move?move->type:TypeId::GHOST);
        const char* name=move?move->name:MarketTutorText::SUB_CLOSED;
        text(120,144,name,textWidth(name,32)<=176?32:24);
        char info[128];moveInfoLine(info,sizeof(info),move,true);body(info,{120,178,176,36},false,muted(),16);
        for(int i=0;i<members(s);++i)memberCard(s,i,!s.marketTutor.taught&&Game::MarketTutor::eligibility(s.team[i],s.marketTutor.move)==Game::MarketTutor::Eligibility::CAN_LEARN);
        ctext(318,MarketTutorText::CHOOSE_MONSTER,16,muted());
    }
    void renderMembers(const Game::GameState& s,bool training) const {
        text(68,146,training?MarketTutorText::MENU_TRAIN:MarketTutorText::MENU_RECALL);
        for(int i=0;i<members(s);++i)memberCard(s,i,memberEnabled(s,i));
        ctext(318,MarketTutorText::CHOOSE_MONSTER,16,muted());
    }
    void renderSlot(const Game::GameState& s) const {
        text(68,146,MarketTutorText::FORGET_TITLE);const auto& m=s.team[member_];
        for(int i=1;i<3;++i){const auto r=moveRow(i-1);const auto* move=findMove(slotMove(m,i));char p[32];powerLabel(p,sizeof(p),move);rowFrame(r,false);rowText(r,68,move?move->name:MarketTutorText::EMPTY,"",move?p:"",false,0,16);}
    }
    Rect confirmBodyRect() const {return {68,192,224,mode_==Mode::RECALL||confirmMessage_?48:90};}
    int confirmScrollMaximum() const {return std::max(0,confirmBody_.measure().height*confirmTextSize_/32-confirmBodyRect().h);}
    bool prepareConfirmBody(const Game::GameState& s) const {
        const auto& m=s.team[member_];const auto* old=findMove(slotMove(m,slot_));const auto* move=findMove(chosenMove(s));
        char first[96],second[96]{},value[192];
        if(old){std::snprintf(first,sizeof(first),MarketTutorText::TEACH_LINE1,speciesName(m),old->name);std::snprintf(second,sizeof(second),mode_==Mode::TEACH?MarketTutorText::TEACH_LINE2:MarketTutorText::RECALL_LINE2,move?move->name:"");}
        else std::snprintf(first,sizeof(first),mode_==Mode::TEACH?MarketTutorText::TEACH_EMPTY:MarketTutorText::RECALL_EMPTY,speciesName(m),move?move->name:"");
        std::snprintf(value,sizeof(value),"%s%s",first,second);
        const auto previous=confirmBody_.options();const bool changed=confirmBody_.setText(value);
        const char* why=nullptr;unsigned long need=0;const bool disabled=confirmDisabled(s,&why,&need);
        confirmMessage_=error_||(disabled&&why!=MarketTutorText::NO_COINS);
        confirmTextSize_=RoundMarketStall::fit(confirmBody_,value,224,48,mode_==Mode::TEACH);
        return changed||!(previous==confirmBody_.options());
    }
    void renderConfirm(const Game::GameState& s) const {
        const auto* move=findMove(chosenMove(s));char title[128];
        if(mode_==Mode::RECALL){scaleIcon(64,144);std::snprintf(title,sizeof(title),MarketTutorText::RECALL_CONFIRM_TITLE,move?move->name:"");}
        else {discIcon(64,144,move?move->type:TypeId::GHOST);std::snprintf(title,sizeof(title),"%s",move?move->name:"");}
        const int size=textWidth(title,32)<=176?32:textWidth(title,28)<=176?28:24;text(124,144,title,size);
        prepareConfirmBody(s);auto r=confirmBodyRect();r.y+=panelOffset();
        RoundMarketStall::body(confirmBody_,confirmBody_.text(),r,mode_==Mode::TEACH,ink(),mode_==Mode::TEACH&&!confirmMessage_,48,32,confirmScroll_);
        const char* why=nullptr;unsigned long need=0;const bool disabled=confirmDisabled(s,&why,&need);char label[64];
        if(error_||(disabled&&why!=MarketTutorText::NO_COINS))body(error_?error_:why,{84,244,192,36},true,red());
        else if(mode_==Mode::RECALL){const auto have=Game::ItemInventory::count(s,Game::ItemId::HEART_SCALE);std::snprintf(label,sizeof(label),MarketTutorText::SCALE_CHANGE,have,have?have-1:0);ctext(262,label,16);}
        if(mode_==Mode::TEACH)std::snprintf(label,sizeof(label),MarketTutorText::TEACH_BUTTON,Game::MarketTutor::lessonPrice(chosenMove(s)));
        else std::snprintf(label,sizeof(label),"%s",MarketTutorText::RECALL_BUTTON);
        button(wide(),label,true,disabled||pending_);
    }
    void renderTrainMoves(const Game::GameState& s) const {
        const auto& m=s.team[member_];
        const char* why=nullptr;unsigned long need=0;const bool disabled=!trainable(s,slot_)||confirmDisabled(s,&why,&need);
        if(error_||(disabled&&why&&why!=MarketTutorText::NO_COINS))body(error_?error_:why,{68,144,224,36},true,red(),16);
        else text(68,146,MarketTutorText::TRAIN_MOVE_TITLE);
        for(int i=1;i<3;++i){const auto* move=findMove(slotMove(m,i));const auto r=moveRow(i-1);rowFrame(r,i==slot_&&trainable(s,i));
            char cost[32]{};if(move)std::snprintf(cost,sizeof(cost),MarketTutorText::TRAIN_COST,Game::MarketTutor::trainStep(m.moveProficiency[i]).cost);
            rowText(r,68,move?move->name:MarketTutorText::EMPTY,"",move?(trainable(s,i)?cost:MarketTutorText::MAXED):"",!trainable(s,i),0,16);}
        char label[32];std::snprintf(label,sizeof(label),MarketTutorText::TRAIN_BUTTON,Game::MarketTutor::trainStep(m.moveProficiency[slot_]).cost);button(wide(),label,true,disabled||pending_);
    }
    void renderResult(const Game::GameState& s) const {
        const auto* move=findMove(chosenMove(s));
        if(mode_==Mode::TEACH)discIcon(156,144,move?move->type:TypeId::GHOST);
        else if(mode_==Mode::TRAIN)trainIcon(156,144);else scaleIcon(156,144);
        char value[sizeof(result1_)+1];std::snprintf(value,sizeof(value),"%s",result1_);
        if(mode_!=Mode::TRAIN&&move){char* at=std::strstr(value,move->name);if(at&&at!=value){const size_t n=std::strlen(at);if(std::strlen(value)+1<sizeof(value)){std::memmove(at+1,at,n+1);*at='\n';}}}
        body(value,{68,192,224,76},true);if(result2_[0])ctext(260,result2_,16);
        button(wide(),MarketTutorText::HOME,true);
    }
    void renderPerformance(const Game::GameState& s) const {
        const bool learning=page_==Page::TEACHING;ctext(148,learning?MarketTutorText::LEARNING:MarketTutorText::TRAINING);
        MarketTutorPerformance::render(learning?MarketTutorPerformance::learningStyle(findMove(chosenMove(s))):MarketTutorPerformance::trainingStyle(findMove(chosenMove(s))),showMs_,s.team[member_].speciesId,{.70f,51,26+panelOffset()});
    }
    void renderRecallList(const Game::GameState& s) const {
        Game::MoveId moves[Game::MoveManagementService::MAX_RECALLABLE_MOVE_COUNT];const auto n=recallList(s,moves);auto& c=PixelRenderer::canvas();
        const int l=c.clipLeft(),t=c.clipTop(),r=c.clipRight(),b=c.clipBottom();const int x=std::max(l,54),y=std::max(t,LIST_TOP+panelOffset());
        c.setClipRect(x,y,std::max(0,std::min(r,306)-x),std::max(0,std::min(b,LIST_BOTTOM+panelOffset())-y));
        const auto* species=findSpecies(s.team[member_].speciesId);
        for(int i=0;i<n;++i){const int top=LIST_TOP+i*ROW_H-scroll_;if(top+ROW_H<=LIST_TOP||top>=LIST_BOTTOM)continue;
            const auto* move=findMove(moves[i]);const Rect row{54,top,252,ROW_H};rowFrame(row,false);
            const char* name=move?move->name:"";text(68,top+1,name,textWidth(name,32)<=224?32:24);
            char info[160];moveInfoLine(info,sizeof(info),move,false);const unsigned level=species?moveLearnLevelForSpecies(*species,moves[i]):0;
            if(level){const size_t used=std::strlen(info);std::snprintf(info+used,sizeof(info)-used," / Lv%u",level);}
            body(info,{68,top+36,224,18},false,muted(),16);
        }c.setClipRect(l,t,r-l,b-t);
        const int total=n*ROW_H+ROW_H,h=LIST_BOTTOM-LIST_TOP,thumb=std::max(24,h*h/std::max(h,total));
        c.fillRect(298,LIST_TOP+panelOffset(),3,h,line());
        c.fillRect(298,LIST_TOP+scroll_*(h-thumb)/std::max(1,total-h)+panelOffset(),3,thumb,edge());
    }
};
} // namespace Ui
