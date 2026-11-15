#pragma once
#include "presentation/TextLayout.h"
#include "presentation/PixelRenderer.h"

namespace Ui {
// TextLayout plus Canvas drawing. A reveal/window never changes cached breaks.
class TextBlock : public TextLayout {
public:
    struct DrawRange {
        int firstLine=0,lineCount=-1,visibleCharacters=-1;
    };

    void draw(Canvas565& canvas,int x,int y,uint16_t color,int scrollY=0) const {
        drawLayout(canvas,*this,x,y,color,DrawRange{},scrollY);
    }
    void draw(Canvas565& canvas,int x,int y,uint16_t color,DrawRange range,int scrollY=0) const {
        drawLayout(canvas,*this,x,y,color,range,scrollY);
    }
    static void drawLayout(Canvas565& canvas,const TextLayout& layout,int x,int y,
                           uint16_t color,DrawRange range,int scrollY=0) {
        const auto& m=layout.measure();const auto& o=layout.options();
        if(!m.lineCount||(!m.visibleLines&&o.overflow==Overflow::ELLIPSIS))return;
        int available=std::min(m.lineCount,LINE_CAPACITY);
        if(o.maxLines)available=std::min(available,int(o.maxLines));
        if(o.overflow==Overflow::ELLIPSIS)available=m.visibleLines;
        const int first=std::clamp(range.firstLine,0,available);
        const int count=range.lineCount<0?available-first:std::min(range.lineCount,available-first);
        if(!count||range.visibleCharacters==0)return;
        const int stride=BitmapText::height(o.font)+o.lineGap;
        const int h=o.maxHeight?o.maxHeight:count*stride-o.lineGap;
        const int left=canvas.clipLeft(),top=canvas.clipTop(),right=canvas.clipRight(),bottom=canvas.clipBottom();
        const int cx=std::max(left,x),cy=std::max(top,y);
        canvas.setClipRect(cx,cy,std::max(0,std::min(right,x+o.width)-cx),
                          std::max(0,std::min(bottom,y+h)-cy));
        int remaining=range.visibleCharacters;
        for(int row=0;row<count;++row) {
            const auto& l=layout.line(first+row);
            int n=l.end-l.begin,dots=l.dots;
            if(remaining>=0) {
                const char* p=layout.text()+l.begin;const char* end=layout.text()+l.end;
                while(p<end&&remaining){BitmapText::decode(p,end);--remaining;}
                n=int(p-(layout.text()+l.begin));
                dots=n==l.end-l.begin?std::min(dots,remaining):0;
                remaining-=dots;
            }
            const int ty=y+row*stride-std::max(0,scrollY);
            // Consume the reveal budget before testing dirty clipping, so
            // reconstructing one row cannot reveal later text prematurely.
            if(ty+BitmapText::height(o.font)<=cy||ty>=std::min(bottom,y+h))continue;
            char buffer[TEXT_CAPACITY+4];
            std::memcpy(buffer,layout.text()+l.begin,n);
            for(int j=0;j<dots;++j)buffer[n+j]='.';
            buffer[n+dots]=0;
            const int offset=o.align==Align::CENTER?(o.width-l.width)/2:
                             o.align==Align::RIGHT?o.width-l.width:0;
            PixelRenderer::bitmapText(canvas,x+offset,ty,buffer,color,o.font);
        }
        canvas.setClipRect(left,top,right-left,bottom-top);
    }
};
}
