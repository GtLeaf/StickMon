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
namespace AmoledV1 {
struct AmoledAppTestAccess {
    static void run(AmoledApp& app,FaultStore& blobs) {
        auto& s=app.gameState;s={};s.oobeDone=true;s.coins=27400;s.teamCount=2;s.marketTutorGifted=1;
        s.team[0]=Game::MonsterFactory::create(25,20);s.team[1]=Game::MonsterFactory::create(133,20);
        s.team[0].move2Id=98;s.team[0].moveProficiency[1]=10;
        s.team[1].move2Id=129;s.team[1].moveProficiency[1]=10;
        s.team[0].affection=s.team[1].affection=100;s.team[0].bond=s.team[1].bond=30;s.bag.heartScale=1;
        app.syncHomeActors(0);uint32_t t=0;
        auto advance=[&](int ms){while(ms>0){const int step=std::min(ms,33);t+=step;app.update(t);ms-=step;}};
        auto tap=[&](int x,int y){TouchEvent e{};e.x=x;e.y=y;e.type=TouchEventType::DOWN;e.timestampMs=++t;app.handleTouch(e);
            e.type=TouchEventType::UP;e.timestampMs=(t+=60);app.handleTouch(e);app.update(t);};
        auto shot=[&](const char* name){app.render(PixelRenderer::canvas());capture(name);};
        auto header=[&](){app.render(PixelRenderer::canvas());std::vector<uint16_t> values;
            for(int y=18;y<46;++y)for(int x=270;x<340;++x)values.push_back(PixelRenderer::canvas().readPixel(x,y));return values;};
        assert(app.startMarket(t));advance(4000);
        auto enter=[&](int species){
            app.closeMarketTutor();int resident=-1;
            for(int i=0;i<app.market_->data().residentCount;++i)if(app.market_->actor(i).species==species)resident=i;
            assert(resident>=0&&marketStandNear(*app.market_,resident));app.marketUi_.chatTarget=resident;app.marketUi_.refresh(*app.market_);
            tap(184,399);for(int i=0;i<60&&!app.marketServiceActive();++i)advance(100);
            assert(app.marketServiceActive());advance(400);tap(184,380);advance(500);
        };
        enter(352);tap(184,280);
        auto& ui=app.marketShopUi_;
        TouchEvent drag{};drag.type=TouchEventType::DOWN;drag.x=184;drag.y=410;drag.timestampMs=++t;app.handleTouch(drag);
        drag.type=TouchEventType::MOVE;drag.y=58;drag.timestampMs=(t+=100);app.handleTouch(drag);
        drag.type=TouchEventType::UP;drag.timestampMs=(t+=200);app.handleTouch(drag);app.update(t);tap(184,404);
        assert(ui.page()==Ui::MarketShopUi::Page::CONFIRM);const auto old=header();shot("01-before");
        app.saveWritesBlocked=true;tap(272,414);assert(s.coins==27400&&ui.page()==Ui::MarketShopUi::Page::CONFIRM);assert(header()==old);
        app.saveWritesBlocked=false;blobs.fail="snapshots";tap(272,414);blobs.fail=nullptr;
        assert(s.coins==27400&&ui.page()==Ui::MarketShopUi::Page::CONFIRM&&header()==old);
        blobs.fail="state";tap(272,414);blobs.fail=nullptr;
        assert(s.coins==27100&&ui.page()==Ui::MarketShopUi::Page::BOX_ARRIVE);
        // A durable payment settles immediately, but its first HUD frame must still be the old balance.
        assert(header()==old);shot("02-paid-start");
        for(int i=0;i<20;++i){char name[40];std::snprintf(name,sizeof(name),"spend-%02d",i);shot(name);advance(50);}
        assert(s.coins==27100&&header()!=old);
        SaveManager manager;Game::GameState loaded;MainSceneViewState view;
        assert(manager.load(loaded,view)&&loaded.coins==27100&&loaded.marketSeason.stock[Game::MarketShop::BOX]==0);
        tap(184,316);advance(1300);assert(ui.page()==Ui::MarketShopUi::Page::RESULT);
        auto paid=[&](uint32_t before,uint32_t amount,const std::vector<uint16_t>& oldHeader,const char* name){
            assert(s.coins==before-amount&&header()==oldHeader);
            const auto f=app.marketCoinMotion_.frame();assert(f.balance==s.coins&&f.rolling);
            assert(f.tags[0].delta==-int64_t(amount)&&f.tags[0].visible);
            advance(150);shot(name);advance(900);assert(!app.marketCoinMotion_.frame().rolling);
        };
        auto openTutor=[&](){
            s.marketTutor={};s.marketTutor.periodDay=Game::MarketTutor::periodDay(s);s.marketTutor.move=247;s.marketTutor.drawnSpecies=133;
            enter(94);
        };
        openTutor();tap(184,250);tap(270,320);tap(184,294);
        assert(app.marketTutorUi_.page()==Ui::MarketTutorUi::Page::CONFIRM);
        auto oldHeader=header();auto before=s.coins;tap(184,414);
        assert(app.marketTutorUi_.page()==Ui::MarketTutorUi::Page::TEACHING);paid(before,400,oldHeader,"03-teach");
        openTutor();tap(184,386);tap(98,320);
        assert(app.marketTutorUi_.page()==Ui::MarketTutorUi::Page::TRAIN_MOVE);
        oldHeader=header();before=s.coins;tap(184,414);
        assert(app.marketTutorUi_.page()==Ui::MarketTutorUi::Page::TRAINING);paid(before,100,oldHeader,"04-train");
        enter(113);tap(184,250);tap(184,244);assert(app.marketCareUi_.page()==Ui::MarketCareUi::Page::CONFIRM);
        oldHeader=header();before=s.coins;tap(272,414);paid(before,150,oldHeader,"05-massage");
        enter(113);tap(184,318);tap(184,300);assert(app.marketCareUi_.page()==Ui::MarketCareUi::Page::CONFIRM);
        oldHeader=header();before=s.coins;tap(272,414);paid(before,200,oldHeader,"06-tea");
        // Free service and heart-scale spending use the same boundary, with no coin notification.
        enter(113);tap(184,386);tap(184,244);before=s.coins;
        assert(app.marketCareUi_.page()==Ui::MarketCareUi::Page::EVAL);
        assert(!app.marketCoinMotion_.frame().tags[0].visible&&s.coins==before);
        openTutor();tap(184,318);tap(98,320);tap(184,244);tap(184,294);
        assert(app.marketTutorUi_.page()==Ui::MarketTutorUi::Page::CONFIRM);before=s.coins;const auto scales=s.bag.heartScale;
        tap(184,414);assert(app.marketTutorUi_.page()==Ui::MarketTutorUi::Page::RESULT&&s.coins==before&&s.bag.heartScale==scales-1);
        assert(!app.marketCoinMotion_.frame().tags[0].visible);
        enter(352);tap(184,354);before=s.coins;tap(184,244);
        assert(ui.page()==Ui::MarketShopUi::Page::ADVICE_DONE&&s.coins==before&&!app.marketCoinMotion_.frame().tags[0].visible);
        tap(184,414);before=s.coins;
        // Controlled signed-income fixtures exercise the shared entry, not a new gameplay income source.
        auto tx=app.commitMarketTransaction(t,[&](Game::GameState& state){state.coins+=500;return Game::MarketShop::Result::OK;});
        assert(tx.status==Game::MarketTransaction::Status::COMMITTED&&s.coins==before+500);
        for(int i=0;i<4;++i){char name[40];std::snprintf(name,sizeof(name),"gain-%02d",i);shot(name);advance(50);}
        auto f=app.marketCoinMotion_.frame();
        tx=app.commitMarketTransaction(t,[&](Game::GameState& state){state.coins-=100;return Game::MarketShop::Result::OK;});
        assert(tx.status==Game::MarketTransaction::Status::COMMITTED);
        auto retargeted=app.marketCoinMotion_.frame();for(int i=0;i<10;++i)assert(std::abs(f.wheels[i]-retargeted.wheels[i])<.001f);
        assert(retargeted.tags[0].delta==500&&retargeted.tags[1].delta==-100);
        app.displayLocked=true;advance(2000);auto paused=app.marketCoinMotion_.frame();
        for(int i=0;i<10;++i)assert(std::abs(paused.wheels[i]-retargeted.wheels[i])<.001f);
        app.displayLocked=false;app.onWake(t);
        for(int i=0;i<20;++i){char name[40];std::snprintf(name,sizeof(name),"mixed-%02d",i);shot(name);advance(50);}
        assert(manager.load(loaded,view)&&loaded.coins==s.coins);
        s.coins=999;enter(352);
        tx=app.commitMarketTransaction(t,[](Game::GameState& state){++state.coins;return Game::MarketShop::Result::OK;});
        for(int i=0;i<14;++i){char name[40];std::snprintf(name,sizeof(name),"carry-up-%02d",i);shot(name);advance(50);}
        tx=app.commitMarketTransaction(t,[](Game::GameState& state){--state.coins;return Game::MarketShop::Result::OK;});
        for(int i=0;i<14;++i){char name[40];std::snprintf(name,sizeof(name),"carry-down-%02d",i);shot(name);advance(50);}
        assert(s.coins==999);s.coins=UINT32_MAX;enter(352);shot("07-max-balance");
        assert(!app.marketCoinMotion_.frame().tags[0].visible);
        s.coins=5000;enter(352);blobs.delayNextMs=1000;
        tx=app.commitMarketTransaction(t,[](Game::GameState& state){state.coins-=100;return Game::MarketShop::Result::OK;});
        assert(tx.status==Game::MarketTransaction::Status::COMMITTED&&s.coins==4900);
        app.update(t);app.update(t+20); // main loop still carries its pre-save timestamp
        t+=1100;app.update(t);
        assert(app.marketCoinMotion_.frame().rolling&&app.marketCoinMotion_.frame().tags[0].elapsed==100);
        shot("08-delayed-save-100ms");advance(350);assert(!app.marketCoinMotion_.frame().rolling);
        // Native renderer must preserve clipping and decode identically for either stored byte order.
        auto& canvas=PixelRenderer::canvas();auto* original=canvas.rawPixels();const auto swapped=canvas.byteSwapped();
        Ui::MarketCoinMotion sample;sample.reset(999);sample.committed({999,1000});sample.update(60);
        std::vector<uint16_t> pixels(368*448),reference;
        for(int height:{28,32})for(bool swap:{false,true}){
            canvas.attach({pixels.data(),368,448,swap});canvas.fillSprite(PixelRenderer::rgb(40,60,80));canvas.setClipRect(200,8,148,150);
            Ui::MarketCoinHud::render(1000,&sample,16,height);
            assert(canvas.clipLeft()==200&&canvas.clipTop()==8&&canvas.clipRight()==348&&canvas.clipBottom()==158);
            std::vector<uint16_t> actual;for(int y=0;y<448;++y)for(int x=0;x<368;++x)actual.push_back(canvas.readPixel(x,y));
            if(!swap)reference=actual;else assert(actual==reference);
        }
        canvas.attach({original,368,448,swapped});app.closeMarketTutor();
        assert(!app.marketCoinMotion_.frame().tags[0].visible&&!app.marketCoinMotion_.frame().rolling);
        std::puts("market-coins-app: all payments/free services, atomic/partial/slow saves, rolling/retarget/pause/carry, byte-order/clip/reset and persistence OK");
    }
};
}
int main(int argc,char** argv) {
    assert(argc==3);output=argv[2];DesktopPlatform desktop(argv[1]);auto& base=desktop.serviceBundle();FaultStore blobs(base.blobs);
    Platform::Services services{base.lifecycle,base.clock,base.logger,base.input,base.display,base.audio,base.imu,base.power,blobs,base.resources,base.memory,base.peers};
    Platform::bind(services);assert(desktop.begin());assert(Platform::resources().mount());assert(GameAssets::begin());assert(FontResource::ins().begin());
    std::vector<uint16_t> pixels(368*448);auto& c=PixelRenderer::canvas();c.attach({pixels.data(),368,448,false});c.setCoordinateScale(1);c.setLayoutScale(1);c.setAssetScale(1);c.setNativeText(true);
    AmoledV1::AmoledApp app;app.begin(0);AmoledV1::AmoledAppTestAccess::run(app,blobs);
}
