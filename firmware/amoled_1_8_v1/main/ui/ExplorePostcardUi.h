#pragma once
#include "ui/ExplorePostcardArt.h"
#include "ui/ExplorePostcardLayout.h"
#include "ui/ExploreScreens.h"
#include "ui/UiCommon.h"
#include "ui/common/MarketView.h"
#include "core/UiStrings.h"
#include "presentation/PixelRenderer.h"
#include <cstdio>

namespace AmoledV1 {
inline void renderExplorePostcard(Canvas565& c,const ExploreViewModel& m,
                                 uint16_t rowBegin,uint16_t rowEnd) {
    const auto rgb=PixelRenderer::rgb;
    const auto ink=rgb(226,238,233),accent=rgb(115,226,183),muted=rgb(126,145,145);
    auto text=[&](int x,int y,const char* s,int size,uint16_t color){
        const bool native=c.nativeText();c.setNativeText(size==32);
        PixelRenderer::text(c,x,y,s,color);c.setNativeText(native);
    };
    auto width=[](const char* s,int size){int n=0;for(auto p=(const unsigned char*)s;*p;){
        if(*p<128){n+=size/2;++p;}else{n+=size;p+=3;}}return n;};
    auto outlined=[&](int x,int y,const char* s,int size){
        for(int d:{-1,1}){text(x+d,y,s,size,0x0001);text(x,y+d,s,size,0x0001);}
        text(x,y,s,size,ink);
    };
    c.fillRect(0,0,368,448,0);
    if(rowBegin<76)UiCommon::drawPageHeader(c,"探险");
    const int count=std::min<int>(m.visibleAreaCount,7);
    const int selected=count?std::min<int>(m.selectedArea,count-1):0;
    const int id=m.marketSelected?6:m.destinationMenu?std::min<int>(m.destinationIds[selected],6):selected;
    const bool locked=!count || (m.destinationMenu?!m.destinationUnlocked[selected]:selected>m.unlockedArea);
    const char* names[]={Ui::Explore::GRASS_PATH,Ui::Explore::CREEK_SLOPE,
        Ui::Explore::TALL_GRASS_PARK,Ui::Explore::FROST_CRYSTAL_CAVE,
        Ui::Explore::MIST_FOREST_PATH,Ui::Explore::ANCIENT_WATERFALL_VALLEY,Ui::MarketView::NAME};
    const auto& art=ExplorePostcardArt::scenes[id];
    const int first=std::max(0,(int(rowBegin)-POSTCARD_SCENE_Y)/2);
    const int last=std::min(146,(int(rowEnd)-POSTCARD_SCENE_Y+1)/2);
    for(int y=first;y<last;++y){
        int x=0;const int inset=postcardCardInset(y*2)/2;
        for(int j=art.rows[y];j<art.rows[y+1];j+=2){
            const int end=x+art.runs[j],a=std::max(x,inset),b=std::min(end,168-inset);
            if(b>a)c.fillRect(16+a*2,POSTCARD_SCENE_Y+y*2,(b-a)*2,2,art.palette[art.runs[j+1]]);
            x=end;
        }
    }
    // Artwork and gradient share every scanline boundary, including corners.
    for(int y=std::max(0,int(rowBegin)-POSTCARD_SCENE_Y);
        y<std::min(POSTCARD_SCENE_H,int(rowEnd)-POSTCARD_SCENE_Y);++y){
        float q=std::clamp((y-100.f)/191.f,0.f,1.f);q=q*q*(3-2*q);
        const int inset=postcardCardInset(y);
        c.blendRectAlphaNative(16+inset,POSTCARD_SCENE_Y+y,336-inset*2,1,
                               rgb(8,18,24),uint8_t(179*q));
    }
    if(rowBegin<151 && rowEnd>93){
        outlined(28,94,names[id],32);
        if(id==6){
            char status[64];
            if(m.marketDaysUntilOpen)std::snprintf(status,sizeof(status),Ui::MarketText::NEXT_OPEN,
                                                  (unsigned long)m.marketDaysUntilOpen);
            else std::snprintf(status,sizeof(status),"%s",Ui::MarketText::OPEN_TODAY);
            outlined(28,134,status,16);
        }
    }
    if(rowBegin<196 && rowEnd>156)for(int side=0;side<2;++side){
        if((!side && !selected)||(side && selected+1>=count))continue;
        const int x=side?314:18;c.fillRoundRect(x,156,36,40,6,rgb(27,43,51));
        for(int k=0;k<4;++k){const int dx=side?k:3-k;
            c.fillRect(x+10+dx*3,167+k*3,3,3,ink);c.fillRect(x+10+dx*3,185-k*3,3,3,ink);}
    }
    if(rowEnd>POSTCARD_POOL_TOP && rowBegin<POSTCARD_POOL_BOTTOM){
        if(id==6 || locked){
            const char* title=id==6?"奇珍好物":Ui::Explore::AREA_LOCKED;
            const char* detail=id==6?"道具补给与技能特训":Ui::Explore::DEFEAT_PREVIOUS_BOSS;
            text((368-width(title,32))/2,276,title,32,id==6?accent:ink);
            text((368-width(detail,16))/2,324,detail,16,muted);
        }else{
            const auto layout=postcardPoolLayout(m.previewFrames,m.previewPool.count,m.previewOffset);
            int x=(368-layout.width)/2;
            for(int i=layout.start;i<layout.start+layout.count;++i){
                const auto* f=m.previewFrames[i];
                if(!f){c.drawCircle(x+84,324,6,muted);x+=168+POSTCARD_POOL_GAP;continue;}
                c.fillRoundRect(x+f->width/2-20,352,40,4,2,rgb(12,22,28));
                // No scaling: hidden and known species both use native FRONT.
                if(m.previewHidden[i])PokemonSprites::drawFrameSilhouette(f,x,350-f->height,muted);
                else PokemonSprites::drawFrame(f,x,350-f->height);
                x+=f->width+POSTCARD_POOL_GAP;
            }
            if(layout.pages>1)for(int i=0;i<layout.pages;++i)
                c.fillCircle(184+(2*i-layout.pages+1)*5,363,2,i==layout.page?accent:rgb(67,97,101));
        }
    }
    if(rowEnd>378 && rowBegin<383)for(int i=0;i<count;++i)
        c.fillRoundRect((368-count*14)/2+i*14,378,i==selected?12:5,5,2,i==selected?accent:rgb(67,97,101));
    if(rowEnd>388 && rowBegin<436){
        c.fillRoundRect(20,388,328,48,8,locked?rgb(39,53,59):rgb(42,61,68));
        c.drawRoundRect(20,388,328,48,8,locked?rgb(67,97,101):accent);
        const char* action=id==6?(locked?Ui::MarketText::CLOSED:"前往市集"):Ui::Explore::DEPART;
        text((368-width(action,32))/2,396,action,32,locked?muted:ink);
    }
}
}
