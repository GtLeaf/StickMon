#pragma once

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "assets/PokemonSprites.h"
#include "core/TouchTap.h"
#include "core/UiStrings.h"
#include "game/MarketCareService.h"
#include "game/Species.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TextBlock.h"
#include "core/BuildConfig.h"
#include "ui/common/MarketStallMotion.h"
#include "ui/common/MarketUiFeedback.h"
#include "ui/common/MarketCoinHud.h"
#include "ui/common/MarketListScroll.h"
#include "ui/common/MarketView.h"

// Native360 round Chansey UI: greeting, a
// three-service menu, partner lists, confirmation, a free relationship check
// and a short performance in the dialogue box. The controller owns page state
// and hit geometry; the application applies a returned Request, saves, and
// reports back through committed(), so a failed save never shows success.
// The performance starts only after the save, so skipping it is harmless.
namespace Ui {
class RoundMarketCareUi {
public:
    enum class Page : uint8_t { CLOSED, GREET, MENU, LIST, CONFIRM, EVAL, SHOW };
    enum class Mode : uint8_t { MASSAGE, TEA, EVAL };
    using Target = Game::MarketCare::Target;
    struct Request {
        enum Kind : uint8_t { NONE, MASSAGE, TEA } kind = NONE;
        Target target{};
        uint32_t period = 0, iv=0, metAt=0;
    };
    struct Rect { int x, y, w, h; bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; } };

    static constexpr int W = 360, H = 360, PANEL_Y = 144, ROW_Y = 156, ROW_H = 56, MENU_ROW_H = 46, GREET_H = 136;
    static constexpr int LIST_TOP = 152, LIST_BOTTOM = 304;
    static constexpr uint32_t GREET_HOLD_MS = 600, GREET_IN_MS = 360, OPEN_MS = 420, CLOSE_MS = 520,
                              SHRINK_MS = 360, SHOW_MS = 1200, ERROR_MS = 1800;

    bool takeSfx(SfxCue& cue) { return feedback_.takeSfx(cue); }
    bool active() const { return page_ != Page::CLOSED; }
    Page page() const { return page_; }
    Mode mode() const { return mode_; }
    bool pending() const { return pending_; }
    bool animating() const { return transition_ != Transition::NONE; }
    bool showDone() const { return page_ == Page::SHOW && !animating() && showMs_ >= SHOW_MS; }
    Target target() const { return target_; }
    int scrollOffset() const {return scroll_;}
    int panelTop() const {
        if (page_ == Page::GREET) return H - GREET_H + panelOffset();
        return PANEL_Y + panelOffset();
    }
    // Coin badge: hidden while greeting, slides with the panel on the first
    // open and the final close, stays put around a performance.
    int hudOffset() const {
        if (page_ == Page::GREET) return MarketStallMotion::HUD_HIDDEN;
        if (transition_ == Transition::CLOSING) return MarketStallMotion::tween(0, MarketStallMotion::HUD_HIDDEN, transitionMs_, duration());
        if (transition_ == Transition::OPENING && openFromGreet_)
            return MarketStallMotion::tween(MarketStallMotion::HUD_HIDDEN, 0, transitionMs_, duration());
        return 0;
    }

