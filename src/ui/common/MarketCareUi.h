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
#include "ui/common/MarketStallMotion.h"
#include "ui/common/MarketUiFeedback.h"
#include "ui/common/MarketListScroll.h"
#include "ui/common/MarketView.h"

// Chansey stall (暖心小屋) pages for the 368x448 touch layout: greeting, a
// three-service menu, partner lists, confirmation, a free relationship check
// and a short performance in the dialogue box. The controller owns page state
// and hit geometry; the application applies a returned Request, saves, and
// reports back through committed(), so a failed save never shows success.
// The performance starts only after the save, so skipping it is harmless.
namespace Ui {
class MarketCareUi {
public:
    enum class Page : uint8_t { CLOSED, GREET, MENU, LIST, CONFIRM, EVAL, SHOW };
    enum class Mode : uint8_t { MASSAGE, TEA, EVAL };
    using Target = Game::MarketCare::Target;
    struct Request {
        enum Kind : uint8_t { NONE, MASSAGE, TEA } kind = NONE;
        Target target{};
        uint32_t period = 0;
    };
    struct Rect { int x, y, w, h; bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; } };

    static constexpr int W = 368, H = 448, PANEL_Y = 168, ROW_Y = 216, ROW_H = 56, MENU_ROW_H = 68, GREET_H = 120;
    static constexpr int LIST_TOP = 216, LIST_BOTTOM = 384;
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
    int panelTop() const {
        if (page_ == Page::GREET || (page_ == Page::SHOW && !animating())) return H - GREET_H + panelOffset();
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
        *this = MarketCareUi{};
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
            // Hops and hearts while performing, then the bouncing next marker.
            const bool wasDone = showMs_ >= SHOW_MS;
            showMs_ += deltaMs;
            if (!wasDone && showMs_ >= SHOW_MS) feedback_.set(SfxCue::UI_CONFIRM);
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
        default: return page_ == Page::GREET || page_ == Page::SHOW ? greeting : opened;
        }
    }

    Request touch(const Platform::IInputDevice::TouchEvent& e, const Game::GameState& state) {
        using Type = Platform::IInputDevice::TouchType;
        Request none{};
        if (pending_ || page_ == Page::CLOSED || animating()) { cancelTouch(); return none; }
        if (page_ == Page::GREET || page_ == Page::SHOW) {
            // Lifting the finger anywhere advances; it never lands on a row.
            if (tap_.update(e, 0) != 0) return none;
            if (page_ == Page::GREET) { feedback_.set(SfxCue::UI_CONFIRM); openPanel(); }
            else if (showMs_ < SHOW_MS) { showMs_ = SHOW_MS; feedback_.set(SfxCue::UI_CONFIRM); }
            else { feedback_.set(SfxCue::UI_CONFIRM); page_ = Page::MENU; transition_ = Transition::OPENING; transitionMs_ = 0; openFromGreet_ = false; }
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
        page_ = Page::SHOW; transition_ = Transition::SHRINKING; transitionMs_ = 0; showMs_ = 0;
    }
    void rejected(Game::MarketCare::Result r) { pending_ = false; feedback_.set(SfxCue::UI_CANCEL); error_ = reason(r); errorMs_ = 0; }

    void render(const Game::GameState& state) const {
        if (page_ == Page::CLOSED) return;
        if (page_ == Page::GREET) { renderGreet(); return; }
        if (page_ == Page::SHOW) { renderShow(state); return; }
        panel(PANEL_Y + panelOffset());
        switch (page_) {
        case Page::MENU: renderMenu(state); break;
        case Page::LIST: renderList(state); break;
        case Page::CONFIRM: renderConfirm(state); break;
        case Page::EVAL: renderEval(state); break;
        default: break;
        }
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
    static Rect menuRow(int i) { return {10, ROW_Y + i * MENU_ROW_H, 348, MENU_ROW_H}; }
    static Rect footer() { return {16, 394, 336, 44}; }
    static Rect back() { return {16, 390, 160, 48}; }
    static Rect primary() { return {192, 390, 160, 48}; }
    static Rect wide() { return {16, 390, 336, 48}; }

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
        return scrollMotion_.touch(e, {10, LIST_TOP, 348, LIST_BOTTOM - LIST_TOP},
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
        if (!mon) return;
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
            if (MarketStallMotion::closeHit().contains(x, y)) return CLOSE;
            for (int i = 0; i < 3; ++i) if (menuRow(i).contains(x, y)) return ROW + i;
            return -1;
        case Page::LIST: {
            if (footer().contains(x, y)) return FOOTER;
            if (y < LIST_TOP || y >= LIST_BOTTOM || x < 10 || x >= 358) return -1;
            Target items[Game::MarketCare::TARGET_CAP];
            const int n = list(s, items);
            const int index = (y - LIST_TOP + scroll_) / ROW_H;
            return index < n ? (rowEnabled(s, items[index]) ? ROW + index : DISABLED) : -1;
        }
        case Page::CONFIRM:
            if (back().contains(x, y)) return BACK;
            return primary().contains(x, y) ? (Game::MarketCare::check(s, service(mode_), target_, period_) ==
                Game::MarketCare::Result::OK ? PRIMARY : DISABLED) : -1;
        case Page::EVAL:
            if (recommended(s) == Mode::EVAL) return wide().contains(x, y) ? BACK : -1;
            if (back().contains(x, y)) return BACK;
            return primary().contains(x, y) ? PRIMARY : -1;
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
            r.target = target_; r.period = period_;
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
    // Panel content follows the panel; `y` is the settled layout coordinate.
    void text(int x, int y, const char* s, int size = 32, uint16_t color = 0) const { textAt(x, y + panelOffset(), s, size, color); }
    static void textAt(int x, int y, const char* s, int size, uint16_t color) {
        auto& c = PixelRenderer::canvas();
        const bool native = c.nativeText();
        c.setNativeText(size == 32);
        PixelRenderer::text(x, y, s, color ? color : ink());
        c.setNativeText(native);
    }
    void rtext(int right, int y, const char* s, int size = 32, uint16_t color = 0) const { text(right - textWidth(s, size), y, s, size, color); }
    void ctext(int y, const char* s, int size = 32, uint16_t color = 0) const { text((W - textWidth(s, size)) / 2, y, s, size, color); }

    static void panel(int y) {
        auto& c = PixelRenderer::canvas();
        c.fillRect(0, y, W, H - y, paper());
        c.fillRect(0, y, W, 2, edge());
        c.drawFastHLine(0, y + 4, W, light());
        c.drawFastHLine(1, y + 6, W - 2, edge());
        c.fillRect(1, y + 6, 1, H - y - 6, edge());
        c.fillRect(W - 1, y + 6, 1, H - y - 6, edge());
    }
    void button(Rect r, const char* label, bool primaryStyle, bool disabled = false) const {
        auto& c = PixelRenderer::canvas();
        const int y = r.y + panelOffset();
        c.fillRect(r.x, y, r.w, r.h, disabled ? off() : primaryStyle ? edge() : paper());
        const uint16_t border = disabled ? line() : edge();
        c.drawRect(r.x, y, r.w, r.h, border); c.drawRect(r.x + 1, y + 1, r.w - 2, r.h - 2, border);
        text(r.x + (r.w - textWidth(label, 32)) / 2, r.y + (r.h - 32) / 2, label, 32,
             disabled ? muted() : primaryStyle ? paper() : ink());
    }
    void rowLine(int y) const { PixelRenderer::canvas().drawFastHLine(16, y + panelOffset(), 336, line()); }
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
    void monsterIcon(int x, int y, uint16_t species) const { monsterIconAt(x, y + panelOffset(), species); }
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
    // Wraps a UTF-8 string into up to `lines` lines of 16px text within width.
    void wrap16(int x, int y, const char* s, int width, int lines) const {
        char buf[160]; int row = 0;
        const auto* p = reinterpret_cast<const uint8_t*>(s);
        while (*p && row < lines) {
            int w = 0; size_t o = 0;
            while (*p) {
                const int n = *p < 0x80 ? 1 : *p < 0xe0 ? 2 : *p < 0xf0 ? 3 : 4;
                const int adv = n == 1 ? 8 : 16;
                if (w + adv > width || o + n >= sizeof(buf)) break;
                std::memcpy(buf + o, p, n); o += n; p += n; w += adv;
            }
            buf[o] = 0; text(x, y + row * 18, buf, 16); ++row;
        }
    }
    static const char* speciesName(uint16_t id) { const Species* s = findSpecies(id); return s ? s->name : ""; }
    static const char* levelName(int8_t bond) { return Storage::BOND_LEVELS[uint8_t(Game::Bond::levelFor(bond)) - 1]; }

    // ---- pages
    void renderGreet() const {
        panel(H - GREET_H + panelOffset());
        char line1[sizeof(greet_)], line2[sizeof(greet_)]; line2[0] = 0;
        std::snprintf(line1, sizeof(line1), "%s", greet_);
        size_t split = 0; int w = 0;
        for (const auto* p = reinterpret_cast<const uint8_t*>(line1); *p;) {
            const int n = *p < 0x80 ? 1 : *p < 0xe0 ? 2 : *p < 0xf0 ? 3 : 4;
            const int adv = n == 1 ? 16 : 32;
            if (w + adv > W - 48) break;
            w += adv; p += n; split += size_t(n);
        }
        if (line1[split]) { std::snprintf(line2, sizeof(line2), "%s", line1 + split); line1[split] = 0; }
        text(24, H - 106, line1); if (line2[0]) text(24, H - 66, line2);
    }

    void renderMenu(const Game::GameState& s) const {
        text(20, 178, MarketCareText::TITLE);
        MarketStallMotion::drawClose(panelOffset(), edge());
        const char* names[] = {MarketCareText::MENU_MASSAGE, MarketCareText::MENU_TEA, MarketCareText::MENU_EVAL};
        const bool open = Game::MarketCare::remaining(s) > 0;
        const bool enabled[] = {open && Game::MarketCare::anyEligible(s, Game::MarketCare::Service::MASSAGE),
                                open && Game::MarketCare::anyEligible(s, Game::MarketCare::Service::TEA), true};
        for (int i = 0; i < 3; ++i) {
            const Rect r = menuRow(i);
            if (i < 2) rowLine(r.y + r.h - 1);
            menuIcon(i, 20, r.y + (MENU_ROW_H - 48) / 2);
            text(88, r.y + (r.h - 32) / 2, names[i], 32, enabled[i] ? ink() : muted());
        }
        const int y = ROW_Y + 3 * MENU_ROW_H + 6;
        if (error_) { ctext(y, error_, 16, red()); return; }
        char quota[48];
        std::snprintf(quota, sizeof(quota), MarketCareText::QUOTA, Game::MarketCare::remaining(s), Game::MARKET_CARE_PERIOD_CAP);
        ctext(y, quota, 16, muted());
    }

    void renderList(const Game::GameState& s) const {
        text(20, 178, mode_ == Mode::MASSAGE ? MarketCareText::MASSAGE_LIST :
                      mode_ == Mode::TEA ? MarketCareText::TEA_LIST : MarketCareText::EVAL_LIST);
        Target items[Game::MarketCare::TARGET_CAP];
        const uint8_t n = list(s, items);
        auto& c = PixelRenderer::canvas();
        const int left = c.clipLeft(), top = c.clipTop(), right = c.clipRight(), bottom = c.clipBottom();
        const int clipTop = std::max(top, LIST_TOP + panelOffset());
        const int clipBottom = std::min(bottom, LIST_BOTTOM + panelOffset());
        c.setClipRect(left, clipTop, right - left, std::max(0, clipBottom - clipTop));
        for (uint8_t i = 0; i < n; ++i) {
            const int y = LIST_TOP + i * ROW_H - scroll_;
            if (y + ROW_H <= LIST_TOP || y >= LIST_BOTTOM) continue;
            const Game::MonsterRuntime* m = Game::MarketCare::monster(s, items[i]);
            if (!m) continue;
            const bool on = rowEnabled(s, items[i]);
            char sub[48], status[32]; status[0] = 0;
            if (mode_ == Mode::MASSAGE)
                std::snprintf(sub, sizeof(sub), MarketCareText::AFFECTION_CHANGE, m->affection, Game::MarketCare::affectionAfter(*m));
            else if (mode_ == Mode::TEA)
                std::snprintf(sub, sizeof(sub), MarketCareText::BOND_SUB, levelName(m->bond), int(m->bond));
            else
                std::snprintf(sub, sizeof(sub), MarketCareText::EVAL_SUB, m->affection, levelName(m->bond), int(m->bond));
            if (!on) std::snprintf(status, sizeof(status), "%s", reason(rowState(s, items[i]), mode_));
            else if (items[i].kind == Target::TEAM && mode_ != Mode::MASSAGE) std::snprintf(status, sizeof(status), "%s", MarketCareText::IN_TEAM);
            rowLine(y + ROW_H - 1);
            monsterIcon(16, y + 4, m->speciesId);
            text(72, y + 1, speciesName(m->speciesId), 32, on ? ink() : muted());
            text(72, y + 36, sub, 16, muted());
            if (status[0]) rtext(344, y + 20, status, 16, on ? muted() : red());
        }
        c.setClipRect(left, top, right - left, bottom - top);
        const int total = n * ROW_H, view = LIST_BOTTOM - LIST_TOP;
        if (total > view) {
            const int bar = std::max(24, view * view / total);
            const int pos = LIST_TOP + (view - bar) * scroll_ / std::max(1, total - view);
            c.fillRect(360, LIST_TOP + panelOffset(), 3, view, line());
            c.fillRect(359, pos + panelOffset(), 5, bar, edge());
        }
        button(footer(), MarketCareText::BACK, false);
    }

    void renderConfirm(const Game::GameState& s) const {
        const Game::MonsterRuntime* m = Game::MarketCare::monster(s, target_);
        if (!m) { button(back(), MarketCareText::BACK, false); return; }
        const bool tea = mode_ == Mode::TEA;
        const char* name = speciesName(m->speciesId);
        char title[64], l1[48], l2[48], info1[64], info2[64], cost[64];
        info1[0] = info2[0] = 0;
        monsterIcon(16, 176, m->speciesId);
        if (!tea) {
            std::snprintf(title, sizeof(title), MarketCareText::MASSAGE_TITLE, name);
            std::snprintf(l1, sizeof(l1), MarketCareText::AFFECTION_CHANGE, m->affection, Game::MarketCare::affectionAfter(*m));
            std::snprintf(l2, sizeof(l2), MarketCareText::MOOD_CHANGE, m->mood, Game::MarketCare::moodAfter(*m));
            if (evolveHint_) std::snprintf(info1, sizeof(info1), "%s", MarketCareText::EVOLVE_HINT);
        } else {
            const int8_t after = Game::MarketCare::bondAfter(*m);
            std::snprintf(title, sizeof(title), MarketCareText::TEA_TITLE, name);
            std::snprintf(l1, sizeof(l1), MarketCareText::BOND_CHANGE, int(m->bond), int(after));
            if (Game::Bond::levelFor(after) != Game::Bond::levelFor(m->bond))
                std::snprintf(l2, sizeof(l2), MarketCareText::LEVEL_CHANGE, levelName(m->bond), levelName(after));
            else std::snprintf(l2, sizeof(l2), MarketCareText::LEVEL_KEEP, levelName(after));
            if (target_.kind == Target::CONTACT) {
                std::snprintf(info1, sizeof(info1), MarketCareText::INVITE_CHANCE,
                              Game::MarketCare::inviteChance(target_, m->bond), Game::MarketCare::inviteChance(target_, after));
                if (Game::Bond::inviteLockedToday(m->petCountToday, Game::Bond::invitationDay(s.gameMinutesTotal)))
                    std::snprintf(info2, sizeof(info2), "%s", MarketCareText::INVITE_LOCKED);
            }
        }
        const uint16_t price = Game::MarketCare::price(service(mode_));
        const uint8_t left = Game::MarketCare::remaining(s);
        std::snprintf(cost, sizeof(cost), MarketCareText::COST_LINE, static_cast<unsigned long>(s.coins),
                      static_cast<unsigned long>(s.coins >= price ? s.coins - price : 0), left, left ? left - 1 : 0);
        text(80, 178, title);
        text(24, 228, l1); text(24, 264, l2);
        if (info1[0]) text(24, 304, info1, 16, muted());
        if (info2[0]) text(24, 324, info2, 16, muted());
        text(24, 344, cost, 16);
        const auto r = Game::MarketCare::check(s, service(mode_), target_, period_);
        char why[64]; why[0] = 0;
        if (r == Game::MarketCare::Result::NO_COINS)
            std::snprintf(why, sizeof(why), MarketCareText::NO_COINS, static_cast<unsigned long>(price - s.coins));
        else if (r == Game::MarketCare::Result::FIRST_INVITE) std::snprintf(why, sizeof(why), "%s", MarketCareText::WHY_FIRST_LONG);
        else if (r != Game::MarketCare::Result::OK) std::snprintf(why, sizeof(why), "%s", reason(r, mode_));
        const char* shown = error_ ? error_ : why[0] ? why : nullptr;
        if (shown) ctext(366, shown, 16, red());
        button(back(), MarketCareText::BACK, false);
        char label[24];
        if (r == Game::MarketCare::Result::OK)
            std::snprintf(label, sizeof(label), tea ? MarketCareText::TEA_BUTTON : MarketCareText::MASSAGE_BUTTON, price);
        else std::snprintf(label, sizeof(label), "%s", MarketCareText::UNAVAILABLE);
        button(primary(), label, true, r != Game::MarketCare::Result::OK || pending_);
    }

    void renderEval(const Game::GameState& s) const {
        const Game::MonsterRuntime* m = Game::MarketCare::monster(s, target_);
        if (!m) { button(wide(), MarketCareText::BACK, false); return; }
        monsterIcon(16, 176, m->speciesId);
        text(80, 178, speciesName(m->speciesId));
        char line1[48];
        std::snprintf(line1, sizeof(line1), MarketCareText::AFFECTION, m->affection);
        text(24, 228, line1);
        auto& c = PixelRenderer::canvas();
        const int barY = 266 + panelOffset();
        c.drawRect(24, barY, 320, 10, edge());
        c.fillRect(26, barY + 2, 316 * m->affection / 255, 6, pink());
        std::snprintf(line1, sizeof(line1), MarketCareText::BOND, levelName(m->bond), int(m->bond));
        text(24, 284, line1);
        if (target_.kind == Target::CONTACT) {
            std::snprintf(line1, sizeof(line1), MarketCareText::INVITE_NOW,
                          Game::Bond::inviteChance(m->bond, Game::MarketCare::firstInvitePending(s, target_)));
            text(24, 324, line1, 16, muted());
        } else text(24, 324, MarketCareText::IN_TEAM, 16, muted());
        wrap16(24, 346, MarketCareText::ADVICE[uint8_t(Game::MarketCare::advice(s, target_))], 320, 2);
        const Mode next = recommended(s);
        if (next == Mode::EVAL) { button(wide(), MarketCareText::BACK, false); return; }
        button(back(), MarketCareText::BACK, false);
        button(primary(), next == Mode::TEA ? MarketCareText::BOOK_TEA : MarketCareText::BOOK_MASSAGE, true);
    }

    // Performance inside the dialogue box; the reward is already saved.
    void renderShow(const Game::GameState& s) const {
        const int top = animating() ? PANEL_Y + panelOffset() : H - GREET_H;
        panel(top);
        if (animating()) return;
        const Game::MonsterRuntime* m = Game::MarketCare::monster(s, target_);
        const uint16_t species = m ? m->speciesId : 0;
        const bool tea = mode_ == Mode::TEA;
        const bool performing = showMs_ < SHOW_MS;
        static constexpr int HOP[] = {0, -3, -5, -3};
        const int hopA = performing ? HOP[(showMs_ / 120) % 4] : 0;
        const int hopB = performing ? HOP[(showMs_ / 120 + 2) % 4] : 0;
        // The performance shows both partners in a larger 64px cell.
        const int iconY = top + 28;
        const int targetX = tea ? 104 : 84;
        monsterIconAt(16, iconY + hopA, Game::MarketCare::STALL_SPECIES, 2.0f, 64);
        if (species) monsterIconAt(targetX, iconY + hopB, species, 2.0f, 64);
        if (tea) cupAt(80, iconY + 40);
        // Three hearts rise in turn while performing; one stays afterwards.
        const int heartX = tea ? 92 : targetX + 32;
        for (int i = 0; i < 3; ++i) {
            const int t = int(showMs_) - i * 300;
            if (performing && (t < 0 || t > 700)) continue;
            if (!performing && i) continue;
            const int rise = performing ? t * 30 / 700 : 18;
            heartAt(heartX + (i - 1) * 10, iconY - 2 - rise + 12, 4, pink());
        }
        const int tx = 176;
        if (performing) { textAt(tx, top + 30, tea ? MarketCareText::TEA_TIME : MarketCareText::MASSAGING, 32, ink()); return; }
        char gain[24], line1[48], line2[48]; line2[0] = 0;
        if (tea) {
            std::snprintf(gain, sizeof(gain), MarketCareText::BOND_GAIN, int(afterBond_) - int(beforeBond_));
            std::snprintf(line1, sizeof(line1), "%s", MarketCareText::TEA_LINES[uint8_t(Game::Bond::levelFor(afterBond_)) - 1]);
            if (Game::Bond::levelFor(afterBond_) != Game::Bond::levelFor(beforeBond_))
                std::snprintf(line2, sizeof(line2), MarketCareText::LEVEL_CHANGE, levelName(beforeBond_), levelName(afterBond_));
        } else {
            std::snprintf(gain, sizeof(gain), MarketCareText::AFFECTION_GAIN, unsigned(afterAffection_ - beforeAffection_));
            std::snprintf(line1, sizeof(line1), MarketCareText::RELAXED, speciesName(species));
            if (evolveHint_) std::snprintf(line2, sizeof(line2), "%s", MarketCareText::EVOLVE_SHORT);
        }
        textAt(tx, top + 14, gain, 32, good());
        textAt(tx, top + 54, line1, 16, ink());
        if (line2[0]) textAt(tx, top + 76, line2, 16, muted());
        // Bouncing next marker, same red as the dialogue arrow.
        static constexpr int BOB[] = {0, 1, 2, 1};
        const int by = H - 22 + BOB[(showMs_ / 160) % 4];
        auto& c = PixelRenderer::canvas();
        c.fillTriangle(W - 34, by - 7, W - 18, by - 7, W - 26, by + 1, edge());
        c.fillTriangle(W - 32, by - 6, W - 20, by - 6, W - 26, by, red());
    }
};
} // namespace Ui
