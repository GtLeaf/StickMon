#pragma once
#include <cstdio>
#include <cstring>
#include "ui/common/MarketCoinHud.h"

// Shared numeric capacity change; the font has no right-arrow glyph.
namespace Ui::MarketExpansionUi {
inline constexpr const char* SUCCESS="扩容成功";
inline constexpr const char* LEAVE="离开";
inline void capacity(uint8_t before,uint8_t after,int cx,int y,int height,uint16_t ink,bool success=false) {
    auto& c=PixelRenderer::canvas();char left[4],right[4];
    std::snprintf(left,sizeof(left),"%u",before);std::snprintf(right,sizeof(right),"%u",after);
    const int leftWidth=int(std::strlen(left))*height/2,rightWidth=int(std::strlen(right))*height/2;
    int x=cx-(leftWidth+rightWidth+64)/2;
    for(const char* p=left;*p;++p,x+=height/2)MarketCoinHud::detail::glyph(c,*p,x,y,height,ink);
    x+=16;const int cy=y+height/2;
    c.fillRect(x,cy-1,28,3,ink);
    for(int i=0;i<7;++i){c.fillRect(x+22+i,cy-7+i,3,3,ink);c.fillRect(x+22+i,cy+5-i,3,3,ink);}
    x+=48;
    const auto color=success?PixelRenderer::rgb(45,125,99):ink;
    for(const char* p=right;*p;++p,x+=height/2)MarketCoinHud::detail::glyph(c,*p,x,y,height,color);
}
}
