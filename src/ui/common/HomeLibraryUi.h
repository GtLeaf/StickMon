#pragma once
#include <algorithm>
#include "core/RoomRenderer.h"
#include "core/TouchTap.h"
#include "core/UiStrings.h"
#include "game/HomeFurnitureService.h"
#include "game/MarketWorkshopService.h"
#include "presentation/FurnitureArt.h"
#include "presentation/TextBlock.h"
#include "ui/common/MarketListScroll.h"
#include "ui/common/MarketUiFeedback.h"

namespace Ui {
class HomeLibraryUi {
public:
    struct Layout {int width=368,height=448;bool round=false;};
    enum class Page:uint8_t {CLOSED,LIBRARY,PREVIEW};
    struct Request {enum Kind:uint8_t {NONE,PLACE,REMOVE};Kind kind=NONE;uint16_t id=0;};
    HomeLibraryUi()=default;
    explicit HomeLibraryUi(Layout layout):layout_(layout){}
    bool active()const{return page_!=Page::CLOSED;}
    bool keepsDisplayAwake()const{return page_==Page::PREVIEW;}
    Page page()const{return page_;}
    int listOffset()const{return offset_;}
    bool moving()const{return scroll_.moving();}
    bool takeSfx(SfxCue& cue){return feedback_.takeSfx(cue);}
    void open(){close();page_=Page::LIBRARY;offset_=0;filter_=-1;error_=false;}
    void close(){page_=Page::CLOSED;pending_=false;cancelTouch();feedback_.clear();}
    void cancelTouch(){tap_.cancel();scroll_.cancel();}
    Request back(const Game::GameState&){
        if(pending_)return {};
        cancelTouch();error_=false;
        if(page_==Page::PREVIEW){
            if(draftPlaced_!=originalPlaced_){pending_=true;return {draftPlaced_?Request::PLACE:Request::REMOVE,selected_};}
            page_=Page::LIBRARY;
        }else close();
        return {};
    }
    bool update(uint32_t ms,const Game::GameState& s){return page_==Page::LIBRARY&&scroll_.update(ms,maximum(s),offset_);}
    Request touch(const Platform::IInputDevice::TouchEvent& e,const Game::GameState& s){
        if(!active()||pending_){cancelTouch();return {};}
        const int dx=e.x-layout_.width/2,dy=e.y-layout_.height/2;
        if(e.x<0||e.x>=layout_.width||e.y<0||e.y>=layout_.height||(layout_.round&&dx*dx+dy*dy>=180*180)){cancelTouch();return {};}
        if(page_==Page::LIBRARY&&scroll_.touch(e,listBounds(s),maximum(s),offset_,tap_))return {};
        const int activated=tap_.update(e,target(e.x,e.y,s));
        if(activated<0)return {};
        if(activated==BACK)return back(s);
        if(activated>=CATEGORY){filter_=activated-CATEGORY-1;offset_=0;cancelTouch();return {};}
        if(page_==Page::LIBRARY){
            if(const auto* selected=ownedAt(s,activated)){
                selected_=selected->id;originalPlaced_=Game::HomeFurniture::placed(s,selected_);draftPlaced_=originalPlaced_;
                page_=Page::PREVIEW;error_=false;cancelTouch();feedback_.set(SfxCue::UI_CONFIRM);
            }
            return {};
        }
        if(activated==CONFIRM&&Game::HomeFurniture::owned(s,selected_)){
            draftPlaced_=!draftPlaced_;error_=false;cancelTouch();feedback_.set(SfxCue::UI_CONFIRM);
        }
        return {};
    }
    void committed(bool saved){pending_=false;cancelTouch();error_=!saved;if(saved)page_=Page::LIBRARY;feedback_.set(saved?SfxCue::UI_CONFIRM:SfxCue::UI_CANCEL);}
    void render(const Game::GameState& s)const{
        if(!active())return;
        auto& c=PixelRenderer::canvas();c.fillRect(0,0,layout_.width,layout_.height,0);
        if(page_==Page::LIBRARY){
            if(categories(s)>1){
                const int count=1+categories(s),w=(layout_.width-2*listLeft())/count;
                int index=0;
                for(int slot=-1;slot<int(Game::HomeFurniture::Slot::COUNT);++slot){
                    if(slot>=0&&!hasCategory(s,slot))continue;
                    const int x=listLeft()+index++*w;
                    c.fillRoundRect(x,categoryTop(),w-4,44,8,filter_==slot?color(42,61,68):0);
                    label(slotLabel(slot),x,categoryTop()+10,w-4,24,color(226,238,233));
                }
            }
            const auto bounds=listBounds(s);
            const int oldLeft=c.clipLeft(),oldTop=c.clipTop(),oldRight=c.clipRight(),oldBottom=c.clipBottom();
            c.setClipRect(std::max(oldLeft,bounds.x),std::max(oldTop,bounds.y),std::max(0,std::min(oldRight,bounds.x+bounds.w)-std::max(oldLeft,bounds.x)),std::max(0,std::min(oldBottom,bounds.y+bounds.h)-std::max(oldTop,bounds.y)));
            int count=0;
            while(const auto* item=ownedAt(s,count)){
                const int y=bounds.y+count*80-offset_;++count;if(y+80<=bounds.y||y>=layout_.height)continue;
                FurnitureArt::drawFrame(c,bounds.x+38,y+38,48);
                label(name(item->id),bounds.x+78,y+10,bounds.w-84,32,color(226,238,233),TextBlock::Align::LEFT);
                if(Game::HomeFurniture::placed(s,item->id))label(HomeLibraryText::PLACED,bounds.x+78,y+48,bounds.w-84,16,color(126,175,175),TextBlock::Align::LEFT);
                c.drawFastHLine(bounds.x+8,y+79,bounds.w-16,color(45,61,67));
            }
            c.setClipRect(oldLeft,oldTop,oldRight-oldLeft,oldBottom-oldTop);
            if(!count)label(HomeLibraryText::EMPTY,listLeft(),layout_.height/2-16,layout_.width-2*listLeft(),32,color(168,184,184));
            const int max=maximum(s);if(max>0){const int h=layout_.round?170:layout_.height-bounds.y-24;const int thumb=std::max(24,h*bounds.h/(max+bounds.h));const int x=layout_.width-listLeft()-3;c.fillRoundRect(x,bounds.y+8,3,h,1,color(32,45,50));c.fillRoundRect(x,bounds.y+8+(h-thumb)*offset_/max,3,thumb,1,color(126,175,175));}
        }else{
            const int top=layout_.round?60:80,height=layout_.round?184:224;
            const bool shown=RoomRenderer::drawPreview(layout_.round?40:16,top,layout_.round?280:336,height,false,Game::HomeFurniture::previewMask(s,selected_,draftPlaced_), nullptr, Game::MarketWorkshop::equippedWallpaper(s));
            if(!shown)label(HomeLibraryText::NO_PREVIEW,48,top+64,layout_.width-96,24,color(168,184,184));
            if(error_)label(HomeLibraryText::FAILED,48,layout_.round?272:350,layout_.width-96,16,color(238,120,105));
            const auto button=actionBounds();const bool enabled=Game::HomeFurniture::owned(s,selected_);
            c.fillRoundRect(button.x,button.y,button.w,button.h,10,enabled?color(42,73,70):color(24,34,42));
            label(draftPlaced_?HomeLibraryText::REMOVE:HomeLibraryText::PLACE,button.x,button.y+6,button.w,32,color(226,238,233));
        }
        header();
    }
private:
    static constexpr int BACK=1000,CONFIRM=1001,CATEGORY=1100;
    Layout layout_{};Page page_=Page::CLOSED;uint16_t selected_=0;int offset_=0,filter_=-1;bool error_=false,pending_=false;
    bool originalPlaced_=false,draftPlaced_=false;
    TouchTap tap_;MarketListScroll scroll_;MarketUiFeedback feedback_;mutable TextBlock text_;
    static uint16_t color(int r,int g,int b){return PixelRenderer::rgb(uint8_t(r),uint8_t(g),uint8_t(b));}
    static const char* name(uint16_t id){const auto* e=Game::HomeFurniture::entry(id);return e?e->name:"";}
    static const char* slotLabel(int slot){using S=Game::HomeFurniture::Slot;return slot<0?HomeLibraryText::ALL:slot==int(S::BED)?HomeLibraryText::BED:slot==int(S::RUG)?HomeLibraryText::RUG:HomeLibraryText::WALL;}
    static bool hasCategory(const Game::GameState& s,int slot){for(const auto& e:Game::HomeFurniture::CATALOG)if(int(e.slot)==slot&&Game::HomeFurniture::owned(s,e.id))return true;return false;}
    static int categories(const Game::GameState& s){int count=0;for(int slot=0;slot<int(Game::HomeFurniture::Slot::COUNT);++slot)count+=hasCategory(s,slot);return count;}
    const Game::HomeFurniture::Entry* ownedAt(const Game::GameState& s,int index)const{for(const auto& e:Game::HomeFurniture::CATALOG)if(Game::HomeFurniture::owned(s,e.id)&&(filter_<0||filter_==int(e.slot))&&index--==0)return &e;return nullptr;}
    int listLeft()const{return layout_.round?48:16;}
    int categoryTop()const{return layout_.round?60:84;}
    MarketListScroll::Bounds listBounds(const Game::GameState& s)const{const int top=categories(s)>1?categoryTop()+52:layout_.round?84:92;return {listLeft(),top,layout_.width-2*listLeft(),layout_.height-top};}
    int maximum(const Game::GameState& s)const{int count=0;while(ownedAt(s,count))++count;return std::max(0,count*80+(layout_.round?80:16)-listBounds(s).h);}
    MarketListScroll::Bounds actionBounds()const{return layout_.round?MarketListScroll::Bounds{104,300,152,44}:MarketListScroll::Bounds{64,390,240,48};}
    MarketListScroll::Bounds backBounds()const{return layout_.round?MarketListScroll::Bounds{136,10,44,44}:MarketListScroll::Bounds{8,8,72,64};}
    int target(int x,int y,const Game::GameState& s)const{
        if(backBounds().contains(x,y))return BACK;
        if(page_==Page::PREVIEW)return actionBounds().contains(x,y)?CONFIRM:-1;
        if(categories(s)>1&&y>=categoryTop()&&y<categoryTop()+44&&x>=listLeft()&&x<layout_.width-listLeft()){
            const int index=(x-listLeft())/((layout_.width-2*listLeft())/(categories(s)+1));int at=0;
            for(int slot=-1;slot<int(Game::HomeFurniture::Slot::COUNT);++slot){
                if(slot<0||hasCategory(s,slot)){if(at++==index)return CATEGORY+1+slot;}
            }
            return -1;
        }
        const auto bounds=listBounds(s);if(!bounds.contains(x,y))return -1;
        const int index=(y-bounds.y+offset_)/80;return ownedAt(s,index)?index:-1;
    }
    void label(const char* value,int x,int y,int width,int size,uint16_t ink,TextBlock::Align align=TextBlock::Align::CENTER)const{
        TextBlock::Options o;o.width=width;o.maxHeight=size;o.font=size==32?BitmapText::Font::PX32:BitmapText::Font::PX16;o.align=align;text_.setText(value);text_.setOptions(o);text_.draw(PixelRenderer::canvas(),x,y,ink,TextBlock::DrawRange{0,1,-1});
    }
    void header()const{
        auto& c=PixelRenderer::canvas();const bool round=layout_.round;const int cx=round?158:42,cy=round?32:40;
        c.fillRect(0,0,layout_.width,round?52:80,0);c.fillCircle(cx,cy,round?16:24,color(27,43,51));
        for(int n=-1;n<=1;++n){c.drawLine(cx+2+n,cy-7,cx-5+n,cy,color(226,238,233));c.drawLine(cx-5+n,cy,cx+2+n,cy+7,color(226,238,233));}
        const char* title=page_==Page::LIBRARY?HomeLibraryText::TITLE:HomeLibraryText::PREVIEW;
        if(round)PixelRenderer::textSmooth(182,22,title,color(226,238,233),20);else label(title,84,24,layout_.width-168,32,color(226,238,233));
    }
};
}
