#pragma once

#include <algorithm>
#include <cmath>
#include "assets/GameAssets.h"
#include "presentation/PixelRenderer.h"

// Presentation only: the caller has already saved the paid reward.
namespace Ui::MarketBoxPerformance {
inline constexpr uint32_t ARRIVE_MS = 600, OPEN_MS = 1250, REVEAL_MS = 900;
namespace detail {
    inline constexpr float PI = 3.14159265358979323846f;
    inline float p(uint32_t t,uint32_t a,uint32_t b) {
        return std::clamp((float(t)-float(a))/float(b-a),0.0f,1.0f);
    }
    inline float smooth(float q) { return q*q*(3-2*q); }
    inline int lerp(int a,int b,float q) { return int(std::lround(a+(b-a)*q)); }
    inline uint16_t color(int r,int g,int b) { return PixelRenderer::rgb(uint8_t(r),uint8_t(g),uint8_t(b)); }
    inline uint16_t fade(int r,int g,int b,float alpha) {
        return color(lerp(248,r,alpha),lerp(248,g,alpha),lerp(224,b,alpha));
    }
    inline void shadow(int y,float alpha=1) {
        PixelRenderer::canvas().fillEllipse(184,y,27,4,fade(186,196,160,alpha*0.45f));
    }
    inline void box(int x,int y,float size,float alpha=1) {
        if(alpha<=0)return;
        GameAssets::drawCenteredAlpha(GameAssets::Kind::MARKET_MYSTERY_BOX,x,y,size/48,
                                     uint8_t(std::lround(alpha*255)));
    }
    inline void star(int x,int y,int r,float alpha,bool purple) {
        if(alpha<=0)return;
        auto& c=PixelRenderer::canvas();
        const auto col=purple?fade(189,164,220,alpha):fade(230,183,90,alpha);
        c.fillRect(x-r,y-1,2*r+1,3,col);c.fillRect(x-1,y-r,3,2*r+1,col);
        c.fillRect(x-2,y-2,5,5,col);
    }
    inline void reward(Game::ItemId id,int y,float scale,float alpha) {
        if(alpha<=0)return;
        const auto original=GameAssets::itemKind(id);
        auto native=GameAssets::Kind::COUNT;
        if(original<=GameAssets::Kind::ITEM_STAR_PIECE)
            native=GameAssets::Kind(uint16_t(GameAssets::Kind::ITEM48_POKE_BALL)+uint16_t(original));
        else if(original==GameAssets::Kind::ITEM_HEART_SCALE)native=GameAssets::Kind::ITEM48_HEART_SCALE;
        const auto opacity=uint8_t(std::lround(alpha*255));
        if(native!=GameAssets::Kind::COUNT&&GameAssets::drawCenteredAlpha(native,184,y,scale,opacity))return;
        if(original!=GameAssets::Kind::COUNT)
            GameAssets::drawCenteredAlpha(original,184,y,scale*48/36,opacity);
    }
}
inline uint8_t detailFade(uint32_t ms) {
    return uint8_t(std::lround(detail::smooth(detail::p(ms,0,180))*255));
}
inline void arrival(uint32_t ms) {
    using namespace detail;
    const auto q=smooth(p(ms,180,ARRIVE_MS));
    const int x=lerp(44,184,q),y=lerp(240,316,q)-int(std::lround(std::sin(PI*q)*18));
    if(q>.9f)shadow(354,smooth(std::clamp((q-.9f)*10,0.0f,1.0f)));
    box(x,y,float(lerp(48,72,q)));
}
inline void waiting(uint32_t ms) {
    using namespace detail;
    const auto cycle=ms%1500;
    float dx=0,dy=0;
    if(cycle<420){const float q=float(cycle)/420;dx=std::sin(q*PI*4)*3*(1-q);dy=-std::abs(std::sin(q*PI*2))*1.5f;}
    shadow(354);box(184+int(std::lround(dx)),316+int(std::lround(dy)),72);
}
inline void opening(uint32_t ms,Game::ItemId id) {
    using namespace detail;
    if(ms<140){const float press=std::sin(p(ms,0,140)*PI);shadow(354);box(184,316+int(press*3),72-press*5);return;}
    const float q=smooth(p(ms,140,630));
    shadow(354,1-q);
    // Fade one existing sprite into a small pixel burst; no per-fragment decode/cache.
    box(184,316-int(q*18),72+q*5,1-smooth(p(ms,140,400)));
    const float burst=p(ms,200,1000);
    if(burst>0&&burst<1)for(int i=0;i<10;++i){
        const float angle=i*PI*2/10-.7f,radius=15+52*burst;
        const int x=184+int(std::lround(std::cos(angle)*radius));
        const int y=309+int(std::lround(std::sin(angle)*radius*.7f-burst*16));
        star(x,y,burst>.7f?2:3,(1-burst)*.9f,i%3==0);
    }
    if(ms>=350){
        const float show=smooth(p(ms,350,850));
        int y=lerp(308,264,show)-int(std::lround(std::sin(PI*show)*17));
        if(ms>850&&ms<1000)y-=int(std::lround(std::sin(p(ms,850,1000)*PI)*4));
        reward(id,y,(26+22*show)/48,smooth(p(ms,350,520)));
    }
}
} // namespace Ui::MarketBoxPerformance
