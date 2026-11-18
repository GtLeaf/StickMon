#include "ui/PhotoScreens.h"
#include "ui/UiCommon.h"
#include "core/UiStrings.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TextBlock.h"
#include <algorithm>
#include <cstdio>

namespace AmoledV1 {
using Ui::TextBlock;
namespace {
uint16_t ink(){return UiCommon::rgb(226,238,233);}
uint16_t muted(){return UiCommon::rgb(126,158,158);}
void label(Canvas565& c,const char*s,int x,int y,int width,int size=32,uint16_t color=0){
    TextBlock block;TextBlock::Options o;o.width=width;o.maxHeight=size;o.align=TextBlock::Align::CENTER;o.font=size==16?BitmapText::Font::PX16:BitmapText::Font::PX32;
    block.setText(s);block.setOptions(o);block.draw(c,x,y,color?color:ink(),TextBlock::DrawRange{0,1,-1});
}
void back(Canvas565& c,int y=36){c.fillCircle(36,y,22,UiCommon::rgb(27,43,51));for(int n=-1;n<=1;++n){c.drawLine(40+n,y-8,32+n,y,ink());c.drawLine(32+n,y,40+n,y+8,ink());}}
void button(Canvas565& c,const char*s,int x,int y,int width,bool primary=false){c.fillRoundRect(x,y,width,44,10,primary?UiCommon::rgb(42,73,70):UiCommon::rgb(24,34,42));label(c,s,x,y+6,width);}
void image(Canvas565& c,const uint16_t*p,int x,int y,int size,int source){
    if(!p){c.fillRoundRect(x,y,size,size,8,UiCommon::rgb(24,34,42));label(c,Ui::Photo::CORRUPT,x,y+size/2-8,size,16,muted());return;}
    const int top=std::max(y,c.clipTop()),bottom=std::min(y+size,c.clipBottom()),left=std::max(x,c.clipLeft()),right=std::min(x+size,c.clipRight());
    uint16_t row[320];
    for(int yy=top;yy<bottom;++yy){
        int inset=0;
        if(source==int(Photos::THUMB_WIDTH)){
            const int local=yy-y,dy=local<8?8-local:local>=size-8?local-(size-9):0;
            while(dy&&inset<8&&(8-inset)*(8-inset)+dy*dy>64)++inset;
        }
        const int rowLeft=std::max(left,x+inset),rowRight=std::min(right,x+size-inset);
        for(int xx=rowLeft;xx<rowRight;++xx)row[xx-rowLeft]=p[((yy-y)*source/size)*source+(xx-x)*source/size];
        if(rowRight>rowLeft)c.pushImage(rowLeft,yy,rowRight-rowLeft,1,row);
    }
}
void trash(Canvas565& c){const auto col=ink();c.drawFastHLine(318,28,24,col);c.drawFastHLine(318,29,24,col);c.drawRect(324,21,12,8,col);c.drawRect(320,31,20,22,col);c.drawFastVLine(327,35,13,col);c.drawFastVLine(333,35,13,col);}
}
void drawPhotoCameraIcon(Canvas565& c,int cx,int cy){
    const auto col=ink();c.drawRoundRect(cx-18,cy-11,36,28,5,col);c.drawRoundRect(cx-17,cy-10,34,26,4,col);
    c.drawLine(cx-11,cy-11,cx-7,cy-18,col);c.drawLine(cx-7,cy-18,cx+7,cy-18,col);c.drawLine(cx+7,cy-18,cx+11,cy-11,col);
    c.drawCircle(cx,cy+3,8,col);c.drawCircle(cx,cy+3,7,col);
}
void renderPhotoScreen(Canvas565& c,const PhotoSession& flow,uint16_t top,uint16_t bottom,uint16_t left,uint16_t right){
    const UiCommon::PageClip clip(c,top,bottom,left,right);
    using P=PhotoSession::Page;const auto page=flow.page();char count[20];
    if(page==P::VIEWFINDER){
        c.fillRect(0,344,368,104,0);
        const int frameX=flow.frameX(),frameY=flow.frameY(),size=PhotoSession::FRAME_SIZE;
        const int frameRight=frameX+size,frameBottom=frameY+size;
        PixelRenderer::fillRectAlpha(0,0,frameX,PhotoSession::MAP_HEIGHT,0,118);
        PixelRenderer::fillRectAlpha(frameRight,0,PhotoSession::MAP_WIDTH-frameRight,PhotoSession::MAP_HEIGHT,0,118);
        PixelRenderer::fillRectAlpha(frameX,0,size,frameY,0,118);
        PixelRenderer::fillRectAlpha(frameX,frameBottom,size,PhotoSession::MAP_HEIGHT-frameBottom,0,118);
        for(int side=0;side<2;++side)for(int edge=0;edge<2;++edge){
            const int x=side?frameRight-1:frameX,y=edge?frameBottom-1:frameY;
            for(int n=0;n<2;++n){
                c.drawFastHLine(side?x-18:x,y+(edge?-n:n),19,ink());
                c.drawFastVLine(x+(side?-n:n),edge?y-18:y,19,ink());
            }
        }
        back(c,28);c.fillRoundRect(278,10,78,36,12,UiCommon::rgb(27,43,51));std::snprintf(count,sizeof(count),"%u/%u",flow.count(),Photos::CAPACITY);label(c,count,278,12,78);
        c.drawCircle(184,396,29,ink());c.drawCircle(184,396,28,ink());c.fillCircle(184,396,22,ink());
    }else{
        c.fillRect(0,0,368,448,0);back(c);
        if(page==P::PREVIEW){label(c,Ui::Photo::PREVIEW,84,20,200);image(c,flow.image(),24,72,320,Photos::WIDTH);button(c,Ui::Photo::RETAKE,24,400,152);button(c,Ui::Photo::SAVE,192,400,152,true);}
        else if(page==P::ALBUM){
            label(c,Ui::Photo::ALBUM,84,20,200);std::snprintf(count,sizeof(count),"%u/%u",flow.count(),Photos::CAPACITY);label(c,count,292,24,64,16,muted());
            if(!flow.count()&&!flow.error())label(c,Ui::Photo::EMPTY,24,208,320);
            const int oldTop=c.clipTop();c.setClipRect(c.clipLeft(),std::max(80,oldTop),c.clipRight()-c.clipLeft(),std::max(0,c.clipBottom()-std::max(80,oldTop)));
            for(unsigned i=0;i<flow.count();++i)image(c,flow.thumbnail(i),24+int(i%2)*168,88+int(i/2)*168-flow.offset(),152,Photos::THUMB_WIDTH);
        }else{
            std::snprintf(count,sizeof(count),"%u/%u",flow.selected()+1,flow.count());label(c,count,120,24,128,16,muted());trash(c);image(c,flow.image(),24,80,320,Photos::WIDTH);
            const auto& e=flow.entry(flow.selected());const char* const areas[]={Ui::Explore::GRASS_PATH,Ui::Explore::CREEK_SLOPE,Ui::Explore::TALL_GRASS_PARK,Ui::Explore::FROST_CRYSTAL_CAVE,Ui::Explore::MIST_FOREST_PATH,Ui::Explore::ANCIENT_WATERFALL_VALLEY};
            const char* location = e.area == Photos::MARKET_AREA ? Ui::MarketText::NAME : areas[e.area];
            char metadata[64];std::snprintf(metadata,sizeof(metadata),Ui::Photo::META,location,unsigned(e.day));label(c,metadata,24,420,320,16,muted());
            if(page==P::DELETE_CONFIRM){PixelRenderer::fillRectAlpha(0,0,368,448,0,150);c.fillRoundRect(24,152,320,144,12,UiCommon::rgb(17,27,34));label(c,Ui::Photo::DELETE_CONFIRM,24,176,320);button(c,Ui::Photo::CANCEL,40,232,136);button(c,Ui::Photo::DELETE,192,232,136,true);}
        }
    }
    if(flow.error()){
        const int y=page==P::VIEWFINDER?346:page==P::ALBUM?208:page==P::DELETE_CONFIRM?302:350;
        c.fillRoundRect(32,y,304,30,8,UiCommon::rgb(17,27,34));label(c,flow.error(),40,y+7,288,16,UiCommon::rgb(238,158,142));
    }
}
}
