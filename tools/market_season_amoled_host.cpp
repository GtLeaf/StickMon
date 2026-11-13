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
class FaultStore final : public Platform::IBlobStore {
public:
    explicit FaultStore(Platform::IBlobStore& backing): backing_(backing) {}
    const char* failKey = nullptr;
    bool initialize() override { return backing_.initialize(); }
    size_t blobSize(const char* ns,const char* key) override { return backing_.blobSize(ns,key); }
    bool readBlob(const char* ns,const char* key,void* out,size_t size) override { return backing_.readBlob(ns,key,out,size); }
    bool writeBlob(const char* ns,const char* key,const void* data,size_t size) override {
        if(failKey && (!std::strcmp(failKey,key) ||
            (!std::strcmp(failKey,"slots") && (!std::strcmp(key,"state_a") || !std::strcmp(key,"state_b"))) ||
            (!std::strcmp(failKey,"snapshots") && !std::strncmp(key,"state",5)))) return false;
        return backing_.writeBlob(ns,key,data,size);
    }
    bool removeBlob(const char* ns,const char* key) override { return backing_.removeBlob(ns,key); }
    bool clearNamespace(const char* ns) override { return backing_.clearNamespace(ns); }
private:
    Platform::IBlobStore& backing_;
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
        auto& s=app.gameState;s={};s.oobeDone=true;s.teamCount=2;s.coins=20000;
        s.team[0]=Game::MonsterFactory::create(25,20);s.team[1]=Game::MonsterFactory::create(133,20);
        for(auto& clear:s.explorePoolRerollCounts)clear=1;
        app.syncHomeActors(0);uint32_t t=0,touch=1000;
        auto advance=[&](int count){while(count--){t+=100;app.update(t);}};
        auto tap=[&](int x,int y){touch=std::max(touch,t+1);TouchEvent e{};e.x=x;e.y=y;e.timestampMs=touch;e.type=TouchEventType::DOWN;app.handleTouch(e);
            e.type=TouchEventType::UP;e.timestampMs+=60;app.handleTouch(e);touch+=200;};
        auto shot=[&](const char* name){app.render(PixelRenderer::canvas());capture(name);};
        s.gameMinutesTotal=1440;assert(!app.startMarket(0));
        app.sceneFlow.enter(AppSceneFlow::Scene::EXPLORE_AREAS);app.selectedExploreDestination_=ExploreDestinations::MARKET;app.loadExplorePreview(0);
        assert(!ExploreDestinations::unlocked(ExploreDestinations::MARKET,s));shot("01-market-closed");
        s.gameMinutesTotal=0;assert(app.startMarket(0));advance(40);
        auto enter=[&](int species){
            int target=-1;for(int i=0;i<app.market_->data().residentCount;++i)if(app.market_->actor(i).species==species)target=i;
            assert(target>=0&&marketStandNear(*app.market_,target));app.marketUi_.chatTarget=target;app.marketUi_.refresh(*app.market_);tap(184,399);
            for(int i=0;i<60&&!app.marketServiceActive();++i)advance(1);
            assert(app.marketServiceActive());advance(4);
        };
        enter(352);tap(184,380);advance(5);
        auto& ui=app.marketShopUi_;assert(ui.page()==Ui::MarketShopUi::Page::SHELF);shot("02-shelf-top");
        auto scroll=[&](int desired){const int delta=desired-ui.scrollOffset();TouchEvent e{};e.x=184;e.y=400;e.timestampMs=touch;e.type=TouchEventType::DOWN;app.handleTouch(e);
            e.type=TouchEventType::MOVE;e.y=400-delta;app.handleTouch(e);e.type=TouchEventType::UP;e.timestampMs+=200;app.handleTouch(e);touch+=300;assert(ui.scrollOffset()==desired);};
        auto select=[&](uint8_t slot){const int offset=std::clamp(int(slot)*72-72,0,352);scroll(offset);tap(184,216+slot*72-offset+36);};
        select(Game::MarketShop::NUTRIENT);assert(ui.page()==Ui::MarketShopUi::Page::SELECT_ITEM);shot("03-nutrient-choice");
        tap(272,262);assert(ui.page()==Ui::MarketShopUi::Page::CONFIRM);shot("04-nutrient-confirm");
        app.saveWritesBlocked=true;tap(272,414);assert(s.coins==20000&&s.marketSeason.stock[Game::MarketShop::NUTRIENT]==2&&!Game::ItemInventory::count(s,Game::ItemId::PROTEIN));
        s.gameMinutesTotal=1440;tap(272,414);
        assert(s.coins==20000&&s.marketSeason.stock[Game::MarketShop::NUTRIENT]==2&&!Game::ItemInventory::count(s,Game::ItemId::PROTEIN));
        s.gameMinutesTotal=0;
        app.saveWritesBlocked=false;blobs.failKey="snapshots";tap(272,414);
        assert(ui.page()==Ui::MarketShopUi::Page::CONFIRM&&s.coins==20000&&s.marketSeason.stock[Game::MarketShop::NUTRIENT]==2);
        // One durable copy is a committed transaction even if its mirror fails.
        blobs.failKey="state";tap(272,414);blobs.failKey=nullptr;
        assert(ui.page()==Ui::MarketShopUi::Page::RESULT&&s.coins==19400&&s.marketSeason.stock[Game::MarketShop::NUTRIENT]==1);
        assert(Game::ItemInventory::count(s,Game::ItemId::PROTEIN)==1);shot("05-nutrient-result");tap(184,414);
        assert(ui.scrollOffset()==144);select(Game::MarketShop::RESET_BERRY);shot("06-reset-choice");tap(96,312);
        app.encounterHistory.add(150);app.encounterHistoryDirty=true;blobs.failKey="encounters";tap(272,414);
        assert(ui.page()==Ui::MarketShopUi::Page::RESULT&&app.encounterHistoryDirty);
        blobs.failKey=nullptr;assert(app.saveState()&&!app.encounterHistoryDirty);
        assert(s.marketSeason.stock[Game::MarketShop::RESET_BERRY]==2&&Game::ItemInventory::count(s,Game::ItemId::QUALOT_BERRY)==1);tap(184,414);
        select(Game::MarketShop::CANDY);tap(272,414);assert(s.marketSeason.stock[Game::MarketShop::CANDY]==0);tap(184,414);
        s.marketSeason.boxRoll=100;select(Game::MarketShop::BOX);shot("07-box-confirm");tap(272,414);
        assert(s.bag.candy==3&&s.marketSeason.stock[Game::MarketShop::BOX]==0&&s.candyPurchasesToday==0);shot("08-box-bonus-result");tap(184,414);
        SaveManager manager;Game::GameState loaded;MainSceneViewState view;assert(manager.load(loaded,view));
        assert(loaded.marketSeason.stock[Game::MarketShop::CANDY]==0&&loaded.marketSeason.stock[Game::MarketShop::BOX]==0&&Game::ItemInventory::count(loaded,Game::ItemId::PROTEIN)==1);
        tap(184,100);advance(6);assert(!ui.active()&&!app.market_->conversing());
        blobs.failKey="slots";enter(94);blobs.failKey=nullptr;
        assert(s.marketTutorGifted&&s.bag.heartScale==1);shot("09-gengar-welcome");
        app.marketTutorUi_.close();app.closeMarketTutor();enter(94);assert(s.bag.heartScale==1);
        assert(manager.load(loaded,view)&&loaded.marketTutorGifted&&loaded.bag.heartScale==1);
        app.marketTutorUi_.close();app.closeMarketTutor();s.marketTutorGifted=0;s.bag.heartScale=0;blobs.failKey="snapshots";
        enter(94);assert(!s.marketTutorGifted&&!s.bag.heartScale);shot("10-gengar-gift-retry");
        app.marketTutorUi_.close();app.closeMarketTutor();blobs.failKey=nullptr;enter(94);assert(s.marketTutorGifted&&s.bag.heartScale==1);
        app.marketTutorUi_.close();app.closeMarketTutor();s.marketTutorGifted=0;s.bag.heartScale=99;enter(94);assert(!s.marketTutorGifted&&s.bag.heartScale==99);
        app.marketTutorUi_.close();app.closeMarketTutor();s.bag.heartScale=98;enter(94);assert(s.marketTutorGifted&&s.bag.heartScale==99);
        app.closeMarketTutor();s.gameMinutesTotal=1440;app.gameClock.set(t,s.gameMinutesTotal);
        s.marketTutorGifted=0;s.bag.heartScale=0;
        // Closing day during an existing visit must release a refused NPC
        // conversation, keeping the market walk and exit controls usable.
        for(int species:{352,94,113}) {
            int target=-1;for(int i=0;i<app.market_->data().residentCount;++i)
                if(app.market_->actor(i).species==species)target=i;
            assert(target>=0&&marketStandNear(*app.market_,target));
            app.marketUi_.chatTarget=target;app.marketUi_.refresh(*app.market_);
            tap(184,399);advance(60);
            if(app.market_->conversing()||app.marketTutorPending_||app.marketServiceActive())
                std::fprintf(stderr,"closed-stall species=%d t=%u locked=%d conversing=%d ready=%d pending=%d active=%d\n",species,t,app.displayLocked,app.market_->conversing(),app.market_->conversationReady(),app.marketTutorPending_,app.marketServiceActive());
            assert(!app.market_->conversing()&&!app.marketTutorPending_&&!app.marketServiceActive());
        }
        assert(!s.marketTutorGifted&&!s.bag.heartScale);
        app.closeMarket(t);
        std::puts("market-season-app: closed countdown, eight-row scroll, shared selector, atomic purchases, bonus candy and persisted once-only/retried/full gift OK");
    }
};
}
int main(int argc,char**argv){
    assert(argc==2||argc==3);output=argc==3?argv[2]:nullptr;
    DesktopPlatform desktop(argv[1]);auto& base=desktop.serviceBundle();FaultStore blobs(base.blobs);
    Platform::Services services{base.lifecycle,base.clock,base.logger,base.input,base.display,base.audio,base.imu,base.power,blobs,base.resources,base.memory,base.peers};
    Platform::bind(services);assert(desktop.begin());assert(Platform::resources().mount());assert(GameAssets::begin());assert(FontResource::ins().begin());
    std::vector<uint16_t> pixels(368*448);auto& c=PixelRenderer::canvas();c.attach({pixels.data(),368,448,false});c.setCoordinateScale(1);c.setLayoutScale(1);c.setAssetScale(1);c.setNativeText(true);
    AmoledV1::AmoledApp app;app.begin(0);AmoledV1::AmoledAppTestAccess::run(app,blobs);
}
