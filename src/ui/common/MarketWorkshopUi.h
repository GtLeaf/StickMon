#pragma once
#include <algorithm>
#include <cmath>
#include "presentation/FurnitureArt.h"
#include "core/TouchTap.h"
#include "core/UiStrings.h"
#include "game/MarketWorkshopService.h"
#include "presentation/TextBlock.h"
#include "ui/common/MarketCoinHud.h"
#include "ui/common/MarketStallMotion.h"
#include "ui/common/MarketUiFeedback.h"
#include "ui/common/MarketView.h"

namespace Ui {
// A gift conversation only. The application owns durable state and saving.
class MarketWorkshopUi {
public:
    struct Layout {int width=368,height=448;bool round=false;};
    enum class Page:uint8_t {CLOSED,DIALOGUE,REWARD};
    enum class Request:uint8_t {NONE,GRANT};
    MarketWorkshopUi()=default;
    explicit MarketWorkshopUi(Layout layout):layout_(layout){}
    bool active()const{return page_!=Page::CLOSED;}
    Page page()const{return page_;}
    unsigned dialogueIndex()const{return dialogue_;}
    bool pending()const{return pending_;}
    bool takeSfx(SfxCue& cue){return feedback_.takeSfx(cue);}
    void open(const Game::GameState& state){
        close();page_=Page::DIALOGUE;repeat_=Game::MarketWorkshop::hasFrame(state);
        dialogue_=repeat_?2:0;openingMs_=0;error_=false;
    }
    void close(){page_=Page::CLOSED;pending_=false;tap_.cancel();feedback_.clear();}
    void cancelTouch(){tap_.cancel();}
    bool update(uint32_t ms){
        if(!active()||openingMs_>=360)return false;
        openingMs_=std::min<uint32_t>(360,openingMs_+std::min<uint32_t>(ms,360));return true;
    }
    Request touch(const Platform::IInputDevice::TouchEvent& e,const Game::GameState&){
        if(!active()||pending_||openingMs_<360){tap_.cancel();return Request::NONE;}
        if(e.x<0||e.y<0||e.x>=layout_.width||e.y>=layout_.height||
           (layout_.round&&(e.x-180)*(e.x-180)+(e.y-180)*(e.y-180)>=180*180)){
            tap_.cancel();return Request::NONE;
        }
        if(tap_.update(e,0)!=0)return Request::NONE;
        if(page_==Page::REWARD||repeat_){close();return Request::NONE;}
        feedback_.set(SfxCue::UI_CONFIRM);
        if(dialogue_<2){++dialogue_;return Request::NONE;}
        pending_=true;feedback_.clear();return Request::GRANT;
    }
    void committed(bool saved){
        pending_=false;tap_.cancel();
        if(!saved){error_=true;feedback_.set(SfxCue::UI_CANCEL);return;}
        error_=false;page_=Page::REWARD;feedback_.set(SfxCue::ITEM_GET);
    }
    void already(){pending_=false;tap_.cancel();repeat_=true;dialogue_=2;error_=false;page_=Page::DIALOGUE;}
    int sceneShift(const Market::Session& s,const MarketView::Viewport& v)const{
        int top=32767,bottom=-32767;
        for(int role=0;role<2;++role){const int actor=s.conversationActor(role);if(actor<0)continue;
            MarketView::detail::ActorDraw draw;
            if(!MarketView::detail::prepareActor(s,v,s.timeMs(),v.cameraX(s),v.cameraY(s),s.pose(actor),draw))continue;
            const auto b=PokemonSprites::frameVisibleBounds(draw.sprite);
            top=std::min(top,draw.y+int(b.top*draw.spriteScale));
            bottom=std::max(bottom,std::max(draw.y+int((b.top+b.height)*draw.spriteScale),draw.shadow.y+draw.shadow.radiusY));
        }
        const int shift=bottom<top?0:(top+bottom)/2-(layout_.round?88:156);
        return MarketStallMotion::tween(0,shift,openingMs_,360);
    }
    int sceneShiftX(const Market::Session& s,const MarketView::Viewport& v)const{
        const int a=s.conversationActor(0),b=s.conversationActor(1);if(a<0||b<0)return 0;
        const int shift=(v.pixel(s.pose(a).x)+v.pixel(s.pose(b).x))/2-layout_.width/2-v.cameraX(s);
        return MarketStallMotion::tween(0,shift,openingMs_,360);
    }
    void render(const Game::GameState& state,const MarketCoinMotion* motion=nullptr)const{
        if(!active())return;
        auto& c=PixelRenderer::canvas();
        if(page_==Page::REWARD){
            c.blendRectAlphaNative(0,0,layout_.width,layout_.height,0,156);
            drawFrame(layout_.width/2,layout_.height/2);
            constexpr int labelWidth=4*32;
            PixelRenderer::bitmapText(c,(layout_.width-labelWidth)/2,layout_.height/2+76,MarketWorkshopText::RECEIVED,color(248,248,224),BitmapText::Font::PX32);
            return;
        }
        const int y=layout_.height-140+MarketStallMotion::tween(140,0,openingMs_,360);
        c.fillRoundRect(0,y,layout_.width,180,16,color(248,248,224));c.drawRoundRect(0,y,layout_.width,180,16,color(48,64,112));
        const char* remaining=error_?MarketWorkshopText::RETRY:dialogue_==0?MarketWorkshopText::WELCOME:dialogue_==1?MarketWorkshopText::GIFT:MarketWorkshopText::OPENING;
        for(int row=0;row<3&&*remaining;++row){
            const int width=layout_.round?(row<2?256:192):320;
            TextBlock::Options o;o.width=width;o.maxHeight=32;o.font=BitmapText::Font::PX32;o.align=TextBlock::Align::CENTER;
            dialogueText_.setText(remaining);dialogueText_.setOptions(o);const auto line=dialogueText_.line(0);if(!line.end)break;
            dialogueText_.draw(c,(layout_.width-width)/2,y+8+row*34,color(40,56,80),TextBlock::DrawRange{0,1,-1});
            remaining+=line.end;while(*remaining=='\n'||*remaining==' ')++remaining;
        }
        int right=340;
        if(layout_.round){MarketCoinMotion still;if(!motion)still.reset(state.coins);const auto frame=motion?motion->frame():still.frame();right=layout_.width/2+int(std::lround((frame.widthDigits+1)*8));}
        MarketCoinHud::render(state.coins,motion,16,32,{right,32,48,48});
    }
    static void drawFrame(int cx,int cy){
        FurnitureArt::drawFrame(PixelRenderer::canvas(),cx,cy);
    }
private:
    Layout layout_{};Page page_=Page::CLOSED;uint32_t openingMs_=0;uint8_t dialogue_=0;
    bool repeat_=false,pending_=false,error_=false;
    TouchTap tap_;MarketUiFeedback feedback_;mutable TextBlock dialogueText_;
    static uint16_t color(int r,int g,int b){return PixelRenderer::rgb(uint8_t(r),uint8_t(g),uint8_t(b));}
};
}
