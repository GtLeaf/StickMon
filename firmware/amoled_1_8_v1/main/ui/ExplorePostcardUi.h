#pragma once
#include "ui/ExplorePostcardArt.h"
#include "ui/ExplorePostcardLayout.h"
#include "ui/ExploreScreens.h"
#include "ui/UiCommon.h"
#include "ui/ThemedUi.h"
#include "ui/common/MarketView.h"
#include "core/UiStrings.h"
#include "presentation/PixelRenderer.h"
#include <array>
#include <cstdio>

namespace AmoledV1 {
namespace ExplorePostcardIcons {
// Bake 4x coverage at compile time; rendering only blends the small icon mask.
inline constexpr auto eyes=[]() {
    std::array<std::array<uint8_t,29*29>,2> masks{};
    for(int state=0;state<2;++state){
        for(int y=0;y<29;++y)for(int x=0;x<29;++x){
            int coverage=0;
            for(int sy=0;sy<4;++sy)for(int sx=0;sx<4;++sx){
                const double px=x+(sx+0.5)/4-14.5;
                const double py=y+(sy+0.5)/4-14.5;
                const double ax=px<0?-px:px,ay=py<0?-py:py;
                const double arc=ay-7+7*px*px/144;
                const double slope=14*px/144;
                const bool outline=(ax<=12 && arc*arc<=1+slope*slope) ||
                    ((ax-12)*(ax-12)+py*py<=1);
                const double radius2=px*px+py*py;
                const bool pupil=radius2>=1.75*1.75 && radius2<=3.5*3.5;
                const double projection=(px+py)/2;
                const double q=projection<-11?-11:projection>11?11:projection;
                const bool slash=state && (px-q)*(px-q)+(py-q)*(py-q)<=1;
                if(outline || pupil || slash)++coverage;
            }
            masks[state][y*29+x]=uint8_t((coverage*255+8)/16);
        }
    }
    return masks;
}();
inline void drawEye(Canvas565& c,bool crossed,uint16_t color){
    const auto& mask=eyes[crossed?1:0];
    for(int y=0;y<29;++y)for(int x=0;x<29;){
        const auto alpha=mask[y*29+x];
        int end=x+1;
        while(end<29 && mask[y*29+end]==alpha)++end;
        if(alpha)c.blendRectAlphaNative(296+x,104+y,end-x,1,color,alpha);
        x=end;
    }
}
}
inline void renderExplorePostcard(Canvas565& c,const ExploreViewModel& m,
                                 uint16_t rowBegin,uint16_t rowEnd) {
    const auto& colors=UiTheme::current();
    const auto page=colors.background,ink=colors.text,accent=colors.accent;
    const auto muted=colors.muted,onAccent=colors.accentText;
    auto text=[&](int x,int y,const char* s,int size,uint16_t color){
        const bool native=c.nativeText();c.setNativeText(size==32);
        PixelRenderer::text(c,x,y,s,color);c.setNativeText(native);
    };
    auto width=[](const char* s,int size){int n=0;for(auto p=(const unsigned char*)s;*p;){
        if(*p<128){n+=size/2;++p;}else{n+=size;p+=3;}}return n;};
    c.fillRect(0,0,368,448,page);
    const int count=std::min<int>(m.visibleAreaCount,7);
    const int selected=count?std::min<int>(m.selectedArea,count-1):0;
    const int id=m.marketSelected?6:m.destinationMenu?std::min<int>(m.destinationIds[selected],6):selected;
    const bool locked=!count || (m.destinationMenu?!m.destinationUnlocked[selected]:selected>m.unlockedArea);
    const bool canPreview=!locked || id==6;
    const bool showPets=m.previewVisible && canPreview;
    const char* names[]={Ui::Explore::GRASS_PATH,Ui::Explore::CREEK_SLOPE,
        Ui::Explore::TALL_GRASS_PARK,Ui::Explore::FROST_CRYSTAL_CAVE,
        Ui::Explore::MIST_FOREST_PATH,Ui::Explore::ANCIENT_WATERFALL_VALLEY,Ui::MarketView::NAME};

    // Draw directly into the page clip, rounding each row without a scratch buffer.
    c.fillRoundRect(23,83,322,246,21,colors.border);
    const auto& art=ExplorePostcardArt::scenes[id];
    for(int dy=std::max(0,int(rowBegin)-84);dy<std::min(244,int(rowEnd)-84);++dy){
        const int edge=std::min(dy,243-dy);
        const int inset=edge<20?20-int(std::sqrt(float(400-(20-edge)*(20-edge)))):0;
        const int left=24+inset,right=344-inset,y=84+dy;
        int x=24;
        for(int j=art.rows[dy/2];j<art.rows[dy/2+1];j+=2){
            const int end=x+art.runs[j]*2,l=std::max(left,x),r=std::min(right,end);
            if(r>l)c.fillRect(l,y,r-l,1,art.palette[art.runs[j+1]]);
            x=end;
        }
        int alpha=dy<104?135-(dy*115/104):20+(dy-104)*23/140;
        if(showPets && dy>90)alpha=std::max(alpha,(dy-90)*180/153);
        c.blendRectAlphaNative(left,y,right-left,1,colors.sceneScrim,uint8_t(alpha));
    }
    if(rowBegin<76){
        ThemedUi::drawHeader(c,"探险");
    }
    if(rowBegin<144 && rowEnd>96){
        text(40,100,names[id],32,colors.sceneText);
        if(showPets)c.fillRoundRect(288,96,44,44,14,accent);
        else c.blendRoundRectAlphaNative(288,96,44,44,14,colors.sceneControl,184);
        const auto eye=canPreview?colors.sceneText:muted;
        ExplorePostcardIcons::drawEye(c,showPets,eye);
    }
    if(id==6 && m.marketDaysUntilOpen && rowBegin<156 && rowEnd>140){
        char countdown[48];
        std::snprintf(countdown,sizeof(countdown),Ui::MarketText::SELECTOR_NEXT,
                      (unsigned long)m.marketDaysUntilOpen);
        text(40,140,countdown,16,colors.sceneStatus);
    }
    if(rowEnd>POSTCARD_POOL_TOP && rowBegin<POSTCARD_POOL_BOTTOM){
        if(showPets){
            const auto layout=postcardPoolLayout(m.previewFrames,m.previewPool.count,m.previewOffset);
            int x=(368-layout.width)/2;
            const int feet=id==6?284:304;
            for(int i=layout.start;i<layout.start+layout.count;++i){
                const auto* f=m.previewFrames[i];
                if(!f){c.drawCircle(x+84,feet-20,6,onAccent);x+=168+POSTCARD_POOL_GAP;continue;}
                c.fillRoundRect(x+f->width/2-20,feet+2,40,4,2,colors.sceneShadow);
                if(m.previewHidden[i])PokemonSprites::drawFrameSilhouette(f,x,feet-f->height,0);
                else PokemonSprites::drawFrame(f,x,feet-f->height);
                x+=f->width+POSTCARD_POOL_GAP;
            }
        }
        if(id!=6 && locked){
            text(40,272,Ui::Explore::AREA_LOCKED,16,onAccent);
            text(40,300,Ui::Explore::DEFEAT_PREVIOUS_BOSS,16,onAccent);
        }
    }
    if(rowBegin<382 && rowEnd>334){
        const int total=count?count*6+12+(count-1)*8:0;
        int x=(368-total)/2;
        for(int i=0;i<count;++i){const int w=i==selected?18:6;c.fillRoundRect(x,355,w,6,3,i==selected?accent:muted);x+=w+8;}
        for(int side=0;side<2;++side){
            const int cx=side?322:46,cy=358;
            const auto color=(!side && !selected)||(side && selected+1>=count)?muted:ink;
            for(int y=-10;y<=10;++y){const int dx=std::abs(y)/2-5;c.fillRect(cx+(side?-dx:dx),cy+y,2,1,color);}
        }
    }
    if(rowEnd>388 && rowBegin<436){
        c.fillRoundRect(20,388,328,48,16,locked?colors.disabledSurface:accent);
        text((368-width(Ui::Explore::DEPART,32))/2,396,Ui::Explore::DEPART,32,locked?colors.disabledText:onAccent);
    }
}
}
