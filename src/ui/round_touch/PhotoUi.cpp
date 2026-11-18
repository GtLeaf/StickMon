#include "ui/round_touch/PhotoUi.h"
#include "core/UiStrings.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TextBlock.h"
#include <algorithm>
#include <cstdio>

namespace Ui::RoundPhotoUi {
namespace {
uint16_t ink(){return PixelRenderer::rgb(226,238,233);}
void label(const char* text,int x,int y,int width,int size=32,int height=0,uint16_t color=0){
    TextBlock b;TextBlock::Options o;o.width=width;o.maxHeight=height?height:size;
    o.align=TextBlock::Align::CENTER;o.font=size==16?BitmapText::Font::PX16:BitmapText::Font::PX32;
    b.setText(text);b.setOptions(o);b.draw(PixelRenderer::canvas(),x,y,color?color:ink());
}
void button(const PhotoFlow::Rect& r,const char* text,bool primary=false){
    PixelRenderer::canvas().fillRoundRect(r.x,r.y,r.width,r.height,10,
        primary?PixelRenderer::rgb(42,73,70):PixelRenderer::rgb(24,34,42));
    label(text,r.x,r.y+6,r.width);
}
void image(const uint16_t* pixels,int x,int y,int size,int source){
    auto& c=PixelRenderer::canvas();
    if(!pixels){c.fillRoundRect(x,y,size,size,8,PixelRenderer::rgb(24,34,42));label(Photo::CORRUPT,x,y+size/2-8,size,16);return;}
    uint16_t row[224];
    const int left=std::max(x,c.clipLeft()),right=std::min(x+size,c.clipRight());
    for(int yy=std::max(y,c.clipTop());yy<std::min(y+size,c.clipBottom());++yy){
        for(int xx=left;xx<right;++xx)row[xx-left]=pixels[(yy-y)*source/size*source+(xx-x)*source/size];
        if(right>left)c.pushImage(left,yy,right-left,1,row);
    }
}
void back(const PhotoFlow& f){
    auto& c=PixelRenderer::canvas();const auto r=f.backBounds();const int x=r.x+r.width/2,y=r.y+r.height/2;
    c.fillCircle(x,y,20,PixelRenderer::rgb(27,43,51));
    for(int n=0;n<2;++n){c.drawLine(x+4+n,y-8,x-4+n,y,ink());c.drawLine(x-4+n,y,x+4+n,y+8,ink());}
}
}
void cameraIcon(Canvas565& c,int x,int y){
    const auto col=ink();c.fillRoundRect(x-20,y-20,40,40,10,PixelRenderer::rgb(23,45,50));
    for(int n=0;n<2;++n)c.drawRoundRect(x-14+n,y-8+n,28-2*n,20-2*n,3,col);
    c.fillRect(x-7,y-12,14,5,col);c.drawCircle(x,y+2,6,col);c.drawCircle(x,y+2,5,col);
}
void notice(const char* text){
    PixelRenderer::canvas().fillRoundRect(68,252,224,44,10,PixelRenderer::rgb(17,27,34));
    label(text,76,258,208);
}
void render(const PhotoFlow& f){
    auto& c=PixelRenderer::canvas();using P=PhotoFlow::Page;const auto page=f.page();char text[64];
    if(page==P::VIEWFINDER){
        const int x=f.frameX(),y=f.frameY(),size=f.frameSize(),r=x+size,b=y+size;
        PixelRenderer::fillRectAlpha(0,0,x,360,0,118);PixelRenderer::fillRectAlpha(r,0,360-r,360,0,118);
        PixelRenderer::fillRectAlpha(x,0,size,y,0,118);PixelRenderer::fillRectAlpha(x,b,size,360-b,0,118);
        const auto s=f.shutterBounds();const int cx=s.x+s.width/2,cy=s.y+s.height/2;
        c.fillCircle(cx,cy,24,PixelRenderer::rgb(17,27,34));c.drawCircle(cx,cy,21,ink());c.drawCircle(cx,cy,20,ink());c.fillCircle(cx,cy,15,ink());
    }else{
        c.fillRect(0,0,360,360,0);
        if(page==P::PREVIEW){
            label(Photo::PREVIEW,136,30,88);image(f.image(),72,68,216,Photos::WIDTH);
            button(f.retakeBounds(),Photo::RETAKE);button(f.saveBounds(),Photo::SAVE,true);
        }else if(page==P::ALBUM){
            label(Photo::ALBUM,136,30,88);
            if(!f.count()&&!f.error())label(Photo::EMPTY,60,164,240);
            const int l=c.clipLeft(),t=c.clipTop(),r=c.clipRight(),b=c.clipBottom();const auto bounds=f.albumBounds();
            const int ll=std::max(l,bounds.x),tt=std::max(t,bounds.y),rr=std::min(r,bounds.x+bounds.width),bb=std::min(b,bounds.y+bounds.height);
            if(rr>ll&&bb>tt){c.setClipRect(ll,tt,rr-ll,bb-tt);
                for(unsigned i=0;i<f.count();++i){const auto item=f.thumbBounds(i);image(f.thumbnail(i),item.x,item.y,item.width,Photos::THUMB_WIDTH);}
            }
            c.setClipRect(l,t,r-l,b-t);
        }else if(page==P::DETAIL||page==P::DELETE_CONFIRM){
            std::snprintf(text,sizeof(text),"%u/%u",f.selected()+1,f.count());label(text,140,34,80,16);
            const auto trash=f.trashBounds();const int x=trash.x+trash.width/2,y=trash.y+trash.height/2;
            c.drawRect(x-8,y-5,16,17,ink());c.drawFastHLine(x-10,y-9,20,ink());c.drawRect(x-4,y-13,8,4,ink());
            image(f.image(),80,76,200,Photos::WIDTH);
            const char* areas[]={Explore::GRASS_PATH,Explore::CREEK_SLOPE,Explore::TALL_GRASS_PARK,Explore::FROST_CRYSTAL_CAVE,Explore::MIST_FOREST_PATH,Explore::ANCIENT_WATERFALL_VALLEY};
            const auto& e=f.entry(f.selected());std::snprintf(text,sizeof(text),Photo::META,areas[e.area<6?e.area:0],unsigned(e.day));
            label(text,60,286,240,16);
            if(page==P::DELETE_CONFIRM){
                PixelRenderer::fillRectAlpha(0,0,360,360,0,150);c.fillRoundRect(60,132,240,132,12,PixelRenderer::rgb(17,27,34));
                if(f.error())label(f.error(),68,148,224,16,48,PixelRenderer::rgb(238,158,142));
                else label(Photo::DELETE_CONFIRM,64,150,232);
                button(f.cancelBounds(),Photo::CANCEL);button(f.deleteBounds(),Photo::DELETE,true);
            }
        }
    }
    back(f);
    if(page==P::VIEWFINDER||page==P::ALBUM){
        const auto r=f.trashBounds();c.fillRoundRect(r.x,r.y,r.width,r.height,10,PixelRenderer::rgb(27,43,51));
        std::snprintf(text,sizeof(text),"%u/%u",f.count(),Photos::CAPACITY);label(text,r.x,r.y+8,r.width,16);
    }
    // Crop marks remain visible even when a legal corner crosses a header badge.
    if(page==P::VIEWFINDER){
        const int x=f.frameX(),y=f.frameY(),r=x+f.frameSize(),b=y+f.frameSize();
        for(int side=0;side<2;++side)for(int edge=0;edge<2;++edge){
            const int xx=side?r-1:x,yy=edge?b-1:y;
            for(int n=0;n<2;++n){c.drawFastHLine(side?xx-16:xx,yy+(edge?-n:n),17,ink());c.drawFastVLine(xx+(side?-n:n),edge?yy-16:yy,17,ink());}
        }
    }
    if(f.error()&&page!=P::DELETE_CONFIRM){
        c.fillRoundRect(60,232,240,48,10,PixelRenderer::rgb(17,27,34));
        label(f.error(),68,240,224,16,32,PixelRenderer::rgb(238,158,142));
    }
}
}
