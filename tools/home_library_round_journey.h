#pragma once
#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vector>
#include "platform/desktop/DesktopPlatform.h"
#define private public
#include "core/GameEngine.h"
#include "scenes/MenuScene.h"
#undef private
#include "core/RoomResource.h"
#include "core/SaveManager.h"
#include "game/MonsterFactory.h"
#include "ui/SceneSurface.h"

#include "home_host_power.h"
class HomeBlobs final:public Platform::IBlobStore {
public:
    explicit HomeBlobs(Platform::IBlobStore& b):base(b){}
    bool fail=false;
    bool initialize()override{return base.initialize();}
    size_t blobSize(const char*n,const char*k)override{return base.blobSize(n,k);}
    bool readBlob(const char*n,const char*k,void*d,size_t s)override{return base.readBlob(n,k,d,s);}
    bool writeBlob(const char*n,const char*k,const void*d,size_t s)override{return !fail&&base.writeBlob(n,k,d,s);}
    bool removeBlob(const char*n,const char*k)override{return base.removeBlob(n,k);}
    bool clearNamespace(const char*n)override{return base.clearNamespace(n);}
private:Platform::IBlobStore& base;
};
static void homeShot(const std::string& out,const char* name){
    auto& c=PixelRenderer::canvas();auto* f=std::fopen((out+"/"+name+".ppm").c_str(),"wb");assert(f);std::fprintf(f,"P6\n360 360\n255\n");
    for(int y=0;y<360;++y)for(int x=0;x<360;++x){const auto p=c.readPixel(x,y);const unsigned char rgb[]={uint8_t(((p>>11)&31)*255/31),uint8_t(((p>>5)&63)*255/63),uint8_t((p&31)*255/31)};std::fwrite(rgb,1,3,f);}std::fclose(f);
}
static void homeRoundJourney(int argc,char**argv){
    assert(argc==3);DesktopPlatform desktop(argv[1]);auto& base=desktop.serviceBundle();HomePower power(base.power);HomeBlobs blobs(base.blobs);
    Platform::Services services{base.lifecycle,base.clock,base.logger,base.input,base.display,base.audio,base.imu,power,blobs,base.resources,base.memory,base.peers};
    Platform::bind(services);assert(desktop.begin());auto& engine=GameEngine::ins();assert(engine.begin());assert(RoomResource::ins().begin());
    auto& state=engine.gameState();state={};state.oobeDone=true;state.teamCount=1;state.team[0]=Game::MonsterFactory::create(25,20);state.coins=0;state.room.ownedFurniture=0x4000;state.room.placedFurniture=0x4000;
    engine.switchScene(SceneID::MENU,false);auto& scene=*static_cast<MenuScene*>(engine.currentScene.get());scene.pushView(MenuScene::ViewMode::ROOM);
    using T=Platform::IInputDevice::TouchType;uint32_t time=1000;
    const auto event=[&](T type,int x,int y){time+=40;Platform::IInputDevice::TouchEvent e{type,int16_t(x),int16_t(y),time};Ui::SceneSurface::InputScope scope(desktop.frameBuffer());assert(Ui::SceneSurface::mapTouch(e));scene.onTouch(e);};
    const auto tap=[&](int x,int y){event(T::DOWN,x,y);event(T::UP,x,y);};
    const auto shot=[&](const char* name){assert(Ui::SceneSurface::begin(desktop.frameBuffer()));scene.render();Ui::SceneSurface::finish();homeShot(argv[2],name);};
    auto& ui=scene.roomUi.homeLibrary;using P=Ui::HomeLibraryUi::Page;
    shot("round-01-room-menu");tap(180,256);assert(ui.active()&&Ui::SceneSurface::width()==360);shot("round-02-empty");tap(180,120);assert(ui.page()==P::LIBRARY);
    tap(158,32);assert(!ui.active()&&Ui::SceneSurface::width()==240&&scene.viewMode==MenuScene::ViewMode::ROOM);
    state.room.ownedFurniture|=1;assert(engine.saveNow());tap(180,256);shot("round-03-owned");
    event(T::UP,180,120);assert(ui.page()==P::LIBRARY);
    event(T::DOWN,180,120);scene.cancelTouch();event(T::UP,180,120);assert(ui.page()==P::LIBRARY);
    tap(180,120);assert(ui.page()==P::PREVIEW&&state.room.placedFurniture==0x4000);shot("round-04-preview");assert(PixelRenderer::canvas().readPixel(200,290)==0&&"Old room tutorial covers the furniture preview");
    for(int y=246;y<298;++y)for(int x=48;x<312;++x)assert(PixelRenderer::canvas().readPixel(x,y)==0&&"Preview must not contain furniture icon/name/status");
    state.settings.idleTimeoutIndex=0;engine.resetIdle(time);engine.updateIdle(time+31000);assert(!engine.idleActive&&"Furniture comparison must keep the display awake");

    // Compare locally; intermediate ordinary saves must retain the original room.
    tap(180,320);assert(ui.page()==P::PREVIEW&&state.room.placedFurniture==0x4000&&"Toggling furniture must not save or leave preview");
    shot("round-05-trial");assert(engine.saveNow());
    SaveManager saves;assert(saves.begin());Game::GameState loaded;MainSceneViewState view{};assert(saves.load(loaded,view));assert(loaded.room.placedFurniture==0x4000);
    tap(180,320);assert(ui.page()==P::PREVIEW&&state.room.placedFurniture==0x4000);tap(158,32);assert(ui.page()==P::LIBRARY);
    engine.resetIdle(time);engine.updateIdle(time+31000);assert(engine.idleActive&&"Library must restore ordinary idle policy");engine.resetIdle(time);

    tap(180,120);tap(180,320);blobs.fail=true;tap(158,32);blobs.fail=false;assert(ui.page()==P::PREVIEW&&state.room.placedFurniture==0x4000);shot("round-06-save-failed");
    tap(158,32);assert(ui.page()==P::LIBRARY&&state.room.placedFurniture==0x4001&&state.room.ownedFurniture==0x4001&&state.coins==0);shot("round-07-placed");
    assert(saves.load(loaded,view));assert(loaded.room.placedFurniture==0x4001&&loaded.room.ownedFurniture==0x4001);
    tap(180,120);tap(180,320);assert(ui.page()==P::PREVIEW&&state.room.placedFurniture==0x4001);tap(180,320);tap(158,32);assert(state.room.placedFurniture==0x4001&&ui.page()==P::LIBRARY);
    tap(180,120);tap(180,320);assert(scene.onButton({1,BtnAction::LONG_PRESS}));assert(ui.page()==P::LIBRARY&&state.room.placedFurniture==0x4000&&state.room.ownedFurniture==0x4001);shot("round-08-removed");
    assert(saves.load(loaded,view));assert(loaded.room.placedFurniture==0x4000&&loaded.room.ownedFurniture==0x4001);
    tap(158,32);tap(180,100);assert(scene.viewMode==MenuScene::ViewMode::FOOD);scene.popView();assert(scene.viewMode==MenuScene::ViewMode::ROOM);
    // The common room cache must replace a preview on the next explicit draw.
    auto& c=PixelRenderer::canvas();c.attach(desktop.frameBuffer());c.setCoordinateScale(1);c.setLayoutScale(1);c.setAssetScale(1);
    std::vector<uint16_t> normal;
    for(int pass=0;pass<3;++pass){c.fillSprite(0);assert(RoomRenderer::drawPreview(0,0,360,250,false,pass==1?1:0));std::vector<uint16_t> now;for(int y=0;y<360;++y)for(int x=0;x<360;++x)now.push_back(c.readPixel(x,y));if(!pass)normal=now;else assert((now==normal)==(pass==2));}
    for(bool night:{false,true})for(bool swap:{false,true}){
        std::vector<uint16_t> pixels(360*360),expected;c.attach({pixels.data(),360,360,swap});c.fillSprite(0);assert(RoomRenderer::drawPreview(0,0,360,250,night,1));for(int y=0;y<360;++y)for(int x=0;x<360;++x)expected.push_back(c.readPixel(x,y));
        c.fillSprite(0);for(int y=0;y<360;y+=13){c.setClipRect(0,y,360,std::min(13,360-y));assert(RoomRenderer::drawPreview(0,0,360,250,night,1));assert(c.clipTop()==y);}c.clearClipRect();for(int y=0;y<360;++y)for(int x=0;x<360;++x)assert(c.readPixel(x,y)==expected[y*360+x]);
    }
    c.attach(desktop.frameBuffer());engine.currentScene.reset();
    std::puts("home-library actual round: black room entry/empty/owned/preview/cancel/place/remove, local compare/undo, save-on-back rollback/reload, keep-awake/idle restore, food return, cache isolation/night/clip/swap OK");
}
