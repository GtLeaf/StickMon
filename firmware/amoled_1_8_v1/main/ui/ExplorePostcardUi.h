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
// One themed ink with transparent cutouts; supersampling only softens edges.
inline constexpr auto ball=[](){
    std::array<uint8_t,32*32> mask{};
    for(int y=0;y<32;++y)for(int x=0;x<32;++x){
        int coverage=0;
        for(int sy=0;sy<4;++sy)for(int sx=0;sx<4;++sx){
            const double px=x+(sx+.5)/4-16,py=y+(sy+.5)/4-16;
            const double radius2=px*px+py*py;
            const bool rim=radius2>=13*13 && radius2<=15*15;
            const bool center=radius2>=4*4 && radius2<=6*6;
            const bool seam=py>=-1.25 && py<=1.25 && radius2>=6*6 && radius2<=13*13;
            if(rim || center || seam)++coverage;
        }
        mask[y*32+x]=uint8_t((coverage*255+8)/16);
    }
    return mask;
}();
inline void drawBall(Canvas565& c,uint16_t color){
    for(int y=0;y<32;++y)for(int x=0;x<32;){
        const auto alpha=ball[y*32+x];
        int end=x+1;
        while(end<32 && ball[y*32+end]==alpha)++end;
        if(alpha)c.blendRectAlphaNative(294+x,102+y,end-x,1,color,alpha);
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
    if(rowBegin<142 && rowEnd>94){
        text(40,100,names[id],32,colors.sceneText);
        const auto button=POSTCARD_TOGGLE_VISUAL;
        if(showPets){
            c.fillRoundRect(button.x,button.y,button.width,button.height,16,colors.control);
            c.drawRoundRect(button.x,button.y,button.width,button.height,16,accent);
        }else c.blendRoundRectAlphaNative(button.x,button.y,button.width,button.height,16,colors.sceneControl,184);
        ExplorePostcardIcons::drawBall(c,showPets?accent:canPreview?colors.sceneText:muted);
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
        const auto layout=postcardPoolLayout(m.previewFrames,m.previewPool.count,m.previewOffset);
        const int pages=showPets?layout.pages:count;
        const int pageIndex=showPets?layout.page:selected;
        if(!showPets || pages>1){
            const int total=pages?pages*6+12+(pages-1)*8:0;
            int x=(368-total)/2;
            for(int i=0;i<pages;++i){
                const int w=i==pageIndex?18:6;
                c.fillRoundRect(x,355,w,6,3,i==pageIndex?accent:muted);x+=w+8;
            }
        }
        if(!showPets)for(int side=0;side<2;++side){
            const int cx=side?322:46,cy=358;
            const auto color=(!side && !selected)||(side && selected+1>=count)?muted:ink;
            for(int y=-10;y<=10;++y){const int dx=std::abs(y)/2-5;c.fillRect(cx+(side?-dx:dx),cy+y,2,1,color);}
        }
    }
    if(rowEnd>383 && rowBegin<431){
        c.fillRoundRect(20,383,328,48,16,locked?colors.disabledSurface:accent);
        text((368-width(Ui::Explore::DEPART,32))/2,391,Ui::Explore::DEPART,32,locked?colors.disabledText:onAccent);
    }
}
}
