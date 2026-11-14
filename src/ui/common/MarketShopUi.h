#pragma once

#include <algorithm>
#include <cstdio>
#include <cstring>
#include "assets/GameAssets.h"
#include "core/TouchTap.h"
#include "core/UiStrings.h"
#include "game/MarketShopService.h"
#include "game/MarketSupply.h"
#include "presentation/PixelRenderer.h"
#include "ui/common/MarketStallMotion.h"
#include "ui/common/MarketListScroll.h"
#include "ui/common/MarketUiFeedback.h"
#include "ui/common/MarketView.h"

// Kecleon stall (奇珍小铺) for the 368x448 touch layout: greeting, scrollable
// shelf, single-purchase confirmation and success page. Shares the
// Gengar stall panel style and slide-in: the greeting box rises from the
// bottom edge and then grows into the panel, which sinks away on close. The
// shelf returns home by tapping the scene above it. The application applies a returned
// Request to its state, saves, and reports back through committed().
namespace Ui {
class MarketShopUi {
public:
    enum class Page : uint8_t { CLOSED, GREET, MENU, SHELF, ADVICE, ADVICE_DONE, CONFIRM, RESULT };
    struct Request {
        enum Kind : uint8_t { NONE, BUY, ADVISE } kind = NONE;
        uint8_t slot = 0;
        uint32_t period = 0;
        Game::MarketSupply::Advice advice = Game::MarketSupply::Advice::NONE;
    };
    struct Rect { int x, y, w, h; bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; } };

    static constexpr int W = 368, H = 448, PANEL_Y = 208, ROW_Y = 216, ROW_H = 72, GREET_H = 120;
    static constexpr int LIST_TOP = ROW_Y, LIST_BOTTOM = 440;
    static constexpr uint32_t GREET_HOLD_MS = 600, GREET_IN_MS = 360, OPEN_MS = 420, CLOSE_MS = 520;

    bool takeSfx(SfxCue& cue) { return feedback_.takeSfx(cue); }
    bool active() const { return page_ != Page::CLOSED; }
    Page page() const { return page_; }
    bool pending() const { return pending_; }
    bool animating() const { return transition_ != Transition::NONE; }
    uint8_t selected() const { return slot_; }
    int scrollOffset() const { return scroll_; }
    int panelTop() const { return page_ == Page::GREET ? H - GREET_H + offset() : PANEL_Y + offset(); }
    // Vertical offset of the coin badge: hidden while greeting, sliding in
    // and out with the panel.
    int hudOffset() const { return MarketStallMotion::hudOffset(page_ == Page::GREET, transition_ == Transition::OPENING,
                                                                 transition_ == Transition::CLOSING, transitionMs_,
                                                                 duration()); }

    void open(const Game::GameState& state) {
        *this = MarketShopUi{};
        page_ = Page::GREET; transition_ = Transition::GREET_IN;
        greet_ = Game::MarketShop::soldOut(state) ? MarketShopText::GREET_SOLD_OUT : MarketShopText::GREET;
        period_ = state.marketSeason.periodDay;
    }
    void close() { feedback_.clear(); page_ = Page::CLOSED; transition_ = Transition::NONE; cancelTouch(); pending_ = false; }
    void cancelTouch() { tap_.cancel(); scrollMotion_.cancel(); }

