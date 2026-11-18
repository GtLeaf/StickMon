#include "AmoledApp.h"
#include "core/FontResource.h"
#include "game/MonsterFactory.h"
#include "platform/desktop/DesktopPlatform.h"
#include "market_dialogue_fixture.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static const char* output;
class FaultStore final:public Platform::IBlobStore {
public:
    explicit FaultStore(Platform::IBlobStore& store):store_(store){}
    const char* fail=nullptr;
    uint32_t delayNextMs=0;
    bool initialize() override{return store_.initialize();}
    size_t blobSize(const char* n,const char* k) override{return store_.blobSize(n,k);}
    bool readBlob(const char* n,const char* k,void* b,size_t s) override{return store_.readBlob(n,k,b,s);}
    bool writeBlob(const char* n,const char* k,const void* b,size_t s) override {
        if(delayNextMs){Platform::clock().sleepMs(delayNextMs);delayNextMs=0;}
        if(fail&&(!std::strcmp(fail,k)||(!std::strcmp(fail,"snapshots")&&!std::strncmp(k,"state",5))))return false;
        return store_.writeBlob(n,k,b,s);
    }
    bool removeBlob(const char* n,const char* k) override{return store_.removeBlob(n,k);}
    bool clearNamespace(const char* n) override{return store_.clearNamespace(n);}
private:Platform::IBlobStore& store_;
};
static void capture(const char* name) {
    if(!output)return;
    auto path=std::string(output)+"/"+name+".ppm";auto* f=std::fopen(path.c_str(),"wb");assert(f);
    std::fprintf(f,"P6\n368 448\n255\n");
    for(int y=0;y<448;++y)for(int x=0;x<368;++x){const auto p=PixelRenderer::canvas().readPixel(x,y);
        const unsigned char rgb[]={uint8_t(((p>>11)&31)*255/31),uint8_t(((p>>5)&63)*255/63),uint8_t((p&31)*255/31)};std::fwrite(rgb,1,3,f);}
    std::fclose(f);
}
#include "core/RoomResource.h"
#include "presentation/MenuGrid.h"
class HomePower final:public Platform::IPowerDevice {
public:
    explicit HomePower(Platform::IPowerDevice& b):base(b){}
    int batteryLevel()override{return base.batteryLevel();}
    Platform::WakeReason wakeReason()const override{return base.wakeReason();}
    uint32_t hardwareRandom()override{return base.hardwareRandom();}
    size_t externalMemorySize()const override{return 8*1024*1024;}
    size_t externalMemoryFree()const override{return 8*1024*1024;}
    void restart()override{base.restart();}
    void enterDeepSleep(uint64_t ms,bool ext)override{base.enterDeepSleep(ms,ext);}
private:Platform::IPowerDevice& base;
};
namespace AmoledV1 {
struct AmoledAppTestAccess {
 static void run(AmoledApp& app,FaultStore& blobs){
    using P=Ui::HomeLibraryUi::Page;using T=TouchEventType;
    auto& s=app.gameState;s={};s.oobeDone=true;s.coins=0;s.teamCount=1;s.team[0]=Game::MonsterFactory::create(25,20);
    s.room.ownedFurniture=0x4000;s.room.placedFurniture=0x4000;app.syncHomeActors(0);
    uint32_t time=1000;
    const auto event=[&](T type,int x,int y){TouchEvent e{};e.x=x;e.y=y;e.type=type;e.timestampMs=(time+=40);app.handleTouch(e);};
    const auto tap=[&](int x,int y){event(T::DOWN,x,y);event(T::UP,x,y);};
    const auto shot=[&](const char* name){app.forceFullRender();app.render(PixelRenderer::canvas());capture(name);};
    app.sceneFlow.openMenu(AppSceneFlow::Scene::HOME);app.menuScroll=0;
    const auto cell=MenuGrid::mainLayout(368,448,2).cell(2,0);tap(cell.x+cell.width/2,cell.y+cell.height/2);
    assert(app.sceneFlow.current()==AppSceneFlow::Scene::ROOM);shot("amoled-01-room-menu");
    tap(184,376);auto& ui=app.homeLibraryUi_;assert(ui.page()==P::LIBRARY);shot("amoled-02-empty");
    tap(184,132);assert(ui.page()==P::LIBRARY);tap(42,40);assert(!ui.active()&&app.sceneFlow.current()==AppSceneFlow::Scene::ROOM);
    s.room.ownedFurniture|=1;assert(app.saveState());tap(184,376);shot("amoled-03-owned");
    auto& canvas=PixelRenderer::canvas();std::vector<uint16_t> complete;
    for(int y=0;y<448;++y)for(int x=0;x<368;++x)complete.push_back(canvas.readPixel(x,y));
    canvas.fillSprite(0x31a5);app.forceRenderRows(120,121);app.render(canvas);
    for(int y=0;y<448;++y)for(int x=0;x<368;++x)assert(canvas.readPixel(x,y)==(y==120?complete[y*368+x]:0x31a5)&&"Home library rendered outside the requested dirty band");
    app.forceFullRender();app.render(canvas);

    event(T::DOWN,184,132);app.setDisplayLocked(true);app.setDisplayLocked(false);event(T::UP,184,132);assert(ui.page()==P::LIBRARY);
    tap(184,132);assert(ui.page()==P::PREVIEW&&s.room.placedFurniture==0x4000);shot("amoled-04-original");
    for(int y=316;y<386;++y)for(int x=24;x<344;++x)assert(canvas.readPixel(x,y)==0&&"Preview must not contain furniture icon/name/status");
    s.settings.idleTimeoutIndex=0;app.lockRequested=false;app.lastInteractionMs=time;time+=31000;app.update(time);assert(!app.consumeLockRequest()&&"Furniture comparison must suppress automatic lock");
    tap(184,414);assert(ui.page()==P::PREVIEW&&s.room.placedFurniture==0x4000);shot("amoled-05-trial");
    assert(app.saveState());SaveManager saves;assert(saves.begin());Game::GameState loaded;MainSceneViewState v{};assert(saves.load(loaded,v));assert(loaded.room.placedFurniture==0x4000);
    tap(184,414);tap(42,40);assert(ui.page()==P::LIBRARY&&s.room.placedFurniture==0x4000);
    app.lockRequested=false;app.lastInteractionMs=time;time+=31000;app.update(time);assert(app.consumeLockRequest()&&"Library must restore the normal idle policy");
    tap(184,132);tap(184,414);blobs.fail="snapshots";tap(42,40);blobs.fail=nullptr;assert(ui.page()==P::PREVIEW&&s.room.placedFurniture==0x4000);shot("amoled-06-save-failed");
    blobs.delayNextMs=350;tap(42,40);assert(ui.page()==P::LIBRARY&&s.room.placedFurniture==0x4001&&s.coins==0);shot("amoled-07-placed");
    assert(saves.load(loaded,v));assert(loaded.room.placedFurniture==0x4001&&loaded.room.ownedFurniture==0x4001);
    // The real home view consumes saved placement even after a cancelled preview.
    tap(42,40);tap(42,40);assert(app.sceneFlow.current()==AppSceneFlow::Scene::MAIN_MENU);app.sceneFlow.enter(AppSceneFlow::Scene::HOME);shot("amoled-07-home-placed");
    HomeViewModel savedHome;app.fillHomePetModel(savedHome);assert(savedHome.placedFurniture==1);
    app.openRoomScene();tap(184,376);tap(184,132);event(T::DOWN,184,414);app.cancelTouch();event(T::UP,184,414);assert(ui.page()==P::PREVIEW&&s.room.placedFurniture==0x4001);
    tap(184,414);assert(ui.page()==P::PREVIEW&&s.room.placedFurniture==0x4001);tap(184,414);tap(42,40);assert(ui.page()==P::LIBRARY&&s.room.placedFurniture==0x4001);
    tap(184,132);tap(184,414);tap(42,40);assert(ui.page()==P::LIBRARY&&s.room.placedFurniture==0x4000&&s.room.ownedFurniture==0x4001);shot("amoled-08-removed");
    tap(42,40);tap(184,136);assert(app.sceneFlow.current()==AppSceneFlow::Scene::ROOM_FOOD);tap(42,40);assert(app.sceneFlow.current()==AppSceneFlow::Scene::ROOM);
    tap(184,376);tap(42,40);tap(42,40);assert(app.sceneFlow.current()==AppSceneFlow::Scene::HOME&&!ui.active());
    std::puts("home-library actual AMOLED: real menu entry/empty/owned/local compare/undo/save-on-back, failed+slow durable save, preview keep-awake/idle restore, lock/cancel, home placement, remove and food/outer returns OK");
 }
};
}
int main(int argc,char** argv) {
    assert(argc==3);output=argv[2];DesktopPlatform desktop(argv[1]);auto& base=desktop.serviceBundle();FaultStore blobs(base.blobs);HomePower power(base.power);
    Platform::Services services{base.lifecycle,base.clock,base.logger,base.input,base.display,base.audio,base.imu,power,blobs,base.resources,base.memory,base.peers};
    Platform::bind(services);assert(desktop.begin());assert(Platform::resources().mount());assert(GameAssets::begin());assert(FontResource::ins().begin());
    std::vector<uint16_t> pixels(368*448);auto& c=PixelRenderer::canvas();c.attach({pixels.data(),368,448,false});c.setCoordinateScale(1);c.setLayoutScale(1);c.setAssetScale(1);c.setNativeText(true);
    AmoledV1::AmoledApp app;app.begin(0);AmoledV1::AmoledAppTestAccess::run(app,blobs);
}
