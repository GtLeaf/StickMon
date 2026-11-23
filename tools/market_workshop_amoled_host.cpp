#include "AmoledApp.h"
#include "core/FontResource.h"
#include "game/MonsterFactory.h"
#include "platform/desktop/DesktopPlatform.h"
#include "market_dialogue_fixture.h"
#include "home_host_power.h"
#include "market_workshop_wallpaper_journey.h"
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
namespace AmoledV1 {
struct AmoledAppTestAccess {
    static void run(AmoledApp& app,FaultStore& blobs) {
        using P=Ui::MarketWorkshopUi::Page;
        auto& s=app.gameState;s={};s.oobeDone=true;s.teamCount=2;s.coins=0;
        s.team[0]=Game::MonsterFactory::create(25,20);s.team[1]=Game::MonsterFactory::create(133,20);
        s.room.ownedFurniture=0x4000;s.room.placedFurniture=0x4000;
        app.syncHomeActors(0);uint32_t t=0;
        const auto advance=[&](int ms){while(ms>0){int step=std::min(ms,66);t+=step;app.update(t);ms-=step;}};
        const auto event=[&](TouchEventType type,int x=184,int y=340){TouchEvent e{};e.x=x;e.y=y;e.type=type;e.timestampMs=(t+=40);app.handleTouch(e);};
        const auto tap=[&](int x=184,int y=340){event(TouchEventType::DOWN,x,y);event(TouchEventType::UP,x,y);};
        const auto shot=[&](const char* name){app.forceFullRender();app.render(PixelRenderer::canvas());capture(name);};
        assert(app.startMarket(t));advance(4000);
        const auto open=[&](){
            int keeper=-1;for(int i=0;i<app.market_->data().residentCount;++i)if(app.market_->actor(i).species==115)keeper=i;
            assert(keeper>=0&&marketStandNear(*app.market_,keeper));app.marketUi_.chatTarget=keeper;app.marketUi_.refresh(*app.market_);
            tap(184,399);assert(app.marketTutorPending_&&!app.marketUi_.dialogue().active());advance(1500);
            assert(app.marketWorkshopUi_.active()&&app.market_->conversing());
        };
        open();assert(app.marketWorkshopUi_.dialogueIndex()==0);shot("amoled-workshop-01-welcome");
        event(TouchEventType::DOWN);app.setDisplayLocked(true);advance(1000);app.setDisplayLocked(false);event(TouchEventType::UP);
        assert(app.marketWorkshopUi_.dialogueIndex()==0&&!Game::MarketWorkshop::hasFrame(s));
        tap();assert(app.marketWorkshopUi_.dialogueIndex()==1);shot("amoled-workshop-02-gift");
        tap();assert(app.marketWorkshopUi_.dialogueIndex()==2&&!Game::MarketWorkshop::hasFrame(s));shot("amoled-workshop-03-opening");
        blobs.fail="snapshots";tap();blobs.fail=nullptr;
        assert(app.marketWorkshopUi_.page()==P::DIALOGUE&&s.room.ownedFurniture==0x4000);shot("amoled-workshop-04-save-failed");
        blobs.delayNextMs=350;tap();assert(app.marketWorkshopUi_.page()==P::REWARD&&s.room.ownedFurniture==0x4001&&s.room.placedFurniture==0x4000&&s.coins==0);
        event(TouchEventType::UP);assert(app.marketWorkshopUi_.page()==P::REWARD);advance(5000);shot("amoled-workshop-05-received");
        SaveManager saves;assert(saves.begin());Game::GameState loaded;MainSceneViewState v{};assert(saves.load(loaded,v));
        assert(loaded.room.ownedFurniture==0x4001&&loaded.room.placedFurniture==0x4000&&loaded.coins==0);
        event(TouchEventType::DOWN);app.cancelTouch();event(TouchEventType::UP);assert(app.marketWorkshopUi_.page()==P::REWARD);
        event(TouchEventType::DOWN);app.setDisplayLocked(true);advance(500);app.setDisplayLocked(false);event(TouchEventType::UP);assert(app.marketWorkshopUi_.page()==P::REWARD);
        tap(184,80);advance(800);
        assert(app.marketWorkshopUi_.page()==P::MENU&&app.market_->conversing());
        tap(180,100);advance(800);
        assert(!app.marketServiceActive()&&!app.market_->conversing());

        // A pre-wallpaper save with only the first-visit gift goes straight to the stall.
        s=loaded;open();assert(app.marketWorkshopUi_.page()==P::MENU);
        const auto setHour=[&](int hour){
            s.gameMinutesTotal=hour*60;
            app.gameClock.set(t,s.gameMinutesTotal);
        };
#ifdef STICKMON_TEST_WALLPAPER_CATALOG
        bool checkedDragFrame = false;
        const auto verifyDragFrame = [&](int middleX, int y) {
            auto& canvas = PixelRenderer::canvas();
            advance(66);
            app.forceFullRender();
            app.render(canvas);
            app.markRendered();
            const auto selection = app.marketWorkshopUi_.selectedWallpaper();
            const auto initialOffset = app.marketWorkshopUi_.swatchOffset();
            event(TouchEventType::MOVE, middleX, y);
            assert(app.marketWorkshopUi_.swatchOffset() != initialOffset);
            assert(app.marketWorkshopUi_.selectedWallpaper() == selection && !app.needsRender());
            advance(40);
            const auto row = Ui::MarketWorkshopView{}.selectionRow();
            assert(app.marketListFrame() && app.renderRowBegin() == row.y && app.renderRowEnd() == row.y + row.h);
            app.render(canvas);
            const auto count = size_t(canvas.physicalWidth()) * canvas.physicalHeight();
            const std::vector<uint16_t> partial(canvas.rawPixels(), canvas.rawPixels() + count);
            app.markRendered();
            app.forceFullRender();
            app.render(canvas);
            assert(std::equal(partial.begin(), partial.end(), canvas.rawPixels()));
            app.markRendered();
            checkedDragFrame = true;
        };
#endif
        WorkshopTest::wallpaperJourney(false,app.marketWorkshopUi_,s,tap,
            [&](int x,int y,int endX,int endY,auto duringMove){
                event(TouchEventType::DOWN,x,y);
#ifdef STICKMON_TEST_WALLPAPER_CATALOG
                if (!checkedDragFrame) verifyDragFrame((x + endX)/2, y);
#endif
                event(TouchEventType::MOVE,endX,endY);duringMove();
                event(TouchEventType::UP,endX,endY);
            },shot,setHour,advance,[&](){app.marketCoinMotion_.reset(s.coins);return app.saveState();},
            [&](bool fail){blobs.fail=fail?"snapshots":nullptr;});
#ifdef STICKMON_TEST_WALLPAPER_CATALOG
        assert(checkedDragFrame);
#endif
        tap(180,100);advance(800);assert(!app.market_->conversing());app.closeMarket(t);
        assert(saves.load(loaded,v));s=loaded;
        app.sceneFlow.enter(AppSceneFlow::Scene::HOME);
        setHour(12);shot(WorkshopTest::homeDayShot);
#ifdef STICKMON_TEST_WALLPAPER_CATALOG
        WorkshopTest::expectFixtureWall(true);
#endif
        const auto day=WorkshopTest::roomBand(100,130);
        setHour(22);shot(WorkshopTest::homeNightShot);
        assert(WorkshopTest::roomBand(100,130)!=day);
        s.wallpaperId=0;setHour(12);shot("wallpaper-12-home-original");
        assert(WorkshopTest::roomBand(100,130)!=day);
#ifdef STICKMON_TEST_WALLPAPER_CATALOG
        WorkshopTest::expectFixtureWall(false);
#endif

        s=loaded;s.gameMinutesTotal=10*1440;app.gameClock.set(t,s.gameMinutesTotal);
        assert(app.startMarket(t));advance(4000);open();assert(app.marketWorkshopUi_.page()==P::MENU);
        tap(180,100);advance(800);assert(!app.market_->conversing());app.closeMarket(t);
        std::puts("AMOLED workshop gift, old save, wallpaper previews, durable purchase/equip, rollback and home day/night OK");
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
