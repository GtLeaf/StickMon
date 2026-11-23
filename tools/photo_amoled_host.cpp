#include "AmoledApp.h"
#include "core/FontResource.h"
#include "core/RoomResource.h"
#include "ui/UiCommon.h"
#include "game/MonsterFactory.h"
#include "game/ExploreRouteGeometry.h"
#include "platform/common/PosixUserFiles.h"
#include "platform/desktop/DesktopPlatform.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>
#include <chrono>
struct PhotoFiles:Platform::PosixUserFiles {
    using PosixUserFiles::PosixUserFiles;bool failWrite=false,failDelete=false;DesktopPlatform* desktop=nullptr;uint32_t delay=0;
    bool writeNew(const char*n,const void*p,size_t len,Platform::FileYield yield=nullptr)override{if(delay){desktop->sleepMs(delay);delay=0;}return !failWrite&&PosixUserFiles::writeNew(n,p,len,yield);}
    bool remove(const char*n)override{return !failDelete&&PosixUserFiles::remove(n);}
};

struct PhotoPower final : Platform::IPowerDevice {
    explicit PhotoPower(Platform::IPowerDevice& power) : power(power) {}
    Platform::IPowerDevice& power;
    int batteryLevel() override { return power.batteryLevel(); }
    Platform::WakeReason wakeReason() const override { return power.wakeReason(); }
    uint32_t hardwareRandom() override { return power.hardwareRandom(); }
    size_t externalMemorySize() const override { return 8 * 1024 * 1024; }
    size_t externalMemoryFree() const override { return externalMemorySize(); }
    void restart() override { power.restart(); }
    void enterDeepSleep(uint64_t duration, bool external) override { power.enterDeepSleep(duration, external); }
};
namespace AmoledV1 {
struct AmoledAppTestAccess {
static void run(DesktopPlatform& desktop,PhotoFiles& files,const char* output){
    using P=PhotoSession::Page;using S=AppSceneFlow::Scene;
    desktop.sleepMs(1000);AmoledApp app;app.begin(desktop.millis());app.gameState.oobeDone=true;app.gameState.teamCount=2;
    app.gameState.team[0]=Game::MonsterFactory::create(25,20);app.gameState.team[1]=Game::MonsterFactory::create(1,20);
    app.gameState.team[0].satiety=app.gameState.team[1].satiety=100;
    app.selectedExploreArea=0;assert(app.startExploreRoute(desktop.millis()));
    // Resume a mid-route fixture so rendered photo subjects are fully visible.
    auto& route=const_cast<ExploreSession::State&>(app.exploreSession_.state());
    const auto& path=route.exploreRouteMap.paths[route.exploreRoutePath];
    route.exploreRouteIndex=uint8_t(path.pointCount/2);route.exploreRouteSteps=route.exploreRouteIndex;
    app.resetExploreMapPresentation(desktop.millis());
    auto& visual=const_cast<ExplorePresenter::State&>(app.explorePresenter_.state());
    const auto trailing=ExploreRouteGeometry::pathPoint(path,route.exploreRouteIndex?route.exploreRouteIndex-1:0);
    visual.exploreRouteFollowerWorldX=visual.exploreRouteFollowerFromX=visual.exploreRouteFollowerTargetX=trailing.x;
    visual.exploreRouteFollowerWorldY=visual.exploreRouteFollowerFromY=visual.exploreRouteFollowerTargetY=trailing.y;
    app.sceneFlow.enter(AppSceneFlow::Scene::EXPLORE_ROUTE);
    uint32_t time=desktop.millis();
    auto event=[&](TouchEventType type,int x,int y){desktop.sleepMs(40);time=desktop.millis();app.handleTouch({type,int16_t(x),int16_t(y),int16_t(x),int16_t(y),time});};
    auto tap=[&](int x,int y){event(TouchEventType::DOWN,x,y);event(TouchEventType::UP,x,y);};
    auto openCamera=[&](){
        tap(120,396);assert(app.sceneFlow.current()==S::EXPLORE_MENU);
        tap(308,52);
        assert(app.sceneFlow.current()==S::EXPLORE_ROUTE&&app.photoFlow_.viewfinder()&&"First menu row must open camera");
    };
    auto shot=[&](const char* name){
        app.forceFullRender();app.render(PixelRenderer::canvas());app.markRendered();
        const auto path=std::string(output)+"/"+name+".ppm";FILE*f=std::fopen(path.c_str(),"wb");assert(f);std::fprintf(f,"P6\n368 448\n255\n");
        auto& c=PixelRenderer::canvas();for(int y=0;y<448;++y)for(int x=0;x<368;++x){auto p=c.readPixel(x,y);const unsigned char rgb[]={uint8_t(((p>>11)&31)*255/31),uint8_t(((p>>5)&63)*255/63),uint8_t((p&31)*255/31)};std::fwrite(rgb,1,3,f);}std::fclose(f);
    };
    auto bands=[&](){
        auto& c=PixelRenderer::canvas();auto* original=c.rawPixels();
        for(bool swap:{false,true}){
            std::vector<uint16_t> pixels(368*448),full;c.attach({pixels.data(),368,448,swap});app.forceFullRender();app.render(c);for(int y=0;y<448;++y)for(int x=0;x<368;++x)full.push_back(c.readPixel(x,y));
            c.fillSprite(0x1234);for(int y=0;y<448;y+=13){app.forceRenderRegion(0,y,368,std::min(448,y+13));app.render(c);}for(int y=0;y<448;++y)for(int x=0;x<368;++x)assert(c.readPixel(x,y)==full[y*368+x]);
        }
        c.attach({original,368,448,false});app.forceFullRender();
    };
    const auto before=app.exploreRouteModel();const auto fingerprint=ExploreMapGenerator::fingerprint(*before.map);
    shot("01-entry");
    tap(44,396);
    assert(app.sceneFlow.current()==S::EXPLORE_ROUTE && app.photoFlow_.viewfinder());
    assert(app.photoFlow_.page()==P::VIEWFINDER&&app.exploreSession_.state().exploreRoutePaused);
    app.gameState.settings.idleTimeoutIndex=0;
    event(TouchEventType::DOWN,36,28);
    desktop.sleepMs(40);
    const auto backLoopTime=desktop.millis();
    // Like a hardware loop, sample the event time before processing input.
    // The clock read while closing the camera can be a millisecond later.
    desktop.sleepMs(1);
    app.handleTouch({TouchEventType::UP,36,28,36,28,backLoopTime});
    assert(!app.photoFlow_.active()&&!app.exploreSession_.state().exploreRoutePaused);
    app.update(backLoopTime);
    if(app.consumeLockRequest()) app.prepareDisplayLock(backLoopTime);
    assert(app.sceneFlow.current()==S::EXPLORE_ROUTE&&app.exploreSessionActive&&
           app.exploreTransition_.state().phase==AmoledApp::ExpeditionDeparturePhase::NONE&&
           "Viewfinder back must stay in the expedition when the update time predates the tap handler");
    const auto careTime=app.lastCareMs;
    const auto gameMinutes=app.gameState.gameMinutesTotal;
    app.gameState.settings.idleTimeoutIndex=4;
    app.lastInteractionMs=desktop.millis()-0x80000010u;
    app.update(desktop.millis());
    assert(app.lastCareMs==careTime&&app.gameState.gameMinutesTotal==gameMinutes&&
           "An old interaction with idle timeout disabled must not rewind the update clock");
    app.gameState.settings.idleTimeoutIndex=0;
    tap(44,396);
    assert(app.photoFlow_.viewfinder());
    shot("02-viewfinder");bands();event(TouchEventType::UP,184,396);assert(app.photoFlow_.page()==P::VIEWFINDER);
    event(TouchEventType::DOWN,184,396);app.cancelTouch();event(TouchEventType::UP,184,396);assert(app.photoFlow_.page()==P::VIEWFINDER);
    desktop.sleepMs(31000);app.update(desktop.millis());assert(!app.consumeLockRequest());
    // Geometry is asserted through production pixels, including off-center frames.
    auto frameAt=[&](int x,int y,bool force=true,int size=224){
        if(force)app.forceFullRender();app.render(PixelRenderer::canvas());app.markRendered();
        const auto ink=UiCommon::rgb(226,238,233);auto& canvas=PixelRenderer::canvas();
        assert(canvas.readPixel(x,y)==ink&&"Expected crop top-left corner");
        assert(canvas.readPixel(x+size-1,y+size-1)==ink&&"Expected crop bottom-right corner");
        assert(canvas.readPixel(x+size-11,y+size-5)==ink&&"Expected resize grip");
    };
    frameAt(72,60);shot("14-frame-centered");
    const auto symbolInk=UiCommon::rgb(226,238,233);
    auto& canvas=PixelRenderer::canvas();
    for(int x : {92,276}){
        assert(canvas.readPixel(x-8,396)==symbolInk);
        assert(canvas.readPixel(x+8,388)==0);
    }
    assert(canvas.readPixel(92,388)==0 && canvas.readPixel(276,388)==symbolInk);
    tap(276,396);frameAt(80,68,false,208);
    tap(92,396);frameAt(72,60,false);
    // Sub-threshold jitter and a gesture starting outside the crop do nothing.
    event(TouchEventType::DOWN,184,172);event(TouchEventType::MOVE,189,177);event(TouchEventType::UP,189,177);frameAt(72,60);
    event(TouchEventType::DOWN,20,172);event(TouchEventType::MOVE,120,220);event(TouchEventType::UP,120,220);frameAt(72,60);
    // Starting away from the center keeps the finger's original crop offset.
    event(TouchEventType::DOWN,120,130);frameAt(72,60);
    event(TouchEventType::MOVE,136,140);assert(!app.needsRender()&&"MOVE must be coalesced");
    event(TouchEventType::MOVE,144,144);assert(!app.needsRender());
    desktop.sleepMs(66);app.update(desktop.millis());
    assert(app.needsRender()&&app.dirtyRowBegin==0&&app.dirtyRowEnd==344);
    // Verify actual pending rows before UP can mask a stale MOVE composition.
    frameAt(96,74,false);
    event(TouchEventType::UP,152,150);assert(app.needsRender());frameAt(104,80,false);
    assert(!app.photoFlow_.update(66)&&"Released crop must not queue a duplicate redraw");
    // Drag to upper-left, with no automatic recentering or inertia.
    event(TouchEventType::DOWN,184,172);event(TouchEventType::MOVE,64,62);event(TouchEventType::UP,64,62);frameAt(0,0);
    shot("15-frame-upper-left");bands();desktop.sleepMs(66);app.update(desktop.millis());frameAt(0,0);
    // Back remains a button when the crop is underneath it; reopening resets crop.
    tap(36,28);assert(!app.photoFlow_.active());openCamera();frameAt(72,60);
    // Reaching all four sides clamps to the viewport, never the104px control bar.
    event(TouchEventType::DOWN,184,172);event(TouchEventType::MOVE,4,4);event(TouchEventType::UP,4,4);frameAt(0,0);
    event(TouchEventType::DOWN,100,100);event(TouchEventType::MOVE,300,100);event(TouchEventType::UP,300,100);frameAt(144,0);
    event(TouchEventType::DOWN,200,100);event(TouchEventType::MOVE,200,343);event(TouchEventType::UP,200,343);frameAt(144,120);
    event(TouchEventType::DOWN,200,200);event(TouchEventType::MOVE,4,200);event(TouchEventType::UP,4,200);frameAt(0,120);
    // CANCEL/outside-screen release end drag; subsequent MOVE/UP cannot revive it.
    event(TouchEventType::DOWN,100,200);event(TouchEventType::MOVE,140,180);frameAt(40,100);
    using InputType=Platform::IInputDevice::TouchType;
    assert(app.photoFlow_.touch({InputType::CANCEL,140,180,desktop.millis()})==PhotoSession::Request::NONE);
    event(TouchEventType::MOVE,200,220);event(TouchEventType::UP,184,396);assert(app.photoFlow_.viewfinder());frameAt(40,100);
    event(TouchEventType::DOWN,140,180);event(TouchEventType::MOVE,156,180);frameAt(56,100);
    event(TouchEventType::UP,-1,180);event(TouchEventType::MOVE,200,220);event(TouchEventType::UP,184,396);frameAt(56,100);
    // Dragging onto shutter consumes the gesture and keeps the map camera fixed.
    event(TouchEventType::DOWN,184,172);event(TouchEventType::MOVE,300,300);event(TouchEventType::UP,300,300);frameAt(144,120);
    event(TouchEventType::DOWN,184,172);event(TouchEventType::MOVE,184,396);event(TouchEventType::UP,184,396);
    assert(app.photoFlow_.viewfinder());frameAt(144,120);shot("16-frame-lower-right");bands();
    // Drag the corner from inside the frame, then photograph the smaller crop.
    event(TouchEventType::DOWN,346,322);event(TouchEventType::MOVE,334,310);
    event(TouchEventType::UP,334,310);frameAt(156,132,true,200);
    tap(276,396);frameAt(164,140,false,184);
    const auto moved=app.exploreRouteModel();
    assert(moved.cameraX==before.cameraX&&moved.cameraY==before.cameraY&&moved.worldX==before.worldX&&moved.worldY==before.worldY);
    std::vector<uint16_t> expected(Photos::PIXELS);
    app.forceFullRender();app.renderPhotoWorld(PixelRenderer::canvas(),0,448);
    for(unsigned y=0;y<192;++y)for(unsigned x=0;x<192;++x)expected[y*192+x]=PixelRenderer::canvas().readPixel(164+int((2*x+1)*184/384),140+int((2*y+1)*184/384));
    shot("02-viewfinder");const auto originalMinutes=app.gameState.gameMinutesTotal;app.gameState.gameMinutesTotal+=1440;
    const auto capturedDay=app.gameState.gameMinutesTotal/1440+1;
    const auto captureStarted=std::chrono::steady_clock::now();
    tap(184,396);app.gameState.gameMinutesTotal=originalMinutes;
    const auto captureUs=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-captureStarted).count();
    assert(app.photoFlow_.page()==P::PREVIEW&&std::equal(expected.begin(),expected.end(),app.photoFlow_.image()));shot("03-preview");shot("17-moved-capture");
    tap(100,420);assert(app.photoFlow_.viewfinder());frameAt(164,140,true,184);
    // No MOVE in a new gesture: the release itself still carries final drag position.
    event(TouchEventType::DOWN,184,172);event(TouchEventType::UP,172,162);frameAt(152,130,true,184);
    event(TouchEventType::DOWN,184,172);event(TouchEventType::UP,196,182);frameAt(164,140,true,184);
    app.gameState.gameMinutesTotal+=1440;tap(184,396);app.gameState.gameMinutesTotal=originalMinutes;
    assert(app.photoFlow_.page()==P::PREVIEW&&std::equal(expected.begin(),expected.end(),app.photoFlow_.image()));
    app.update(desktop.millis());shot("03-preview");bands();shot("03-preview");
    for(int i=0;i<3;++i){desktop.sleepMs(66);app.update(desktop.millis());assert(!app.needsRender()&&"Static photo preview must park");}
    files.failWrite=true;tap(268,420);files.failWrite=false;assert(app.photoFlow_.page()==P::PREVIEW&&app.photoFlow_.error()&&app.photoFlow_.count()==0);assert(std::equal(expected.begin(),expected.end(),app.photoFlow_.image()));shot("04-save-failed");
    files.delay=1100;const auto saveStarted=std::chrono::steady_clock::now();tap(268,420);const auto saveUs=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-saveStarted).count();assert(!app.photoFlow_.active()&&app.sceneFlow.current()==S::EXPLORE_ROUTE&&!app.exploreSession_.state().exploreRoutePaused);
    const auto after=app.exploreRouteModel();assert(ExploreMapGenerator::fingerprint(*after.map)==fingerprint&&after.worldX==before.worldX&&after.worldY==before.worldY&&after.cameraX==before.cameraX&&after.cameraY==before.cameraY);
    Photos::Library library;assert(library.refresh()==Photos::Result::OK&&library.count()==1);assert(library.entry(0).day==capturedDay&&"Photo date must be the shutter day, not viewfinder entry day");std::vector<uint16_t> saved(Photos::PIXELS);assert(library.loadImage(library.entry(0),saved.data())==Photos::Result::OK&&saved==expected);
    // The original exploration menu still provides the bag.
    tap(120,396);assert(app.sceneFlow.current()==S::EXPLORE_MENU);
    tap(308,172);assert(app.sceneFlow.current()==S::BAG);
    app.closeItemScene();assert(app.sceneFlow.current()==S::EXPLORE_MENU);
    tap(120,396);assert(app.sceneFlow.current()==S::EXPLORE_ROUTE);
    openCamera();tap(184,396);assert(app.photoFlow_.page()==P::PREVIEW);tap(100,420);assert(app.photoFlow_.viewfinder());tap(36,28);assert(!app.photoFlow_.active()&&library.count()==1);
    for(int i=0;i<3;++i)assert(library.save(expected.data(),uint8_t(i),12+i)==Photos::Result::OK);
    app.openComputerScene();shot("05-computer");tap(120,290);assert(app.photoFlow_.page()==P::ALBUM&&app.photoFlow_.count()==4);shot("06-album");
    tap(90,140);assert(app.photoFlow_.page()==P::DETAIL&&app.photoFlow_.image());shot("07-detail");
    event(TouchEventType::DOWN,220,220);event(TouchEventType::UP,-20,220);assert(app.photoFlow_.selected()==0&&"Out-of-panel release must cancel a photo swipe");
    event(TouchEventType::DOWN,240,220);event(TouchEventType::MOVE,120,220);event(TouchEventType::UP,120,220);assert(app.photoFlow_.selected()==1);
    tap(330,36);assert(app.photoFlow_.page()==P::DELETE_CONFIRM);shot("08-delete");tap(100,250);assert(app.photoFlow_.page()==P::DETAIL);
    tap(330,36);files.failDelete=true;tap(260,250);files.failDelete=false;assert(app.photoFlow_.page()==P::DELETE_CONFIRM&&app.photoFlow_.count()==4);tap(260,250);assert(app.photoFlow_.page()==P::ALBUM&&app.photoFlow_.count()==3);
    // Static screens obey dirty bands and both stored framebuffer byte orders.
    bands();app.markRendered();desktop.sleepMs(31000);app.update(desktop.millis());assert(app.consumeLockRequest()&&"Album must retain normal automatic lock policy");
    tap(36,36);assert(!app.photoFlow_.active()&&app.sceneFlow.current()==S::COMPUTER);
    PhotoSession reboot;reboot.openAlbum();assert(reboot.count()==3);reboot.close();
    assert(library.refresh()==Photos::Result::OK);while(library.count()<5)assert(library.save(expected.data(),0,50)==Photos::Result::OK);
    app.photoFlow_.refreshCatalogue();shot("10-computer-full");
    tap(120,290);assert(app.photoFlow_.page()==P::ALBUM&&app.photoFlow_.count()==5);
    shot("11-album-full");tap(90,140);shot("12-detail-full");tap(330,36);shot("13-delete-full");tap(100,250);tap(36,36);
    event(TouchEventType::DOWN,100,350);event(TouchEventType::MOVE,100,270);event(TouchEventType::UP,100,270);
    assert(app.photoFlow_.page()==P::ALBUM&&app.photoFlow_.offset()>0&&app.photoFlow_.moving());
    desktop.sleepMs(66);app.update(desktop.millis());assert(app.photoFlow_.page()==P::ALBUM);shot("09-album-scroll");
    app.cancelTouch();event(TouchEventType::UP,100,270);assert(app.photoFlow_.page()==P::ALBUM&&!app.photoFlow_.moving());tap(36,36);assert(!app.photoFlow_.active());
    app.sceneFlow.enter(S::EXPLORE_ROUTE);openCamera();assert(app.photoFlow_.viewfinder()&&app.photoFlow_.count()==5);tap(184,396);assert(app.photoFlow_.viewfinder()&&app.photoFlow_.error());
    assert(!app.prepareDisplayLock(desktop.millis())&&!app.photoFlow_.active());assert(library.refresh()==Photos::Result::OK&&library.count()==5);

