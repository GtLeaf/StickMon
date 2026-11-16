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
#include "presentation/TextBlock.h"
#include "ui/common/MarketStallMotion.h"
#include "ui/common/MarketStallIcons.h"
#include "ui/common/MarketListScroll.h"
#include "ui/common/MarketUiFeedback.h"
#include "ui/common/MarketCoinHud.h"
#include "ui/common/MarketBoxPerformance.h"
#include "ui/common/MarketView.h"

// Native 360px round layout; shared game services own all stock and pricing.
namespace Ui {
class RoundMarketShopUi {
public:
    enum class Page : uint8_t { CLOSED, GREET, MENU, SHELF, ADVICE, ADVICE_DONE, CONFIRM, RESULT,
                                BOX_ARRIVE, BOX_WAIT, BOX_OPEN, ERROR };
    struct Request {
        enum Kind : uint8_t { NONE, BUY, ADVISE } kind = NONE;
        uint8_t slot = 0;
        uint32_t period = 0;
        Game::MarketSupply::Advice advice = Game::MarketSupply::Advice::NONE;
    };
    struct Rect { int x, y, w, h; bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; } };

    static constexpr int W = 360, H = 360, PANEL_Y = 144, ROW_Y = 152, ROW_H = 64, GREET_H = 136;
    static constexpr int LIST_TOP = ROW_Y, LIST_BOTTOM = 304;
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

