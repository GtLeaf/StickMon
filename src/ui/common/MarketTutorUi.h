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
#include "ui/common/MarketStallMotion.h"
#include "ui/common/MarketUiFeedback.h"
#include "ui/common/MarketTutorPerformance.h"
#include "ui/common/MarketView.h"

// Gengar stall (秘技小屋) pages for the 368x448 touch layout. The controller
// owns page state and hit geometry; the application owns saving. A request is
// returned from touch(), applied to a GameState copy and confirmed through
// committed(), so a failed save never shows success or keeps a charge. The
// menu closes by tapping the scene above it; sub-pages keep their back button.
namespace Ui {
class MarketTutorUi {
public:
    enum class Page : uint8_t { CLOSED, GREET, MENU, TUTOR, SLOT, CONFIRM, RESULT,
                                TRAIN_MEMBER, TRAIN_MOVE, RECALL_MEMBER, RECALL_LIST, TRAINING, TEACHING };
    enum class Mode : uint8_t { TEACH, TRAIN, RECALL };
    struct Request {
        enum Kind : uint8_t { NONE, CLOSE, TEACH, TRAIN, RECALL } kind = NONE;
        uint8_t member = 0, slot = 0;
        Game::MoveId move = 0;
    };
    struct Rect { int x, y, w, h; bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; } };

    static constexpr int W = 368, H = 448, PANEL_Y = 208, ROW_Y = 216, ROW_H = 56, GREET_H = 120;
    static constexpr int MENU_Y = PANEL_Y, MENU_ROW_H = 68;
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

