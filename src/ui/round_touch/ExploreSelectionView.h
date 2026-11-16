#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include "assets/ExplorePostcardArt.h"
#include "core/UiStrings.h"
#include "presentation/PixelRenderer.h"
#include "ui/round_touch/ExploreSelectUi.h"
#include "ui/round_touch/RoundPage.h"

namespace Ui::RoundExplore::SelectionView {
inline uint16_t rgb(int r,int g,int b){return PixelRenderer::rgb(r,g,b);}
inline constexpr auto ball=[](){
    std::array<uint8_t,32*32> mask{};
    for(int y=0;y<32;++y)for(int x=0;x<32;++x){int coverage=0;
        for(int sy=0;sy<4;++sy)for(int sx=0;sx<4;++sx){const double px=x+(sx+.5)/4-16,py=y+(sy+.5)/4-16,r=px*px+py*py;
            if((r>=169&&r<=225)||(r>=16&&r<=36)||(py>=-1.25&&py<=1.25&&r>=36&&r<=169))++coverage;}
        mask[y*32+x]=uint8_t((coverage*255+8)/16);
    }return mask;
}();
inline void drawBall(bool shown,bool enabled,int pressed){
    auto& c=PixelRenderer::canvas();const auto color=shown?rgb(168,89,56):enabled?rgb(255,244,224):rgb(182,160,128);
    const int x=252,y=100;
    c.blendRoundRectAlphaNative(x-2,y-2,36,36,12,shown?rgb(234,215,186):rgb(48,32,22),pressed==TOGGLE_PREVIEW?240:184);
    for(int row=0;row<32;++row)for(int col=0;col<32;){const auto alpha=ball[row*32+col];int end=col+1;while(end<32&&ball[row*32+end]==alpha)++end;
        if(alpha)c.blendRectAlphaNative(x+col,y+row,end-col,1,color,alpha);
        col=end;}
}
inline void arrow(MenuGrid::Rect r,bool next,bool enabled,bool pressed){
    auto& c=PixelRenderer::canvas();const int x=r.x+r.width/2,y=r.y+r.height/2;
    c.fillCircle(x,y,18,pressed?rgb(244,230,207):rgb(234,215,186));const auto color=enabled?rgb(89,67,46):rgb(182,160,128);const int d=next?1:-1;
    for(int offset=-1;offset<=1;++offset){c.drawLine(x-d*3+offset,y-7,x+d*4+offset,y,color);c.drawLine(x+d*4+offset,y,x-d*3+offset,y+7,color);}
}
inline void centered(int y,const char* value,int size,uint16_t color){int w=0;for(const auto* p=reinterpret_cast<const uint8_t*>(value);*p;){int n=*p<128?1:*p<224?2:*p<240?3:4;w+=n==1?size/2:size;p+=n;}PixelRenderer::textSmooth((360-w)/2,y,value,color,uint8_t(size));}
inline void render(const ExploreRouteFrame& f,int pressed,bool shown,uint8_t selectedPage){
    auto& c=PixelRenderer::canvas();const int count=std::min<int>(f.visibleAreaCount,ExploreRouteFrame::AREA_CAP);const bool valid=f.areaCursor<count;
    const bool unlocked=valid&&f.areaUnlocked[f.areaCursor];const int id=f.marketPreview?6:valid&&f.areaIds[f.areaCursor]<6?f.areaIds[f.areaCursor]:0;
    c.fillRect(0,0,360,360,rgb(247,236,217));const auto card=CARD_BOUNDS;constexpr int radius=14;
    c.fillRoundRect(card.x-1,card.y-1,card.width+2,card.height+2,radius+1,rgb(213,186,147));
    uint16_t row[160];int last=-1;const auto& art=ExplorePostcardArt::scenes[id];
    for(int dy=std::max(0,c.clipTop()-card.y);dy<std::min(card.height,c.clipBottom()-card.y);++dy){
        const int sy=dy*122/card.height;if(sy!=last){int pos=0;for(int j=art.rows[sy];j<art.rows[sy+1];j+=2)for(int n=0;n<art.runs[j];++n)row[pos++]=art.palette[art.runs[j+1]];last=sy;}
        const int edge=std::min(dy,card.height-1-dy),inset=edge<radius?radius-int(std::sqrt(float(radius*radius-(radius-edge)*(radius-edge)))):0;
        const int left=std::max(card.x+inset,c.clipLeft()),right=std::min(card.x+card.width-inset,c.clipRight());
        for(int x=left;x<right;++x)c.drawPixel(x,card.y+dy,row[(x-card.x)*160/card.width]);
        int alpha=dy<78?135-dy*115/78:20+(dy-78)*23/(card.height-78);
        if(shown&&dy>30)alpha=std::max(alpha,(dy-30)*190/(card.height-30));
        c.blendRectAlphaNative(left,card.y+dy,std::max(0,right-left),1,rgb(34,21,10),uint8_t(alpha));
    }
    const char* name=valid&&f.areaNames[f.areaCursor]?f.areaNames[f.areaCursor]:"";
    PixelRenderer::textSmooth(80,104,name,rgb(255,244,224),20);drawBall(shown,canPreview(f),pressed);
    if(f.marketPreview&&!f.marketOpen&&f.marketDaysUntilOpen){char value[48];std::snprintf(value,sizeof(value),Ui::MarketText::SELECTOR_NEXT,static_cast<unsigned long>(f.marketDaysUntilOpen));PixelRenderer::textSmooth(80,130,value,rgb(255,241,217),16);}
    const auto pages=previewPages(f);const int page=pages.count?std::min<int>(selectedPage,pages.count-1):0;
    if(shown){
        if(!f.areaPreviewReady)centered(218,"...",16,rgb(255,247,235));
        else if(pages.count){
            const int begin=pages.starts[page],end=pages.ends[page];int width=0;for(int i=begin;i<end;++i)width+=PreviewLayout::width(f.areaPreviewFrames[i])+(i>begin?PreviewLayout::SPACING:0);
            int x=(360-width)/2;const int l=c.clipLeft(),t=c.clipTop(),r=c.clipRight(),b=c.clipBottom();const int cx=std::max(l,PET_BOUNDS.x),cy=std::max(t,PET_BOUNDS.y);
            c.setClipRect(cx,cy,std::max(0,std::min(r,PET_BOUNDS.x+PET_BOUNDS.width)-cx),std::max(0,std::min(b,PET_BOUNDS.y+PET_BOUNDS.height)-cy));
            for(int i=begin;i<end;++i){const auto* sprite=f.areaPreviewFrames[i];const int w=PreviewLayout::width(sprite);
                if(sprite){if(f.areaPreviewHidden[i])PokemonSprites::drawFrameSilhouette(sprite,x,PET_BOUNDS.y+PET_BOUNDS.height-sprite->height,0);else PokemonSprites::drawFrame(sprite,x,PET_BOUNDS.y+PET_BOUNDS.height-sprite->height);}
                x+=w+PreviewLayout::SPACING;
            }c.setClipRect(l,t,r-l,b-t);
        }
    }else if(valid&&!unlocked&&!f.marketPreview){centered(210,Ui::Explore::AREA_LOCKED,16,rgb(255,247,235));centered(234,Ui::Explore::DEFEAT_PREVIOUS_BOSS,16,rgb(255,247,235));}
    if(!shown){arrow(PREVIOUS_BOUNDS,false,valid&&f.areaCursor>0,pressed==PREVIOUS);arrow(NEXT_BOUNDS,true,valid&&f.areaCursor+1<count,pressed==NEXT);}
    const int dots=shown?pages.count:count,selected=shown?page:f.areaCursor;
    for(int i=0;i<dots;++i)c.fillCircle(180+(2*i-dots+1)*7,286,i==selected?3:2,i==selected?rgb(168,89,56):rgb(213,186,147));
    const bool enabled=unlocked&&(!f.marketPreview||f.marketOpen);const auto button=DEPART_BOUNDS;
    c.fillRoundRect(button.x,button.y,button.width,button.height,12,enabled?(pressed==DEPART?rgb(128,83,57):rgb(168,89,56)):rgb(227,212,191));
    centered(button.y+10,Ui::Explore::DEPART,20,enabled?rgb(255,247,235):rgb(154,134,110));
    if(f.toast){c.fillRoundRect(60,224,240,34,7,rgb(255,246,231));centered(232,f.toast,16,rgb(89,67,46));}
    RoundPage::nativeHeader(Ui::EXPLORE,pressed==BACK,{rgb(247,236,217),rgb(89,67,46),pressed==BACK?rgb(244,230,207):rgb(234,215,186),rgb(213,186,147)});
}
}
