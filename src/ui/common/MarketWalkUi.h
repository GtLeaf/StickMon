#pragma once

#include "core/TouchTap.h"
#include "ui/common/MarketView.h"
#include "ui/common/MarketDialogueUi.h"

namespace Ui {
// Shared floating controls for free-walk markets on touch displays.
class MarketWalkUi {
public:
    enum Action { NONE, CHAT };
    struct Rect {
        int x,y,w,h;
        bool contains(int px,int py) const {return px>=x&&py>=y&&px<x+w&&py<y+h;}
    };
    bool dragging=false;
    int direction=-1,chatTarget=-1,knobX=0,knobY=0;
    static MarketView::Viewport viewport() {
        auto v=MarketView::sceneViewport();v.freeWalk=true;return v;
    }
    Rect joystick() const {return {anchorX_-34,anchorY_-34,68,68};}
    static Rect chat(const MarketView::Viewport& v) {
        const int size=36*v.controlScale;
        return {v.width/2-size/2,v.round?286:v.height-13-size,size,size};
    }
    static Rect chatHit(const MarketView::Viewport& v) {
        const auto b=chat(v);const int pad=4*v.controlScale;
        return {b.x-pad,b.y-pad,b.w+2*pad,b.h+2*pad};
    }
    void cancel() {dragging=false;direction=-1;knobX=knobY=0;tap_.cancel();dialogueTap_.cancel();pressedNpc_=-1;dialogue_.suspendClock();}
    void reset() {cancel();chatTarget=-1;dialogue_.reset();dialogueEnabled_=false;serviceRequest_=-1;}
    void enableDialogue() {dialogueEnabled_=true;dialogue_.load();}
    void setServiceSpecies(uint16_t species) {for(auto& v:serviceSpecies_)if(!v||v==species){v=species;return;}}
    int takeServiceRequest() {const int r=serviceRequest_;serviceRequest_=-1;return r;}
    const MarketDialogue& dialogue() const {return dialogue_;}
    bool update(Market::Session& s,uint32_t now,bool active=true) {
        s.setDirection(direction);
        const bool changed=s.update(now,active);
        const bool dialogueChanged=dialogueEnabled_&&dialogue_.update(s,now,active&&!s.paused());
        refresh(s);return changed||dialogueChanged;
    }
    void refresh(const Market::Session& s) {
        if(s.exiting()||s.paused())cancel();
        chatTarget=dialogue_.active()||s.conversing()?-1:s.nearbyNpc(chatTarget,dialogueEnabled_);
    }
    void touchSession(Market::Session& s,const Platform::IInputDevice::TouchEvent& e,
                      const MarketView::Viewport& v,bool canWalk=true) {
        using Type=Platform::IInputDevice::TouchType;
        if(dialogueEnabled_&&(!canWalk||e.type==Type::CANCEL||!inside(v,e.x,e.y))){cancel();return;}
        if(dialogue_.active()) {
            const int target=dialogue_.visible()&&MarketDialogueUi::layout(v).contains(e.x,e.y)?0:-1;
            if(dialogueTap_.update(e,target)==0) {
                dialogue_.advance(s);cancel();refresh(s);
            }
            return;
        }
        const int pressed=pressedNpc_;
        const auto action=touch(e,v,canWalk);
        if(e.type==Type::DOWN)pressedNpc_=chatTarget>=0&&chatHit(v).contains(e.x,e.y)?chatTarget:-1;
        if(action==CHAT&&dialogueEnabled_&&pressed>=0) {
            // Stall keepers with a service turn to face the party and hand
            // over to the application's stall page instead of chatting.
            if(pressed<s.data().residentCount&&isService(s.actor(pressed).species)) {
                if(s.beginConversation(pressed)){serviceRequest_=pressed;cancel();refresh(s);}
            } else {
                const auto b=MarketDialogueUi::layout(v);
                if(dialogue_.begin(s,pressed,b.textWidth,b.glyph)){cancel();refresh(s);}
            }
        }
        if(e.type==Type::UP)pressedNpc_=-1;
        s.setDirection(direction);
    }
    Action touch(const Platform::IInputDevice::TouchEvent& e,const MarketView::Viewport& v,bool canWalk=true) {
        using Type=Platform::IInputDevice::TouchType;
        if(e.type==Type::CANCEL||!inside(v,e.x,e.y)){cancel();return NONE;}
        const int target=chatTarget>=0&&chatHit(v).contains(e.x,e.y)?0:-1;
        if(e.type==Type::DOWN) {
            cancel();
            // The initial press owns the gesture. Moving off a button never
            // starts walking, and walking across a button never activates it.
            if(target<0&&canWalk) {
                anchorX_=e.x;anchorY_=e.y;dragging=true;
            }
        }
        if(dragging) {
            if(e.type==Type::UP||!canWalk){cancel();return NONE;}
            int dx=e.x-anchorX_,dy=e.y-anchorY_;
            const float length=std::sqrt(float(dx*dx+dy*dy));
            knobX=int(std::lround(dx*(length>26?26/length:1)));
            knobY=int(std::lround(dy*(length>26?26/length:1)));
            if(length<8)direction=-1;
            else {
                // Eight equal sectors with five degrees of angular hysteresis.
                constexpr float sector=3.14159265f/4;
                const float angle=std::atan2(float(-dx),float(dy));
                const int closest=(int(std::lround(angle/sector))+8)%8;
                bool retain=false;
                if(direction>=0) {
                    const float current=Market::DIRECTION_SECTOR[direction]*sector;
                    const float difference=std::remainder(angle-current,8*sector);
                    retain=std::abs(difference)<=sector/2+3.14159265f/36;
                }
                if(!retain)direction=Market::DIRECTION_RING[closest];
            }
            return NONE;
        }
        const int clicked=tap_.update(e,target);
        return clicked==0?CHAT:NONE;
    }
    void render(const Market::Session& s,const MarketView::Viewport& v,uint32_t minutes,bool touchInput=device.touchInput) const {
        const auto worldView=MarketDialogueUi::viewport(s,v,dialogue_);
        MarketView::render(s,worldView,s.timeMs(),minutes,dialogue_.environmentMs());
        if(dialogue_.active()){MarketDialogueUi::render(dialogue_,v);return;}
        auto& c=PixelRenderer::canvas();
        if(touchInput&&dragging&&!s.exiting()&&!s.paused()) {
            const auto j=joystick();const int x=j.x+j.w/2,y=j.y+j.h/2;
            panel(j,34,0x08a3,110);
            c.drawCircle(x,y,34,0x9d34);c.drawCircle(x,y,33,0x632c);
            disc(x+knobX,y+knobY,13,dragging?240:210);
            c.drawCircle(x+knobX,y+knobY,14,0xdedb);
        }
        if(chatTarget>=0&&!s.exiting()) {
            const auto b=chat(v);panel(b,8*v.controlScale,0x1926,210);
            if(!GameAssets::drawCentered(GameAssets::Kind::MARKET_CHAT,b.x+b.w/2,b.y+b.h/2,float(v.controlScale)))
                PixelRenderer::text(b.x+(b.w-32)/2,b.y+(b.h-16)/2,Ui::MarketText::CHAT,0xffff);
        }
    }
private:
    TouchTap tap_;
    TouchTap dialogueTap_;
    MarketDialogue dialogue_;
    bool dialogueEnabled_=false;
    int pressedNpc_=-1;
    int serviceRequest_=-1;
    uint16_t serviceSpecies_[4]{};
    bool isService(uint16_t species) const {for(auto v:serviceSpecies_)if(v&&v==species)return true;return false;}
    int anchorX_=0,anchorY_=0;
    static bool inside(const MarketView::Viewport& v,int x,int y) {
        if(x<0||y<0||x>=v.width||y>=v.height)return false;
        const int dx=2*x-v.width+1,dy=2*y-v.height+1;
        return !v.round||dx*dx+dy*dy<v.width*v.width;
    }
    static void panel(Rect b,int radius,uint16_t color,uint8_t alpha) {
        auto& c=PixelRenderer::canvas();
        if(!c.blendRoundRectAlphaNative(b.x,b.y,b.w,b.h,radius,color,alpha))
            c.fillRoundRect(b.x,b.y,b.w,b.h,radius,color);
    }
    static void disc(int cx,int cy,int radius,int alpha) {
        auto& c=PixelRenderer::canvas();
        for(int y=-radius;y<=radius;++y)for(int x=-radius;x<=radius;++x)if(x*x+y*y<=radius*radius) {
            const uint16_t b=c.readPixel(cx+x,cy+y);const int a=255-alpha;
            const auto r=(((b>>11)&31)*a+24*alpha)/255;
            const auto g=(((b>>5)&63)*a+52*alpha)/255;
            const auto blue=((b&31)*a+27*alpha)/255;
            c.drawPixel(cx+x,cy+y,uint16_t((r<<11)|(g<<5)|blue));
        }
    }
};
} // namespace Ui
