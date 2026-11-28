#pragma once
#include <algorithm>
#include "presentation/FurnitureArt.h"
#include "presentation/HomePhotoFrame.h"
#include "ui/common/MarketWorkshopView.h"
#include "core/TouchTap.h"
#include "core/UiStrings.h"
#include "game/MarketWorkshopService.h"
#include "presentation/TextBlock.h"
#include "ui/common/MarketCoinHud.h"
#include "ui/common/MarketDialogueUi.h"
#include "ui/common/MarketStallMotion.h"
#include "ui/common/MarketUiFeedback.h"

namespace Ui {
// The application owns durable state. Swatches only change the local preview.
class MarketWorkshopUi {
public:
    using Layout = MarketWorkshopView;
    enum class Page:uint8_t {CLOSED,DIALOGUE,REWARD,MENU,FURNITURE,WALLPAPER};
    enum class Request:uint8_t {NONE,GRANT,APPLY_WALLPAPER};
    MarketWorkshopUi()=default;
    explicit MarketWorkshopUi(Layout layout):layout_(layout){}
    bool active()const{return page_!=Page::CLOSED;}
    Page page()const{return page_;}
    unsigned dialogueIndex()const{return dialogue_;}
    bool pending()const{return pending_;}
    bool exiting()const{return exit_!=Exit::NONE;}
    Game::MarketWorkshop::Wallpaper selectedWallpaper() const {
        return Game::MarketWorkshop::WALLPAPERS[selected_].id;
    }
    int swatchOffset() const { return offset_; }
    bool scrollRows(int& top,int& bottom) const {
        if (page_!=Page::WALLPAPER) return false;
        const auto row=layout_.selectionRow();
        top=row.y;bottom=row.y+row.h;
        return true;
    }
    int panelShift(int distance) const {
        if(page_!=Page::MENU && page_!=Page::FURNITURE && page_!=Page::WALLPAPER)return 0;
        return MarketStallMotion::tween(0,distance,openingMs_,MOTION_MS)-exitOffset(distance);
    }
    bool takeSfx(SfxCue& cue){return feedback_.takeSfx(cue);}
    void open(const Game::GameState& state){
        close();
        if(Game::MarketWorkshop::hasFrame(state)){showMenu(true);return;}
        page_=Page::DIALOGUE;dialogue_=0;openingMs_=0;error_=false;startDialogue();
    }
    void close(){page_=Page::CLOSED;exit_=Exit::NONE;pending_=false;cancelTouch();feedback_.clear();}
    void cancelTouch(){
        tap_.cancel();
        if (swipe_ != Swipe::NONE) offset_ = layout_.settledOffset(swipeOrigin_, selected_);
        swipe_ = Swipe::NONE;
        dragging_=false;
    }
    bool update(uint32_t ms){
        if(!active()||!ms)return false;
        const auto elapsed=std::min(ms,MOTION_MS);
        if(exiting()){
            exitMs_=std::min(MOTION_MS,exitMs_+elapsed);
            if(exitMs_==MOTION_MS){
                if(exit_==Exit::TO_REWARD)showReward();
                else if(exit_==Exit::TO_MENU)showMenu(true);
                else close();
            }
            return true;
        }
        if (page_==Page::WALLPAPER && snapElapsed_<SNAP_MS) {
            snapElapsed_=std::min(SNAP_MS,snapElapsed_+elapsed);
            const int previousOffset=offset_;
            offset_=MarketStallMotion::tween(snapFrom_,snapTo_,snapElapsed_,SNAP_MS);
            return offset_!=previousOffset;
        }
        if(openingMs_==MOTION_MS)
            return layout_.round&&page_==Page::DIALOGUE&&!pending_&&greeting_.update(elapsed);
        openingMs_=std::min(MOTION_MS,openingMs_+elapsed);return true;
    }
    Request touch(const Platform::IInputDevice::TouchEvent& e,const Game::GameState& state, MarketCoinMotion* coins=nullptr){
        if(!active()||pending_||exiting()||openingMs_<MOTION_MS){cancelTouch();return Request::NONE;}
        if(e.x<0||e.y<0||e.x>=layout_.width||e.y>=layout_.height||
           (layout_.round&&(e.x-180)*(e.x-180)+(e.y-180)*(e.y-180)>=180*180)){
            cancelTouch();return Request::NONE;
        }
        const bool showsBalance = page_ != Page::REWARD && !(layout_.round && page_ == Page::DIALOGUE);
        if (showsBalance && MarketCoinHud::inspect(e,coins,16,32,layout_.round ? MarketCoinHud::ROUND : MarketCoinHud::Layout{}))
            return Request::NONE;
        if(page_==Page::FURNITURE){
            const int target=layout_.furnitureBack().contains(e.x,e.y) ? BACK :
                             layout_.panel().contains(e.x,e.y) ? -1 : OUTSIDE;
            const int hit=tap_.update(e,target);
            if(hit==BACK || hit==OUTSIDE){showMenu(false);feedback_.set(SfxCue::UI_CANCEL);}
            return Request::NONE;
        }
        if(page_==Page::MENU){
            const int target=layout_.row().contains(e.x,e.y) ? OPEN :
                             layout_.furnitureRow().contains(e.x,e.y) ? OPEN_FURNITURE :
                             layout_.panel().contains(e.x,e.y) ? -1 : OUTSIDE;
            const int hit=tap_.update(e,target);
            if(hit==OUTSIDE){beginExit(Exit::TO_WALK);feedback_.set(SfxCue::UI_CANCEL);}
            if(hit==OPEN_FURNITURE){
                page_=Page::FURNITURE;
                cancelTouch();
                feedback_.set(SfxCue::UI_CONFIRM);
            }
            if(hit==OPEN){
                page_=Page::WALLPAPER;
                selected_=int(Game::MarketWorkshop::wallpaperIndex(Game::MarketWorkshop::equippedWallpaper(state)));
                offset_=layout_.settledOffset(0,selected_);
                snapElapsed_=SNAP_MS;
                error_=false;cancelTouch();
                photo_.enter(Game::HomeFurniture::placed(state,0));
                feedback_.set(SfxCue::UI_CONFIRM);
            }
            return Request::NONE;
        }
        if(page_==Page::WALLPAPER)return wallpaperTouch(e,state);
        if(layout_.round&&page_==Page::DIALOGUE) {
            if(!greeting_.touch(e,tap_))return Request::NONE;
        } else if(tap_.update(e,0)!=0)return Request::NONE;
        if(page_==Page::REWARD){
            if(layout_.round)beginExit(Exit::TO_MENU);
            else showMenu(true);
            return Request::NONE;
        }
        feedback_.set(SfxCue::UI_CONFIRM);
        if(dialogue_<2){++dialogue_;startDialogue();return Request::NONE;}
        pending_=true;feedback_.clear();return Request::GRANT;
    }
    void committed(bool saved){
        pending_=false;tap_.cancel();
        if(!saved){error_=true;startDialogue();feedback_.set(SfxCue::UI_CANCEL);return;}
        error_=false;
        if(layout_.round)beginExit(Exit::TO_REWARD);
        else showReward();
    }
    void already(){pending_=false;showMenu(true);}
    void wallpaperCommitted(bool saved){
        pending_=false;cancelTouch();error_=!saved;
        feedback_.set(saved ? SfxCue::UI_CONFIRM : SfxCue::UI_CANCEL);
    }
    void render(const Game::GameState& state,const MarketCoinMotion* motion=nullptr)const{
        if(!active())return;
        auto& c=PixelRenderer::canvas();
        if(page_==Page::MENU || page_==Page::FURNITURE){
            const int travel=layout_.height-layout_.panel().y;
            layout_.menu(state,motion,travel-panelShift(travel),panelShift(64)-64,page_==Page::FURNITURE);
            return;
        }
        if(page_==Page::WALLPAPER){
            layout_.wallpaper(state,selected_,offset_,error_,photo_.wallPhotoPixels(),motion);
            return;
        }
        if(page_==Page::REWARD){
            const int alpha=exiting()?MarketStallMotion::tween(156,0,exitMs_,MOTION_MS):156;
            c.blendRectAlphaNative(0,0,layout_.width,layout_.height,0,uint8_t(alpha));
            if(exiting())return;
            drawFrame(layout_.width/2,layout_.height/2);
            constexpr int labelWidth=4*32;
            PixelRenderer::bitmapText(c,(layout_.width-labelWidth)/2,layout_.height/2+76,MarketWorkshopText::RECEIVED,color(248,248,224),BitmapText::Font::PX32);
            return;
        }
        if(layout_.round){
            const int height=MarketDialogueUi::ROUND_HEIGHT;
            greeting_.render(MarketStallMotion::tween(height,0,openingMs_,MOTION_MS)+exitOffset(height));
            return;
        }
        const int y=layout_.height-140+MarketStallMotion::tween(140,0,openingMs_,MOTION_MS)+exitOffset(140);
        c.fillRoundRect(0,y,layout_.width,180,16,color(248,248,224));c.drawRoundRect(0,y,layout_.width,180,16,color(48,64,112));
        const char* remaining=dialogueText();
        for(int row=0;row<3&&*remaining;++row){
            constexpr int width=320;
            TextBlock::Options o;o.width=width;o.maxHeight=32;o.font=BitmapText::Font::PX32;o.align=TextBlock::Align::CENTER;
            text_.setText(remaining);text_.setOptions(o);const auto line=text_.line(0);if(!line.end)break;
            text_.draw(c,(layout_.width-width)/2,y+8+row*34,color(40,56,80),TextBlock::DrawRange{0,1,-1});
            remaining+=line.end;while(*remaining=='\n'||*remaining==' ')++remaining;
        }
        MarketCoinHud::render(state.coins,motion,16+exitOffset(MarketStallMotion::HUD_HIDDEN),32,{340,32,48,48});
    }
    static void drawFrame(int cx,int cy){
        FurnitureArt::drawFrame(PixelRenderer::canvas(),cx,cy);
    }
private:
    enum class Exit : uint8_t { NONE, TO_REWARD, TO_MENU, TO_WALK };
    static constexpr uint32_t MOTION_MS=360;
    Layout layout_{};Page page_=Page::CLOSED;uint32_t openingMs_=0;uint8_t dialogue_=0;
    Exit exit_=Exit::NONE;
    uint32_t exitMs_=0;
    enum Target { OPEN, OPEN_FURNITURE, OUTSIDE, BACK, APPLY, FIRST_SWATCH };
    enum class Swipe : uint8_t { NONE, ROOM, STRIP };
    static constexpr uint32_t SNAP_MS=150;
    int selected_=0, offset_=0;
    int snapFrom_=0, snapTo_=0;
    uint32_t snapElapsed_=SNAP_MS;
    HomePhotoFrame photo_;
    bool pending_=false,error_=false,dragging_=false;
    Swipe swipe_=Swipe::NONE;
    int swipeX_=0,swipeY_=0,swipeOrigin_=0;
    TouchTap tap_;MarketUiFeedback feedback_;mutable TextBlock text_;
    MarketDialogueUi::RoundGreeting greeting_;