    void open(const Game::GameState& state, bool initialized = true) {
        *this = RoundMarketShopUi{};
        page_ = initialized ? Page::GREET : Page::ERROR; transition_ = initialized ? Transition::GREET_IN : Transition::OPENING;
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
        if (page_ == Page::BOX_ARRIVE) {
            boxMs_ = std::min(MarketBoxPerformance::ARRIVE_MS, boxMs_ + deltaMs);
            if (boxMs_ == MarketBoxPerformance::ARRIVE_MS) { changePage(Page::BOX_WAIT); boxMs_ = 0; }
            return true;
        }
        if (page_ == Page::BOX_WAIT) {
            boxMs_ += deltaMs;
            if (boxMs_ >= 3000) boxMs_ -= 1500; // bounded clock, keep the initial hint fade complete
            return true;
        }
        if (page_ == Page::BOX_OPEN) {
            const auto before = boxMs_;
            boxMs_ = std::min(MarketBoxPerformance::OPEN_MS, boxMs_ + deltaMs);
            if (before < MarketBoxPerformance::REVEAL_MS && boxMs_ >= MarketBoxPerformance::REVEAL_MS)
                feedback_.set(SfxCue::ITEM_GET);
            if (boxMs_ == MarketBoxPerformance::OPEN_MS) changePage(Page::RESULT);
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
        if (!inside(e.x,e.y)) { cancelTouch(); return none; }
        if (pending_ || page_ == Page::CLOSED || animating()) { cancelTouch(); return none; }
        if (page_ == Page::BOX_ARRIVE || page_ == Page::BOX_OPEN) { cancelTouch(); return none; }
        if (page_ == Page::BOX_WAIT) {
            const int target = Rect{4, PANEL_Y, W-8, H-PANEL_Y-4}.contains(e.x,e.y) ? PRIMARY : -1;
            if (tap_.update(e,target) == PRIMARY) { changePage(Page::BOX_OPEN); boxMs_ = 0; }
            return none;
        }
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
            return activate((page_ == Page::MENU || page_ == Page::ERROR) ? CLOSE : page_ == Page::RESULT ? PRIMARY : BACK, state);
        return activate(clicked, state);
    }

    void committed(bool saved, Game::ItemId reward = Game::ItemId::COUNT, uint8_t amount = 1) {
        pending_ = false;
        if (!saved) { feedback_.set(SfxCue::UI_CANCEL); error_ = MarketShopText::SAVE_FAILED; return; }
        error_ = nullptr; rewardItem_ = reward; rewardAmount_ = amount;
        if (slot_ == Game::MarketShop::BOX) { feedback_.clear(); boxMs_ = 0; changePage(Page::BOX_ARRIVE); }
        else { feedback_.set(SfxCue::BUY_ITEM); changePage(Page::RESULT); }
    }
    void adviceCommitted(bool saved) {
        pending_ = false;
        if (!saved) { feedback_.set(SfxCue::UI_CANCEL); error_ = MarketShopText::ADVICE_SAVE_FAILED; return; }
        feedback_.set(SfxCue::UI_CONFIRM); error_ = nullptr; changePage(Page::ADVICE_DONE);
    }
    void rejected(Game::MarketShop::Result r) { pending_ = false; feedback_.set(SfxCue::UI_CANCEL); error_ = reason(r, 0); }

    void render(const Game::GameState& s, const MarketCoinMotion* coins = nullptr) const {
        if (page_ == Page::CLOSED) return;
        if (page_ == Page::GREET) { renderGreet(); return; }
        panel();
        if (page_ == Page::MENU) renderMenu();
        else if (page_ == Page::SHELF) renderShelf(s);
        else if (page_ == Page::ADVICE) renderAdvice(s);
        else if (page_ == Page::ADVICE_DONE) renderAdviceDone();
        else if (page_ == Page::CONFIRM) renderConfirm(s);
        else if (page_ == Page::ERROR) { drawBody(MarketShopText::SAVE_FAILED,{68,204,224,52},true,4,red()); button(wide(),"离开",true); }
        else if (page_ == Page::BOX_ARRIVE) {
            renderConfirm(s,true);
            PixelRenderer::canvas().blendRoundRectAlphaNative(5,PANEL_Y+1,350,232,18,paper(),MarketBoxPerformance::detailFade(boxMs_));
            MarketBoxPerformance::arrival(boxMs_,boxStage);
        }
        else if (page_ == Page::BOX_WAIT) {
            MarketBoxPerformance::waiting(boxMs_,boxStage);
            ctext(284,MarketShopText::BOX_OPEN,16,MarketBoxPerformance::detail::fade(89,105,88,
                MarketBoxPerformance::detail::smooth(MarketBoxPerformance::detail::p(boxMs_,0,220))));
        }
        else if (page_ == Page::BOX_OPEN) { MarketBoxPerformance::opening(boxMs_,rewardItem_,boxStage); renderBoxText(boxMs_); }
        else renderResult(s);
        const int hudY=52+hudOffset();
        auto& c=PixelRenderer::canvas();
        c.fillRoundRect(56,hudY-4,96,32,6,paper());
        PixelRenderer::textSmooth(64,hudY+6,MarketShopText::TITLE,ink(),20);
        MarketCoinHud::render(s.coins,coins,hudY,20,{288,24,32,36});
    }

private:
    static bool inside(int x,int y) { return (x-180)*(x-180)+(y-180)*(y-180)<180*180; }
    inline static constexpr MarketBoxPerformance::Layout boxStage{180,232,88,178,270,192};
    enum class Transition : uint8_t { NONE, GREET_IN, OPENING, CLOSING };
    enum Target { CLOSE = 10, BACK, PRIMARY, OUTSIDE, OPEN_SHELF, OPEN_ADVICE, ADVICE0 = 40 };
    Page page_ = Page::CLOSED;
    Transition transition_ = Transition::NONE;
    uint32_t transitionMs_ = 0, greetMs_ = 0, period_ = 0;
    uint32_t boxMs_ = 0;
    TouchTap tap_;
    MarketUiFeedback feedback_;
    uint8_t slot_ = 0;
    uint8_t stockBefore_ = 0;
    bool pending_ = false;
    MarketListScroll scrollMotion_;
    int scroll_ = 0;
    Game::ItemId rewardItem_ = Game::ItemId::COUNT;
    uint8_t rewardAmount_ = 1;
    const char* error_ = nullptr;
    const char* greet_ = MarketShopText::GREET;
    mutable TextBlock bodyText_;

    Rect row(int i) const { return {60, ROW_Y + i * ROW_H - scroll_, 240, ROW_H}; }
    static Rect back() { return {84, 288, 88, 40}; }
    static Rect primary() { return {188, 288, 88, 40}; }
    static Rect wide() { return {80, 288, 200, 40}; }
    static Rect menuRow(int index) { return {64, 166 + index * 68, 232, 56}; }
    static Rect adviceRow(int index) { return {64, 162 + index * 46, 232, 46}; }
    static constexpr int maxScroll() { return Game::MarketShop::ROW_COUNT * ROW_H - (LIST_BOTTOM - LIST_TOP); }
    bool listDrag(const Platform::IInputDevice::TouchEvent& e) {
        return scrollMotion_.touch(e, {60, LIST_TOP, 240, LIST_BOTTOM - LIST_TOP},
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
        case R::NO_COINS: return "金币不足";
        default: return MarketShopText::LOCKED;
        }
    }

    int hit(int x, int y, const Game::GameState& s) const {
        // The exposed market above the panel is a separate tap target.
        if (x >= 0 && x < W && y >= 0 && y < PANEL_Y) return OUTSIDE;
        if (page_ == Page::ERROR) return wide().contains(x,y) ? CLOSE : -1;
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
        if (page_ == Page::ERROR) { transition_ = Transition::CLOSING; transitionMs_ = 0; return r; }
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
            stockBefore_ = s.marketSeason.stock[slot_];
            feedback_.clear(); pending_ = true; r.kind = Request::BUY; r.slot = slot_; r.period = period_; return r;
        }
        changePage(Page::SHELF); return r; // success page: back to the same shelf
    }