    // Exercise the placed frame through the real HOME touch route.
    app.exploreTransition_.reset();
    app.exploreSessionActive=false;
    app.sceneFlow.enter(S::HOME);
    assert(RoomResource::ins().available());
    app.homeMainActor.x=193;
    app.homeMainActor.y=RoomResource::ins().roomY()+108;
    app.updateCamera();
    const int frameX=app.worldToScreenX(203);
    const int frameY=app.worldToScreenY(RoomResource::ins().roomY()+59.5f);
    assert(frameX>=0&&frameX<368&&frameY>=48&&frameY<344);
    app.gameState.room.ownedFurniture|=1;
    app.gameState.room.placedFurniture|=1;
    shot("17-wall-frame-blank");
    const auto blank=PixelRenderer::canvas().readPixel(frameX,frameY);
    event(TouchEventType::DOWN,frameX,frameY);app.cancelTouch();event(TouchEventType::UP,frameX,frameY);
    tap(184,365);tap(184,365);
    uint64_t binding=0;
    assert(library.readFrameBinding(binding)==Photos::Result::OK&&!binding);
    tap(frameX,frameY);assert(app.homePhotoFrame_.active());shot("18-wall-frame-preview");
    tap(184,344);shot("19-wall-frame-picker");
    auto& panel=PixelRenderer::canvas();
    const auto rightRail=panel.readPixel(249,216);
    event(TouchEventType::DOWN,245,216);
    event(TouchEventType::MOVE,181,216);
    assert(app.needsRender() && "Photo carousel must render during finger movement");
    app.render(panel);app.markRendered();
    assert(panel.readPixel(249,216)!=rightRail);
    shot("19a-frame-half-drag");
    event(TouchEventType::MOVE,245,216);
    event(TouchEventType::UP,245,216);
    app.render(panel);app.markRendered();
    assert(panel.readPixel(249,216)==rightRail);
    tap(184,344);
    assert(library.readFrameBinding(binding)==Photos::Result::OK&&binding==library.entry(0).id);
    shot("20-wall-frame-selected");bands();
    tap(184,64);shot("21-wall-frame-mounted");
    assert(PixelRenderer::canvas().readPixel(frameX,frameY)!=blank);
    tap(frameX,frameY);tap(184,344);shot("21a-frame-picker");
    tap(314,216);
    desktop.sleepMs(42);app.update(desktop.millis());app.render(panel);app.markRendered();
    assert(panel.readPixel(249,216)!=rightRail);
    desktop.sleepMs(160);app.update(desktop.millis());app.render(panel);app.markRendered();
    assert(panel.readPixel(249,216)==rightRail);
    tap(184,344);tap(184,64);
    assert(library.readFrameBinding(binding)==Photos::Result::OK&&binding==library.entry(1).id);
    std::puts("photo AMOLED: placed wall frame opens, selects, changes and displays the durable photo; cancel and dirty bands OK");
    std::puts("photo AMOLED: actual shortcut/clean capture/keep-awake/park/retry/slow write/reload/route return/bag/album/swipe/delete/dirty bands/byte order OK");
    std::printf("native host timing only: clean capture%lldus, save%lldus (not device measurements)\n",static_cast<long long>(captureUs),static_cast<long long>(saveUs));
}
};
}
int main(int argc,char**argv){
    assert(argc==3);DesktopPlatform desktop(argv[1]);auto& base=desktop.serviceBundle();
    PhotoPower power(base.power);
    Platform::Services services{base.lifecycle,base.clock,base.logger,base.input,base.display,
        base.audio,base.imu,power,base.blobs,base.resources,base.memory,base.peers};
    const auto path=std::string(argv[2])+"/photos";PhotoFiles files(path);files.desktop=&desktop;services.userFiles=&files;
    Platform::bind(services);assert(desktop.begin()&&Platform::resources().mount());assert(GameAssets::begin()&&FontResource::ins().begin());
    std::vector<uint16_t> pixels(368*448);auto& c=PixelRenderer::canvas();c.attach({pixels.data(),368,448,false});c.setCoordinateScale(1);c.setLayoutScale(1);c.setAssetScale(1);c.setNativeText(true);
    AmoledV1::AmoledAppTestAccess::run(desktop,files,argv[2]);
}
