#pragma once

#include <algorithm>
#include <cstdio>
#include <cstring>
#include "assets/GameAssets.h"
#include "core/TouchTap.h"
#include "core/UiStrings.h"
#include "game/MarketShopService.h"
#include "ui/common/MarketExpansionUi.h"
#include "game/MarketSupply.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TextBlock.h"
#include "ui/common/MarketStallMotion.h"
#include "ui/common/MarketStallIcons.h"
#include "ui/common/MarketListScroll.h"
#include "ui/common/MarketUiFeedback.h"
#include "ui/common/MarketCoinHud.h"
#include "ui/common/MarketBoxPerformance.h"
#include "ui/common/MarketDialogueUi.h"
#include "ui/common/MarketView.h"
#include "ui/round_touch/MarketStallUi.h"

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
    using Rect = RoundMarketStall::Rect;

    static constexpr int W = 360, H = 360, PANEL_Y = 132, ROW_Y = 140, ROW_H = 72, GREET_H = MarketDialogueUi::ROUND_HEIGHT;
    static constexpr int LIST_TOP = ROW_Y, LIST_BOTTOM = 360;
    static constexpr uint32_t GREET_IN_MS = 360, OPEN_MS = 420, CLOSE_MS = 520;

    bool takeSfx(SfxCue& cue) { return feedback_.takeSfx(cue); }
    bool active() const { return page_ != Page::CLOSED; }
    Page page() const { return page_; }
    bool pending() const { return pending_; }
    bool animating() const { return transition_ != Transition::NONE; }
    uint8_t selected() const { return slot_; }
    int scrollOffset() const { return scroll_; }
    int panelTop() const { return page_ == Page::GREET ? H - GREET_H + offset() : PANEL_Y + offset(); }
    int hudOffset() const {
        if (page_ == Page::GREET) return MarketStallMotion::HUD_HIDDEN;
        if (transition_ == Transition::OPENING && page_ != Page::ERROR) return tween(MarketStallMotion::HUD_HIDDEN,0);
        return transition_==Transition::CLOSING?tween(0,MarketStallMotion::HUD_HIDDEN):0;
    }

    void open(const Game::GameState& state, bool initialized = true) {
        *this = RoundMarketShopUi{};
        page_ = initialized ? Page::GREET : Page::ERROR; transition_ = initialized ? Transition::GREET_IN : Transition::OPENING;
        if(initialized)greeting_.start(Game::MarketShop::soldOut(state) ? MarketShopText::GREET_SOLD_OUT : MarketShopText::GREET);
        period_ = state.marketSeason.periodDay;
        rowCount_ = Game::MarketShop::rowCount(state);
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
        return page_ == Page::GREET && greeting_.update(deltaMs);
    }

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
        rowCount_ = Game::MarketShop::rowCount(state);
        scroll_ = std::min(scroll_,maxScroll());
        if (!inside(e.x,e.y)) { cancelTouch(); return none; }
        if (pending_ || page_ == Page::CLOSED || animating()) { cancelTouch(); return none; }
        if (page_ != Page::GREET && MarketCoinHud::inspect(e,coins,16+hudOffset(),32,MarketCoinHud::ROUND)) return none;
        if (page_ == Page::BOX_ARRIVE || page_ == Page::BOX_OPEN) { cancelTouch(); return none; }
        if (page_ == Page::BOX_WAIT) {
            const int target = RoundMarketStall::PANEL.contains(e.x,e.y) ? PRIMARY : -1;
            if (tap_.update(e,target) == PRIMARY) { changePage(Page::BOX_OPEN); boxMs_ = 0; }
            return none;
        }
        if (page_ == Page::GREET) {
            if (greeting_.touch(e,tap_)) { feedback_.set(SfxCue::UI_CONFIRM); openPanel(); }
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
        if (page_ == Page::GREET) { greeting_.render(offset()); return; }
        panel();
        if (page_ == Page::MENU) renderMenu();
        else if (page_ == Page::SHELF) renderShelf(s);
        else if (page_ == Page::ADVICE) renderAdvice(s);
        else if (page_ == Page::ADVICE_DONE) renderAdviceDone();
        else if (page_ == Page::CONFIRM) renderConfirm(s);
        else if (page_ == Page::ERROR) { drawBody(MarketShopText::SAVE_FAILED,{68,204,224,52},true,4,red()); button(wide(),"离开",true); }
        else if (page_ == Page::BOX_ARRIVE) {
            renderConfirm(s,true);
            PixelRenderer::canvas().blendRoundRectAlphaNative(49,PANEL_Y+1,262,234,13,paper(),MarketBoxPerformance::detailFade(boxMs_));
            MarketBoxPerformance::arrival(boxMs_,boxStage);
        }
        else if (page_ == Page::BOX_WAIT) {
            MarketBoxPerformance::waiting(boxMs_,boxStage);
            ctext(284,MarketShopText::BOX_OPEN,32,MarketBoxPerformance::detail::fade(89,105,88,
                MarketBoxPerformance::detail::smooth(MarketBoxPerformance::detail::p(boxMs_,0,220))));
        }
        else if (page_ == Page::BOX_OPEN) { MarketBoxPerformance::opening(boxMs_,rewardItem_,boxStage); renderBoxText(boxMs_); }
        else renderResult(s);
        RoundMarketStall::hud(s.coins,coins,hudOffset());
    }