    void open(const Game::GameState& state) {
        *this = RoundMarketCareUi{};
        if(!BuildConfig::DEBUG_FEATURES)return;
        page_ = Page::GREET; transition_ = Transition::GREET_IN;
        period_ = Game::MarketCare::periodDay(state);
        std::snprintf(greet_, sizeof(greet_), "%s%s", MarketCareText::SPEAKER,
                      Game::MarketCare::remaining(state) ? MarketCareText::GREET : MarketCareText::GREET_FULL);
    }
    void close() { feedback_.clear(); page_ = Page::CLOSED; transition_ = Transition::NONE; cancelTouch(); pending_ = false; }
    void cancelTouch() { tap_.cancel(); scrollMotion_.cancel(); }

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
                else { transition_ = Transition::NONE; transitionMs_ = 0; }
            }
            return true;
        }
        if (page_ == Page::SHOW) {
            if(showMs_>=SHOW_MS)return changed;
            showMs_=std::min(SHOW_MS,showMs_+deltaMs);
            if(showMs_==SHOW_MS)feedback_.set(SfxCue::UI_CONFIRM);
            return true;
        }
        if (page_ == Page::LIST)
            return scrollMotion_.update(deltaMs, scrollMaximum_, scroll_) || changed;
        scrollMotion_.cancel();
        if (page_ != Page::GREET) return changed;
        greetMs_ += deltaMs;
        if (greetMs_ < GREET_HOLD_MS + 40U * uint32_t(std::strlen(greet_) / 3)) return changed;
        openPanel(); return true;
    }

    // Camera shift that keeps the facing actors above the box or panel.
    int sceneShift(const Market::Session& s, const MarketView::Viewport& v) const {
        const int camera = v.cameraY(s);
        int lowest = -100000;
        for (int role = 0; role < 4; ++role) {
            const int i = s.conversationActor(role);
            if (i >= 0) lowest = std::max(lowest, v.pixel(s.pose(i).y) - camera);
        }
        if (lowest < -10000) return 0;
        const int greeting = std::clamp(lowest - (H - 148), 0, 120);
        const int opened = std::clamp(lowest - (PANEL_Y - 28), -240, 240);
        switch (transition_) {
        case Transition::GREET_IN: return tween(0, greeting);
        case Transition::OPENING: return tween(greeting, opened);
        case Transition::CLOSING: return tween(opened, 0);
        case Transition::SHRINKING: return tween(opened, greeting);
        default: return page_ == Page::GREET ? greeting : opened;
        }
    }

    Request touch(const Platform::IInputDevice::TouchEvent& e, const Game::GameState& state) {
        using Type = Platform::IInputDevice::TouchType;
        Request none{};
        if (pending_ || page_ == Page::CLOSED || animating()) { cancelTouch(); return none; }
        if ((int(e.x)-180)*(int(e.x)-180)+(int(e.y)-180)*(int(e.y)-180)>=180*180) {cancelTouch();return none;}
        if(page_==Page::GREET) {if(tap_.update(e,0)==0){feedback_.set(SfxCue::UI_CONFIRM);openPanel();}return none;}
        if(page_==Page::SHOW) {
            const bool done=showMs_>=SHOW_MS;
            const int target=done?(wide().contains(e.x,e.y)||e.y<PANEL_Y?PRIMARY:-1):(e.y>=PANEL_Y?PRIMARY:-1);
            if(tap_.update(e,target)==PRIMARY){feedback_.set(SfxCue::UI_CONFIRM);tap_.cancel();if(!done)showMs_=SHOW_MS;else {page_=Page::MENU;transition_=Transition::NONE;}}
            return none;
        }
        if (page_ == Page::LIST && listDrag(e, state)) return none;
        const int hitTarget = e.type == Type::DOWN || e.type == Type::UP ? hit(e.x, e.y, state) : -1;
        const int clicked = tap_.update(e, hitTarget);
        if (clicked < 0) return none;
        // The exposed market above the panel acts as the page's own way back,
        // like the other stalls: close on the menu, back elsewhere.
        if (clicked == OUTSIDE) return activate(page_ == Page::MENU ? CLOSE : page_ == Page::LIST ? FOOTER : BACK, state);
        return activate(clicked, state);
    }

    // Result of the application's save for the last request.
    void committed(bool saved, const Game::GameState& state) {
        pending_ = false;
        if (!saved) { feedback_.set(SfxCue::UI_CANCEL); error_ = MarketCareText::SAVE_FAILED; errorMs_ = 0; return; }
        error_ = nullptr;
        const Game::MonsterRuntime* m = Game::MarketCare::monster(state, target_);
        if (m) { afterAffection_ = m->affection; afterBond_ = m->bond; }
        page_ = Page::SHOW; transition_ = Transition::NONE; transitionMs_ = 0; showMs_ = 0;
    }
    void rejected(Game::MarketCare::Result r) { pending_ = false; feedback_.set(SfxCue::UI_CANCEL); error_ = reason(r); errorMs_ = 0; }

    void render(const Game::GameState& state, const MarketCoinMotion* coins = nullptr) const {
        if (page_ == Page::CLOSED) return;
        if (page_ == Page::GREET) { renderGreet(); return; }
        if (page_ == Page::SHOW) { renderShow(state); renderHud(state,coins); return; }
        panel(PANEL_Y + panelOffset());
        switch (page_) {
        case Page::MENU: renderMenu(state); break;
        case Page::LIST: renderList(state); break;
        case Page::CONFIRM: renderConfirm(state); break;
        case Page::EVAL: renderEval(state); break;
        default: break;
        }
        renderHud(state,coins);
    }

