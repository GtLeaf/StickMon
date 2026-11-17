#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "presentation/TextBlock.h"
#include "ui/common/MarketCoinHud.h"
#include "ui/common/MarketView.h"

// Native round-screen presentation only. No transactions or scene-sized buffers.
namespace Ui::RoundMarketStall {
struct Rect {
    int x,y,w,h;
    bool contains(int px,int py) const { return px>=x&&py>=y&&px<x+w&&py<y+h; }
};
inline constexpr Rect PANEL{48,132,264,236},DIALOGUE{0,220,360,180},ACTION{96,282,168,44};
inline uint16_t paper(){return PixelRenderer::rgb(248,248,224);}
inline uint16_t ink(){return PixelRenderer::rgb(40,56,80);}
inline uint16_t edge(){return PixelRenderer::rgb(48,64,112);}
inline uint16_t muted(){return PixelRenderer::rgb(89,105,88);}
inline uint16_t line(){return PixelRenderer::rgb(186,196,160);}
inline uint16_t off(){return PixelRenderer::rgb(225,227,207);}

inline int width(const char* value,int size=32) {
    int result=0;const char* p=value?value:"";const char* end=p+std::strlen(p);
    while(p<end)result+=BitmapText::advance(BitmapText::decode(p,end),BitmapText::Font::PX32)*size/32;
    return result;
}

// Sample one native glyph at a time: 2KiB bounded scratch, regardless of text length.
// Bounds are measured in the same pass used to paint, including punctuation baselines.
inline void sampledText(Canvas565* destination,int x,int y,const char* value,int length,int size,
                        uint16_t color,int* first=nullptr,int* last=nullptr) {
    uint16_t pixels[32*32]{};Canvas565 glyph;glyph.attach({pixels,32,32,false});
    const char* p=value;const char* end=p+length;
    while(p<end){
        const char* begin=p;const auto cp=BitmapText::decode(p,end);char encoded[5]{};
        const int n=int(p-begin);std::memcpy(encoded,begin,n);
        glyph.fillSprite(0);PixelRenderer::bitmapText(glyph,0,0,encoded,0xffff,BitmapText::Font::PX32);
        const int w=BitmapText::advance(cp,BitmapText::Font::PX32)*size/32;
        for(int row=0;row<size;++row)for(int col=0;col<w;++col){
            if(!glyph.readPixel(col*32/size,row*32/size))continue;
            if(first)*first=std::min(*first,y+row);
            if(last)*last=std::max(*last,y+row);
            if(destination)destination->drawPixel(x+col,y+row,color);
        }
        x+=w;
    }
}
inline void text(int x,int y,const char* value,int size=32,uint16_t color=0) {
    if(!value||!*value)return;
    if(size==16||size==32)PixelRenderer::bitmapText(PixelRenderer::canvas(),x,y,value,color?color:ink(),
        size==16?BitmapText::Font::PX16:BitmapText::Font::PX32);
    else sampledText(&PixelRenderer::canvas(),x,y,value,int(std::strlen(value)),size,color?color:ink());
}
inline void centered(int y,const char* value,int size=32,uint16_t color=0){text((360-width(value,size))/2,y,value,size,color);}
inline void right(int x,int y,const char* value,int size=32,uint16_t color=0){text(x-width(value,size),y,value,size,color);}

inline int fit(TextBlock& block,const char* value,int w,int h,bool centered=true,int maxSize=32) {
    block.setText(value);TextBlock::Options o;o.font=BitmapText::Font::PX32;o.lineGap=2;
    o.align=centered?TextBlock::Align::CENTER:TextBlock::Align::LEFT;
    int size=16;
    for(int candidate:{32,28,24,20,16}){
        if(candidate>maxSize)continue;
        o.width=w*32/candidate;block.setOptions(o);
        if(block.measure().height*candidate<=h*32){size=candidate;break;}
    }
    o.width=w*32/size;block.setOptions(o);return size;
}
inline int body(TextBlock& block,const char* value,Rect r,bool centered=true,uint16_t color=0,
                bool vertical=false,int fitHeight=0,int maxSize=32,int scroll=0) {
    const int size=fit(block,value,r.w,fitHeight?fitHeight:r.h,centered,maxSize);
    const auto& m=block.measure();const auto& o=block.options();
    int y=r.y;
    if(vertical&&m.height*size/32<=r.h){
        int first=32767,last=-1;
        for(int i=0;i<m.lineCount;++i){const auto& l=block.line(i);
            sampledText(nullptr,0,i*34*size/32,block.text()+l.begin,l.end-l.begin,size,0,&first,&last);}
        if(last>=first)y+=(r.h-(last-first+1))/2-first;
    }
    auto& c=PixelRenderer::canvas();const int left=c.clipLeft(),top=c.clipTop(),right=c.clipRight(),bottom=c.clipBottom();
    const int x=std::max(left,r.x),cy=std::max(top,r.y);
    c.setClipRect(x,cy,std::max(0,std::min(right,r.x+r.w)-x),std::max(0,std::min(bottom,r.y+r.h)-cy));
    for(int i=0;i<m.lineCount;++i){const auto& l=block.line(i);const int ty=y+i*34*size/32-scroll;
        if(ty+size<=cy||ty>=std::min(bottom,r.y+r.h))continue;
        const int tx=r.x+(centered?(o.width-l.width)*size/64:0);
        sampledText(&c,tx,ty,block.text()+l.begin,l.end-l.begin,size,color?color:ink());}
    c.setClipRect(left,top,right-left,bottom-top);
    return m.height*size/32;
}
inline void panel(int offset=0,bool dialogue=false) {
    const auto r=dialogue?DIALOGUE:PANEL;auto& c=PixelRenderer::canvas();
    c.fillRoundRect(r.x,r.y+offset,r.w,r.h,dialogue?16:14,paper());
    c.drawRoundRect(r.x,r.y+offset,r.w,r.h,dialogue?16:14,edge());
}
inline void button(Rect r,const char* label,bool primary,bool disabled=false,int offset=0) {
    auto& c=PixelRenderer::canvas();r.y+=offset;
    const auto border=disabled?line():edge();
    c.fillRoundRect(r.x,r.y,r.w,r.h,10,disabled?off():primary?edge():paper());
    c.drawRoundRect(r.x,r.y,r.w,r.h,10,border);c.drawRoundRect(r.x+1,r.y+1,r.w-2,r.h-2,9,border);
    const int size=width(label,32)<=r.w-12?32:width(label,24)<=r.w-12?24:20;
    text(r.x+(r.w-width(label,size))/2,r.y+(r.h-size)/2,label,size,disabled?muted():primary?paper():ink());
}
inline void dialogue(const char* value,int offset=0,int textY=228) {
    panel(offset,true);TextBlock block;const char* remaining=value;
    for(int row=0;row<3&&*remaining;++row){
        const int w=row<2?256:192;TextBlock::Options o;o.width=w;o.maxHeight=32;o.font=BitmapText::Font::PX32;o.align=TextBlock::Align::CENTER;
        block.setText(remaining);block.setOptions(o);const auto l=block.line(0);if(!l.end)break;
        block.draw(PixelRenderer::canvas(),(360-w)/2,textY+offset+row*34,ink(),TextBlock::DrawRange{0,1,-1});
        remaining+=l.end;while(*remaining=='\n'||*remaining==' ')++remaining;
    }
}
inline void hud(uint32_t balance,const MarketCoinMotion* motion,int offset=0) {
    MarketCoinMotion still;if(!motion)still.reset(balance);const auto f=motion?motion->frame():still.frame();
    const int right=180+int(std::lround((f.widthDigits+1)*8));
    MarketCoinHud::render(balance,motion,16+offset,32,{right,32,48,48});
}

// Frame the conversing player and vendor, excluding distant companions.
inline int sceneShift(const Market::Session& session,const MarketView::Viewport& v) {
    const int cx=v.cameraX(session),cy=v.cameraY(session);int top=32767,bottom=-32767;
    for(int role=0;role<2;++role){const int index=session.conversationActor(role);if(index<0)continue;
        MarketView::detail::ActorDraw actor;
        if(!MarketView::detail::prepareActor(session,v,session.timeMs(),cx,cy,session.pose(index),actor))continue;
        const auto bounds=PokemonSprites::frameVisibleBounds(actor.sprite);
        top=std::min(top,actor.y+int(bounds.top*actor.spriteScale));
        bottom=std::max(bottom,std::max(actor.y+int((bounds.top+bounds.height)*actor.spriteScale),actor.shadow.y+actor.shadow.radiusY));
    }
    return bottom<top?0:(top+bottom)/2-88;
}
inline int sceneShiftX(const Market::Session& session,const MarketView::Viewport& v) {
    const int player=session.conversationActor(0),vendor=session.conversationActor(1);
    if(player<0||vendor<0)return 0;
    return (v.pixel(session.pose(player).x)+v.pixel(session.pose(vendor).x))/2-180-v.cameraX(session);
}
} // namespace Ui::RoundMarketStall
