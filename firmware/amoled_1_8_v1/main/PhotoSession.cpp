#include "PhotoSession.h"
#include "core/AudioManager.h"
#include "core/UiStrings.h"
#include "presentation/Canvas565.h"
#include <algorithm>
#include <cstdlib>

namespace AmoledV1 {
namespace {constexpr int BACK=1000,SHUTTER=1001,RETAKE=1002,SAVE=1003,TRASH=1004,CANCEL=1005,DELETE=1006;}
void PhotoSession::error(Photos::Result r){
    using R=Photos::Result;
    error_=r==R::OK?nullptr:r==R::FULL?Ui::Photo::FULL:r==R::UNAVAILABLE?Ui::Photo::UNAVAILABLE:
        r==R::NO_MEMORY?Ui::Photo::NO_MEMORY:r==R::CORRUPT?Ui::Photo::CORRUPT:Ui::Photo::FAILED;
}
void PhotoSession::close(){page_=Page::CLOSED;cancelTouch();image_.reset();thumbs_.reset();imageValid_=false;error_=nullptr;saved_=false;frameDirty_=false;}
void PhotoSession::cancelTouch(){tap_.cancel();scroll_.cancel();swipeDown_=false;frameDown_=frameDragging_=false;}
void PhotoSession::openCamera(uint8_t area,uint32_t day){
    close();area_=area;day_=day;error(library_.refresh());page_=Page::VIEWFINDER;
    frameX_=(MAP_WIDTH-FRAME_SIZE)/2;frameY_=(MAP_HEIGHT-FRAME_SIZE)/2;
}
void PhotoSession::openAlbum(){close();page_=Page::ALBUM;offset_=0;error(library_.refresh());if(!error_)thumbnails();}
void PhotoSession::thumbnails(){
    std::fill_n(thumbValid_,Photos::CAPACITY,false);
    if(!count()){thumbs_.reset();return;}
    if(!thumbs_.allocate(Photos::CAPACITY*Photos::THUMB_PIXELS)){error(Photos::Result::NO_MEMORY);return;}
    for(unsigned i=0;i<count();++i)thumbValid_[i]=library_.loadThumbnail(entry(i),thumbs_.data()+i*Photos::THUMB_PIXELS)==Photos::Result::OK;
}
void PhotoSession::show(unsigned index){
    if(index>=count())return;
    cancelTouch();selected_=index;page_=Page::DETAIL;imageValid_=false;
    if(!image_.allocate(Photos::PIXELS)){error(Photos::Result::NO_MEMORY);return;}
    const auto result=library_.loadImage(entry(index),image_.data());imageValid_=result==Photos::Result::OK;error(result);
}
int PhotoSession::maximum()const{return std::max(0,int((count()+1)/2)*168-360);}
bool PhotoSession::update(uint32_t elapsed){
    if(viewfinder()){const bool changed=frameDirty_;frameDirty_=false;return changed;}
    return page_==Page::ALBUM&&scroll_.update(elapsed,maximum(),offset_);
}
bool PhotoSession::capture(const Canvas565& canvas,uint32_t day){
    if(!viewfinder())return false;
    if(!image_.allocate(Photos::PIXELS)){error(Photos::Result::NO_MEMORY);return false;}
    // Sample the same bounded square used by the overlay, without HUD pixels.
    for(unsigned y=0;y<Photos::WIDTH;++y)for(unsigned x=0;x<Photos::WIDTH;++x)
        image_.data()[y*Photos::WIDTH+x]=canvas.readPixel(frameX_+int((2*x+1)*FRAME_SIZE/(2*Photos::WIDTH)),frameY_+int((2*y+1)*FRAME_SIZE/(2*Photos::WIDTH)));
    day_=day;imageValid_=true;page_=Page::PREVIEW;error_=nullptr;cancelTouch();return true;
}
int PhotoSession::target(int x,int y)const{
    if(x<0||x>=368||y<0||y>=448)return -1;
    if(x<80&&y<76)return BACK;
    if(page_==Page::VIEWFINDER)return x>=140&&x<228&&y>=352?SHUTTER:-1;
    if(page_==Page::PREVIEW){if(y>=396&&y<448){if(x>=24&&x<176)return RETAKE;if(x>=192&&x<344)return SAVE;}return -1;}
    if(page_==Page::DELETE_CONFIRM){if(y>=232&&y<280){if(x>=40&&x<176)return CANCEL;if(x>=192&&x<328)return DELETE;}return -1;}
    if(page_==Page::DETAIL)return x>=292&&y<76?TRASH:-1;
    if(page_==Page::ALBUM&&y>=88){const int yy=y-88+offset_,xx=x-24;if(xx<0||xx>=320||xx%168>=152||yy%168>=152)return -1;const int i=(yy/168)*2+xx/168;return i<int(count())?i:-1;}
    return -1;
}
PhotoSession::Request PhotoSession::touch(const Platform::IInputDevice::TouchEvent&e){
    using T=Platform::IInputDevice::TouchType;
    if(!active())return Request::NONE;
    if(e.type==T::CANCEL){cancelTouch();return Request::NONE;}
    if(e.x<0||e.x>=368||e.y<0||e.y>=448){cancelTouch();return Request::NONE;}
    if(viewfinder()){
        if(e.type==T::DOWN){
            cancelTouch();
            const bool header=target(e.x,e.y)==BACK||(e.x>=278&&e.x<356&&e.y>=10&&e.y<46);
            frameDown_=!header&&e.x>=frameX_&&e.x<frameX_+FRAME_SIZE&&e.y>=frameY_&&e.y<frameY_+FRAME_SIZE;
            if(frameDown_){
                frameTouchX_=e.x;frameTouchY_=e.y;frameOriginX_=frameX_;frameOriginY_=frameY_;
                return Request::NONE;
            }
        }else if(frameDown_){
            const int dx=e.x-frameTouchX_,dy=e.y-frameTouchY_;
            if(dx*dx+dy*dy>64)frameDragging_=true;
            if(frameDragging_){
                const int x=std::clamp(frameOriginX_+dx,0,MAP_WIDTH-FRAME_SIZE);
                const int y=std::clamp(frameOriginY_+dy,0,MAP_HEIGHT-FRAME_SIZE);
                frameDirty_|=x!=frameX_||y!=frameY_;frameX_=x;frameY_=y;
            }
            // A crop gesture owns its release, even over shutter or back.
            if(e.type==T::UP){
                frameDown_=frameDragging_=false;
                // AmoledApp renders every release immediately; do not redraw it
                // again on the next coalesced MOVE tick.
                frameDirty_=false;
            }
            return Request::NONE;
        }
    }
    if(page_==Page::ALBUM&&scroll_.touch(e,{0,80,368,368},maximum(),offset_,tap_))return Request::NONE;
    if(page_==Page::DETAIL){
        if(e.type==T::DOWN&&e.y>=80&&e.y<400){swipeDown_=true;swipeX_=e.x;swipeY_=e.y;}
        if(swipeDown_&&e.type==T::MOVE&&std::abs(e.x-swipeX_)>12)tap_.cancel();
        if(e.type==T::UP&&swipeDown_){
            const int dx=e.x-swipeX_,dy=e.y-swipeY_;swipeDown_=false;
            if(std::abs(dx)>=40&&std::abs(dx)>std::abs(dy)){
                const int next=int(selected_)+(dx<0?1:-1);tap_.cancel();if(next>=0&&next<int(count()))show(unsigned(next));return Request::NONE;
            }
        }
    }
    const int activated=tap_.update(e,target(e.x,e.y));if(activated<0)return Request::NONE;
    if(activated==BACK){
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
