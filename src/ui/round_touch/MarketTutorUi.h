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
#include "ui/common/MarketView.h"

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
    struct Rect { int x, y, w, h; bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; } };

    static constexpr int W = 360, H = 360, PANEL_Y = 144, ROW_Y = 156, ROW_H = 56, GREET_H = 136;
    static constexpr int MENU_Y = PANEL_Y, MENU_ROW_H = 46;
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
    // Vertical offset of the coin badge: hidden while greeting, sliding in
    // and out with the panel.
    int hudOffset() const { return MarketStallMotion::hudOffset(page_ == Page::GREET, transition_ == Transition::OPENING,
                                                                 transition_ == Transition::CLOSING, transitionMs_,
                                                                 duration()); }

    void open(const Game::GameState& state, const char* welcome = nullptr, bool initialized = true) {
        *this = RoundMarketTutorUi{};
        period_=state.marketTutor.periodDay;
        page_ = initialized ? Page::GREET : Page::ERROR;
        transition_ = Transition::GREET_IN;
        const auto& t = state.marketTutor;
        if (t.move && !t.taught) {
            const MoveInfo* move = findMove(t.move);
            std::snprintf(greet_, sizeof(greet_), MarketTutorText::GREET_CLASS, move ? move->name : "");
        } else {
            std::snprintf(greet_, sizeof(greet_), "%s", t.move ? MarketTutorText::GREET_TAUGHT : MarketTutorText::GREET_CLOSED);
        }
        if (welcome) std::snprintf(greet_, sizeof(greet_), "%s", welcome);
    }
    void close() { feedback_.clear(); page_ = Page::CLOSED; transition_ = Transition::NONE; cancelTouch(); pending_ = false; }
    void cancelTouch() { tap_.cancel(); scrollMotion_.cancel(); confirmScrollMotion_.cancel(); }

    // Only an explicit completed tap advances the greeting.
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

    // Camera shift that keeps the facing actors inside the top scene band.
    // The greeting box covers the bottom 120px, the stall panel everything
    // below the current panel top; feet land about 28px above the covered edge.
    int sceneShift(const Market::Session& s, const MarketView::Viewport& v) const {
        const int camera = v.cameraY(s);
        int lowest = -100000;
        for (int role = 0; role < 4; ++role) {
            const int i = s.conversationActor(role);
            if (i >= 0) lowest = std::max(lowest, v.pixel(s.pose(i).y) - camera);
        }
        if (lowest < -10000) return 0;
        const int greeting = std::clamp(lowest - (H - 148), 0, 120);
        const int opened = std::clamp(lowest - (basePanelTop() - 28), -240, 240);
        if (transition_ == Transition::GREET_IN) return tween(0, greeting);
        if (transition_ == Transition::OPENING) return tween(greeting, opened);
        if (transition_ == Transition::CLOSING) return tween(opened, 0);
        return page_ == Page::GREET ? greeting : opened;
    }

    Request touch(const Platform::IInputDevice::TouchEvent& e, const Game::GameState& state) {
        using Type = Platform::IInputDevice::TouchType;
        Request none{};
        if ((int(e.x)-180)*(int(e.x)-180)+(int(e.y)-180)*(int(e.y)-180)>=180*180) {cancelTouch();return none;}
        if (pending_ || page_ == Page::CLOSED || animating()) { cancelTouch(); return none; }
        if (performing()) {
            if (showMs_ < TRAIN_SKIP_MS) { tap_.cancel(); return none; }
            if (tap_.update(e, 0) == 0) finishPerformance();
            return none;
        }
        if (page_ == Page::GREET) {
            // The lifting finger of the skip tap never lands on the menu.
            if (tap_.update(e, 0) == 0) { feedback_.set(SfxCue::UI_CONFIRM); openPanel(); }
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
        if (page_ == Page::GREET) { renderGreet(); return; }
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
        if (page_ == Page::MENU && error_) body(error_,{72,304,216,18},true,red());
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
    mutable TextBlock bodyText_;
    MarketListScroll confirmScrollMotion_;
    int confirmScroll_ = 0;
    bool pending_ = false;
    const char* error_ = nullptr;
    char greet_[96]{}, result1_[96]{}, result2_[64]{};

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
    static Rect moveRow(int i) { return {68, 190 + i * 44, 224, 42}; }
    static Rect menuRow(int i) { return {64, ROW_Y + i * MENU_ROW_H, 232, MENU_ROW_H}; }
    static Rect wide() { return {80, 288, 200, 40}; }
    static constexpr int TUTOR_MEMBER_TOP = 220, LIST_TOP = 152, LIST_BOTTOM = 304;

    static Rect tutorMember(int i, int count) {
        return {count == 1 ? 132 : 76 + i * 112, TUTOR_MEMBER_TOP, 96, 76};
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
        return std::max(0, recallList(s, moves) * ROW_H - (LIST_BOTTOM - LIST_TOP));
    }
    bool listDrag(const Platform::IInputDevice::TouchEvent& e, const Game::GameState& s) {
        scrollMaximum_ = maxScroll(s);
        return scrollMotion_.touch(e, {60, LIST_TOP, 240, LIST_BOTTOM - LIST_TOP},
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
        if (x >= 0 && x < W && y >= 0 && y < basePanelTop()) return OUTSIDE;
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
            if (y < LIST_TOP || y >= LIST_BOTTOM || x < 60 || x >= 300) return -1;
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

    static int textWidth(const char* s, int size) {
        int w = 0;
        for (const auto* p = reinterpret_cast<const uint8_t*>(s); *p;) {
            const int n = *p < 0x80 ? 1 : *p < 0xe0 ? 2 : *p < 0xf0 ? 3 : 4;
            w += n == 1 ? size / 2 : size; p += n;
        }
        return w;
    }
    void text(int x, int y, const char* s, int size = 20, uint16_t color = 0) const {
        PixelRenderer::textSmooth(x,y+panelOffset(),s,color?color:ink(),uint8_t(size));
    }
    void rtext(int right, int y, const char* s, int size = 20, uint16_t color = 0) const { text(right - textWidth(s, size), y, s, size, color); }
    void ctext(int y, const char* s, int size = 20, uint16_t color = 0) const { text((W - textWidth(s, size)) / 2, y, s, size, color); }

    void renderHud(const Game::GameState& s,const MarketCoinMotion* motion) const {
        const int y=52+hudOffset();auto& c=PixelRenderer::canvas();c.fillRoundRect(56,y-4,96,32,6,paper());
        PixelRenderer::textSmooth(64,y+6,MarketTutorText::TITLE,ink(),20);MarketCoinHud::render(s.coins,motion,y,20,{288,24,32,36});
    }
    void panel(int y) const {
        auto& c = PixelRenderer::canvas();
        const int height = 232;
        y += panelOffset();
        c.fillRoundRect(4, y, W - 8, height, 14, paper());
        c.drawRoundRect(4, y, W - 8, height, 14, edge());
    }
    void button(Rect r,const char* label,bool primary,bool disabled=false) const {
        auto& c=PixelRenderer::canvas();const int y=r.y+panelOffset();
        c.fillRoundRect(r.x,y,r.w,r.h,8,disabled?off():primary?edge():paper());
        c.drawRoundRect(r.x,y,r.w,r.h,8,disabled?line():edge());
        text(r.x+(r.w-textWidth(label,20))/2,r.y+(r.h-20)/2,label,20,disabled?muted():primary?paper():ink());
    }
    void rowFrame(Rect r, bool selected) const {
        r.y += panelOffset();
        auto& c = PixelRenderer::canvas();
        if (selected) c.fillRect(r.x, r.y, r.w, r.h, green());
        c.drawFastHLine(68, r.y + r.h - 1, 224, line());
    }
    void rowText(Rect r,int x,const char* main,const char*,const char* right,bool disabled,uint16_t=0,int rightSize=20) const {
        const int size=textWidth(main,20)+(right?textWidth(right,rightSize):0)>204?16:20;
        text(x,r.y+(r.h-size)/2,main,size,disabled?muted():ink());
        if(right&&*right)rtext(284,r.y+(r.h-rightSize)/2,right,rightSize,disabled?muted():ink());
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

    void body(const char* value,Rect r,bool centered=false,uint16_t color=0) const {
        TextBlock::Options o;o.width=r.w;o.maxHeight=r.h;o.font=BitmapText::Font::PX16;o.lineGap=2;
        o.align=centered?TextBlock::Align::CENTER:TextBlock::Align::LEFT;
        bodyText_.setText(value);bodyText_.setOptions(o);bodyText_.draw(PixelRenderer::canvas(),r.x,r.y+panelOffset(),color?color:ink());
    }
    void renderGreet() const {panel(H-GREET_H);body(greet_,{72,242,216,70},true);}
    void renderMenu(const Game::GameState& s) const {
        const char* names[]={MarketTutorText::MENU_TUTOR,MarketTutorText::MENU_RECALL,MarketTutorText::MENU_TRAIN};
        for(int i=0;i<3;++i){const auto r=menuRow(i);rowFrame(r,false);
            if(i==0)discIcon(76,r.y+7,TypeId::GHOST,32);
            else if(i==1)GameAssets::drawCentered(GameAssets::Kind::ITEM_HEART_SCALE,92,r.y+23+panelOffset(),32.f/36.f);
            else {auto& c=PixelRenderer::canvas();const int y=r.y+panelOffset();c.fillRect(79,y+18,27,8,edge());c.fillRect(76,y+10,5,24,edge());c.fillRect(104,y+10,5,24,edge());}
            text(126,r.y+13,names[i],20,menuEnabled(s,i)?ink():muted());
        }
    }
    void memberCard(const Game::GameState& s,int i,bool enabled) const {
        const auto r=tutorMember(i,members(s));auto& c=PixelRenderer::canvas();
        c.fillRoundRect(r.x,r.y+panelOffset(),r.w,r.h,10,enabled?green():off());
        monsterIcon(r.x+(r.w-56)/2,r.y+4,s.team[i].speciesId,56);
        text(r.x+(r.w-textWidth(speciesName(s.team[i]),16))/2,r.y+r.h-20,speciesName(s.team[i]),16,enabled?ink():muted());
        if(!enabled)c.blendRectAlphaNative(r.x,r.y+panelOffset(),r.w,r.h,off(),100);
    }
    void renderTutor(const Game::GameState& s) const {
        const auto* move=findMove(s.marketTutor.move);discIcon(72,151,move?move->type:TypeId::GHOST,32);
        text(116,154,move?move->name:MarketTutorText::SUB_CLOSED);
        char info[128];moveInfoLine(info,sizeof(info),move,true);body(info,{68,182,224,34});
        for(int i=0;i<members(s);++i)memberCard(s,i,!s.marketTutor.taught&&Game::MarketTutor::eligibility(s.team[i],s.marketTutor.move)==Game::MarketTutor::Eligibility::CAN_LEARN);
        ctext(306,MarketTutorText::CHOOSE_MONSTER,16,muted());
    }
    void renderMembers(const Game::GameState& s,bool training) const {
        ctext(156,training?MarketTutorText::MENU_TRAIN:MarketTutorText::MENU_RECALL);
        for(int i=0;i<members(s);++i)memberCard(s,i,memberEnabled(s,i));
        ctext(306,MarketTutorText::CHOOSE_MONSTER,16,muted());
    }
    void renderSlot(const Game::GameState& s) const {
        ctext(156,MarketTutorText::FORGET_TITLE);const auto& m=s.team[member_];
        for(int i=1;i<3;++i){const auto r=moveRow(i-1);const auto* move=findMove(slotMove(m,i));char p[32];powerLabel(p,sizeof(p),move);rowFrame(r,false);rowText(r,76,move?move->name:MarketTutorText::EMPTY,"",move?p:"",false,0,16);}
    }
    Rect confirmBodyRect() const {return {68,190,224,72};}
    int confirmScrollMaximum() const {return std::max(0,bodyText_.measure().height-confirmBodyRect().h);}
    bool prepareConfirmBody(const Game::GameState& s) const {
        const auto& m=s.team[member_];const auto* old=findMove(slotMove(m,slot_));const auto* move=findMove(chosenMove(s));
        char first[96],second[96]{},value[192];
        if(old){std::snprintf(first,sizeof(first),MarketTutorText::TEACH_LINE1,speciesName(m),old->name);std::snprintf(second,sizeof(second),mode_==Mode::TEACH?MarketTutorText::TEACH_LINE2:MarketTutorText::RECALL_LINE2,move?move->name:"");}
        else std::snprintf(first,sizeof(first),mode_==Mode::TEACH?MarketTutorText::TEACH_EMPTY:MarketTutorText::RECALL_EMPTY,speciesName(m),move?move->name:"");
        std::snprintf(value,sizeof(value),"%s%s",first,second);TextBlock::Options o;o.width=224;o.maxHeight=72;o.font=BitmapText::Font::PX16;o.lineGap=2;o.align=TextBlock::Align::CENTER;o.overflow=TextBlock::Overflow::CLIP;
        const bool changed=bodyText_.setText(value);return bodyText_.setOptions(o)||changed;
    }
    void renderConfirm(const Game::GameState& s) const {
        const auto* move=findMove(chosenMove(s));ctext(156,move?move->name:"");prepareConfirmBody(s);
        const auto r=confirmBodyRect();bodyText_.draw(PixelRenderer::canvas(),r.x,r.y+panelOffset(),ink(),confirmScroll_);
        char label[40];if(mode_==Mode::TEACH)std::snprintf(label,sizeof(label),MarketTutorText::TEACH_BUTTON,Game::MarketTutor::lessonPrice(chosenMove(s)));
        else std::snprintf(label,sizeof(label),"%s",MarketTutorText::RECALL_BUTTON);
        const char* why=nullptr;unsigned long need=0;const bool disabled=confirmDisabled(s,&why,&need);char reasonText[96]{};
        if(why==MarketTutorText::NO_COINS)std::snprintf(reasonText,sizeof(reasonText),why,need);else if(why)std::snprintf(reasonText,sizeof(reasonText),"%s",why);
        if(error_||disabled)body(error_?error_:reasonText,{68,266,224,18},true,red());
        button(wide(),label,true,disabled||pending_);
    }
    void renderTrainMoves(const Game::GameState& s) const {
        const auto& m=s.team[member_];ctext(154,MarketTutorText::TRAIN_MOVE_TITLE);
        for(int i=1;i<3;++i){const auto* move=findMove(slotMove(m,i));const auto r=moveRow(i-1);rowFrame(r,i==slot_&&trainable(s,i));
            char cost[32]{};if(move)std::snprintf(cost,sizeof(cost),MarketTutorText::TRAIN_COST,Game::MarketTutor::trainStep(m.moveProficiency[i]).cost);
            rowText(r,76,move?move->name:MarketTutorText::EMPTY,"",move?(trainable(s,i)?cost:MarketTutorText::MAXED):"",!trainable(s,i),0,16);
        }
        const char* why=nullptr;unsigned long need=0;const bool disabled=!trainable(s,slot_)||confirmDisabled(s,&why,&need);char reasonText[96]{};
        if(why==MarketTutorText::NO_COINS)std::snprintf(reasonText,sizeof(reasonText),why,need);else if(why)std::snprintf(reasonText,sizeof(reasonText),"%s",why);
        if(error_||disabled)body(error_?error_:reasonText,{68,174,224,16},true,red());
        char label[32];std::snprintf(label,sizeof(label),MarketTutorText::TRAIN_BUTTON,Game::MarketTutor::trainStep(m.moveProficiency[slot_]).cost);button(wide(),label,true,disabled||pending_);
    }
    void renderResult(const Game::GameState&) const {
        discIcon(164,162,TypeId::GHOST,32);body(result1_,{68,210,224,54},true);
        if(result2_[0])ctext(268,result2_,16);
        button(wide(),MarketTutorText::HOME,true);
    }
    void renderPerformance(const Game::GameState& s) const {
        const bool learning=page_==Page::TEACHING;ctext(154,learning?MarketTutorText::LEARNING:MarketTutorText::TRAINING);
        MarketTutorPerformance::render(learning?MarketTutorPerformance::learningStyle(findMove(chosenMove(s))):MarketTutorPerformance::trainingStyle(findMove(chosenMove(s))),showMs_,s.team[member_].speciesId,{.76f,40,-18});
    }
    void renderRecallList(const Game::GameState& s) const {
        Game::MoveId moves[Game::MoveManagementService::MAX_RECALLABLE_MOVE_COUNT];const auto n=recallList(s,moves);auto& c=PixelRenderer::canvas();
        const int l=c.clipLeft(),t=c.clipTop(),r=c.clipRight(),b=c.clipBottom();const int x=std::max(l,60),y=std::max(t,LIST_TOP+panelOffset());
        c.setClipRect(x,y,std::max(0,std::min(r,300)-x),std::max(0,std::min(b,LIST_BOTTOM+panelOffset())-y));
        for(int i=0;i<n;++i){const int top=LIST_TOP+i*ROW_H-scroll_;if(top+ROW_H<=LIST_TOP||top>=LIST_BOTTOM)continue;
            const auto* move=findMove(moves[i]);const Rect row{60,top,240,ROW_H};rowFrame(row,false);text(72,top+5,move?move->name:"");
            char info[128];moveInfoLine(info,sizeof(info),move,false);body(info,{72,top+30,216,16},false,muted());
        }c.setClipRect(l,t,r-l,b-t);
        if(n*ROW_H>LIST_BOTTOM-LIST_TOP){const int h=LIST_BOTTOM-LIST_TOP,thumb=std::max(16,h*h/(n*ROW_H));c.fillRect(299,LIST_TOP+scroll_*(h-thumb)/std::max(1,n*ROW_H-h)+panelOffset(),3,thumb,line());}
    }
};
} // namespace Ui