private:
    enum class Transition : uint8_t { NONE, GREET_IN, OPENING, CLOSING, SHRINKING };
    enum Target_ { CLOSE = 10, FOOTER, BACK, PRIMARY, OUTSIDE, DISABLED, ROW = 20 };
    Page page_ = Page::CLOSED;
    Mode mode_ = Mode::MASSAGE;
    Transition transition_ = Transition::NONE;
    uint32_t transitionMs_ = 0, greetMs_ = 0, showMs_ = 0, errorMs_ = 0, period_ = 0;
    TouchTap tap_;
    MarketUiFeedback feedback_;
    Target target_{};
    uint32_t targetIv_=0,targetMetAt_=0;
    mutable TextBlock bodyText_;
    int scroll_ = 0, scrollMaximum_ = 0;
    MarketListScroll scrollMotion_;
    bool pending_ = false, fromEval_ = false, openFromGreet_ = true;
    uint8_t beforeAffection_ = 0, afterAffection_ = 0;
    int8_t beforeBond_ = 0, afterBond_ = 0;
    bool evolveHint_ = false;
    const char* error_ = nullptr;
    char greet_[128]{};

    uint32_t duration() const {
        switch (transition_) {
        case Transition::GREET_IN: return GREET_IN_MS;
        case Transition::CLOSING: return CLOSE_MS;
        case Transition::SHRINKING: return SHRINK_MS;
        default: return OPEN_MS;
        }
    }
    int tween(int from, int to) const {
        return MarketStallMotion::tween(from, to, transitionMs_, duration());
    }
    // The greeting box rises from the bottom edge, grows into the panel,
    // shrinks back into the box for a performance and sinks away on close.
    int panelOffset() const {
        switch (transition_) {
        case Transition::GREET_IN: return tween(GREET_H, 0);
        case Transition::OPENING: return tween(H - GREET_H - PANEL_Y, 0);
        case Transition::CLOSING: return tween(0, H - PANEL_Y);
        case Transition::SHRINKING: return tween(0, H - GREET_H - PANEL_Y);
        default: return 0;
        }
    }
    void openPanel() { page_ = Page::MENU; transition_ = Transition::OPENING; transitionMs_ = 0; openFromGreet_ = true; tap_.cancel(); }
    void showError(const char* text) { feedback_.set(SfxCue::UI_CANCEL); error_ = text; errorMs_ = ERROR_MS; }

    // ---- geometry shared by hit testing and drawing
    static Rect menuRow(int i) { return {64, ROW_Y + i * MENU_ROW_H, 232, MENU_ROW_H}; }
    static Rect wide() { return {80, 288, 200, 40}; }

    static Game::MarketCare::Service service(Mode m) {
        return m == Mode::TEA ? Game::MarketCare::Service::TEA : Game::MarketCare::Service::MASSAGE;
    }
    uint8_t list(const Game::GameState& s, Target* out) const {
        return Game::MarketCare::targets(s, out, mode_ == Mode::MASSAGE);
    }
    Game::MarketCare::Result rowState(const Game::GameState& s, Target t) const {
        return Game::MarketCare::check(s, service(mode_), t, period_);
    }
    bool rowEnabled(const Game::GameState& s, Target t) const {
        if (mode_ == Mode::EVAL) return true;
        const auto r = rowState(s, t);
        return r == Game::MarketCare::Result::OK || r == Game::MarketCare::Result::NO_COINS;
    }
    int maxScroll(const Game::GameState& s) const {
        Target items[Game::MarketCare::TARGET_CAP];
        return std::max(0, list(s, items) * ROW_H - (LIST_BOTTOM - LIST_TOP));
    }
    bool listDrag(const Platform::IInputDevice::TouchEvent& e, const Game::GameState& s) {
        scrollMaximum_ = maxScroll(s);
        return scrollMotion_.touch(e, {60, LIST_TOP, 240, LIST_BOTTOM - LIST_TOP},
                                   scrollMaximum_, scroll_, tap_);
    }

    static const char* reason(Game::MarketCare::Result r, Mode mode = Mode::MASSAGE) {
        using R = Game::MarketCare::Result;
        switch (r) {
        case R::STALE: return MarketCareText::WHY_STALE;
        case R::VISITOR: return MarketCareText::WHY_VISITOR;
        case R::FAINTED: return MarketCareText::WHY_FAINTED;
        case R::FULL: return mode == Mode::TEA ? MarketCareText::WHY_FULL_BOND : MarketCareText::WHY_FULL_AFFECTION;
        case R::FIRST_INVITE: return MarketCareText::WHY_FIRST;
        case R::USED: return MarketCareText::WHY_USED;
        case R::QUOTA_FULL: return MarketCareText::WHY_QUOTA;
        default: return MarketCareText::UNAVAILABLE;
        }
    }
    // Would this massage cross into a friendship evolution?
    static bool evolveAfterMassage(const Game::GameState& s, const Game::MonsterRuntime& m) {
        const Species* sp = findSpecies(m.speciesId);
        if (!sp || sp->evolveMethod == EvolutionMethod::LEVEL) return false;
        Game::MonsterRuntime after = m; after.affection = Game::MarketCare::affectionAfter(m);
        return !levelUpEvolutionTarget(*sp, m, s.gameMinutesTotal) && levelUpEvolutionTarget(*sp, after, s.gameMinutesTotal);
    }
    void beginConfirm(const Game::GameState& s, Mode m) {
        mode_ = m; page_ = Page::CONFIRM; error_ = nullptr; errorMs_ = 0;
        const Game::MonsterRuntime* mon = Game::MarketCare::monster(s, target_);
        if (!mon||mon->ivPacked!=targetIv_||mon->metAt!=targetMetAt_) {page_=Page::LIST;showError(MarketCareText::UNAVAILABLE);return;}
        beforeAffection_ = mon->affection; beforeBond_ = mon->bond;
        evolveHint_ = m == Mode::MASSAGE && evolveAfterMassage(s, *mon);
    }
    // Recommended service for the evaluation page, or EVAL when none is open.
    Mode recommended(const Game::GameState& s) const {
        using A = Game::MarketCare::Advice;
        using R = Game::MarketCare::Result;
        const A a = Game::MarketCare::advice(s, target_);
        const auto ok = [&](Mode m) {
            const R r = Game::MarketCare::check(s, service(m), target_, period_);
            return r == R::OK || r == R::NO_COINS;
        };
        const Mode first = a == A::TEA ? Mode::TEA : Mode::MASSAGE;
        const Mode second = first == Mode::TEA ? Mode::MASSAGE : Mode::TEA;
        if (ok(first)) return first;
        if (ok(second)) return second;
        return Mode::EVAL;
    }

    int hit(int x, int y, const Game::GameState& s) const {
        if (x >= 0 && x < W && y >= 0 && y < PANEL_Y) return OUTSIDE;
        switch (page_) {
        case Page::MENU:
            for (int i = 0; i < 3; ++i) if (menuRow(i).contains(x, y)) return ROW + i;
            return -1;
        case Page::LIST: {
            if (y < LIST_TOP || y >= LIST_BOTTOM || x < 60 || x >= 300) return -1;
            Target items[Game::MarketCare::TARGET_CAP];
            const int n = list(s, items);
            const int index = (y - LIST_TOP + scroll_) / ROW_H;
            return index < n ? (rowEnabled(s, items[index]) ? ROW + index : DISABLED) : -1;
        }
        case Page::CONFIRM:
            return wide().contains(x, y) ? (Game::MarketCare::check(s, service(mode_), target_, period_) ==
                Game::MarketCare::Result::OK ? PRIMARY : DISABLED) : -1;
        case Page::EVAL:
            if (recommended(s) == Mode::EVAL) return -1;
            return wide().contains(x, y) ? PRIMARY : -1;
        default: return -1;
        }
    }

    Request activate(int hitTarget, const Game::GameState& s) {
        Request r{};
        scrollMotion_.cancel();
        feedback_.set(hitTarget == CLOSE || hitTarget == FOOTER || hitTarget == BACK ? SfxCue::UI_CANCEL : SfxCue::UI_CONFIRM);
        if (hitTarget == DISABLED) { feedback_.set(SfxCue::UI_CANCEL); return r; }
        switch (page_) {
        case Page::MENU: {
            if (hitTarget == CLOSE) { transition_ = Transition::CLOSING; transitionMs_ = 0; tap_.cancel(); return r; }
            const int i = hitTarget - ROW;
            error_ = nullptr; errorMs_ = 0;
            if (i < 2) {
                const auto svc = i == 0 ? Game::MarketCare::Service::MASSAGE : Game::MarketCare::Service::TEA;
                if (!Game::MarketCare::remaining(s)) { showError(MarketCareText::QUOTA_EMPTY); return r; }
                if (!Game::MarketCare::anyEligible(s, svc)) {
                    showError(i == 0 ? MarketCareText::NOBODY_MASSAGE : MarketCareText::NOBODY_TEA); return r;
                }
            }
            mode_ = i == 0 ? Mode::MASSAGE : i == 1 ? Mode::TEA : Mode::EVAL;
            page_ = Page::LIST; scroll_ = 0; return r;
        }
        case Page::LIST: {
            if (hitTarget == FOOTER) { page_ = Page::MENU; return r; }
            Target items[Game::MarketCare::TARGET_CAP];
            list(s, items);
            target_ = items[hitTarget - ROW];
            const auto* selected=Game::MarketCare::monster(s,target_);
            if(!selected)return r;
            targetIv_=selected->ivPacked;targetMetAt_=selected->metAt;
            if (mode_ == Mode::EVAL) { page_ = Page::EVAL; return r; }
            fromEval_ = false; beginConfirm(s, mode_); return r;
        }
        case Page::CONFIRM:
            if (hitTarget == BACK) {
                error_ = nullptr;
                if (fromEval_) { mode_ = Mode::EVAL; page_ = Page::EVAL; }
                else page_ = Page::LIST;
                return r;
            }
            feedback_.clear(); pending_ = true;
            r.kind = mode_ == Mode::TEA ? Request::TEA : Request::MASSAGE;
            r.target = target_; r.period = period_;r.iv=targetIv_;r.metAt=targetMetAt_;
            return r;
        case Page::EVAL:
            if (hitTarget == BACK) { page_ = Page::LIST; return r; }
            fromEval_ = true; beginConfirm(s, recommended(s)); return r;
        default: return r;
        }
    }

    // ---- drawing helpers (palette and metrics match MarketTutorUi)
    static uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return PixelRenderer::rgb(r, g, b); }
    static uint16_t paper() { return rgb(248, 248, 224); }
    static uint16_t ink() { return rgb(40, 56, 80); }
    static uint16_t edge() { return rgb(48, 64, 112); }
    static uint16_t light() { return rgb(128, 152, 184); }
    static uint16_t muted() { return rgb(89, 105, 88); }
    static uint16_t line() { return rgb(186, 196, 160); }
    static uint16_t red() { return rgb(183, 56, 56); }
    static uint16_t off() { return rgb(225, 227, 207); }
    static uint16_t pink() { return rgb(232, 96, 128); }
    static uint16_t good() { return rgb(72, 152, 88); }

    static int textWidth(const char* s, int size) {
        int w = 0;
        for (const auto* p = reinterpret_cast<const uint8_t*>(s); *p;) {
            const int n = *p < 0x80 ? 1 : *p < 0xe0 ? 2 : *p < 0xf0 ? 3 : 4;
            w += n == 1 ? size / 2 : size; p += n;
        }
        return w;
    }
    static void monsterIconAt(int x, int y, uint16_t species, float maxScale = 1.5f, int cell = 48) {
        const auto* frame = PokemonSprites::findCachedSpeciesSprite(species, PokemonSprites::SpriteKind::ICON_0);
        if (!frame) frame = PokemonSprites::findSpeciesSprite(species, PokemonSprites::SpriteKind::ICON_0);
        if (frame) {
            const int w = frame->width, h = frame->height;
            const float s = std::min(maxScale, float(cell) / std::max(w, h));
            if (PokemonSprites::drawFrameScaled(frame, x + (cell - int(w * s)) / 2, y + (cell - int(h * s)) / 2, s, false, true)) return;
        }
        PixelRenderer::canvas().fillCircle(x + cell / 2, y + cell / 2, cell / 3, line());
    }
    static void heartAt(int cx, int cy, int r, uint16_t color) {
        auto& c = PixelRenderer::canvas();
        c.fillCircle(cx - r, cy, r, color); c.fillCircle(cx + r, cy, r, color);
        c.fillTriangle(cx - 2 * r, cy + 1, cx + 2 * r, cy + 1, cx, cy + 2 * r + 2, color);
    }
    static void cupAt(int x, int y) { // 24x20 teacup, top-left at (x, y)
        auto& c = PixelRenderer::canvas();
        c.fillRoundRect(x, y + 4, 18, 14, 4, edge());
        c.fillRoundRect(x + 2, y + 6, 14, 10, 3, rgb(240, 236, 220));
        c.fillRect(x + 3, y + 6, 12, 3, rgb(176, 120, 72));
        c.drawCircle(x + 20, y + 10, 3, edge());
        c.fillRect(x - 2, y + 18, 22, 2, edge());
    }
    void menuIcon(int i, int x, int y) const {
        y += panelOffset();
        auto& c = PixelRenderer::canvas();
        if (i == 0) { heartAt(x + 24, y + 18, 8, pink()); heartAt(x + 36, y + 10, 4, rgb(248, 168, 184)); }
        else if (i == 1) { cupAt(x + 10, y + 16); c.drawLine(x + 16, y + 6, x + 18, y + 14, light()); c.drawLine(x + 24, y + 4, x + 26, y + 14, light()); }
        else {
            c.fillCircle(x + 20, y + 20, 13, edge()); c.fillCircle(x + 20, y + 20, 9, rgb(208, 228, 240));
            for (int d = -1; d <= 1; ++d) c.drawLine(x + 29 + d, y + 30, x + 40 + d, y + 41, edge());
        }
    }
    static const char* speciesName(uint16_t id) { const Species* s = findSpecies(id); return s ? s->name : ""; }
    static const char* levelName(int8_t bond) { return Storage::BOND_LEVELS[uint8_t(Game::Bond::levelFor(bond)) - 1]; }

    static void textAt(int x,int y,const char* value,int size,uint16_t color) {PixelRenderer::textSmooth(x,y,value,color?color:ink(),uint8_t(size));}
    void text(int x,int y,const char* value,int size=20,uint16_t color=0) const {textAt(x,y+panelOffset(),value,size,color);}
    void ctext(int y,const char* value,int size=20,uint16_t color=0) const {text((W-textWidth(value,size))/2,y,value,size,color);}
    void body(const char* value,int x,int y,int w,int h,bool centered=false,uint16_t color=0) const {
        TextBlock::Options o;o.width=w;o.maxHeight=h;o.font=BitmapText::Font::PX16;o.lineGap=2;o.align=centered?TextBlock::Align::CENTER:TextBlock::Align::LEFT;
        bodyText_.setText(value);bodyText_.setOptions(o);bodyText_.draw(PixelRenderer::canvas(),x,y+panelOffset(),color?color:ink());
    }
    static void panel(int y) {auto& c=PixelRenderer::canvas();c.fillRoundRect(4,y,352,232,18,paper());c.drawRoundRect(4,y,352,232,18,edge());}
    void button(Rect r,const char* label,bool primaryStyle,bool disabled=false) const {
        auto& c=PixelRenderer::canvas();const int y=r.y+panelOffset();c.fillRoundRect(r.x,y,r.w,r.h,8,disabled?off():primaryStyle?edge():paper());
        c.drawRoundRect(r.x,y,r.w,r.h,8,disabled?line():edge());text(r.x+(r.w-textWidth(label,20))/2,r.y+(r.h-20)/2,label,20,disabled?muted():primaryStyle?paper():ink());
    }
    void renderHud(const Game::GameState& s,const MarketCoinMotion* coins) const {
        const int y=52+hudOffset();auto& c=PixelRenderer::canvas();c.fillRoundRect(56,y-4,96,32,6,paper());
        PixelRenderer::textSmooth(64,y+6,MarketCareText::TITLE,ink(),20);MarketCoinHud::render(s.coins,coins,y,20,{288,24,32,36});
    }
    void renderGreet() const {panel(H-GREET_H+panelOffset());body(greet_,72,242,216,70,true);}
    void renderMenu(const Game::GameState& s) const {
        const char* labels[]={MarketCareText::MENU_MASSAGE,MarketCareText::MENU_TEA,MarketCareText::MENU_EVAL};
        for(int i=0;i<3;++i){const auto r=menuRow(i);const bool enabled=i==2||Game::MarketCare::anyEligible(s,service(i?Mode::TEA:Mode::MASSAGE));
            menuIcon(i,72,r.y-2);text(126,r.y+13,labels[i],20,enabled?ink():muted());
            if(i<2)PixelRenderer::canvas().drawFastHLine(68,r.y+r.h-1+panelOffset(),224,line());}
        char value[64];std::snprintf(value,sizeof(value),MarketCareText::QUOTA,Game::MarketCare::remaining(s),Game::MARKET_CARE_PERIOD_CAP);
        if(error_)body(error_,92,298,176,36,true,red());
        else body(value,72,306,216,18,true,muted());
    }
    void renderList(const Game::GameState& s) const {
        Target items[Game::MarketCare::TARGET_CAP];const int n=list(s,items);auto& c=PixelRenderer::canvas();
        const int l=c.clipLeft(),t=c.clipTop(),r=c.clipRight(),b=c.clipBottom(),x=std::max(l,60),y=std::max(t,LIST_TOP+panelOffset());
        c.setClipRect(x,y,std::max(0,std::min(r,300)-x),std::max(0,std::min(b,LIST_BOTTOM+panelOffset())-y));
        for(int i=0;i<n;++i){const int top=LIST_TOP+i*ROW_H-scroll_;if(top+ROW_H<=LIST_TOP||top>=LIST_BOTTOM)continue;
            const auto* m=Game::MarketCare::monster(s,items[i]);if(!m)continue;const bool enabled=rowEnabled(s,items[i]);
            monsterIconAt(68,top+8+panelOffset(),m->speciesId,1.5f,40);text(118,top+4,speciesName(m->speciesId),20,enabled?ink():muted());
            char sub[64];
            if(!enabled)std::snprintf(sub,sizeof(sub),"%s",reason(rowState(s,items[i]),mode_));
            else if(mode_==Mode::MASSAGE)std::snprintf(sub,sizeof(sub),MarketCareText::AFFECTION_CHANGE,m->affection,Game::MarketCare::affectionAfter(*m));
            else std::snprintf(sub,sizeof(sub),MarketCareText::BOND_SUB,levelName(m->bond),int(m->bond));
            body(sub,118,top+31,174,16,false,enabled?muted():red());c.drawFastHLine(68,top+ROW_H-1+panelOffset(),224,line());
            if(!enabled)c.blendRectAlphaNative(68,top+8+panelOffset(),40,40,off(),100);
        }c.setClipRect(l,t,r-l,b-t);
        const int h=LIST_BOTTOM-LIST_TOP;if(n*ROW_H>h){const int thumb=std::max(16,h*h/(n*ROW_H));c.fillRect(299,LIST_TOP+scroll_*(h-thumb)/std::max(1,n*ROW_H-h)+panelOffset(),3,thumb,line());}
    }
    void renderConfirm(const Game::GameState& s) const {
        const auto* m=Game::MarketCare::monster(s,target_);if(!m)return;
        monsterIconAt(68,150+panelOffset(),m->speciesId,1.5f,40);char title[80],a[64],b[64],info[128]{};
        std::snprintf(title,sizeof(title),mode_==Mode::TEA?MarketCareText::TEA_TITLE:MarketCareText::MASSAGE_TITLE,speciesName(m->speciesId));text(118,158,title);
        if(mode_==Mode::MASSAGE){std::snprintf(a,sizeof(a),MarketCareText::AFFECTION_CHANGE,m->affection,Game::MarketCare::affectionAfter(*m));std::snprintf(b,sizeof(b),MarketCareText::MOOD_CHANGE,m->mood,Game::MarketCare::moodAfter(*m));if(evolveHint_)std::snprintf(info,sizeof(info),"%s",MarketCareText::EVOLVE_HINT);}
        else {std::snprintf(a,sizeof(a),MarketCareText::BOND_CHANGE,int(m->bond),int(Game::MarketCare::bondAfter(*m)));std::snprintf(b,sizeof(b),MarketCareText::LEVEL_CHANGE,levelName(m->bond),levelName(Game::MarketCare::bondAfter(*m)));if(target_.kind==Target::CONTACT)std::snprintf(info,sizeof(info),MarketCareText::INVITE_CHANCE,Game::MarketCare::inviteChance(target_,m->bond),Game::MarketCare::inviteChance(target_,Game::MarketCare::bondAfter(*m)));}
        text(68,198,a,16);text(68,220,b,16);
        const auto result=Game::MarketCare::check(s,service(mode_),target_,period_);const auto price=Game::MarketCare::price(service(mode_));char why[96]{};
        if(result==Game::MarketCare::Result::NO_COINS)std::snprintf(why,sizeof(why),MarketCareText::NO_COINS,static_cast<unsigned long>(price-s.coins));
        else if(result!=Game::MarketCare::Result::OK)std::snprintf(why,sizeof(why),"%s",reason(result,mode_));
        if(error_||why[0])body(error_?error_:why,68,242,224,36,true,red());
        else body(info,68,242,224,36,false,muted());
        char label[32];std::snprintf(label,sizeof(label),mode_==Mode::TEA?MarketCareText::TEA_BUTTON:MarketCareText::MASSAGE_BUTTON,price);
        button(wide(),label,true,result!=Game::MarketCare::Result::OK||pending_);
    }
    void renderEval(const Game::GameState& s) const {
        const auto* m=Game::MarketCare::monster(s,target_);if(!m)return;
        monsterIconAt(68,150+panelOffset(),m->speciesId,1.5f,40);text(118,158,speciesName(m->speciesId));char value[80];
        std::snprintf(value,sizeof(value),MarketCareText::AFFECTION,m->affection);text(68,198,value,16);
        auto& c=PixelRenderer::canvas();c.drawRect(68,220+panelOffset(),224,6,edge());c.fillRect(70,222+panelOffset(),220*m->affection/255,2,pink());
        std::snprintf(value,sizeof(value),MarketCareText::BOND,levelName(m->bond),int(m->bond));text(68,232,value,16);
        body(MarketCareText::ADVICE[uint8_t(Game::MarketCare::advice(s,target_))],68,252,224,34,false,muted());
        const Mode next=recommended(s);if(next!=Mode::EVAL)button(wide(),next==Mode::TEA?MarketCareText::BOOK_TEA:MarketCareText::BOOK_MASSAGE,true);
    }
    void renderShow(const Game::GameState& s) const {
        panel(PANEL_Y);const auto* m=Game::MarketCare::monster(s,target_);const bool tea=mode_==Mode::TEA,performing=showMs_<SHOW_MS;
        static constexpr int HOP[]={0,-3,-5,-3};const int a=performing?HOP[showMs_/120%4]:0,b=performing?HOP[(showMs_/120+2)%4]:0;
        monsterIconAt(80,184+a,113,2,64);if(m)monsterIconAt(216,184+b,m->speciesId,2,64);if(tea)cupAt(169,224);
        if(performing){ctext(154,tea?MarketCareText::TEA_TIME:MarketCareText::MASSAGING);for(int i=0;i<3;++i){const int t=int(showMs_)-i*300;if(t>=0&&t<700)heartAt(180+(i-1)*12,212-t*30/700,4,pink());}return;}
        char gain[48],result[96];if(tea){std::snprintf(gain,sizeof(gain),MarketCareText::BOND_GAIN,int(afterBond_)-int(beforeBond_));std::snprintf(result,sizeof(result),"%s",MarketCareText::TEA_LINES[uint8_t(Game::Bond::levelFor(afterBond_))-1]);}
        else {std::snprintf(gain,sizeof(gain),MarketCareText::AFFECTION_GAIN,unsigned(afterAffection_-beforeAffection_));std::snprintf(result,sizeof(result),MarketCareText::RELAXED,m?speciesName(m->speciesId):"");}
        char complete[192];
        if(tea&&Game::Bond::levelFor(beforeBond_)!=Game::Bond::levelFor(afterBond_)){
            char level[64];std::snprintf(level,sizeof(level),MarketCareText::LEVEL_CHANGE,levelName(beforeBond_),levelName(afterBond_));
            std::snprintf(complete,sizeof(complete),"%s\n%s",level,result);
        }else if(!tea&&evolveHint_)std::snprintf(complete,sizeof(complete),"%s\n%s",MarketCareText::EVOLVE_SHORT,result);
        else std::snprintf(complete,sizeof(complete),"%s",result);
        ctext(154,gain,20,good());body(complete,68,250,224,36,true);button(wide(),"返回首页",true);
    }
};
} // namespace Ui
