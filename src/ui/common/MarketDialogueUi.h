#pragma once

#include "core/MarketDialogue.h"
#include "ui/common/MarketView.h"

namespace Ui::MarketDialogueUi {
inline constexpr int ARROW_SCALE=2;
inline constexpr int ARROW_WIDTH=9*ARROW_SCALE,ARROW_HEIGHT=7*ARROW_SCALE;
inline constexpr int ARROW_BOB=2*ARROW_SCALE;
struct Layout {
    int x,y,w,h,textX,textY,textWidth,glyph,lineHeight,arrowX,arrowY;
    bool contains(int px,int py) const {return px>=0&&px<w-8&&py>=y&&py<y+h-8;}
};
inline Layout layout(const MarketView::Viewport& v) {
    if(v.round)return {-4,v.height-100,v.width+8,108,76,v.height-86,208,16,24,v.width/2+58,v.height-40};
    return {-4,v.height-120,v.width+8,128,24,v.height-106,v.width-48,32,40,v.width-34,v.height-22};
}
inline MarketView::Viewport viewport(const Market::Session& s,MarketView::Viewport v,const MarketDialogue& dialogue) {
    if(!dialogue.active()||!s.conversationReady())return v;
    const auto box=layout(v);const int camera=v.cameraY(s);int shift=0;
    for(int role=0;role<4;++role) {
        const int i=s.conversationActor(role);if(i<0)continue;
        shift=std::max(shift,v.pixel(s.pose(i).y)-camera+20-box.y);
    }
    v.cameraOffsetY=std::min(shift,120)*dialogue.openingMs()/150;
    return v;
}
inline void render(const MarketDialogue& dialogue,const MarketView::Viewport& v) {
    if(!dialogue.visible())return;
    auto& c=PixelRenderer::canvas();const auto b=layout(v);
    const auto paper=PixelRenderer::rgb(248,248,224),ink=PixelRenderer::rgb(40,56,80);
    const auto edge=PixelRenderer::rgb(48,64,112),light=PixelRenderer::rgb(128,152,184);
    c.fillRect(b.x,b.y,b.w,b.h,paper);
    for(int inset=0;inset<2;++inset)c.drawRect(b.x+inset,b.y+inset,b.w-2*inset,b.h-2*inset,edge);
    c.drawRect(b.x+3,b.y+3,b.w-6,b.h-6,light);c.drawRect(b.x+5,b.y+5,b.w-10,b.h-10,edge);
    char text[256];dialogue.visibleText(text,sizeof(text));
    const bool native=c.nativeText();c.setNativeText(b.glyph==32);
    char* second=std::strchr(text,'\n');if(second)*second++=0;
    PixelRenderer::text(b.textX,b.textY,text,ink);
    if(second)PixelRenderer::text(b.textX,b.textY+b.lineHeight,second,ink);
    c.setNativeText(native);
    if(!dialogue.complete())return;
    static constexpr uint16_t outline[7]={0x0fe,0x101,0x101,0x082,0x044,0x028,0x010};
    static constexpr uint16_t fill[7]={0,0x0fe,0x0fe,0x07c,0x038,0x010,0};
    static constexpr int bob[4]={0,1,2,1};const int ay=b.arrowY+ARROW_SCALE*bob[dialogue.visualMs()/160%4];
    for(int y=0;y<7;++y)for(int x=0;x<9;++x) {
        const auto bit=1U<<(8-x);
        if(outline[y]&bit)c.fillRect(b.arrowX+x*ARROW_SCALE,ay+y*ARROW_SCALE,ARROW_SCALE,ARROW_SCALE,ink);
        else if(fill[y]&bit)c.fillRect(b.arrowX+x*ARROW_SCALE,ay+y*ARROW_SCALE,ARROW_SCALE,ARROW_SCALE,PixelRenderer::rgb(216,56,56));
    }
}
} // namespace Ui::MarketDialogueUi
