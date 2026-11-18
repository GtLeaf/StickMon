#pragma once
#include <algorithm>
#include "presentation/FurnitureArt.h"
#include "core/TouchTap.h"
#include "core/UiStrings.h"
#include "game/MarketWorkshopService.h"
#include "presentation/TextBlock.h"
#include "ui/common/MarketCoinHud.h"
#include "ui/common/MarketDialogueUi.h"
#include "ui/common/MarketStallMotion.h"
#include "ui/common/MarketUiFeedback.h"

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
    bool exiting()const{return exit_!=Exit::NONE;}
    bool takeSfx(SfxCue& cue){return feedback_.takeSfx(cue);}
    void open(const Game::GameState& state){
        close();page_=Page::DIALOGUE;repeat_=Game::MarketWorkshop::hasFrame(state);
        dialogue_=repeat_?2:0;openingMs_=0;error_=false;startDialogue();
    }
    void close(){page_=Page::CLOSED;exit_=Exit::NONE;pending_=false;tap_.cancel();feedback_.clear();}
    void cancelTouch(){tap_.cancel();}
    bool update(uint32_t ms){
        if(!active()||!ms)return false;
        const auto elapsed=std::min(ms,MOTION_MS);
        if(exiting()){
            exitMs_=std::min(MOTION_MS,exitMs_+elapsed);
            if(exitMs_==MOTION_MS){
                if(exit_==Exit::TO_REWARD)showReward();
                else close();
            }
            return true;
        }
        if(openingMs_==MOTION_MS)
            return layout_.round&&page_==Page::DIALOGUE&&!pending_&&greeting_.update(elapsed);
        openingMs_=std::min(MOTION_MS,openingMs_+elapsed);return true;
    }
    Request touch(const Platform::IInputDevice::TouchEvent& e,const Game::GameState&){
        if(!active()||pending_||exiting()||openingMs_<MOTION_MS){tap_.cancel();return Request::NONE;}
        if(e.x<0||e.y<0||e.x>=layout_.width||e.y>=layout_.height||
           (layout_.round&&(e.x-180)*(e.x-180)+(e.y-180)*(e.y-180)>=180*180)){
            tap_.cancel();return Request::NONE;
        }
        if(layout_.round&&page_==Page::DIALOGUE) {
            if(!greeting_.touch(e,tap_))return Request::NONE;
        } else if(tap_.update(e,0)!=0)return Request::NONE;
        if(page_==Page::REWARD||repeat_){
            if(layout_.round)beginExit(Exit::TO_WALK);
            else close();
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
    void already(){pending_=false;tap_.cancel();repeat_=true;dialogue_=2;error_=false;page_=Page::DIALOGUE;startDialogue();}
    void render(const Game::GameState& state,const MarketCoinMotion* motion=nullptr)const{
        if(!active())return;
        auto& c=PixelRenderer::canvas();
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
            dialogueText_.setText(remaining);dialogueText_.setOptions(o);const auto line=dialogueText_.line(0);if(!line.end)break;
            dialogueText_.draw(c,(layout_.width-width)/2,y+8+row*34,color(40,56,80),TextBlock::DrawRange{0,1,-1});
            remaining+=line.end;while(*remaining=='\n'||*remaining==' ')++remaining;
        }
        MarketCoinHud::render(state.coins,motion,16+exitOffset(MarketStallMotion::HUD_HIDDEN),32,{340,32,48,48});
    }
    static void drawFrame(int cx,int cy){
        FurnitureArt::drawFrame(PixelRenderer::canvas(),cx,cy);
    }
private:
    enum class Exit : uint8_t { NONE, TO_REWARD, TO_WALK };
    static constexpr uint32_t MOTION_MS=360;
    Layout layout_{};Page page_=Page::CLOSED;uint32_t openingMs_=0;uint8_t dialogue_=0;
    Exit exit_=Exit::NONE;
    uint32_t exitMs_=0;
    bool repeat_=false,pending_=false,error_=false;
    TouchTap tap_;MarketUiFeedback feedback_;mutable TextBlock dialogueText_;
    MarketDialogueUi::RoundGreeting greeting_;
    const char* dialogueText() const {
        if(error_)return MarketWorkshopText::RETRY;
        if(dialogue_==0)return MarketWorkshopText::WELCOME;
        return dialogue_==1?MarketWorkshopText::GIFT:MarketWorkshopText::OPENING;
    }
    void startDialogue(){if(layout_.round)greeting_.start(dialogueText());}
    void beginExit(Exit exit){exit_=exit;exitMs_=0;tap_.cancel();}
    void showReward(){exit_=Exit::NONE;page_=Page::REWARD;feedback_.set(SfxCue::ITEM_GET);}
    int exitOffset(int distance)const{
        return exiting()?MarketStallMotion::tween(0,distance,exitMs_,MOTION_MS):0;
    }
    static uint16_t color(int r,int g,int b){return PixelRenderer::rgb(uint8_t(r),uint8_t(g),uint8_t(b));}
};
}
