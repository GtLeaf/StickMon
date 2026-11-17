// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "presentation/TextBlock.h"
#include "presentation/MenuGrid.h"

namespace Ui::RoundItemDetail {
constexpr MenuGrid::Rect DESCRIPTION{68,200,224,86};
constexpr MenuGrid::Rect ACTION{88,292,84,40},BACK{188,292,84,40};
inline void title(const char* value,int x,int y,int width) {
    // Names share the native smooth face used by headers and controls.
    static TextLayout layout;
    TextLayout::Options o;o.width=width*4/5;o.font=BitmapText::Font::PX16;o.maxLines=2;
    layout.setOptions(o);layout.setText(value);
    const auto& measured=layout.measure();
    for(int i=0;i<std::min(measured.lineCount,2);++i) {
        const auto& line=layout.line(i);char text[TextLayout::TEXT_CAPACITY];
        const int n=line.end-line.begin;std::memcpy(text,layout.text()+line.begin,n);text[n]=0;
        PixelRenderer::textSmooth(x,y+22*i,text,PixelRenderer::rgb(226,238,233),20);
    }
}
inline void description(const char* value) {
    static TextBlock block;
    TextBlock::Options o;o.width=DESCRIPTION.width;o.maxHeight=DESCRIPTION.height;
    o.font=BitmapText::Font::PX16;o.lineGap=4;
    block.setOptions(o);block.setText(value);
    block.draw(PixelRenderer::canvas(),DESCRIPTION.x,DESCRIPTION.y,PixelRenderer::rgb(126,175,175));
}
inline void button(MenuGrid::Rect r,const char* label,bool pressed,bool cancel=false) {
    const auto fill=cancel?PixelRenderer::rgb(pressed?91:46,pressed?49:37,pressed?55:44):
        PixelRenderer::rgb(pressed?48:36,pressed?74:54,pressed?68:61);
    const auto color=cancel?PixelRenderer::rgb(239,143,148):PixelRenderer::rgb(115,226,183);
    PixelRenderer::canvas().fillRoundRect(r.x,r.y,r.width,r.height,6,fill);
    int width=0;const char* p=label;const char* end=p+std::strlen(p);
    while(p<end)width+=BitmapText::usesUnscii(BitmapText::decode(p,end))?10:20;
    PixelRenderer::textSmooth(r.x+(r.width-width)/2,r.y+(r.height-20)/2,label,color,20);
}
}
