#include <algorithm>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include "platform/common/PosixUserFiles.h"
#include "platform/desktop/DesktopPlatform.h"
#define private public
#include "core/GameEngine.h"
#include "scenes/ExploreScene.h"
#undef private
#include "core/FontResource.h"
#include "core/UiStrings.h"
#include "ui/round_touch/ExploreRouteUi.h"
#include "ui/round_touch/PhotoUi.h"
#include "ui/ApplicationUi.h"
#include "game/MonsterFactory.h"
#include "game/ExploreRouteGeometry.h"
#include "ui/SceneSurface.h"
#include "presentation/PixelRenderer.h"
extern "C" unsigned int uzlib_crc32(const void*,unsigned int,unsigned int){assert(false);return 0;}
extern "C" unsigned int uzlib_adler32(const void*,unsigned int,unsigned int){assert(false);return 0;}

struct TouchInput:Platform::IInputDevice {
    void update()override{} bool pressed(Platform::InputButton)const override{return false;}
    bool touchAvailable()const override{return true;}
};
struct PhotoFiles:Platform::PosixUserFiles {
    using PosixUserFiles::PosixUserFiles;
    bool failWrite=false,failDelete=false;DesktopPlatform* clock=nullptr;uint32_t delay=0;
    bool writeNew(const char*n,const void*p,size_t size,Platform::FileYield yield=nullptr)override{
        if(delay){clock->advanceMs(delay);delay=0;}return !failWrite&&PosixUserFiles::writeNew(n,p,size,yield);
    }
    bool remove(const char*n)override{return !failDelete&&PosixUserFiles::remove(n);}
};
int main(int argc,char**argv){
    assert(argc==3);DesktopPlatform desktop(argv[1]);auto& base=desktop.serviceBundle();TouchInput input;
    PhotoFiles files(std::string(argv[2])+"/photos");files.clock=&desktop;
    Platform::Services services{base.lifecycle,base.clock,base.logger,input,base.display,base.audio,base.imu,base.power,base.blobs,base.resources,base.memory,base.peers,&files};
    Platform::bind(services);assert(desktop.begin());
    auto& engine=GameEngine::ins();assert(engine.begin());auto& state=engine.gameState();
    state.oobeDone=true;state.teamCount=2;state.team[0]=Game::MonsterFactory::create(25,20);state.team[1]=Game::MonsterFactory::create(1,20);
    state.team[0].satiety=state.team[1].satiety=100;
    ExploreScene route;route.onEnter();route.resetWalk();route.autoWalkActive=false;route.routeMoving=false;
    const auto& path=route.generatedMap.paths[route.currentRoutePath];route.routeIndex=uint8_t(path.pointCount/2);
    const auto point=ExploreRouteGeometry::pathPoint(path,route.routeIndex);
    route.routeWorldX=route.routeFromX=route.routeTargetX=point.x;
    route.routeWorldY=route.routeFromY=route.routeTargetY=point.y;
    const auto follower=ExploreRouteGeometry::pathPoint(path,route.routeIndex>1?route.routeIndex-2:0);
    route.routeFollowerWorldX=route.routeFollowerFromX=route.routeFollowerTargetX=follower.x;
    route.routeFollowerWorldY=route.routeFollowerFromY=route.routeFollowerTargetY=follower.y;
    const auto target=ExploreRouteGeometry::pathPoint(path,route.routeIndex+1);++route.routeIndex;
    route.routeTargetX=target.x;route.routeTargetY=target.y;route.routeMoving=route.autoWalkActive=true;
    route.routeMoveDurationMs=route.routeLeaderMoveDurationMs=route.routeFollowerMoveDurationMs=1000;
    route.routeMoveStarted=desktop.millis()-50;
    auto& canvas=PixelRenderer::canvas();std::vector<uint16_t> frame(360*360);
    canvas.attach({frame.data(),360,360,true});canvas.setCoordinateScale(1);canvas.setLayoutScale(1);canvas.setAssetScale(1);
    Ui::SceneSurface::select(SceneID::EXPLORE);route.prepareSurface();
    auto shot=[&](const char* name,auto& scene){
        scene.prepareSurface();assert(Ui::SceneSurface::begin({frame.data(),360,360,true}));scene.render();if(scene.touchNavigation())Ui::ApplicationUi{}.renderNavigation();Ui::SceneSurface::finish();const auto path=std::string(argv[2])+"/"+name+".ppm";
        auto* file=std::fopen(path.c_str(),"wb");assert(file);std::fprintf(file,"P6\n360 360\n255\n");
        for(int y=0;y<360;++y)for(int x=0;x<360;++x){const auto value=Ui::RoundExplore::routePoint(x,y)?canvas.readPixel(x,y):0;
            const unsigned char rgb[]={uint8_t(((value>>11)&31)*255/31),uint8_t(((value>>5)&63)*255/63),uint8_t((value&31)*255/31)};std::fwrite(rgb,1,3,file);
        }std::fclose(file);
    };
    auto clipped=[&](const Ui::PhotoFlow& flow){
        for(bool swap:{false,true}){
            std::vector<uint16_t> pixels(360*360),expected;canvas.attach({pixels.data(),360,360,swap});Ui::RoundPhotoUi::render(flow);
            for(int y=0;y<360;++y)for(int x=0;x<360;++x)expected.push_back(canvas.readPixel(x,y));
            for(const auto clip:{Ui::PhotoFlow::Rect{0,121,360,1},Ui::PhotoFlow::Rect{179,0,1,360}}){
                canvas.clearClipRect();canvas.fillSprite(0x1234);canvas.setClipRect(clip.x,clip.y,clip.width,clip.height);Ui::RoundPhotoUi::render(flow);canvas.clearClipRect();
                for(int y=0;y<360;++y)for(int x=0;x<360;++x)assert(canvas.readPixel(x,y)==(clip.contains(x,y)?expected[y*360+x]:0x1234));
            }
        }
        canvas.attach({frame.data(),360,360,true});
    };
    shot("01-explore",route);
    auto event=[&](Platform::IInputDevice::TouchType type,int x,int y){desktop.advanceMs(40);route.onTouch({type,int16_t(x),int16_t(y),desktop.millis()});};
    using T=Platform::IInputDevice::TouchType;
    event(T::DOWN,180,82);event(T::UP,180,82);
    assert(route.keepsDisplayAwake()&&"Round camera entry must open awake viewfinder");
    const auto pauseElapsed=route.photoPauseAt_-route.routeMoveStarted;
    using P=Ui::PhotoFlow::Page;assert(route.photo_.viewfinder());assert(route.routePausedForTouch);
    assert(route.photo_.frameX()==68&&route.photo_.frameY()==68);shot("02-viewfinder",route);
    event(T::DOWN,180,180);event(T::MOVE,158,175);event(T::UP,158,175);
    assert(route.photo_.frameX()==46&&route.photo_.frameY()==63);route.render();
    assert(canvas.readPixel(269,63)==PixelRenderer::rgb(226,238,233)&&"Capacity badge must not obscure a visible crop corner");
    event(T::DOWN,158,175);event(T::MOVE,180,180);event(T::UP,180,180);
    const auto originalX=route.routeWorldX,originalY=route.routeWorldY;const auto originalIndex=route.routeIndex;
    std::vector<ExploreEcology::Pose> wildlife;
    for(unsigned i=0;i<route.ui.ecology.count();++i)wildlife.push_back(route.ui.ecology.pose(i));
    bool wildlifeChanged=false;
    for(int tick=0;tick<100;++tick){desktop.advanceMs(66);route.update(desktop.millis(),.066f);
        for(unsigned i=0;i<wildlife.size();++i)wildlifeChanged|=!ExploreEcology::samePose(wildlife[i],route.ui.ecology.pose(i));
        assert(route.routeWorldX==originalX&&route.routeWorldY==originalY&&route.routeIndex==originalIndex);
    }
    assert(wildlifeChanged&&"Wildlife must remain live during viewfinder");
    event(T::DOWN,180,180);event(T::MOVE,220,190);event(T::UP,220,190);
    assert(route.photo_.frameX()>68);shot("03-moved-viewfinder",route);
    const int fx=route.photo_.frameX(),fy=route.photo_.frameY();
    for(int x:{fx,fx+223})for(int y:{fy,fy+223})assert(Ui::RoundExplore::routePoint(x,y));
    // Capture at exactly the shutter release's game clock; exclude all overlays.
    event(T::DOWN,180,324);desktop.advanceMs(40);route.ui.renderPhotoWorld();
    std::vector<uint16_t> expected(Photos::PIXELS);
    for(unsigned y=0;y<192;++y)for(unsigned x=0;x<192;++x)expected[y*192+x]=canvas.readPixel(fx+int((2*x+1)*224/384),fy+int((2*y+1)*224/384));
    route.onTouch({T::UP,180,324,desktop.millis()});
    assert(route.photo_.page()==P::PREVIEW&&std::equal(expected.begin(),expected.end(),route.photo_.image()));shot("04-preview",route);clipped(route.photo_);
    auto frozen=route.ui.ecology.pose(0);desktop.advanceMs(31000);const auto parked=route.update(desktop.millis(),31.f);
    assert(!parked.redraw&&parked.nextUpdateDelayMs==SceneUpdateResult::NO_UPDATE&&route.keepsDisplayAwake());assert(ExploreEcology::samePose(frozen,route.ui.ecology.pose(0)));
    event(T::DOWN,132,310);event(T::UP,132,310);assert(route.photo_.viewfinder()&&route.photo_.frameX()==fx&&route.photo_.frameY()==fy);
    event(T::DOWN,180,324);event(T::UP,180,324);assert(route.photo_.page()==P::PREVIEW);
    std::copy_n(route.photo_.image(),Photos::PIXELS,expected.begin());
    files.failWrite=true;files.delay=2000;event(T::DOWN,228,310);event(T::UP,228,310);files.failWrite=false;
    assert(route.photo_.page()==P::PREVIEW&&route.photo_.error());shot("05-save-failed",route);
    event(T::DOWN,228,310);event(T::UP,228,310);
    assert(!route.photo_.active()&&!route.routePausedForTouch&&!route.keepsDisplayAwake());
    assert(route.routeWorldX==originalX&&route.routeWorldY==originalY&&route.routeIndex==originalIndex);shot("06-saved-explore",route);
    Photos::Library library;assert(library.refresh()==Photos::Result::OK&&library.count()==1);
    std::vector<uint16_t> restored(Photos::PIXELS);assert(library.loadImage(library.entry(0),restored.data())==Photos::Result::OK&&restored==expected);
    assert(desktop.millis()-route.routeMoveStarted==pauseElapsed&&"Photo pause must preserve the in-flight route clock");
    desktop.advanceMs(10);route.update(desktop.millis(),.010f);assert(route.routeIndex==originalIndex&&route.routeMoving&&"Slow failed save must not catch up whole route steps");
    // A paused exploration menu must still allow photography, preserving pause on close.
    assert(route.activateExplore(Ui::ExploreAction::OPEN_MENU));
    assert(route.activateExplore(Ui::ExploreAction::MENU_RETURN_PAUSE));
    const auto pausedAt=route.routePausedAt;assert(route.routePausedForTouch);
    event(T::DOWN,180,82);event(T::UP,180,82);
    assert(route.photo_.viewfinder()&&"Camera must also open after closing the exploration menu into pause");
    route.closePhoto();assert(route.routePausedForTouch&&route.routePausedAt==pausedAt);
    route.activateExplore(Ui::ExploreAction::WALK);assert(!route.routePausedForTouch);
    // New camera centers; power lock discards draft without deleting stored photographs.
    event(T::DOWN,180,82);event(T::UP,180,82);assert(route.photo_.frameX()==68&&route.photo_.frameY()==68);
    event(T::DOWN,180,324);event(T::UP,180,324);assert(route.photo_.page()==P::PREVIEW);
    assert(route.prepareDisplayLock()&&!route.photo_.active());assert(library.refresh()==Photos::Result::OK&&library.count()==1);
    for(unsigned i=0;i<4;++i)assert(library.save(expected.data(),route.generatedMap.areaIndex,state.gameMinutesTotal/1440+1)==Photos::Result::OK);
    Ui::SceneSurface::select(SceneID::MENU);MenuScene menu;menu.onEnter();menu.pushView(MenuScene::ViewMode::COMPUTER);menu.computerUi.reset();
    assert(menu.computerPhotoCount()==5);shot("07-computer",menu);
    // Native input travels through the240px computer surface into the360px album.
    auto menuEvent=[&](T type,int x,int y){desktop.advanceMs(40);Ui::SceneSurface::InputScope scope({frame.data(),360,360,true});
        Platform::IInputDevice::TouchEvent e{type,int16_t(x),int16_t(y),desktop.millis()};assert(Ui::SceneSurface::mapTouch(e));menu.onTouch(e);
    };
    menuEvent(T::DOWN,180,260);menuEvent(T::UP,180,260);
    assert(menu.viewMode==MenuScene::ViewMode::ALBUM&&menu.photos_.count()==5&&Ui::SceneSurface::width()==360);
    shot("08-album",menu);clipped(menu.photos_);assert(!menu.keepsDisplayAwake());
    assert(!menu.touchNavigation()&&"Native album must not draw legacy navigation strip");
    auto albumEvent=[&](T type,int x,int y){menuEvent(type,x,y);};
    auto albumTap=[&](int x,int y){albumEvent(T::DOWN,x,y);albumEvent(T::UP,x,y);};
    albumTap(120,130);assert(menu.photos_.page()==P::DETAIL);shot("09-detail",menu);
    albumEvent(T::DOWN,240,180);albumEvent(T::MOVE,130,180);albumEvent(T::UP,130,180);assert(menu.photos_.selected()==1);
    albumTap(254,46);assert(menu.photos_.page()==P::DELETE_CONFIRM);shot("10-delete",menu);
    albumTap(126,230);assert(menu.photos_.page()==P::DETAIL);
    albumTap(254,46);menu.render();std::vector<uint16_t> buttonPixels;
    for(const auto r:{menu.photos_.cancelBounds(),menu.photos_.deleteBounds()})for(int y=r.y;y<r.y+r.height;++y)for(int x=r.x;x<r.x+r.width;++x)buttonPixels.push_back(canvas.readPixel(x,y));
    files.failDelete=true;albumTap(234,230);files.failDelete=false;
    assert(menu.photos_.page()==P::DELETE_CONFIRM&&menu.photos_.count()==5);menu.render();unsigned buttonPixel=0;
    for(const auto r:{menu.photos_.cancelBounds(),menu.photos_.deleteBounds()})for(int y=r.y;y<r.y+r.height;++y)for(int x=r.x;x<r.x+r.width;++x)
        assert(canvas.readPixel(x,y)==buttonPixels[buttonPixel++]&&"Deletion error must not cover action buttons");
    shot("14-delete-failed",menu);albumTap(234,230);
    assert(menu.photos_.page()==P::ALBUM&&menu.photos_.count()==4);
    assert(library.refresh()==Photos::Result::OK&&library.count()==4);assert(library.save(expected.data(),route.generatedMap.areaIndex,state.gameMinutesTotal/1440+1)==Photos::Result::OK);
    menu.photos_.openAlbum();
    albumEvent(T::DOWN,180,260);albumEvent(T::MOVE,180,210);albumEvent(T::UP,180,210);
    assert(menu.photos_.offset()>0&&menu.photos_.moving());desktop.advanceMs(66);menu.update(desktop.millis(),.066f);shot("11-album-scroll",menu);
    menu.cancelTouch();albumEvent(T::UP,180,210);assert(menu.photos_.page()==P::ALBUM&&!menu.photos_.moving());
    albumTap(106,46);assert(menu.viewMode==MenuScene::ViewMode::COMPUTER&&Ui::SceneSurface::width()==240);shot("12-computer-after",menu);
    Ui::SceneSurface::select(SceneID::EXPLORE);route.prepareSurface();
    // Full archive refuses a sixth shutter without replacing a photo.
    event(T::DOWN,180,82);event(T::UP,180,82);event(T::DOWN,180,324);event(T::UP,180,324);
    assert(route.photo_.viewfinder()&&route.photo_.error());shot("13-camera-full",route);route.closePhoto();
    // Both native framebuffer byte orders render the same round UI.
    Ui::SceneSurface::select(SceneID::MENU);menu.openComputerAlbum();std::vector<uint16_t> visual;
    for(bool swap:{false,true}){canvas.attach({frame.data(),360,360,swap});menu.render();std::vector<uint16_t> pixels;
        for(int y=0;y<360;++y)for(int x=0;x<360;++x)pixels.push_back(canvas.readPixel(x,y));
        if(!swap)visual=pixels;else assert(visual==pixels);
    }
    menu.onExit();route.onExit();
    assert(engine.resetGame());Photos::Library retained;assert(retained.refresh()==Photos::Result::OK&&retained.count()==5);
    assert(retained.loadImage(retained.entry(0),restored.data())==Photos::Result::OK&&restored==expected);
    std::puts("SDGOODS photo PASS: actual camera/224 circle drag/192 clean capture/active wildlife/frozen route/parked preview/retake/retry/save/readback/5full/computer/album/scroll/swipe/delete/cancel/lock/byteorder");
}
