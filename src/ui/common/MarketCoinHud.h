#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "core/FontResource.h"
#include "presentation/PixelRenderer.h"
#include "ui/common/MarketCoinMotion.h"

namespace Ui::MarketCoinHud {
struct Layout { int right=340, tagHeight=32, laneGap=48, tagOffset=48; bool centered=false; };
inline constexpr Layout ROUND{180,32,48,48,true};
inline constexpr Layout ROUND_CARE{288,24,32,36};
namespace detail {
    inline uint16_t rgb(int r,int g,int b) { return PixelRenderer::rgb(uint8_t(r),uint8_t(g),uint8_t(b)); }
    inline void pixel(Canvas565& c,int x,int y,uint16_t color,uint8_t alpha) {
        if(!alpha||x<c.clipLeft()||x>=c.clipRight()||y<c.clipTop()||y>=c.clipBottom())return;
        if(alpha==255){c.drawPixel(x,y,color);return;}
        const auto old=c.readPixel(x,y);const unsigned inv=255-alpha;
        const auto r=(((color>>11)&31)*alpha+((old>>11)&31)*inv+127)/255;
        const auto g=(((color>>5)&63)*alpha+((old>>5)&63)*inv+127)/255;
        const auto b=((color&31)*alpha+(old&31)*inv+127)/255;
        c.drawPixel(x,y,uint16_t((r<<11)|(g<<5)|b));
    }
    inline void glyph(Canvas565& c,char ch,int x,int y,int height,uint16_t color,uint8_t alpha=255,
                      bool outline=false,bool bold=false,bool ink=true) {
        const auto* bits=FontResource::ins().findLargeGlyphBitmap(uint8_t(ch),FontFace::UNSCII_ASCII);
        if(!bits)return;
        const int width=height/2;
        const auto raw=[&](int col,int row){
            if(col<0||col>=width||row<0||row>=height)return false;
            const int sy=row*32/height,sx=col*32/height;
            return (bits[sy*4+sx/8]&(128>>(sx%8)))!=0;
        };
        const auto on=[&](int col,int row){return raw(col,row)||(bold&&raw(col-1,row));};
        const int border=outline?1:0;
        for(int row=-border;row<height+border;++row)for(int col=-border;col<width+int(bold)+border;++col){
            if(on(col,row)){if(ink)pixel(c,x+col,y+row,color,alpha);}
            else if(outline&&(on(col-1,row)||on(col+1,row)||on(col,row-1)||on(col,row+1)))
                pixel(c,x+col,y+row,rgb(248,248,224),alpha);
        }
    }
    inline float smooth(float q) { q=std::clamp(q,0.0f,1.0f);return q*q*(3-2*q); }
}
struct Badge {
    int right, numberLeft, symbolX;
    int shift = 0;
    CoinLabel current, next;

