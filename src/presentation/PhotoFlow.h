#pragma once
#include "core/PhotoLibrary.h"
#include "core/TouchTap.h"
#include "ui/common/MarketListScroll.h"
class Canvas565;
namespace Ui {
class PhotoFlow {
public:
    struct Geometry {int width=368,height=448,mapHeight=344;bool circular=false;};
    struct Rect {
        int x,y,width,height;
        bool contains(int px,int py)const{return px>=x&&px<x+width&&py>=y&&py<y+height;}
    };
    PhotoFlow():PhotoFlow(Geometry{}){}
    explicit PhotoFlow(Geometry geometry);
    const Geometry& geometry()const{return geometry_;}
    int frameSize()const{return frameSize_;}
    Rect backBounds()const;
    Rect shutterBounds()const;
    static constexpr Rect ZOOM_OUT_BOUNDS{64,368,56,56};
    static constexpr Rect ZOOM_IN_BOUNDS{248,368,56,56};
    Rect retakeBounds()const;
    Rect saveBounds()const;
    Rect trashBounds()const;
    Rect cancelBounds()const;
    Rect deleteBounds()const;
    Rect albumBounds()const;
    Rect thumbBounds(unsigned index)const;
    enum class Page:uint8_t {CLOSED,VIEWFINDER,PREVIEW,ALBUM,DETAIL,DELETE_CONFIRM};
    enum class Request:uint8_t {NONE,CAPTURE,CLOSE_CAMERA,CLOSE_ALBUM};
    static constexpr int MAP_WIDTH=368,MAP_HEIGHT=344;
    static constexpr int MIN_FRAME_SIZE=160, DEFAULT_FRAME_SIZE=224, MAX_FRAME_SIZE=240;
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
    Geometry geometry_;
    Photos::Library library_;
    Photos::PixelBuffer image_,thumbs_;
    Page page_=Page::CLOSED;
    const char* error_=nullptr;
    unsigned selected_=0;int offset_=0;
    uint8_t area_=0;uint32_t day_=0;
    bool imageValid_=false,saved_=false,thumbValid_[Photos::CAPACITY]{};
    bool swipeDown_=false;int swipeX_=0,swipeY_=0;
    int frameSize_=DEFAULT_FRAME_SIZE;
    int frameX_,frameY_;
    int frameTouchX_=0,frameTouchY_=0,frameOriginX_=0,frameOriginY_=0;
    enum class FrameGesture { NONE, MOVE, RESIZE };
    FrameGesture frameGesture_=FrameGesture::NONE;
    int frameOriginSize_=0;
    bool frameDragging_=false,frameDirty_=false;
    TouchTap tap_;Ui::MarketListScroll scroll_;
    bool contains(int x,int y)const;
    void moveFrame(int x,int y);
    void resizeFrame(Rect origin,int delta);
    void thumbnails();
    void show(unsigned index);
    int target(int x,int y)const;
    int maximum()const;
    void error(Photos::Result result);
};
}