    bool update(uint32_t deltaMs) {
        if (!active() || !deltaMs) return false;
        deltaMs = std::min<uint32_t>(deltaMs, 250);
        if (animating()) {
            transitionMs_ = std::min(duration(), transitionMs_ + deltaMs);
            if (transitionMs_ == duration()) {
                if (transition_ == Transition::CLOSING) close();
                else transition_ = Transition::NONE;
            }
            return true;
        }
        if (page_ == Page::SHELF) return scrollMotion_.update(deltaMs, maxScroll(), scroll_);
        scrollMotion_.cancel();
        if (page_ != Page::GREET) return false;
        greetMs_ += deltaMs;
        if (greetMs_ < GREET_HOLD_MS + 40U * uint32_t(std::strlen(greet_) / 3)) return false;
        openPanel(); return true;
    }

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
        if (transition_ == Transition::GREET_IN) return tween(0, greeting);
        if (transition_ == Transition::OPENING) return tween(greeting, opened);
        if (transition_ == Transition::CLOSING) return tween(opened, 0);
        return page_ == Page::GREET ? greeting : opened;
    }

    Request touch(const Platform::IInputDevice::TouchEvent& e, const Game::GameState& state) {
        using Type = Platform::IInputDevice::TouchType;
        Request none{};
        if (pending_ || page_ == Page::CLOSED || animating()) { cancelTouch(); return none; }
        if (page_ == Page::GREET) {
            // The lifting finger of the skip tap never lands on a shelf row.
            if (tap_.update(e, 0) == 0) { feedback_.set(SfxCue::UI_CONFIRM); openPanel(); }
            return none;
        }
        if (page_ == Page::SHELF && listDrag(e)) return none;
        const int target = e.type == Type::DOWN || e.type == Type::UP ? hit(e.x, e.y, state) : -1;
        const int clicked = tap_.update(e, target);
        if (clicked < 0) return none;
        if (clicked == OUTSIDE)
            return activate(page_ == Page::MENU ? CLOSE : page_ == Page::RESULT ? PRIMARY : BACK, state);
        return activate(clicked, state);
    }

    void committed(bool saved, Game::ItemId reward = Game::ItemId::COUNT, uint8_t amount = 1) {
        pending_ = false;
        if (!saved) { feedback_.set(SfxCue::UI_CANCEL); error_ = MarketShopText::SAVE_FAILED; return; }
        feedback_.set(slot_ == Game::MarketShop::BOX ? SfxCue::ITEM_GET : SfxCue::BUY_ITEM);
        error_ = nullptr; rewardItem_ = reward; rewardAmount_ = amount; changePage(Page::RESULT);
    }
    void adviceCommitted(bool saved) {
        pending_ = false;
        if (!saved) { feedback_.set(SfxCue::UI_CANCEL); error_ = MarketShopText::ADVICE_SAVE_FAILED; return; }
        feedback_.set(SfxCue::UI_CONFIRM); error_ = nullptr; changePage(Page::ADVICE_DONE);
    }
    void rejected(Game::MarketShop::Result r) { pending_ = false; feedback_.set(SfxCue::UI_CANCEL); error_ = reason(r, 0); }

    void render(const Game::GameState& s) const {
        if (page_ == Page::CLOSED) return;
        if (page_ == Page::GREET) { renderGreet(); return; }
        panel();
        if (page_ == Page::MENU) renderMenu();
        else if (page_ == Page::SHELF) renderShelf(s);
        else if (page_ == Page::ADVICE) renderAdvice(s);
        else if (page_ == Page::ADVICE_DONE) renderAdviceDone();
        else if (page_ == Page::CONFIRM) renderConfirm(s);
        else renderResult(s);
        MarketStallMotion::drawBadge(23, 16 + hudOffset(), MarketShopText::TITLE,
                                     width(MarketShopText::TITLE, 28), paper(), ink());
    }