    void selectSwatch(int index) {
        index=std::clamp(index,0,Layout::swatchCount-1);
        if (index!=selected_) {
            selected_=index;
            error_=false;
            feedback_.set(SfxCue::UI_CONFIRM);
        }
        snapFrom_=offset_;
        snapTo_=layout_.settledOffset(offset_,selected_);
        snapElapsed_=snapFrom_==snapTo_ ? SNAP_MS : 0;
    }
    Request wallpaperTouch(const TouchTap::Event& e,const Game::GameState& state){
        if (e.type==TouchTap::Type::CANCEL) { cancelTouch(); return Request::NONE; }
        if (e.type==TouchTap::Type::DOWN) {
            cancelTouch();
            const auto strip=layout_.swatchViewport();
            // The two-swatch layout also accepts horizontal swipes across its label.
            if (strip.contains(e.x,e.y) || (Layout::swatchCount<=2 && layout_.selectionRow().contains(e.x,e.y)))
                swipe_=Swipe::STRIP;
            else if (layout_.room().contains(e.x,e.y)) swipe_=Swipe::ROOM;
            if (swipe_!=Swipe::NONE) {
                snapElapsed_=SNAP_MS;
                swipeX_=e.x;swipeY_=e.y;swipeOrigin_=offset_;
            }
        }
        const int tile=layout_.swatchAt(e.x,e.y,offset_);
        const int target=layout_.back().contains(e.x,e.y) ? BACK :
                         layout_.action().contains(e.x,e.y) ? APPLY : tile>=0 ? FIRST_SWATCH+tile : -1;
        const int hit=tap_.update(e,target);
        if (swipe_!=Swipe::NONE) {
            const int dx=e.x-swipeX_;
            const auto area=swipe_==Swipe::ROOM ? layout_.room() : layout_.selectionRow();
            if (std::abs(e.y-swipeY_)>24 || e.y<area.y || e.y>=area.y+area.h) {
                cancelTouch();
                return Request::NONE;
            }
            if (std::abs(dx)>10) dragging_=true;
            if (swipe_==Swipe::STRIP && dragging_)
                offset_=std::clamp(swipeOrigin_-dx,0,layout_.maxSwatchOffset());
            if (e.type!=TouchTap::Type::UP) return Request::NONE;

            const bool room=swipe_==Swipe::ROOM;
            swipe_=Swipe::NONE;
            if (std::abs(dx)>=(room || !layout_.maxSwatchOffset() ? 32 : layout_.swatchPitch()/2)) {
                const int steps=room ? 1 : (std::abs(dx)+layout_.swatchPitch()/2)/layout_.swatchPitch();
                selectSwatch(selected_+(dx<0 ? steps : -steps));
            } else {
                selectSwatch(!dragging_ && hit>=FIRST_SWATCH ? hit-FIRST_SWATCH : selected_);
            }
            return Request::NONE;
        }
        if(hit==BACK){
            showMenu(false);feedback_.set(SfxCue::UI_CANCEL);
        }else if(hit==APPLY && Game::MarketWorkshop::equippedWallpaper(state)!=selectedWallpaper()){
            if(state.coins<Game::MarketWorkshop::WALLPAPERS[selected_].price) return Request::NONE;
            if(RoomRenderer::preparePreview(layout_.night(state),Game::HomeFurniture::visibleMask(state),selectedWallpaper())){
                pending_=true;feedback_.clear();
                return Request::APPLY_WALLPAPER;
            }
        }
        return Request::NONE;
    }
    void showMenu(bool animate){
        page_=Page::MENU;openingMs_=animate ? 0 : MOTION_MS;
        exit_=Exit::NONE;error_=false;cancelTouch();
    }
    const char* dialogueText() const {
        if(error_)return MarketWorkshopText::RETRY;
        if(dialogue_==0)return MarketWorkshopText::WELCOME;
        return dialogue_==1?MarketWorkshopText::GIFT:MarketWorkshopText::OPENING;
    }
    void startDialogue(){if(layout_.round)greeting_.start(dialogueText());}
    void beginExit(Exit exit){exit_=exit;exitMs_=0;cancelTouch();}
    void showReward(){exit_=Exit::NONE;page_=Page::REWARD;feedback_.set(SfxCue::ITEM_GET);}
    int exitOffset(int distance)const{
        return exiting()?MarketStallMotion::tween(0,distance,exitMs_,MOTION_MS):0;
    }
    static uint16_t color(int r,int g,int b){return PixelRenderer::rgb(uint8_t(r),uint8_t(g),uint8_t(b));}
};
}
