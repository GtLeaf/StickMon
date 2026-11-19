#include "PhotoFlow.h"
#include "core/AudioManager.h"
#include "core/UiStrings.h"
#include "presentation/Canvas565.h"
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <limits>

namespace Ui {
namespace {constexpr int BACK_TARGET=1000,SHUTTER=1001,RETAKE=1002,SAVE=1003,TRASH=1004,CANCEL=1005,DELETE=1006;}
PhotoFlow::PhotoFlow(Geometry geometry):geometry_(geometry){
    frameX_=(geometry_.width-frameSize())/2;frameY_=(geometry_.mapHeight-frameSize())/2;
}
PhotoFlow::Rect PhotoFlow::backBounds()const{return geometry_.circular?Rect{84,24,44,44}:Rect{0,0,80,76};}
PhotoFlow::Rect PhotoFlow::shutterBounds()const{return geometry_.circular?Rect{148,302,64,44}:Rect{140,352,88,96};}
PhotoFlow::Rect PhotoFlow::retakeBounds()const{return geometry_.circular?Rect{88,288,88,44}:Rect{24,396,152,52};}
PhotoFlow::Rect PhotoFlow::saveBounds()const{return geometry_.circular?Rect{184,288,88,44}:Rect{192,396,152,52};}
PhotoFlow::Rect PhotoFlow::trashBounds()const{return geometry_.circular?Rect{232,24,44,44}:Rect{292,0,76,76};}
PhotoFlow::Rect PhotoFlow::cancelBounds()const{return geometry_.circular?Rect{76,208,100,44}:Rect{40,232,136,48};}
PhotoFlow::Rect PhotoFlow::deleteBounds()const{return geometry_.circular?Rect{184,208,100,44}:Rect{192,232,136,48};}
PhotoFlow::Rect PhotoFlow::albumBounds()const{return geometry_.circular?Rect{60,80,240,224}:Rect{0,80,368,368};}
PhotoFlow::Rect PhotoFlow::thumbBounds(unsigned index)const{
    if(geometry_.circular)return {70+int(index%2)*116,84+int(index/2)*116-offset_,104,104};
    return {24+int(index%2)*168,88+int(index/2)*168-offset_,152,152};
}
bool PhotoFlow::contains(int x,int y)const{
    if(x<0||x>=geometry_.width||y<0||y>=geometry_.height)return false;
    if(!geometry_.circular)return true;
    const double dx=x-(geometry_.width-1)/2.0,dy=y-(geometry_.height-1)/2.0;
    const double radius=std::min(geometry_.width,geometry_.height)/2.0;
    return dx*dx+dy*dy<=radius*radius;
}
void PhotoFlow::moveFrame(int x,int y){
    x=std::clamp(x,0,geometry_.width-frameSize());
    y=std::clamp(y,0,geometry_.mapHeight-frameSize());
    if(geometry_.circular){
        // For each integral left edge, the farther horizontal pixel corner
        // bounds both vertical pixel corners. Choose the nearest legal square
        // to the finger's requested origin, retaining its original offset.
        const double cx=(geometry_.width-1)/2.0,cy=(geometry_.height-1)/2.0;
        const double radius=std::min(geometry_.width,geometry_.height)/2.0;
        long best=std::numeric_limits<long>::max();int bestX=frameX_,bestY=frameY_;
        for(int left=0;left<=geometry_.width-frameSize();++left){
            const double dx=std::max(std::abs(left-cx),std::abs(left+frameSize()-1-cx));
            if(dx>radius)continue;
            const double extent=std::sqrt(radius*radius-dx*dx);
            const int top=std::max(0,int(std::ceil(cy-extent)));
            const int bottom=std::min(geometry_.mapHeight-frameSize(),int(std::floor(cy+extent))-frameSize()+1);
            if(top>bottom)continue;
            const int candidateY=std::clamp(y,top,bottom);
            const long distance=long(left-x)*(left-x)+long(candidateY-y)*(candidateY-y);
            if(distance<best){best=distance;bestX=left;bestY=candidateY;}
        }
        x=bestX;y=bestY;
    }
    frameDirty_|=x!=frameX_||y!=frameY_;frameX_=x;frameY_=y;
}
void PhotoFlow::error(Photos::Result r){
    using R=Photos::Result;
    error_=r==R::OK?nullptr:r==R::FULL?Ui::Photo::FULL:r==R::UNAVAILABLE?Ui::Photo::UNAVAILABLE:
        r==R::NO_MEMORY?Ui::Photo::NO_MEMORY:r==R::CORRUPT?Ui::Photo::CORRUPT:Ui::Photo::FAILED;
}
void PhotoFlow::close(){page_=Page::CLOSED;cancelTouch();image_.reset();thumbs_.reset();imageValid_=false;error_=nullptr;saved_=false;frameDirty_=false;}
void PhotoFlow::cancelTouch(){tap_.cancel();scroll_.cancel();swipeDown_=false;frameGesture_=FrameGesture::NONE;frameDragging_=false;}
void PhotoFlow::openCamera(uint8_t area,uint32_t day){
    close();frameSize_=DEFAULT_FRAME_SIZE;area_=area;day_=day;error(library_.refresh());page_=Page::VIEWFINDER;
    frameX_=(geometry_.width-frameSize())/2;frameY_=(geometry_.mapHeight-frameSize())/2;
}
void PhotoFlow::openAlbum(){close();page_=Page::ALBUM;offset_=0;error(library_.refresh());if(!error_)thumbnails();}
void PhotoFlow::thumbnails(){
    std::fill_n(thumbValid_,Photos::CAPACITY,false);
    if(!count()){thumbs_.reset();return;}
    if(!thumbs_.allocate(Photos::CAPACITY*Photos::THUMB_PIXELS)){error(Photos::Result::NO_MEMORY);return;}
    for(unsigned i=0;i<count();++i)thumbValid_[i]=library_.loadThumbnail(entry(i),thumbs_.data()+i*Photos::THUMB_PIXELS)==Photos::Result::OK;
}
void PhotoFlow::show(unsigned index){
    if(index>=count())return;
    cancelTouch();selected_=index;page_=Page::DETAIL;imageValid_=false;
    if(!image_.allocate(Photos::PIXELS)){error(Photos::Result::NO_MEMORY);return;}
    const auto result=library_.loadImage(entry(index),image_.data());imageValid_=result==Photos::Result::OK;error(result);
}
int PhotoFlow::maximum()const{
    if(!geometry_.circular)return std::max(0,int((count()+1)/2)*168-360);
    if(!count())return 0;
    // Keep twelve pixels below the last thumbnail inside the album viewport.
    const auto last=thumbBounds(count()-1),bounds=albumBounds();
    return std::max(0,last.y+offset_+last.height+12-(bounds.y+bounds.height));
}
bool PhotoFlow::update(uint32_t elapsed){
    if(viewfinder()){const bool changed=frameDirty_;frameDirty_=false;return changed;}
    return page_==Page::ALBUM&&scroll_.update(elapsed,maximum(),offset_);
}
bool PhotoFlow::capture(const Canvas565& canvas,uint32_t day){
    if(!viewfinder())return false;
    if(!image_.allocate(Photos::PIXELS)){error(Photos::Result::NO_MEMORY);return false;}
    // Sample the same bounded square used by the overlay, without HUD pixels.
    for(unsigned y=0;y<Photos::WIDTH;++y)for(unsigned x=0;x<Photos::WIDTH;++x)
        image_.data()[y*Photos::WIDTH+x]=canvas.readPixel(frameX_+int((2*x+1)*frameSize()/(2*Photos::WIDTH)),frameY_+int((2*y+1)*frameSize()/(2*Photos::WIDTH)));
    day_=day;imageValid_=true;page_=Page::PREVIEW;error_=nullptr;cancelTouch();return true;
}
int PhotoFlow::target(int x,int y)const{
    if(!contains(x,y))return -1;
    if(backBounds().contains(x,y))return BACK_TARGET;
    if(page_==Page::VIEWFINDER)return shutterBounds().contains(x,y)?SHUTTER:-1;
    if(page_==Page::PREVIEW){if(retakeBounds().contains(x,y))return RETAKE;if(saveBounds().contains(x,y))return SAVE;return -1;}
    if(page_==Page::DELETE_CONFIRM){if(cancelBounds().contains(x,y))return CANCEL;if(deleteBounds().contains(x,y))return DELETE;return -1;}
    if(page_==Page::DETAIL)return trashBounds().contains(x,y)?TRASH:-1;
    if(page_==Page::ALBUM&&albumBounds().contains(x,y)&&(geometry_.circular||y>=88)){
        for(unsigned i=0;i<count();++i)if(thumbBounds(i).contains(x,y))return int(i);
    }
    return -1;
}
PhotoFlow::Request PhotoFlow::touch(const Platform::IInputDevice::TouchEvent&e){
    using T=Platform::IInputDevice::TouchType;
    if(!active())return Request::NONE;
    if(e.type==T::CANCEL){cancelTouch();return Request::NONE;}
    if(!contains(e.x,e.y)){cancelTouch();return Request::NONE;}
    if(viewfinder()){
        if(e.type==T::DOWN){
            cancelTouch();
            const bool header=target(e.x,e.y)>=BACK_TARGET||(geometry_.circular?trashBounds().contains(e.x,e.y):(e.x>=278&&e.x<356&&e.y>=10&&e.y<46));
            const Rect frame{frameX_,frameY_,frameSize_,frameSize_};
            if(!header && frame.contains(e.x,e.y)){
                constexpr int hitSize=44;
                const Rect handle{frameX_+frameSize_-hitSize,frameY_+frameSize_-hitSize,hitSize,hitSize};
                frameGesture_=handle.contains(e.x,e.y)?FrameGesture::RESIZE:FrameGesture::MOVE;
                frameOriginSize_=frameSize_;
                frameTouchX_=e.x;frameTouchY_=e.y;frameOriginX_=frameX_;frameOriginY_=frameY_;
                return Request::NONE;
            }
        }else if(frameGesture_!=FrameGesture::NONE){
            const int dx=e.x-frameTouchX_,dy=e.y-frameTouchY_;
            if(dx*dx+dy*dy>64)frameDragging_=true;
            if(frameDragging_ && frameGesture_==FrameGesture::RESIZE){
                // Project finger motion onto the diagonal. Keeping the center
                // fixed makes each edge travel half the change in side length.
                const int size=std::clamp(frameOriginSize_+dx+dy,MIN_FRAME_SIZE,MAX_FRAME_SIZE);
                frameDirty_|=size!=frameSize_;
                frameSize_=size;
                const int inset=(frameOriginSize_+1)/2-(size+1)/2;
                moveFrame(frameOriginX_+inset,frameOriginY_+inset);
            }else if(frameDragging_){
                moveFrame(frameOriginX_+dx,frameOriginY_+dy);
            }
            // A crop gesture owns its release, even over shutter or back.
            if(e.type==T::UP){
                frameGesture_=FrameGesture::NONE;frameDragging_=false;
                // AmoledApp renders every release immediately; do not redraw it
                // again on the next coalesced MOVE tick.
                frameDirty_=false;
            }
            return Request::NONE;
        }
    }
    if(page_==Page::ALBUM){
        const auto bounds=albumBounds();
        if(scroll_.touch(e,{bounds.x,bounds.y,bounds.width,bounds.height},maximum(),offset_,tap_))return Request::NONE;
    }
    if(page_==Page::DETAIL){
        if(e.type==T::DOWN&&e.y>=(geometry_.circular?76:80)&&e.y<(geometry_.circular?288:400)){swipeDown_=true;swipeX_=e.x;swipeY_=e.y;}
        if(swipeDown_&&e.type==T::MOVE&&std::abs(e.x-swipeX_)>12)tap_.cancel();
        if(e.type==T::UP&&swipeDown_){
            const int dx=e.x-swipeX_,dy=e.y-swipeY_;swipeDown_=false;
            if(std::abs(dx)>=40&&std::abs(dx)>std::abs(dy)){
                const int next=int(selected_)+(dx<0?1:-1);tap_.cancel();if(next>=0&&next<int(count()))show(unsigned(next));return Request::NONE;
            }
        }
    }
    const int activated=tap_.update(e,target(e.x,e.y));if(activated<0)return Request::NONE;
    if(activated==BACK_TARGET){
        cancelTouch();error_=nullptr;
        if(page_==Page::VIEWFINDER)return Request::CLOSE_CAMERA;
        if(page_==Page::ALBUM)return Request::CLOSE_ALBUM;
        if(page_==Page::PREVIEW){page_=Page::VIEWFINDER;imageValid_=false;}
        else if(page_==Page::DELETE_CONFIRM)page_=Page::DETAIL;
        else {page_=Page::ALBUM;image_.reset();imageValid_=false;}
    }else if(activated==SHUTTER){
        if(count()>=Photos::CAPACITY)error(Photos::Result::FULL);
        else return Request::CAPTURE;
    }else if(activated==RETAKE){page_=Page::VIEWFINDER;imageValid_=false;error_=nullptr;cancelTouch();}
    else if(activated==SAVE&&imageValid_){
        const auto result=library_.save(image_.data(),area_,day_,[](){AudioManager::ins().update();});error(result);
        if(result==Photos::Result::OK){saved_=true;return Request::CLOSE_CAMERA;}
    }else if(activated==TRASH){page_=Page::DELETE_CONFIRM;error_=nullptr;cancelTouch();}
    else if(activated==CANCEL){page_=Page::DETAIL;error_=nullptr;cancelTouch();}
    else if(activated==DELETE){
        const auto result=library_.erase(entry(selected_));error(result);
        if(result==Photos::Result::OK){page_=Page::ALBUM;image_.reset();imageValid_=false;offset_=std::min(offset_,maximum());thumbnails();cancelTouch();}
    }else if(page_==Page::ALBUM&&activated<int(count()))show(unsigned(activated));
    return Request::NONE;
}
}