private:
    static bool inside(int x,int y) { return (x-180)*(x-180)+(y-180)*(y-180)<180*180; }
    inline static constexpr MarketBoxPerformance::Layout boxStage{180,224,88,164,262,170};
    enum class Transition : uint8_t { NONE, GREET_IN, OPENING, CLOSING };
    enum Target { CLOSE = 10, BACK, PRIMARY, OUTSIDE, OPEN_SHELF, OPEN_ADVICE, ADVICE0 = 40 };
    Page page_ = Page::CLOSED;
    Transition transition_ = Transition::NONE;
    uint32_t transitionMs_ = 0, period_ = 0;
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
    uint8_t rowCount_ = Game::MarketShop::ROW_COUNT + 1;
    const char* error_ = nullptr;
    MarketDialogueUi::RoundGreeting greeting_;
    mutable TextBlock bodyText_;

    Rect row(int i) const { return {54, ROW_Y + i * ROW_H - scroll_, 252, ROW_H}; }
    static Rect wide() { return RoundMarketStall::ACTION; }
    static Rect purchase() { auto r=wide();r.y+=10;return r; }
    static Rect menuRow(int index) { return {58, 170 + index * 80, 244, 80}; }
    static Rect adviceRow(int index) { return {58, 142 + index * 64, 244, 64}; }
    int maxScroll() const { return rowCount_ * ROW_H + 56 - (LIST_BOTTOM - LIST_TOP); }
    bool listDrag(const Platform::IInputDevice::TouchEvent& e) {
        return scrollMotion_.touch(e, {54, LIST_TOP, 252, LIST_BOTTOM - LIST_TOP},
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
        if (!RoundMarketStall::PANEL.contains(x,y)) return OUTSIDE;
        if (page_ == Page::ERROR) return wide().contains(x,y) ? CLOSE : -1;
        if (page_ == Page::SHELF) {
            if (y < LIST_TOP || y >= LIST_BOTTOM) return -1;
            for (int i = 0; i < Game::MarketShop::rowCount(s); ++i)
                if (row(i).contains(x, y) && Game::MarketShop::stock(s,uint8_t(i)) &&
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
            return (slot_==Game::MarketShop::EXPANSION?expansionAction():purchase()).contains(x, y) && Game::MarketShop::check(s,slot_,period_)==Game::MarketShop::Result::OK ? PRIMARY : -1;
        }
        if (page_ == Page::RESULT || page_ == Page::ADVICE_DONE) return (page_==Page::RESULT&&slot_==Game::MarketShop::EXPANSION?expansionAction():wide()).contains(x, y) ? PRIMARY : -1;
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
            stockBefore_ = Game::MarketShop::stock(s,slot_);
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
    static int width(const char* s,int size) {return RoundMarketStall::width(s,size);}
    void text(int x,int y,const char* s,int size=32,uint16_t color=0) const {RoundMarketStall::text(x,y+offset(),s,size,color);}
    void ctext(int y,const char* s,int size=32,uint16_t color=0) const {text((W-width(s,size))/2,y,s,size,color);}
    void rtext(int x,int y,const char* s,int size=32,uint16_t color=0) const {text(x-width(s,size),y,s,size,color);}
    void panel() const {RoundMarketStall::panel(offset());}
    void button(Rect r,const char* label,bool primary,bool disabled=false) const {RoundMarketStall::button(r,label,primary,disabled,offset());}
    void icon(Game::ItemId id,int cx,int cy) const {
        if(!GameAssets::drawItemIcon48(id,cx,cy+offset()))PixelRenderer::canvas().fillCircle(cx,cy+offset(),14,line());
    }
    void slotIcon(Game::ItemId id,bool box,int cx,int cy) const {
        if(box)GameAssets::drawCentered(GameAssets::Kind::MARKET_MYSTERY_BOX,cx,cy+offset());
        else icon(id,cx,cy);
    }
    void drawBody(const char* value,Rect r,bool centered=false,int=2,uint16_t color=0,int maxSize=32) const {
        r.y+=offset();RoundMarketStall::body(bodyText_,value,r,centered,color,false,0,maxSize);
    }
    void renderShelf(const Game::GameState& s) const {
        constexpr int priceSize=24;
        auto& c=PixelRenderer::canvas();
        const int left=c.clipLeft(),top=c.clipTop(),right=c.clipRight(),bottom=c.clipBottom();
        const int clipX=std::max(left,54),clipY=std::max(top,LIST_TOP+offset());
        c.setClipRect(clipX,clipY,std::max(0,std::min(right,306)-clipX),std::max(0,std::min(bottom,LIST_BOTTOM+offset())-clipY));
        for(uint8_t i=0;i<Game::MarketShop::rowCount(s);++i){
            const auto r=row(i);if(r.y+ROW_H<=LIST_TOP||r.y>=LIST_BOTTOM)continue;
            const bool box=i==Game::MarketShop::BOX;const auto id=Game::MarketShop::item(s,i);
            const bool empty=!box&&id==Game::ItemId::COUNT,sold=empty||!Game::MarketShop::stock(s,uint8_t(i));
            if(sold)c.fillRect(r.x,r.y+offset(),r.w,r.h,off());
            slotIcon(empty?Game::ItemId::HEART_SCALE:id,box,88,r.y+36);
            const char* name=empty?MarketShopText::RARE:box?MarketShopText::BOX:Game::ShopService::shortName(id);
            char price[20];if(empty)std::snprintf(price,sizeof(price),"--");
            else std::snprintf(price,sizeof(price),"%u",Game::MarketShop::priceForSlot(s,i));
            const int nameX=i==Game::MarketShop::EXPANSION?120:128;
            const int available=std::min(100,292-width(price,priceSize)-12-nameX);
            const int size=width(name,32)<=available?32:width(name,24)<=available?24:width(name,20)<=available?20:16;
            text(nameX,r.y+(ROW_H-size)/2,name,size,sold?muted():ink());
            rtext(292,r.y+(ROW_H-priceSize)/2,price,priceSize,sold?muted():ink());
            if(sold){c.blendRectAlphaNative(64,r.y+12+offset(),48,48,off(),160);
                c.drawFastHLine(nameX,r.y+36+offset(),width(name,size),muted());
                c.drawFastHLine(292-width(price,priceSize),r.y+36+offset(),width(price,priceSize),muted());}
            if(i+1<Game::MarketShop::rowCount(s))c.drawFastHLine(68,r.y+ROW_H-1+offset(),224,line());
        }
        c.setClipRect(left,top,right-left,bottom-top);
        constexpr int h=LIST_BOTTOM-LIST_TOP;const int total=Game::MarketShop::rowCount(s)*ROW_H+56,thumb=h*h/total;
        c.fillRect(298,LIST_TOP+offset(),3,h,off());
        c.fillRect(298,LIST_TOP+scroll_*(h-thumb)/maxScroll()+offset(),3,thumb,line());
    }
    void listRow(Rect r,const char* label,Game::ItemId id,bool selected,bool divider,int menuIcon=-1) const {
        auto& c=PixelRenderer::canvas();const int cy=r.y+r.h/2;
        if(selected)c.fillRoundRect(r.x,r.y+3+offset(),r.w,r.h-6,8,rgb(224,235,201));
        if(menuIcon>=0)MarketStallIcons::draw(menuIcon==1,92,cy+offset());
        else slotIcon(id,id==Game::ItemId::COUNT,92,cy);
        text(132,cy-16,label);
        if(selected){const auto green=rgb(65,132,83);const int y=cy+offset();
            c.fillRect(276,y,4,4,green);c.fillRect(280,y+4,4,4,green);c.fillRect(284,y,4,4,green);c.fillRect(288,y-4,4,4,green);}
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
        if(error_)drawBody(error_,{108,326,144,18},true,2,red(),16);
    }
    void renderAdviceDone() const {
        drawBody(MarketShopText::ADVICE_DONE,{68,198,224,72},true);
        button(wide(),MarketShopText::HOME,true);
    }
    static Rect expansionAction(){return {96,300,168,44};}
    void renderExpansion(const Game::GameState& s,bool success) const {
        const auto current=Game::StorageCapacity::capacity(s);
        if(success){
            icon(Game::ItemId::COMPUTER_EXPANSION,180,164);ctext(204,MarketExpansionUi::SUCCESS,28);
            MarketExpansionUi::capacity(uint8_t(current-20),current,180,250+offset(),36,ink(),true);
            button(expansionAction(),MarketExpansionUi::LEAVE,true);return;
        }
        icon(Game::ItemId::COMPUTER_EXPANSION,88,168);text(124,155,Game::ShopService::shortName(Game::ItemId::COMPUTER_EXPANSION),24);
        MarketExpansionUi::capacity(current,Game::StorageCapacity::nextCapacity(s),180,230+offset(),36,ink());
        if(error_)drawBody(error_,{68,193,224,24},true,2,red(),16);
        char label[32];std::snprintf(label,sizeof(label),MarketShopText::BUY,Game::MarketShop::priceForSlot(s,slot_));
        button(expansionAction(),label,true,pending_||Game::MarketShop::check(s,slot_,period_)!=Game::MarketShop::Result::OK);
    }

    void renderConfirm(const Game::GameState& s,bool bought=false) const {
        if(slot_==Game::MarketShop::EXPANSION){renderExpansion(s,false);return;}
        const auto id=Game::MarketShop::item(s,slot_);const bool box=slot_==Game::MarketShop::BOX;
        const auto price=Game::MarketShop::priceForSlot(s,slot_);
        if(!bought)slotIcon(id,box,88,164);
        const char* name=box?MarketShopText::BOX:Game::ShopService::shortName(id);
        text(124,148,name,width(name,32)<=112?32:width(name,28)<=112?28:24);rtext(292,148,"×1",32);
        const auto result=bought?Game::MarketShop::Result::OK:Game::MarketShop::check(s,slot_,period_);
        const char* shown=error_?error_:result!=Game::MarketShop::Result::OK&&result!=Game::MarketShop::Result::NO_COINS?reason(result,0):nullptr;
        if(shown)drawBody(shown,{68,184,224,20},false,2,red(),16);
        drawBody(box?MarketShopText::BOX_LINE1:Game::ShopService::shortDescription(id),{68,206,224,36});
        char stock[32];std::snprintf(stock,sizeof(stock),MarketShopText::STOCK,bought?stockBefore_:Game::MarketShop::stock(s,slot_));ctext(244,stock);
        char label[32];if(result==Game::MarketShop::Result::OK||result==Game::MarketShop::Result::NO_COINS)std::snprintf(label,sizeof(label),MarketShopText::BUY,price);
        else std::snprintf(label,sizeof(label),"%s",MarketShopText::UNAVAILABLE);
        button(purchase(),label,true,result!=Game::MarketShop::Result::OK||pending_);
    }
    void renderResult(const Game::GameState& s) const {
        if(slot_==Game::MarketShop::EXPANSION){renderExpansion(s,true);return;}
        const auto id=rewardItem_==Game::ItemId::COUNT?Game::MarketShop::item(s,slot_):rewardItem_;
        if(slot_==Game::MarketShop::BOX){MarketBoxPerformance::detail::reward(id,boxStage.rewardY,1,1,boxStage.cx);renderBoxText(MarketBoxPerformance::OPEN_MS);return;}
        icon(id,W/2,160);char got[96];std::snprintf(got,sizeof(got),MarketShopText::GOT,Game::ShopService::shortName(id));
        drawBody(got,{68,192,224,68},true);ctext(260,MarketShopText::GOT_LINE2,16);button(wide(),MarketShopText::CONTINUE,true);
    }
    void renderBoxText(uint32_t ms) const {
        using namespace MarketBoxPerformance::detail;
        char got[96];std::snprintf(got,sizeof(got),MarketShopText::BOX_GOT,Game::ShopService::shortName(rewardItem_),rewardAmount_);
        const float show=smooth(p(ms,900,1120));
        if(show>0){drawBody(got,{68,200,224,56},true,2,fade(40,56,80,show));ctext(260,MarketShopText::GOT_LINE2,16,fade(89,105,88,show));}
        if(ms>1050){button(wide(),MarketShopText::CONTINUE,true);
            PixelRenderer::canvas().blendRoundRectAlphaNative(wide().x,wide().y+offset(),wide().w,wide().h,10,paper(),uint8_t(std::lround((1-smooth(p(ms,1050,1250)))*255)));}
    }
};
} // namespace Ui
