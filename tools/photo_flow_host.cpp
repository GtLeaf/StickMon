#include "presentation/PhotoFlow.h"
#include "../firmware/amoled_1_8_v1/main/PhotoSession.h"
#include "core/AudioManager.h"
#include "platform/desktop/DesktopPlatform.h"
#include "platform/common/PosixUserFiles.h"
#include "presentation/Canvas565.h"
#include <cassert>
#include <cstdio>
#include <type_traits>
#include <vector>

// Saving still yields through the production flow; this focused host has no audio.
static unsigned audioYields=0;
AudioManager& AudioManager::ins(){static AudioManager a;return a;}
void AudioManager::update(){++audioYields;}
using Flow=Ui::PhotoFlow;
static_assert(std::is_same<AmoledV1::PhotoSession,Flow>::value,"AMOLED alias must retain shared flow");
struct Files:Platform::PosixUserFiles {
    using PosixUserFiles::PosixUserFiles;
    bool failWrite=false;
    bool writeNew(const char* name,const void* bytes,size_t length,Platform::FileYield yield=nullptr)override{
        return !failWrite&&PosixUserFiles::writeNew(name,bytes,length,yield);
    }
};
using T=Platform::IInputDevice::TouchType;
static uint32_t now=0;
static Flow::Request event(Flow& f,T t,int x,int y){return f.touch({t,int16_t(x),int16_t(y),now+=20});}
static Flow::Request tap(Flow& f,Flow::Rect r){event(f,T::DOWN,r.x+r.width/2,r.y+r.height/2);return event(f,T::UP,r.x+r.width/2,r.y+r.height/2);}
static void rect(Flow::Rect r,int x,int y,int w,int h){assert(r.x==x&&r.y==y&&r.width==w&&r.height==h);assert(r.contains(x,y)&&!r.contains(x+w,y));}
static void corners(const Flow& f){
    for(int x:{f.frameX(),f.frameX()+f.frameSize()-1})for(int y:{f.frameY(),f.frameY()+f.frameSize()-1}){
        const double dx=x-179.5,dy=y-179.5;assert(dx*dx+dy*dy<=180.0*180.0);
    }
}
static void resizeGesture(Flow& f,int dx,int dy){
    const int x=f.frameX()+f.frameSize()-22,y=f.frameY()+f.frameSize()-22;
    event(f,T::DOWN,x,y);
    event(f,T::MOVE,x+dx,y+dy);
    assert(event(f,T::UP,x+dx,y+dy)==Flow::Request::NONE);
}
static void checkResize(Flow& f){
    f.openCamera(0,1);
    const int cx=f.geometry().width/2,cy=f.geometry().mapHeight/2;
    assert(f.frameSize()==224);
    resizeGesture(f,-15,-16);
    assert(f.frameSize()==193 && f.frameX()==cx-97 && f.frameY()==cy-97);
    resizeGesture(f,-35,-35);
    assert(f.frameSize()==160 && f.frameX()==cx-80 && f.frameY()==cy-80);
    // Grow after panning the small frame toward the upper-left perimeter.
    event(f,T::DOWN,cx,cy);
    event(f,T::UP,cx-105,cy-105);
    resizeGesture(f,44,44);
    assert(f.frameSize()==240);
    if(f.geometry().circular)corners(f);
    else assert(f.frameX()==0 && f.frameY()==0);
    const int gx=f.frameX()+220,gy=f.frameY()+220;
    event(f,T::DOWN,gx,gy);
    event(f,T::MOVE,gx-8,gy);
    assert(f.frameSize()==240 && !f.update(66));
    event(f,T::MOVE,gx-20,gy);
    assert(f.frameSize()==220 && f.update(66));
    event(f,T::CANCEL,gx-20,gy);
    assert(event(f,T::UP,gx-40,gy)==Flow::Request::NONE && f.frameSize()==220);
    f.openCamera(0,1);
    assert(f.frameSize()==224 && f.frameX()==cx-112 && f.frameY()==cy-112);
}
int main(int argc,char**argv){
    assert(argc==2);DesktopPlatform platform;Files files(argv[1]);
    auto& services=platform.serviceBundle();services.userFiles=&files;Platform::bind(services);
    Flow f({360,360,360,true});
    checkResize(f);
    assert(f.geometry().circular&&f.frameSize()==224);
    rect(f.backBounds(),84,24,44,44);rect(f.shutterBounds(),148,302,64,44);
    rect(f.retakeBounds(),88,288,88,44);rect(f.saveBounds(),184,288,88,44);
    rect(f.trashBounds(),232,24,44,44);rect(f.cancelBounds(),76,208,100,44);rect(f.deleteBounds(),184,208,100,44);
    rect(f.albumBounds(),60,80,240,224);rect(f.thumbBounds(0),70,84,104,104);rect(f.thumbBounds(1),186,84,104,104);
    f.openCamera(2,7);assert(f.frameX()==68&&f.frameY()==68);corners(f);
    // Preserve finger offset and ignore an exact eight-pixel displacement.
    event(f,T::DOWN,101,111);event(f,T::MOVE,109,111);assert(f.frameX()==68&&!f.update(66));
    event(f,T::MOVE,111,111);assert(f.frameX()==78&&f.frameY()==68&&f.update(66)&&!f.update(66));
    event(f,T::UP,112,111);assert(f.frameX()==79&&!f.update(66));
    tap(f,f.backBounds());f.openCamera(2,7);
    // Exhaust every in-circle endpoint from an off-center press: all four
    // corners, rather than only the frame center, must remain on the display.
    for(int y=0;y<360;++y)for(int x=0;x<360;++x){
        const double dx=x-179.5,dy=y-179.5;if(dx*dx+dy*dy>32400)continue;
        event(f,T::DOWN,f.frameX()+33,f.frameY()+43);event(f,T::MOVE,x,y);corners(f);event(f,T::UP,x,y);assert(!f.update(66));
    }
    f.openCamera(2,7);event(f,T::DOWN,180,180);event(f,T::MOVE,180,210);const int before=f.frameY();
    event(f,T::CANCEL,0,0);assert(event(f,T::UP,180,324)==Flow::Request::NONE&&f.frameY()==before);
    event(f,T::DOWN,180,180);event(f,T::MOVE,0,0);assert(event(f,T::UP,180,324)==Flow::Request::NONE&&f.frameY()==before);
    event(f,T::DOWN,180,180);event(f,T::UP,360,180);assert(event(f,T::UP,180,324)==Flow::Request::NONE);
    // Crop owns its whole gesture; controls and count header keep priority.
    f.openCamera(2,7);event(f,T::DOWN,180,180);assert(event(f,T::UP,180,324)==Flow::Request::NONE);corners(f);
    event(f,T::DOWN,106,46);assert(event(f,T::UP,106,46)==Flow::Request::CLOSE_CAMERA);
    f.openCamera(2,7);event(f,T::DOWN,180,180);event(f,T::UP,180,30);
    assert(f.frameY()<68);const int headerX=f.frameX(),headerY=f.frameY();
    event(f,T::DOWN,254,46);event(f,T::MOVE,264,56);event(f,T::UP,264,56);
    assert(f.frameX()==headerX&&f.frameY()==headerY);
    assert(tap(f,f.backBounds())==Flow::Request::CLOSE_CAMERA);
    std::vector<uint16_t> pixels(360*360);Canvas565 canvas;
    canvas.attach({pixels.data(),360,360,false});
    for(int y=0;y<360;++y)for(int x=0;x<360;++x)canvas.drawPixel(x,y,uint16_t(y*360+x));
    // Capture and persisted image sample precisely the runtime frame.
    for(unsigned n=0;n<Photos::CAPACITY;++n){
        f.openCamera(2,7+n);event(f,T::DOWN,180,180);event(f,T::UP,190,180);
        resizeGesture(f,-32,-32);
        assert(f.frameSize()==160);
        const int fx=f.frameX(),fy=f.frameY();assert(tap(f,f.shutterBounds())==Flow::Request::CAPTURE);
        assert(f.capture(canvas,20+n)&&f.page()==Flow::Page::PREVIEW);
        for(unsigned y=0;y<192;++y)for(unsigned x=0;x<192;++x)assert(f.image()[y*192+x]==canvas.readPixel(fx+(2*x+1)*160/384,fy+(2*y+1)*160/384));
        if(!n){tap(f,f.retakeBounds());assert(f.viewfinder()&&f.frameSize()==160&&f.frameX()==fx&&f.frameY()==fy);assert(f.capture(canvas,20));}
        if(!n){
            files.failWrite=true;assert(tap(f,f.saveBounds())==Flow::Request::NONE);
            assert(f.page()==Flow::Page::PREVIEW&&f.image()&&f.count()==0&&f.error());files.failWrite=false;
        }
        assert(tap(f,f.saveBounds())==Flow::Request::CLOSE_CAMERA&&f.saved());
    }
    assert(audioYields>0);f.openCamera(2,99);assert(f.count()==5&&tap(f,f.shutterBounds())==Flow::Request::NONE&&f.error());
    f.openAlbum();assert(f.count()==5&&f.thumbnail(0));assert(tap(f,f.thumbBounds(0))==Flow::Request::NONE&&f.page()==Flow::Page::DETAIL&&f.image());
    event(f,T::DOWN,240,170);event(f,T::UP,120,170);assert(f.selected()==1);
    tap(f,f.trashBounds());assert(f.page()==Flow::Page::DELETE_CONFIRM);tap(f,f.cancelBounds());assert(f.page()==Flow::Page::DETAIL);
    tap(f,f.trashBounds());tap(f,f.deleteBounds());assert(f.count()==4&&f.page()==Flow::Page::ALBUM);
    event(f,T::DOWN,180,250);event(f,T::MOVE,180,150);event(f,T::UP,180,100);assert(f.offset()==12);
    f.openAlbum();assert(f.offset()==0); // four photos need 12px including padding
    // Album list hit testing stops at the viewport edge and gaps.
    event(f,T::DOWN,100,305);event(f,T::UP,100,305);assert(f.page()==Flow::Page::ALBUM);
    tap(f,f.thumbBounds(0));assert(f.page()==Flow::Page::DETAIL);
    f.close();assert(!f.active()&&!f.image());f.openCamera(2,1);assert(f.frameX()==68&&f.frameY()==68);
    Flow rectangular;checkResize(rectangular);
    std::puts("photo flow: round geometry, exhaustive crop corners, offset/threshold/cancel/coalescing, runtime capture, retake/reopen, five real-file photos, album swipe/delete/scroll OK");
}
