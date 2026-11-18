#pragma once

#include "core/MarketDialogue.h"
#include "core/TouchTap.h"
#include "presentation/TextBlock.h"
#include "ui/common/MarketView.h"

namespace Ui::MarketDialogueUi {
inline constexpr int ARROW_SCALE=2;
inline constexpr int ARROW_WIDTH=9*ARROW_SCALE,ARROW_HEIGHT=7*ARROW_SCALE;
inline constexpr int ARROW_BOB=2*ARROW_SCALE;
inline constexpr int ROUND_HEIGHT=100;
inline constexpr MarketView::Viewport ROUND_VIEW{360,360,1,true};
struct Layout {
    int x,y,w,h,textX,textY,textWidth,glyph,lineHeight,arrowX,arrowY;
    bool contains(int px,int py) const {return px>=0&&px<w-8&&py>=y&&py<y+h-8;}
};
inline constexpr Layout layout(const MarketView::Viewport& v) {
    if(v.round)return {-4,v.height-ROUND_HEIGHT,v.width+8,ROUND_HEIGHT+8,70,v.height-86,220,20,28,v.width/2+58,v.height-34};
    return {-4,v.height-120,v.width+8,128,24,v.height-106,v.width-48,32,40,v.width-34,v.height-22};
}
inline void render(const MarketDialogueText& dialogue,const MarketView::Viewport& v,uint32_t visualMs,int offset=0) {
    auto& c=PixelRenderer::canvas();auto b=layout(v);
    b.y+=offset;b.textY+=offset;b.arrowY+=offset;
    const auto paper=PixelRenderer::rgb(248,248,224),ink=PixelRenderer::rgb(40,56,80);
    const auto edge=PixelRenderer::rgb(48,64,112),light=PixelRenderer::rgb(128,152,184);
    c.fillRect(b.x,b.y,b.w,b.h,paper);
    for(int inset=0;inset<2;++inset)c.drawRect(b.x+inset,b.y+inset,b.w-2*inset,b.h-2*inset,edge);
    c.drawRect(b.x+3,b.y+3,b.w-6,b.h-6,light);c.drawRect(b.x+5,b.y+5,b.w-10,b.h-10,edge);
    TextBlock::DrawRange range;
    range.firstLine=dialogue.pageFirstLine();range.lineCount=dialogue.pageLineCount();
    range.visibleCharacters=int(dialogue.revealedCharacters());
    TextBlock::drawLayout(c,dialogue.textLayout(),b.textX,b.textY,ink,range);
    if(!dialogue.complete())return;
    static constexpr uint16_t outline[7]={0x0fe,0x101,0x101,0x082,0x044,0x028,0x010};
    static constexpr uint16_t fill[7]={0,0x0fe,0x0fe,0x07c,0x038,0x010,0};
    static constexpr int bob[4]={0,1,2,1};const int ay=b.arrowY+ARROW_SCALE*bob[visualMs/160%4];
    for(int y=0;y<7;++y)for(int x=0;x<9;++x) {
        const auto bit=1U<<(8-x);
        if(outline[y]&bit)c.fillRect(b.arrowX+x*ARROW_SCALE,ay+y*ARROW_SCALE,ARROW_SCALE,ARROW_SCALE,ink);
        else if(fill[y]&bit)c.fillRect(b.arrowX+x*ARROW_SCALE,ay+y*ARROW_SCALE,ARROW_SCALE,ARROW_SCALE,PixelRenderer::rgb(216,56,56));
    }
}
inline void render(const MarketDialogue& dialogue,const MarketView::Viewport& v) {
    if(dialogue.visible())render(dialogue.text(),v,dialogue.visualMs());
}

class RoundGreeting {
public:
    void start(const char* text,const char* speaker="") {
        text_.start(speaker,text,layout(ROUND_VIEW).textWidth,BitmapText::Font::PX20);
    }
    bool update(uint32_t deltaMs) {return text_.update(deltaMs);}
    bool touch(const Platform::IInputDevice::TouchEvent& event,TouchTap& tap) {
        const int target=layout(ROUND_VIEW).contains(event.x,event.y)?0:-1;
        return tap.update(event,target)==0&&text_.advance();
    }
    void render(int offset=0) const {MarketDialogueUi::render(text_,ROUND_VIEW,text_.visualMs(),offset);}
private:
    MarketDialogueText text_;
};
} // namespace Ui::MarketDialogueUi