private:
    enum class Transition : uint8_t { NONE, GREET_IN, OPENING, CLOSING };
    enum Target { CLOSE = 10, BACK, PRIMARY, OUTSIDE, OPEN_SHELF, OPEN_ADVICE, ADVICE0 = 40 };
    Page page_ = Page::CLOSED;
    Transition transition_ = Transition::NONE;
    uint32_t transitionMs_ = 0, greetMs_ = 0, period_ = 0;
    TouchTap tap_;
    MarketUiFeedback feedback_;
    uint8_t slot_ = 0;
    bool pending_ = false;
    MarketListScroll scrollMotion_;
    int scroll_ = 0;
    Game::ItemId rewardItem_ = Game::ItemId::COUNT;
    uint8_t rewardAmount_ = 1;
    const char* error_ = nullptr;
    const char* greet_ = MarketShopText::GREET;

    Rect row(int i) const { return {10, ROW_Y + i * ROW_H - scroll_, 348, ROW_H}; }
    static Rect back() { return {16, 394, 160, 44}; }
    static Rect primary() { return {192, 394, 160, 44}; }
    static Rect wide() { return {16, 394, 336, 44}; }
    static Rect menuRow(int index) { return {16, 250 + index * 76, 336, 56}; }
    static Rect adviceRow(int index) { return {16, 220 + index * 54, 336, 48}; }
    static constexpr int maxScroll() { return Game::MarketShop::ROW_COUNT * ROW_H - (LIST_BOTTOM - LIST_TOP); }
    bool listDrag(const Platform::IInputDevice::TouchEvent& e) {
        return scrollMotion_.touch(e, {10, LIST_TOP, 348, LIST_BOTTOM - LIST_TOP},
                                   maxScroll(), scroll_, tap_);
    }

    uint32_t duration() const {
        return transition_ == Transition::GREET_IN ? GREET_IN_MS : transition_ == Transition::CLOSING ? CLOSE_MS : OPEN_MS;
    }
    // One progress curve keeps panel, camera and badge in step.
    int tween(int from, int to) const {
        return MarketStallMotion::tween(from, to, transitionMs_, duration());
    }
    int offset() const {
        if (transition_ == Transition::GREET_IN) return tween(GREET_H, 0);
        if (transition_ == Transition::OPENING) return tween(H - GREET_H - PANEL_Y, 0);
        if (transition_ == Transition::CLOSING) return tween(0, H - PANEL_Y);
        return 0;
    }
    void changePage(Page page) { cancelTouch(); page_ = page; }
    void home() { error_ = nullptr; changePage(Page::MENU); }
    void openPanel() { changePage(Page::MENU); transition_ = Transition::OPENING; transitionMs_ = 0; tap_.cancel(); }

    static const char* reason(Game::MarketShop::Result r, unsigned long) {
        using R = Game::MarketShop::Result;
        switch (r) {
        case R::SOLD_OUT: return MarketShopText::WHY_SOLD;
        case R::BAG_FULL: return MarketShopText::BAG_FULL;
        case R::DAILY_LIMIT: return MarketShopText::DAILY_LIMIT;
        case R::STALE: return MarketShopText::STALE;
        case R::NO_COINS: return MarketShopText::NO_COINS;
        default: return MarketShopText::LOCKED;
        }
    }

    int hit(int x, int y, const Game::GameState& s) const {
        // The exposed market above the panel is a separate tap target.
        if (x >= 0 && x < W && y >= 0 && y < PANEL_Y) return OUTSIDE;
        if (page_ == Page::SHELF) {
            if (y < LIST_TOP || y >= LIST_BOTTOM) return -1;
            for (int i = 0; i < Game::MarketShop::ROW_COUNT; ++i)
                if (row(i).contains(x, y) && s.marketSeason.stock[i] &&
                    (i == Game::MarketShop::BOX || Game::MarketShop::item(s, uint8_t(i)) != Game::ItemId::COUNT)) return i;
            return -1;
        }
        if (page_ == Page::MENU) {
            if (menuRow(0).contains(x,y)) return OPEN_SHELF;
            if (menuRow(1).contains(x,y)) return OPEN_ADVICE;
        }
        if (page_ == Page::ADVICE) {
            if (wide().contains(x,y)) return BACK;
            for (int i=0;i<3;++i) if (adviceRow(i).contains(x,y)) return ADVICE0+i;
        }
        if (page_ == Page::CONFIRM) {
            if (back().contains(x, y)) return BACK;
            return primary().contains(x, y) ? PRIMARY : -1;
        }
        if (page_ == Page::RESULT || page_ == Page::ADVICE_DONE) return wide().contains(x, y) ? PRIMARY : -1;
        return -1;
    }

    Request activate(int target, const Game::GameState& s) {
        Request r{};
        feedback_.set(target == CLOSE || target == BACK ? SfxCue::UI_CANCEL : SfxCue::UI_CONFIRM);
        cancelTouch();
        if (page_ == Page::MENU) {
            error_ = nullptr;
            if (target == CLOSE) { transition_ = Transition::CLOSING; transitionMs_ = 0; return r; }
            changePage(target == OPEN_ADVICE ? Page::ADVICE : Page::SHELF); return r;
        }
        if (page_ == Page::SHELF) {
            if (target == BACK) { home(); return r; }
            slot_ = uint8_t(target); error_ = nullptr;
            rewardItem_ = Game::ItemId::COUNT; rewardAmount_ = 1;
            changePage(Page::CONFIRM); return r;
        }
        if (page_ == Page::ADVICE) {
            if (target == BACK) { home(); return r; }
            feedback_.clear(); pending_ = true; error_ = nullptr; r.kind = Request::ADVISE; r.period = period_;
            r.advice = Game::MarketSupply::Advice(target - ADVICE0 + 1); return r;
        }
        if (page_ == Page::ADVICE_DONE) { home(); return r; }
        if (page_ == Page::CONFIRM) {
            if (target == BACK) { error_ = nullptr; changePage(Page::SHELF); return r; }
            const auto result = Game::MarketShop::check(s, slot_, period_);
            if (result != Game::MarketShop::Result::OK) { rejected(result); return r; }
            feedback_.clear(); pending_ = true; r.kind = Request::BUY; r.slot = slot_; r.period = period_; return r;
        }
        changePage(Page::SHELF); return r; // success page: back to the same shelf
    }

    // ---- drawing (palette and metrics match MarketTutorUi)
    static uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return PixelRenderer::rgb(r, g, b); }
    static uint16_t paper() { return rgb(248, 248, 224); }
    static uint16_t ink() { return rgb(40, 56, 80); }
    static uint16_t edge() { return rgb(48, 64, 112); }
    static uint16_t light() { return rgb(128, 152, 184); }
    static uint16_t muted() { return rgb(89, 105, 88); }
    static uint16_t line() { return rgb(186, 196, 160); }
    static uint16_t red() { return rgb(183, 56, 56); }
    static uint16_t off() { return rgb(225, 227, 207); }
    static int width(const char* s, int size) {
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
        PixelRenderer::text(x, y + offset(), s, color ? color : ink());
        c.setNativeText(native);
    }
    void rtext(int right, int y, const char* s, int size = 32, uint16_t color = 0) const { text(right - width(s, size), y, s, size, color); }
    void ctext(int y, const char* s, int size = 32, uint16_t color = 0) const { text((W - width(s, size)) / 2, y, s, size, color); }
    void panel() const {
        const int y = PANEL_Y + offset();
        auto& c = PixelRenderer::canvas();
        c.fillRoundRect(4, y, W - 8, H - PANEL_Y - 4, 14, paper());
        c.drawRoundRect(4, y, W - 8, H - PANEL_Y - 4, 14, edge());
    }
    void button(Rect r, const char* label, bool primary, bool disabled = false) const {
        auto& c = PixelRenderer::canvas();
        const int y = r.y + offset();
        c.fillRect(r.x, y, r.w, r.h, disabled ? off() : primary ? edge() : paper());
        const uint16_t border = disabled ? line() : edge();
        c.drawRect(r.x, y, r.w, r.h, border); c.drawRect(r.x + 1, y + 1, r.w - 2, r.h - 2, border);
        text(r.x + (r.w - width(label, 32)) / 2, r.y + (r.h - 32) / 2, label, 32, disabled ? muted() : primary ? paper() : ink());
    }
    void icon(Game::ItemId id, int cx, int cy) const {
        if (!GameAssets::drawItemIcon48(id, cx, cy + offset()))
            PixelRenderer::canvas().fillCircle(cx, cy + offset(), 14, line());
    }
    void slotIcon(Game::ItemId id, bool box, int cx, int cy) const {
        if (box) GameAssets::drawCentered(GameAssets::Kind::MARKET_MYSTERY_BOX, cx, cy + offset());
        else icon(id, cx, cy);
    }
    // Shop descriptions use a space as a soft break; split them over two lines.
    void description(Game::ItemId id, int y) const {
        char buf[96]; std::snprintf(buf, sizeof(buf), "%s", Game::ShopService::shortDescription(id));
        char* second = std::strchr(buf, ' ');
        if (second) *second++ = 0;
        text(24, y, buf);
        if (second && *second) text(24, y + 34, second);
    }

    void renderGreet() const {
        const int y = H - GREET_H + offset();
        auto& c = PixelRenderer::canvas();
        c.fillRect(0, y, W, H - y, paper());
        c.fillRect(0, y, W, 2, edge()); c.drawFastHLine(0, y + 4, W, light()); c.drawFastHLine(1, y + 6, W - 2, edge());
        char line1[128], line2[128]; line2[0] = 0;
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

    void renderShelf(const Game::GameState& s) const {
        auto& c = PixelRenderer::canvas();
        c.setClipRect(10, LIST_TOP + offset(), 348, LIST_BOTTOM - LIST_TOP);
        for (uint8_t i = 0; i < Game::MarketShop::ROW_COUNT; ++i) {
            const bool box = i == Game::MarketShop::BOX;
            const Game::ItemId id = Game::MarketShop::item(s, i);
            const bool legacyEmpty = !box && id == Game::ItemId::COUNT;
            const Rect r = row(i);
            const bool sold = legacyEmpty || !s.marketSeason.stock[i];
            if (sold) c.fillRect(r.x, r.y + offset(), r.w, r.h, off());
            if (i + 1 < Game::MarketShop::ROW_COUNT) c.drawFastHLine(16, r.y + r.h - 1 + offset(), 336, line());
            slotIcon(legacyEmpty ? Game::ItemId::HEART_SCALE : id, box, 44, r.y + r.h / 2);
            const char* name = legacyEmpty ? MarketShopText::RARE : box ? MarketShopText::BOX : Game::ShopService::shortName(id);
            const int textY = r.y + (r.h - 32) / 2;
            text(84, textY, name, 32, sold ? muted() : ink());
            char price[16];
            if (legacyEmpty) std::snprintf(price, sizeof(price), "--");
            else std::snprintf(price, sizeof(price), "%u", Game::MarketShop::priceForSlot(s, i));
            rtext(344, textY, price, 32, sold ? muted() : ink());
            if (sold) {
                c.blendRectAlphaNative(20, r.y + (r.h - 48) / 2 + offset(), 48, 48, off(), 160);
                c.drawFastHLine(84, textY + 16 + offset(), width(name, 32), muted());
                c.drawFastHLine(344 - width(price, 32), textY + 16 + offset(), width(price, 32), muted());
            }
        }
        c.clearClipRect();
        const int thumb = (LIST_BOTTOM - LIST_TOP) * (LIST_BOTTOM - LIST_TOP) / (Game::MarketShop::ROW_COUNT * ROW_H);
        const int y = LIST_TOP + scroll_ * (LIST_BOTTOM - LIST_TOP - thumb) / maxScroll() + offset();
        c.fillRoundRect(355, y, 3, thumb, 1, line());
    }

    void renderMenu() const {
        button(menuRow(0), MarketShopText::BUY_ITEMS, false);
        button(menuRow(1), MarketShopText::ADVICE, false);
    }

    void renderAdvice(const Game::GameState& s) const {
        const char* labels[] = {MarketShopText::ADVICE_EXPLORE, MarketShopText::ADVICE_GROWTH, MarketShopText::ADVICE_RARE};
        for (int i=0;i<3;++i) {
            const bool selected = s.marketSupply.pendingAdvice == uint8_t(i+1);
            button(adviceRow(i), labels[i], selected, pending_);
        }
        if (error_) ctext(378, error_, 16, red());
        button(wide(), MarketShopText::BACK, false, pending_);
    }

    void renderAdviceDone() const {
        ctext(284, MarketShopText::ADVICE_DONE);
        button(wide(), MarketShopText::HOME, true);
    }

    void renderConfirm(const Game::GameState& s) const {
        const Game::ItemId id = Game::MarketShop::item(s, slot_);
        const bool box = slot_ == Game::MarketShop::BOX;
        const char* name = box ? MarketShopText::BOX : Game::ShopService::shortName(id);
        const uint16_t price = Game::MarketShop::priceForSlot(s, slot_);
        slotIcon(id, box, 44, 240);
        text(80, 224, name);
        rtext(344, 224, "×1");
        if (box) text(24, 282, MarketShopText::BOX_LINE1);
        else description(id, 282);
        char line[64];
        std::snprintf(line, sizeof(line), MarketShopText::STOCK, s.marketSeason.stock[slot_]);
        ctext(354, line);
        const auto r = Game::MarketShop::check(s, slot_, period_);
        char why[64]; why[0] = 0;
        if (r == Game::MarketShop::Result::NO_COINS)
            std::snprintf(why, sizeof(why), MarketShopText::NO_COINS, static_cast<unsigned long>(price - s.coins));
        else if (r != Game::MarketShop::Result::OK) std::snprintf(why, sizeof(why), "%s", reason(r, 0));
        const char* shown = error_ ? error_ : why[0] ? why : nullptr;
        if (shown) text(88, 264, shown, 16, red());
        button(back(), MarketShopText::BACK, false);
        char label[24];
        if (r == Game::MarketShop::Result::OK) std::snprintf(label, sizeof(label), MarketShopText::BUY, price);
        else std::snprintf(label, sizeof(label), "%s", MarketShopText::UNAVAILABLE);
        button(primary(), label, true, r != Game::MarketShop::Result::OK || pending_);
    }

    void renderResult(const Game::GameState& s) const {
        const Game::ItemId id = rewardItem_ == Game::ItemId::COUNT ? Game::MarketShop::item(s, slot_) : rewardItem_;
        icon(id, W / 2, 246);
        char got[64];
        if (slot_ == Game::MarketShop::BOX) std::snprintf(got, sizeof(got), MarketShopText::BOX_GOT, Game::ShopService::shortName(id), rewardAmount_);
        else std::snprintf(got, sizeof(got), MarketShopText::GOT, Game::ShopService::shortName(id));
        ctext(282, got);
        ctext(326, MarketShopText::GOT_LINE2);
        button(wide(), MarketShopText::CONTINUE, true);
    }
};
} // namespace Ui
