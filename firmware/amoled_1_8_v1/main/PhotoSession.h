#pragma once
#include "core/PhotoLibrary.h"
#include "core/TouchTap.h"
#include "ui/common/MarketListScroll.h"
class Canvas565;
namespace AmoledV1 {
class PhotoSession {
public:
    enum class Page:uint8_t {CLOSED,VIEWFINDER,PREVIEW,ALBUM,DETAIL,DELETE_CONFIRM};
    enum class Request:uint8_t {NONE,CAPTURE,CLOSE_CAMERA,CLOSE_ALBUM};
    static constexpr int MAP_WIDTH=368,MAP_HEIGHT=344,FRAME_SIZE=256;
    int frameX()const{return frameX_;}
    int frameY()const{return frameY_;}
    bool active()const{return page_!=Page::CLOSED;}
    bool camera()const{return page_==Page::VIEWFINDER||page_==Page::PREVIEW;}
    bool viewfinder()const{return page_==Page::VIEWFINDER;}
    bool keepsDisplayAwake()const{return camera();}
    Page page()const{return page_;}
    unsigned count()const{return library_.count();}
    const Photos::Entry& entry(unsigned index)const{return library_.entry(index);}
    unsigned selected()const{return selected_;}
    const uint16_t* image()const{return imageValid_?image_.data():nullptr;}
    const uint16_t* thumbnail(unsigned index)const{return index<count()&&thumbValid_[index]?thumbs_.data()+index*Photos::THUMB_PIXELS:nullptr;}
    const char* error()const{return error_;}
    bool saved()const{return saved_;}
    int offset()const{return offset_;}
    bool moving()const{return scroll_.moving();}
    void refreshCatalogue(){library_.refresh();}
    void openCamera(uint8_t area,uint32_t day);
    void openAlbum();
    void close();
    void cancelTouch();
    Request touch(const Platform::IInputDevice::TouchEvent&);
    bool update(uint32_t elapsed);
    bool capture(const Canvas565&,uint32_t day);
private:
    Photos::Library library_;
    Photos::PixelBuffer image_,thumbs_;
    Page page_=Page::CLOSED;
    const char* error_=nullptr;
    unsigned selected_=0;int offset_=0;
    uint8_t area_=0;uint32_t day_=0;
    bool imageValid_=false,saved_=false,thumbValid_[Photos::CAPACITY]{};
    bool swipeDown_=false;int swipeX_=0,swipeY_=0;
    int frameX_=(MAP_WIDTH-FRAME_SIZE)/2,frameY_=(MAP_HEIGHT-FRAME_SIZE)/2;
    int frameTouchX_=0,frameTouchY_=0,frameOriginX_=0,frameOriginY_=0;
    bool frameDown_=false,frameDragging_=false,frameDirty_=false;
    TouchTap tap_;Ui::MarketListScroll scroll_;
    void thumbnails();
    void show(unsigned index);
    int target(int x,int y)const;
    int maximum()const;
    void error(Photos::Result result);
};
}
