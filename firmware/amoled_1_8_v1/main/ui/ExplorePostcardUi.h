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
    c.fillRect(0,0,368,448,0);

    const int count=std::min<int>(m.visibleAreaCount,7);
    const int selected=count?std::min<int>(m.selectedArea,count-1):0;
    const int id=m.marketSelected?6:m.destinationMenu?std::min<int>(m.destinationIds[selected],6):selected;
    const bool locked=!count || (m.destinationMenu?!m.destinationUnlocked[selected]:selected>m.unlockedArea);
    const char* names[]={Ui::Explore::GRASS_PATH,Ui::Explore::CREEK_SLOPE,
        Ui::Explore::TALL_GRASS_PARK,Ui::Explore::FROST_CRYSTAL_CAVE,
        Ui::Explore::MIST_FOREST_PATH,Ui::Explore::ANCIENT_WATERFALL_VALLEY,Ui::MarketView::NAME};
    const auto& art=ExplorePostcardArt::scenes[id];
    const int first=std::max(0,int(rowBegin)/2);
    const int last=std::min(190,(int(rowEnd)+1)/2);
    for(int y=first;y<last;++y){
        int x=0;
        for(int j=art.rows[y];j<art.rows[y+1];j+=2){
            c.fillRect(x*2,y*2,art.runs[j]*2,2,art.palette[art.runs[j+1]]);x+=art.runs[j];
        }
    }
    for(int y=rowBegin;y<std::min(380,int(rowEnd));++y){
        float top=1.f-std::clamp(y/110.f,0.f,1.f);top=top*top*(3-2*top);
        c.blendRectAlphaNative(0,y,368,1,0,uint8_t(255*top));
        float bottom=std::clamp((y-200.f)/179.f,0.f,1.f);bottom=bottom*bottom*(3-2*bottom);
        c.blendRectAlphaNative(0,y,368,1,rgb(8,18,24),uint8_t(200*bottom));
    }
    if(rowBegin<76)UiCommon::drawPageHeader(c,"探险",nullptr,true,false);
    if(rowBegin<132 && rowEnd>84){
        const int w=width(names[id],32)+24;
        for(int y=0;y<48;y++){
            int edge=std::min(y,47-y),inset=edge<2?6:edge<4?3:edge<6?1:0;
            c.blendRectAlphaNative(12+inset,84+y,w-2*inset,1,rgb(248,248,224),204);
        }
        text(24,92,names[id],32,rgb(27,43,51));
    }
    if(rowBegin<199 && rowEnd>153)for(int side=0;side<2;++side){
        if((!side && !selected)||(side && selected+1>=count))continue;
        const int cx=side?336:32,cy=176;
        for(int y=-22;y<=22;y++){
            const int w=int(std::sqrt(float(22*22-y*y)));
            c.blendRectAlphaNative(cx-w,cy+y,2*w+1,1,rgb(14,26,32),155);
        }
        for(int pass=0;pass<2;pass++)for(int y=-10;y<=10;y+=2){
            const int dx=std::abs(y)/2-5,px=cx+(side?-dx:dx),off=pass==0?1:0;
            c.fillRect(px+off,cy+y+off,3,2,pass==0?rgb(20,32,35):rgb(232,242,237));
        }
    }
    if(rowEnd>POSTCARD_POOL_TOP && rowBegin<POSTCARD_POOL_BOTTOM){
        if(id==6 || locked){
            if(id==6){
                char status[48];
                if(m.marketDaysUntilOpen)std::snprintf(status,sizeof(status),Ui::MarketText::SELECTOR_NEXT,(unsigned long)m.marketDaysUntilOpen);
                else std::snprintf(status,sizeof(status),"%s",Ui::MarketText::SELECTOR_ACTIVE);
                text((368-width(status,32))/2,310,status,32,m.marketDaysUntilOpen?ink:accent);
            }else{
                text((368-width(Ui::Explore::AREA_LOCKED,32))/2,276,Ui::Explore::AREA_LOCKED,32,ink);
                text((368-width(Ui::Explore::DEFEAT_PREVIOUS_BOSS,16))/2,324,Ui::Explore::DEFEAT_PREVIOUS_BOSS,16,muted);
            }
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
        }
    }
    if(rowEnd>388 && rowBegin<436){
        c.fillRoundRect(20,388,328,48,8,locked?rgb(39,53,59):rgb(42,61,68));
        c.drawRoundRect(20,388,328,48,8,locked?rgb(67,97,101):accent);
        const char* action=Ui::Explore::DEPART;
        text((368-width(action,32))/2,396,action,32,locked?muted:ink);
    }
}
}