    // Native round-screen presentation. Rules and transaction persistence stay
    // in MarketShopService / ExploreScene; this class only owns UI state.
    static uint16_t rgb(uint8_t r,uint8_t g,uint8_t b) {return PixelRenderer::rgb(r,g,b);}
    static uint16_t paper(){return rgb(248,248,224);}
    static uint16_t ink(){return rgb(40,56,80);}
    static uint16_t edge(){return rgb(48,64,112);}
    static uint16_t muted(){return rgb(89,105,88);}
    static uint16_t line(){return rgb(186,196,160);}
    static uint16_t red(){return rgb(183,56,56);}
    static uint16_t off(){return rgb(225,227,207);}
    static int width(const char* s,int size) {
        int w=0;for(const auto* p=reinterpret_cast<const uint8_t*>(s);*p;){
            const int n=*p<0x80?1:*p<0xe0?2:*p<0xf0?3:4;
            w+=n==1?size/2:size;p+=n;
        }return w;
    }
    void text(int x,int y,const char* s,int size=20,uint16_t color=0) const {
        PixelRenderer::textSmooth(x,y+offset(),s,color?color:ink(),uint8_t(size));
    }
    void ctext(int y,const char* s,int size=20,uint16_t color=0) const {text((W-width(s,size))/2,y,s,size,color);}
    void rtext(int x,int y,const char* s,int size=20,uint16_t color=0) const {text(x-width(s,size),y,s,size,color);}
    void panel() const {
        auto& c=PixelRenderer::canvas();const int y=PANEL_Y+offset();
        c.fillRoundRect(4,y,W-8,232,18,paper());c.drawRoundRect(4,y,W-8,232,18,edge());
    }
    void button(Rect r,const char* label,bool primary,bool disabled=false) const {
        auto& c=PixelRenderer::canvas();const int y=r.y+offset();
        c.fillRoundRect(r.x,y,r.w,r.h,8,disabled?off():primary?edge():paper());
        c.drawRoundRect(r.x,y,r.w,r.h,8,disabled?line():edge());
        text(r.x+(r.w-width(label,20))/2,r.y+(r.h-20)/2,label,20,disabled?muted():primary?paper():ink());
    }
    void icon(Game::ItemId id,int cx,int cy) const {
        if(!GameAssets::drawItemIcon48(id,cx,cy+offset()))PixelRenderer::canvas().fillCircle(cx,cy+offset(),14,line());
    }
    void slotIcon(Game::ItemId id,bool box,int cx,int cy) const {
        if(box)GameAssets::drawCentered(GameAssets::Kind::MARKET_MYSTERY_BOX,cx,cy+offset());
        else icon(id,cx,cy);
    }
    void drawBody(const char* value,Rect r,bool centered=false,int gap=2,uint16_t color=0) const {
        TextBlock::Options options;options.width=r.w;options.maxHeight=r.h;
        options.font=BitmapText::Font::PX16;options.lineGap=uint16_t(gap);
        options.align=centered?TextBlock::Align::CENTER:TextBlock::Align::LEFT;
        bodyText_.setText(value);bodyText_.setOptions(options);
        bodyText_.draw(PixelRenderer::canvas(),r.x,r.y+offset(),color?color:ink());
    }
    void renderGreet() const {
        auto& c=PixelRenderer::canvas();const int y=H-GREET_H+offset();
        c.fillRoundRect(4,y,W-8,160,18,paper());c.drawRoundRect(4,y,W-8,160,18,edge());
        drawBody(greet_,{72,H-GREET_H+18,216,70},true,4);
    }
    void renderShelf(const Game::GameState& s) const {
        auto& c=PixelRenderer::canvas();
        const int left=c.clipLeft(),top=c.clipTop(),right=c.clipRight(),bottom=c.clipBottom();
        const int clipX=std::max(left,60),clipY=std::max(top,LIST_TOP+offset());
        c.setClipRect(clipX,clipY,std::max(0,std::min(right,300)-clipX),std::max(0,std::min(bottom,LIST_BOTTOM+offset())-clipY));
        for(uint8_t i=0;i<Game::MarketShop::ROW_COUNT;++i){
            const bool box=i==Game::MarketShop::BOX;const auto id=Game::MarketShop::item(s,i);
            const bool empty=!box&&id==Game::ItemId::COUNT,sold=empty||!s.marketSeason.stock[i];const Rect r=row(i);
            if(sold)c.fillRect(r.x,r.y+offset(),r.w,r.h,off());
            slotIcon(empty?Game::ItemId::HEART_SCALE:id,box,86,r.y+32);
            const char* name=empty?MarketShopText::RARE:box?MarketShopText::BOX:Game::ShopService::shortName(id);
            text(120,r.y+7,name,20,sold?muted():ink());
            char price[20];if(empty)std::snprintf(price,sizeof(price),"--");
            else std::snprintf(price,sizeof(price),"%u",Game::MarketShop::priceForSlot(s,i));
            text(120,r.y+34,price,16,sold?muted():ink());
            if(sold){
                c.blendRectAlphaNative(62,r.y+8+offset(),48,48,off(),160);
                c.drawFastHLine(120,r.y+17+offset(),width(name,20),muted());
                c.drawFastHLine(120,r.y+42+offset(),width(price,16),muted());
            }
            if(i+1<Game::MarketShop::ROW_COUNT)c.drawFastHLine(68,r.y+ROW_H-1+offset(),224,line());
        }
        c.setClipRect(left,top,right-left,bottom-top);
        constexpr int h=LIST_BOTTOM-LIST_TOP;const int thumb=h*h/(Game::MarketShop::ROW_COUNT*ROW_H);
        c.fillRoundRect(298,LIST_TOP+scroll_*(h-thumb)/maxScroll()+offset(),3,thumb,1,line());
    }
    void listRow(Rect r,const char* label,Game::ItemId id,bool selected,bool divider,int menuIcon=-1) const {
        auto& c=PixelRenderer::canvas();const int cy=r.y+r.h/2;
        if(selected)c.fillRoundRect(r.x,r.y+2+offset(),r.w,r.h-4,8,rgb(224,235,201));
        if(menuIcon>=0)MarketStallIcons::draw(menuIcon==1,92,cy+offset());
        else {
            const auto kind=id==Game::ItemId::COUNT?GameAssets::Kind::MARKET_MYSTERY_BOX:GameAssets::itemKind(id);
            // Advice rows are shorter than shelf rows; all three illustrations
            // use the same32px box rather than touching their dividers.
            const float size=id==Game::ItemId::POTION?32.f/36.f:32.f/48.f;
            GameAssets::drawCentered(kind,92,cy+offset(),size);
        }
        text(126,cy-10,label);
        if(selected){const int y=cy+offset();c.drawLine(274,y,278,y+4,muted());c.drawLine(278,y+4,286,y-5,muted());}
        if(divider)c.drawFastHLine(68,r.y+r.h-1+offset(),224,line());
    }
    void renderMenu() const {
        listRow(menuRow(0),MarketShopText::BUY_ITEMS,Game::ItemId::POTION,false,true,0);
        listRow(menuRow(1),MarketShopText::ADVICE,Game::ItemId::HEART_SCALE,false,false,1);
    }
    void renderAdvice(const Game::GameState& s) const {
        const char* labels[]={MarketShopText::ADVICE_EXPLORE,MarketShopText::ADVICE_GROWTH,MarketShopText::ADVICE_RARE};
        const Game::ItemId icons[]={Game::ItemId::POTION,Game::ItemId::PROTEIN,Game::ItemId::COUNT};
        for(int i=0;i<3;++i)listRow(adviceRow(i),labels[i],icons[i],s.marketSupply.pendingAdvice==uint8_t(i+1),i<2);
        if(error_)drawBody(error_,{72,307,216,18},true,2,red());
    }
    void renderAdviceDone() const {
        drawBody(MarketShopText::ADVICE_DONE,{68,204,224,52},true,4);
        button(wide(),MarketShopText::HOME,true);
    }
    void renderConfirm(const Game::GameState& s,bool bought=false) const {
        const auto id=Game::MarketShop::item(s,slot_);const bool box=slot_==Game::MarketShop::BOX;
        const auto price=Game::MarketShop::priceForSlot(s,slot_);
        if(!bought)slotIcon(id,box,88,178);
        text(124,154,box?MarketShopText::BOX:Game::ShopService::shortName(id));rtext(292,180,"×1");
        char value[32];std::snprintf(value,sizeof(value),"%u",price);text(124,180,value,16,muted());
        const auto result=bought?Game::MarketShop::Result::OK:Game::MarketShop::check(s,slot_,period_);
        char why[64]{};
        if(result==Game::MarketShop::Result::NO_COINS)std::snprintf(why,sizeof(why),MarketShopText::NO_COINS,static_cast<unsigned long>(price-s.coins));
        else if(result!=Game::MarketShop::Result::OK)std::snprintf(why,sizeof(why),"%s",reason(result,0));
        const char* shown=error_?error_:why[0]?why:nullptr;
        drawBody(shown?shown:box?MarketShopText::BOX_LINE1:Game::ShopService::shortDescription(id),{68,208,224,52},false,2,shown?red():ink());
        char stock[32];std::snprintf(stock,sizeof(stock),MarketShopText::STOCK,bought?stockBefore_:s.marketSeason.stock[slot_]);ctext(264,stock);
        button(back(),MarketShopText::BACK,false);
        // Price already appears above; the compact action stays inside its
        // circular safe area even for the largest prices.
        button(primary(),result==Game::MarketShop::Result::OK?"购买":MarketShopText::UNAVAILABLE,true,result!=Game::MarketShop::Result::OK||pending_);
    }
    void renderResult(const Game::GameState& s) const {
        const auto id=rewardItem_==Game::ItemId::COUNT?Game::MarketShop::item(s,slot_):rewardItem_;
        icon(id,W/2,192);
        if(slot_==Game::MarketShop::BOX){renderBoxText(MarketBoxPerformance::OPEN_MS);return;}
        char got[96];std::snprintf(got,sizeof(got),MarketShopText::GOT,Game::ShopService::shortName(id));
        drawBody(got,{68,226,224,34},true);ctext(264,MarketShopText::GOT_LINE2,16);
        button(wide(),MarketShopText::CONTINUE,true);
    }
    void renderBoxText(uint32_t ms) const {
        using namespace MarketBoxPerformance::detail;
        char got[96];std::snprintf(got,sizeof(got),MarketShopText::BOX_GOT,Game::ShopService::shortName(rewardItem_),rewardAmount_);
        const float show=smooth(p(ms,900,1120));
        if(show>0){drawBody(got,{68,226,224,34},true,2,fade(40,56,80,show));ctext(264,MarketShopText::GOT_LINE2,16,fade(89,105,88,show));}
        if(ms>1050){button(wide(),MarketShopText::CONTINUE,true);
            PixelRenderer::canvas().blendRectAlphaNative(wide().x,wide().y+offset(),wide().w,wide().h,paper(),uint8_t(std::lround((1-smooth(p(ms,1050,1250)))*255)));}
    }
};
} // namespace Ui