    Badge(const MarketCoinMotion::Frame& frame, int height, const Layout& layout) {
        const int cell = height/2;
        float cells = frame.widthDigits;
        if (frame.exact) {
            current = CoinLabel(frame.balance, true);
            cells = current.cells();
        } else if (frame.compactRoll) {
            const auto step = static_cast<uint32_t>(frame.labelPosition);
            const float fraction = frame.labelPosition - step;
            current = CoinLabel::at(step);
            if (fraction > 0) next = CoinLabel::at(step + 1);
            else next = current;
            shift = static_cast<int>(std::lround(fraction * height));
            cells = current.cells() + (next.cells() - current.cells()) * fraction;
        }
        const int width = static_cast<int>(std::lround(cells * cell));
        right = layout.centered ? layout.right + (width + cell)/2 : layout.right;
        numberLeft = right - width;
        symbolX = numberLeft - cell;
    }
    bool contains(int x, int y, int top) const {
        return x >= symbolX-8 && x < right+8 && y >= top-4 && y < top+36;
    }
};

inline bool inspect(const TouchTap::Event& event, MarketCoinMotion* motion, int y=16,
                    int height=32, const Layout& layout={}) {
    if (!motion) return false;
    const Badge badge(motion->frame(), height, layout);
    return motion->inspect(event, badge.contains(event.x, event.y, y));
}

// Shared by all three stalls. A standalone renderer without a live model
// synchronizes immediately; rendering itself never advances or changes state.
inline void render(uint32_t balance,const MarketCoinMotion* live,int y,int height=32,const Layout& layout=Layout{}) {
    auto& c=PixelRenderer::canvas();MarketCoinMotion stationary;
    if(!live)stationary.reset(balance);
    const auto f=live?live->frame():stationary.frame();
    const Badge badge(f,height,layout);
    const int right=badge.right, numberLeft=badge.numberLeft;
    const int cell=height/2, symbolX=badge.symbolX, top=y+(32-height)/2;
    c.fillRoundRect(symbolX-8,y-4,right-symbolX+16,40,6,detail::rgb(248,248,224));
    detail::glyph(c,'C',symbolX,top,height,detail::rgb(40,56,80));
    int leading=0;
    for(int i=0;i<f.slots;++i){
        const int lower=int(std::floor(f.wheels[i]))%10;const float fraction=f.wheels[i]-std::floor(f.wheels[i]);
        if((lower!=0&&fraction<.9999f)||((lower+1)%10!=0&&fraction>.0001f))leading=i;
    }
    const int clipX=c.clipLeft(),clipY=c.clipTop(),clipRight=c.clipRight(),clipBottom=c.clipBottom();
    const int left=std::max(clipX,numberLeft),clipTop=std::max(clipY,top);
    c.setClipRect(left,clipTop,std::max(0,std::min(right,clipRight)-left),std::max(0,std::min(top+height,clipBottom)-clipTop));
    if (f.exact || f.compactRoll) {
        const auto drawLabel = [&](const CoinLabel& label, int row) {
            int x = right - label.cells()*cell;
            for (const char* ch=label.text; *ch; ++ch, x+=cell)
                detail::glyph(c,*ch,x,row,height,detail::rgb(40,56,80));
        };
        drawLabel(badge.current, top-badge.shift);
        if (badge.shift) drawLabel(badge.next, top+height-badge.shift);
    } else {
        for(int i=0;i<=leading;++i){
            const float position=f.wheels[i];const int lower=int(std::floor(position))%10;
            const int shift=int(std::lround((position-std::floor(position))*height));
            const int x=right-(i+1)*cell;
            if(i<leading||i==0||lower!=0)
                detail::glyph(c,char('0'+lower),x,top-shift,height,detail::rgb(40,56,80));
            if(shift>0&&(i<leading||i==0||(lower+1)%10!=0))
                detail::glyph(c,char('0'+(lower+1)%10),x,top-shift+height,height,detail::rgb(40,56,80));
        }
    }
    c.setClipRect(clipX,clipY,clipRight-clipX,clipBottom-clipY);
    const float visible=std::clamp(float(y-16+64)/64,0.0f,1.0f);
    for(int lane=0;lane<2;++lane){
        const auto& tag=f.tags[lane];if(!tag.visible)continue;
        const float alpha=detail::smooth(float(tag.elapsed)/120)*(1-detail::smooth((float(tag.elapsed)-650)/250))*visible;
        if(alpha<=0)continue;
        char value[24];std::snprintf(value,sizeof(value),"%+lld",static_cast<long long>(tag.delta));
        const int tagHeight=layout.tagHeight,tagCell=tagHeight/2,laneGap=layout.laneGap;
        const int width=int(std::strlen(value))*tagCell;
        int x=right-width;const int tagY=y+layout.tagOffset+lane*laneGap-int(std::lround(float(tag.elapsed)*12/900));
        const auto color=tag.delta>0?detail::rgb(65,132,83):detail::rgb(180,76,64);
        const auto opacity=uint8_t(std::lround(alpha*255));
        // Stroke the whole label before its ink so adjacent glyph outlines
        // cannot erase the added one-pixel weight of the previous digit.
        for(const char* p=value;*p;++p,x+=tagCell)detail::glyph(c,*p,x,tagY,tagHeight,color,opacity,true,true,false);
        x=right-width;
        for(const char* p=value;*p;++p,x+=tagCell)detail::glyph(c,*p,x,tagY,tagHeight,color,opacity,false,true);
    }
}
// Round market stalls keep the rolling balance centered on the 360 px display.
inline void renderRound(uint32_t balance,const MarketCoinMotion* motion,int offset=0) {
    render(balance,motion,16+offset,32,ROUND);
}
} // namespace Ui::MarketCoinHud