    void open(const Game::GameState& state, const char* welcome = nullptr) {
        *this = MarketTutorUi{};
        page_ = Page::GREET;
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
    void close() { feedback_.clear(); page_ = Page::CLOSED; transition_ = Transition::NONE; tap_.cancel(); pending_ = false; }
    void cancelTouch() { tap_.cancel(); listTracking_ = dragging_ = false; }

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
        if (pending_ || page_ == Page::CLOSED || animating()) { tap_.cancel(); return none; }
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
        const int target = e.type == Type::DOWN || e.type == Type::UP ? hit(e.x, e.y, state) : -1;
        const int clicked = tap_.update(e, target);
        if (clicked < 0) return none;
        if (clicked == OUTSIDE) {
            const int backTarget = page_ == Page::MENU ? CLOSE : page_ == Page::CONFIRM ? BACK :
                page_ == Page::RESULT ? PRIMARY : page_ == Page::TRAIN_MOVE ? BACK : FOOTER;
            return activate(backTarget, state);
        }
        return activate(clicked, state);
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

    void render(const Game::GameState& state) const {
        if (page_ == Page::CLOSED) return;
        if (page_ == Page::GREET) { renderGreet(); return; }
        panel(basePanelTop());
        switch (page_) {
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
        if (page_ == Page::MENU && error_) ctext(ROW_Y + 3 * MENU_ROW_H + 6, error_, 16, red());
        renderHud(state);
    }

private:
    Page page_ = Page::CLOSED;
    enum class Transition : uint8_t { NONE, GREET_IN, OPENING, CLOSING };
    Transition transition_ = Transition::NONE;
    uint32_t transitionMs_ = 0, errorMs_ = 0, showMs_ = 0;
    Mode mode_ = Mode::TEACH;
    TouchTap tap_;
    MarketUiFeedback feedback_;
    uint8_t member_ = 0, slot_ = 1, before_ = 0;
    Game::MoveId recallMove_ = 0;
    int scroll_ = 0, dragStartX_ = 0, dragStartY_ = 0, dragScroll_ = 0;
    bool listTracking_ = false, dragging_ = false, pending_ = false;
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
    static Rect row(int i, int top = ROW_Y) { return {10, top + i * ROW_H, 348, ROW_H}; }
    static Rect moveRow(int i) { return {10, 264 + i * 60, 348, 60}; }
    static Rect menuRow(int i) { return {10, ROW_Y + i * MENU_ROW_H, 348, MENU_ROW_H}; }
    static Rect footer() { return {16, 394, 336, 44}; }
    static Rect back() { return {16, 390, 160, 48}; }
    static Rect primary() { return {192, 390, 160, 48}; }
    static Rect wide() { return {16, 390, 336, 48}; }
    static constexpr int TUTOR_MEMBER_TOP = 280, LIST_TOP = 216, LIST_BOTTOM = 384;

    static Rect tutorMember(int i, int count) {
        return {count == 1 ? 106 : 20 + i * 172, TUTOR_MEMBER_TOP, 156, 96};
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
        using Type = Platform::IInputDevice::TouchType;
        if (e.type == Type::DOWN) {
            listTracking_ = Rect{10, LIST_TOP, 348, LIST_BOTTOM - LIST_TOP}.contains(e.x, e.y);
            dragging_ = false;
            dragStartX_ = e.x; dragStartY_ = e.y; dragScroll_ = scroll_;
            return false;
        }
        if (!listTracking_) return false;
        if (e.type == Type::CANCEL) { cancelTouch(); return true; }
        const int dx = e.x - dragStartX_, dy = e.y - dragStartY_;
        // Once captured, consume through release, even after crossing the
        // panel edge or returning to the gesture's origin at a scroll limit.
        if (!dragging_ && dx * dx + dy * dy > 100) {
            dragging_ = true;
            tap_.cancel();
        }
        const bool consumed = dragging_;
        if (dragging_) scroll_ = std::clamp(dragScroll_ - dy, 0, maxScroll(s));
        if (e.type == Type::UP) listTracking_ = dragging_ = false;
        return consumed;
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
        case Page::MENU:
            for (int i = 0; i < 3; ++i) if (contains(menuRow(i))) return i;
            return -1;
        case Page::TUTOR:
            for (int i = 0; i < members(s); ++i) {
                if (!contains(tutorMember(i, members(s)))) continue;
                return Game::MarketTutor::eligibility(s.team[i], s.marketTutor.move) ==
                    Game::MarketTutor::Eligibility::CAN_LEARN && !s.marketTutor.taught ? i : DISABLED;
            }
            return contains(footer()) ? FOOTER : -1;
        case Page::SLOT:
            for (int i = 1; i < 3; ++i) if (contains(moveRow(i - 1))) return i;
            return contains(footer()) ? FOOTER : -1;
        case Page::CONFIRM: {
            const char* why = nullptr; unsigned long need = 0;
            if (contains(back())) return BACK;
            return contains(primary()) ? (!confirmDisabled(s, &why, &need) ? PRIMARY : DISABLED) : -1;
        }
        case Page::RESULT: return contains(wide()) ? PRIMARY : -1;
        case Page::TRAIN_MEMBER: case Page::RECALL_MEMBER:
            for (int i = 0; i < members(s); ++i) if (contains(tutorMember(i, members(s)))) return memberEnabled(s, i) ? i : DISABLED;
            return contains(footer()) ? FOOTER : -1;
        case Page::TRAIN_MOVE:
            for (int i = 0; i < 2; ++i) if (contains(moveRow(i))) return trainable(s, uint8_t(i + 1)) ? i : DISABLED;
            if (contains(back())) return BACK;
            { const char* why = nullptr; unsigned long need = 0;
              return contains(primary()) ? (trainable(s, slot_) && !confirmDisabled(s, &why, &need) ? PRIMARY : DISABLED) : -1; }
        case Page::RECALL_LIST: {
            if (contains(footer())) return FOOTER;
            if (y < LIST_TOP || y >= LIST_BOTTOM || x < 10 || x >= 358) return -1;
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
        Request r{};
        feedback_.set(target == FOOTER || target == BACK || target == CLOSE || page_ == Page::RESULT ?
                      SfxCue::UI_CANCEL : SfxCue::UI_CONFIRM);
        if (target == DISABLED) { feedback_.set(SfxCue::UI_CANCEL); return r; }
        error_ = nullptr; errorMs_ = 0;
        switch (page_) {
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
    void text(int x, int y, const char* s, int size = 32, uint16_t color = 0) const {
        auto& c = PixelRenderer::canvas();
        const bool native = c.nativeText();
        c.setNativeText(size == 32);
        PixelRenderer::text(x, y + panelOffset(), s, color ? color : ink());
        c.setNativeText(native);
    }
    void rtext(int right, int y, const char* s, int size = 32, uint16_t color = 0) const { text(right - textWidth(s, size), y, s, size, color); }
    void ctext(int y, const char* s, int size = 32, uint16_t color = 0) const { text((W - textWidth(s, size)) / 2, y, s, size, color); }

    void hudBadge(int x, int y, const char* label) const {
        MarketStallMotion::drawBadge(x, y, label, textWidth(label, 28), paper(), ink());
    }
    void renderHud(const Game::GameState& s) const {
        const int y = 16 + hudOffset();
        hudBadge(23, y, MarketTutorText::TITLE);
        char coins[16]; std::snprintf(coins, sizeof(coins), "C%lu", static_cast<unsigned long>(s.coins));
        hudBadge(W - 28 - textWidth(coins, 28), y, coins);
    }
    void panel(int y) const {
        auto& c = PixelRenderer::canvas();
        const int height = H - y - 4;
        y += panelOffset();
        c.fillRoundRect(4, y, W - 8, height, 14, paper());
        c.drawRoundRect(4, y, W - 8, height, 14, edge());
    }
    void button(Rect r, const char* label, bool primary, bool disabled = false) const {
        auto& c = PixelRenderer::canvas();
        c.fillRect(r.x, r.y + panelOffset(), r.w, r.h, disabled ? off() : primary ? edge() : paper());
        const uint16_t border = disabled ? line() : edge();
        c.drawRect(r.x, r.y + panelOffset(), r.w, r.h, border); c.drawRect(r.x + 1, r.y + panelOffset() + 1, r.w - 2, r.h - 2, border);
        text(r.x + (r.w - textWidth(label, 32)) / 2, r.y + (r.h - 32) / 2, label, 32,
             disabled ? muted() : primary ? paper() : ink());
    }
    void rowFrame(Rect r, bool selected) const {
        r.y += panelOffset();
        auto& c = PixelRenderer::canvas();
        if (selected) c.fillRect(r.x, r.y, r.w, r.h, green());
        c.drawFastHLine(16, r.y + r.h - 1, 336, line());
    }
    void rowText(Rect r, int x, const char* main, const char* sub, const char* right, bool disabled,
                        uint16_t subColor = 0, int rightSize = 32) const {
        const bool hasSub = sub && *sub;
        const int rightWidth = right && *right ? textWidth(right, rightSize) + 12 : 0;
        const int mainSize = textWidth(main, 32) <= 344 - x - rightWidth ? 32 : 16;
        const int mainY = r.y + (hasSub ? 1 + (32 - mainSize) / 2 : (r.h - mainSize) / 2);
        text(x, mainY, main, mainSize, disabled ? muted() : ink());
        if (sub && *sub) text(x, r.y + 36, sub, 16, subColor ? subColor : muted());
        if (right && *right) rtext(344, rightSize == 32 ? mainY : r.y + (hasSub ? 10 : (r.h - 16) / 2), right, rightSize, disabled ? muted() : ink());
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
    static int wrappedLines(const char* s, int width, int size) {
        int lines = 1, used = 0;
        for (const auto* p = reinterpret_cast<const uint8_t*>(s); *p;) {
            if (*p == '\n') { ++lines; used = 0; ++p; continue; }
            const int n = *p < 0x80 ? 1 : *p < 0xe0 ? 2 : *p < 0xf0 ? 3 : 4;
            const int advance = n == 1 ? size / 2 : size;
            if (used + advance > width) { ++lines; used = 0; }
            used += advance; p += n;
        }
        return lines;
    }
    void wrapText(int x, int y, const char* s, int width, int lines, int size = 32, bool centered = false) const {
        char buf[192];
        const auto* p = reinterpret_cast<const uint8_t*>(s);
        for (int lineIndex = 0; *p && lineIndex < lines; ++lineIndex) {
            int w = 0; size_t used = 0;
            while (*p) {
                if (*p == '\n') { ++p; break; }
                const int n = *p < 0x80 ? 1 : *p < 0xe0 ? 2 : *p < 0xf0 ? 3 : 4;
                const int advance = n == 1 ? size / 2 : size;
                if (w + advance > width || used + n >= sizeof(buf)) break;
                std::memcpy(buf + used, p, n); used += n; p += n; w += advance;
            }
            buf[used] = 0;
            text(centered ? x + (width - w) / 2 : x, y + lineIndex * (size + 2), buf, size);
        }
    }
    void memberCard(const Game::GameState& s, int i, bool enabled) const {
        const Rect r = tutorMember(i, members(s));
        auto& c = PixelRenderer::canvas();
        c.fillRoundRect(r.x, r.y + panelOffset(), r.w, r.h, 12, enabled ? green() : off());
        c.drawRoundRect(r.x, r.y + panelOffset(), r.w, r.h, 12, enabled ? light() : line());
        monsterIcon(r.x + (r.w - 80) / 2, r.y + (r.h - 80) / 2, s.team[i].speciesId, 80);
        if (!enabled) PixelRenderer::fillRectAlpha(r.x + 2, r.y + panelOffset() + 12, r.w - 4, r.h - 24, off(), 100);
    }
    static void powerLabel(char* out, size_t size, const MoveInfo* move) {
        if (move && move->power) std::snprintf(out, size, MarketTutorText::POWER, move->power);
        else std::snprintf(out, size, "%s", MarketTutorText::POWER_NONE);
    }

    // ---- pages
    void renderGreet() const {
        panel(H - GREET_H);
        char line1[sizeof(MarketTutorText::SPEAKER) + sizeof(greet_) - 1];
        char line2[sizeof(line1)]; line2[0] = 0;
        std::snprintf(line1, sizeof(line1), "%s%s", MarketTutorText::SPEAKER, greet_);
        // Two 32px lines of at most 10 CJK cells each.
        size_t split = 0; int w = 0;
        for (const auto* p = reinterpret_cast<const uint8_t*>(line1); *p;) {
            const int n = *p < 0x80 ? 1 : *p < 0xe0 ? 2 : *p < 0xf0 ? 3 : 4;
            const int adv = n == 1 ? 16 : 32;
            if (w + adv > W - 48) break;
            w += adv; p += n; split += n;
        }
        if (line1[split]) { std::snprintf(line2, sizeof(line2), "%s", line1 + split); line1[split] = 0; }
        text(24, H - 106, line1); if (line2[0]) text(24, H - 66, line2);
    }

    void renderMenu(const Game::GameState& s) const {
        const MoveInfo* move = findMove(s.marketTutor.move);
        const int icon = (MENU_ROW_H - 48) / 2;
        for (int i = 0; i < 3; ++i) {
            const Rect r = menuRow(i);
            if (i < 2) rowFrame(r, false);
            if (i == 0) discIcon(20, r.y + icon, move ? move->type : TypeId::GHOST);
            else if (i == 1) scaleIcon(20, r.y + icon);
            else trainIcon(20, r.y + icon);
            rowText(r, 88, i == 0 ? MarketTutorText::MENU_TUTOR : i == 1 ? MarketTutorText::MENU_RECALL : MarketTutorText::MENU_TRAIN,
                    "", "", !menuEnabled(s, i));
        }
    }

    void renderTutor(const Game::GameState& s) const {
        const auto& t = s.marketTutor;
        const MoveInfo* move = findMove(t.move);
        discIcon(20, 220, move ? move->type : TypeId::GHOST);
        text(76, 220, move ? move->name : "");
        char info[96]; moveInfoLine(info, sizeof(info), move, true);
        text(76, 254, info, 16, muted());
        for (int i = 0; i < members(s); ++i)
            memberCard(s, i, Game::MarketTutor::eligibility(s.team[i], t.move) ==
                       Game::MarketTutor::Eligibility::CAN_LEARN && !t.taught);
        button(footer(), MarketTutorText::BACK, false);
    }

    void renderSlot(const Game::GameState& s) const {
        const auto& m = s.team[member_];
        text(24, 222, MarketTutorText::FORGET_TITLE);
        for (uint8_t slot = 1; slot < 3; ++slot) {
            const MoveInfo* move = findMove(slotMove(m, slot));
            const Rect r = moveRow(slot - 1);
            char power[24]; powerLabel(power, sizeof(power), move);
            rowFrame(r, false);
            rowText(r, 24, move ? move->name : MarketTutorText::EMPTY, "", move ? power : "", false, 0, 16);
        }
        button(footer(), MarketTutorText::BACK, false);
    }

    void renderConfirm(const Game::GameState& s) const {
        const auto& m = s.team[member_];
        const MoveInfo* move = findMove(chosenMove(s));
        char title[96], l1[96], l2[96], n1[96], n2[96], label[24];
        l2[0] = 0;
        const char* who = speciesName(m);
        const MoveInfo* old = findMove(slotMove(m, slot_));
        if (mode_ == Mode::TRAIN) {
            trainIcon(20, 220);
            std::snprintf(title, sizeof(title), MarketTutorText::TRAIN_CONFIRM_TITLE, move ? move->name : "");
            const uint8_t p = m.moveProficiency[slot_], next = Game::MarketTutor::trainedProficiency(p);
            std::snprintf(l1, sizeof(l1), MarketTutorText::TRAIN_CHANGE, p, next);
            std::snprintf(l2, sizeof(l2), MarketTutorText::TRAIN_CHANCE,
                          BattleSystem::proficiencyTriggerChance(p), BattleSystem::proficiencyTriggerChance(next));
            std::snprintf(n1, sizeof(n1), MarketTutorText::TRAIN_QUOTA, who);
            const unsigned cost = Game::MarketTutor::trainStep(p).cost;
            std::snprintf(n2, sizeof(n2), MarketTutorText::COINS_CHANGE, static_cast<unsigned long>(s.coins),
                          static_cast<unsigned long>(s.coins >= cost ? s.coins - cost : 0));
            std::snprintf(label, sizeof(label), MarketTutorText::TRAIN_BUTTON, cost);
        } else {
            if (mode_ == Mode::RECALL) scaleIcon(20, 220);
            if (mode_ == Mode::TEACH) std::snprintf(title, sizeof(title), "%s", move ? move->name : "");
            else std::snprintf(title, sizeof(title), MarketTutorText::RECALL_CONFIRM_TITLE, move ? move->name : "");
            if (old) {
                std::snprintf(l1, sizeof(l1), MarketTutorText::TEACH_LINE1, who, old->name);
                std::snprintf(l2, sizeof(l2), mode_ == Mode::TEACH ? MarketTutorText::TEACH_LINE2 : MarketTutorText::RECALL_LINE2, move ? move->name : "");
            } else {
                std::snprintf(l1, sizeof(l1), mode_ == Mode::TEACH ? MarketTutorText::TEACH_EMPTY : MarketTutorText::RECALL_EMPTY, who, move ? move->name : "");
            }
            std::snprintf(n1, sizeof(n1), MarketTutorText::RESET_NOTE, old ? m.moveProficiency[slot_] : 0);
            if (mode_ == Mode::TEACH) {
                const unsigned price = Game::MarketTutor::lessonPrice(move ? move->id : 0);
                std::snprintf(n2, sizeof(n2), MarketTutorText::COINS_CHANGE, static_cast<unsigned long>(s.coins),
                              static_cast<unsigned long>(s.coins >= price ? s.coins - price : 0));
                std::snprintf(label, sizeof(label), MarketTutorText::TEACH_BUTTON, price);
            } else {
                const unsigned have = Game::ItemInventory::count(s, Game::ItemId::HEART_SCALE);
                std::snprintf(n2, sizeof(n2), MarketTutorText::SCALE_CHANGE, have, have ? have - 1 : 0);
                std::snprintf(label, sizeof(label), "%s", MarketTutorText::RECALL_BUTTON);
            }
        }
        const int titleSize = textWidth(title, 32) > 264 ? 16 : 32;
        const int titleX = mode_ == Mode::TEACH ? (W - 48 - 12 - textWidth(title, titleSize)) / 2 : 20;
        if (mode_ == Mode::TEACH) discIcon(titleX, 220, move ? move->type : TypeId::GHOST);
        text(mode_ == Mode::TEACH ? titleX + 60 : 80, 220 + (32 - titleSize) / 2, title, titleSize);
        if (mode_ == Mode::TRAIN) {
            text(24, 272, l1);
            text(24, 310, l2, 16);
            text(24, 332, n1, 16);
            text(24, 354, n2, 16);
        } else {
            char body[192]; std::snprintf(body, sizeof(body), "%s%s%s", l1, l2[0] ? "\n" : "", l2);
            const int lines = mode_ == Mode::TEACH ? 3 : 2;
            const int size = wrappedLines(body, 320, 32) <= lines ? 32 : 16;
            const int count = wrappedLines(body, 320, size);
            const int bodyY = mode_ == Mode::TEACH ? 270 + (96 - count * (size + 2)) / 2 : 270;
            wrapText(24, bodyY, body, 320, size == 32 ? lines : mode_ == Mode::TEACH ? 5 : 3, size,
                     mode_ == Mode::TEACH);
            if (mode_ == Mode::RECALL) {
                text(24, 338, n1, 16);
                text(24, 356, n2, 16);
            }
        }
        const char* why = nullptr; unsigned long need = 0;
        const bool disabled = confirmDisabled(s, &why, &need);
        char reasonText[64];
        if (why == MarketTutorText::NO_COINS) std::snprintf(reasonText, sizeof(reasonText), why, need);
        else std::snprintf(reasonText, sizeof(reasonText), "%s", why ? why : "");
        const char* shown = error_ ? error_ : disabled ? reasonText : nullptr;
        if (shown && *shown) ctext(374, shown, 16, red());
        button(back(), MarketTutorText::BACK, false);
        button(primary(), label, true, disabled || pending_);
    }

    void renderResult(const Game::GameState& s) const {
        const MoveInfo* move = findMove(chosenMove(s));
        if (mode_ == Mode::TEACH) discIcon(160, 220, move ? move->type : TypeId::GHOST);
        else if (mode_ == Mode::TRAIN) trainIcon(160, 220);
        else scaleIcon(160, 220);
        const int size = wrappedLines(result1_, 320, 32) <= 2 ? 32 : 16;
        const int lines = wrappedLines(result1_, 320, size);
        wrapText(24, 278 + (68 - lines * (size + 2)) / 2, result1_, 320, size == 32 ? 2 : 3, size, true);
        if (result2_[0]) ctext(354, result2_, 16);
        button(wide(), MarketTutorText::HOME, true);
    }

    void renderMembers(const Game::GameState& s, bool training) const {
        text(24, 222, training ? MarketTutorText::MENU_TRAIN : MarketTutorText::MENU_RECALL);
        for (int i = 0; i < members(s); ++i) memberCard(s, i, memberEnabled(s, i));
        button(footer(), MarketTutorText::BACK, false);
    }

    void renderTrainMoves(const Game::GameState& s) const {
        const auto& m = s.team[member_];
        for (uint8_t slot = 1; slot < 3; ++slot) {
            const MoveInfo* move = findMove(slotMove(m, slot));
            const Rect r = moveRow(slot - 1);
            rowFrame(r, slot == slot_ && trainable(s, slot));
            if (!move) { rowText(r, 24, MarketTutorText::EMPTY, "", "", true); continue; }
            const uint8_t p = m.moveProficiency[slot];
            if (p >= Game::MOVE_PROFICIENCY_MAX) {
                rowText(r, 24, move->name, MarketTutorText::TRAIN_MAXED, MarketTutorText::MAXED, true, 0, 16);
                continue;
            }
            char sub[64], right[24];
            const auto step = Game::MarketTutor::trainStep(p);
            std::snprintf(sub, sizeof(sub), MarketTutorText::TRAIN_PROFICIENCY, p, Game::MarketTutor::trainedProficiency(p));
            std::snprintf(right, sizeof(right), MarketTutorText::TRAIN_COST, step.cost);
            rowText(r, 24, move->name, sub, right, false, 0, 16);
        }
        char label[24];
        const auto cost = Game::MarketTutor::trainStep(m.moveProficiency[slot_]).cost;
        std::snprintf(label, sizeof(label), MarketTutorText::TRAIN_BUTTON, cost);
        const char* why = nullptr; unsigned long need = 0;
        const bool disabled = !trainable(s, slot_) || confirmDisabled(s, &why, &need);
        char reasonText[64];
        if (why == MarketTutorText::NO_COINS) std::snprintf(reasonText, sizeof(reasonText), why, need);
        else std::snprintf(reasonText, sizeof(reasonText), "%s", why ? why : "");
        const char* shown = error_ ? error_ : disabled ? reasonText : nullptr;
        if (shown && *shown) ctext(230, shown, 16, red());
        else text(24, 222, MarketTutorText::TRAIN_MOVE_TITLE);
        button(back(), MarketTutorText::BACK, false);
        button(primary(), label, true, disabled || pending_);
    }

    void renderPerformance(const Game::GameState& s) const {
        const bool learning = page_ == Page::TEACHING;
        ctext(232, learning ? MarketTutorText::LEARNING : MarketTutorText::TRAINING);
        MarketTutorPerformance::render(learning ? MarketTutorPerformance::learningStyle(findMove(chosenMove(s))) :
                                                 MarketTutorPerformance::trainingStyle(findMove(chosenMove(s))),
                                       showMs_, s.team[member_].speciesId);
    }

    void renderRecallList(const Game::GameState& s) const {
        const auto& m = s.team[member_];
        Game::MoveId moves[Game::MoveManagementService::MAX_RECALLABLE_MOVE_COUNT];
        const uint8_t n = recallList(s, moves);
        auto& c = PixelRenderer::canvas();
        const int left = c.clipLeft(), top = c.clipTop(), right = c.clipRight(), bottom = c.clipBottom();
        c.setClipRect(left, std::max(top, LIST_TOP + panelOffset()), right - left,
                      std::max(0, std::min(bottom, LIST_BOTTOM + panelOffset()) - std::max(top, LIST_TOP + panelOffset())));
        const Species* sp = findSpecies(m.speciesId);
        for (uint8_t i = 0; i < n; ++i) {
            const int y = LIST_TOP + i * ROW_H - scroll_;
            if (y + ROW_H <= LIST_TOP || y >= LIST_BOTTOM) continue;
            const MoveInfo* move = findMove(moves[i]);
            char sub[96]; moveInfoLine(sub, sizeof(sub), move, false);
            const uint8_t level = sp ? moveLearnLevelForSpecies(*sp, moves[i]) : 0;
            if (level) { const size_t used = std::strlen(sub); std::snprintf(sub + used, sizeof(sub) - used, " / Lv%u", level); }
            const Rect r{10, y, 348, ROW_H};
            rowFrame(r, false);
            rowText(r, 24, move ? move->name : "", sub, "", false);
        }
        c.setClipRect(left, top, right - left, bottom - top);
        const int total = n * ROW_H, view = LIST_BOTTOM - LIST_TOP;
        if (total > view) {
            const int bar = std::max(24, view * view / total);
            const int pos = LIST_TOP + (view - bar) * scroll_ / std::max(1, total - view);
            c.fillRect(360, LIST_TOP + panelOffset(), 3, view, line());
            c.fillRect(359, pos + panelOffset(), 5, bar, edge());
        }
        button(footer(), MarketTutorText::BACK, false);
    }
};
} // namespace Ui
