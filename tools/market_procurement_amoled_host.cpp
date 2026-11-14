#include "AmoledApp.h"
#include "core/FontResource.h"
#include "core/AudioManager.h"
#include "game/MonsterFactory.h"
#include "platform/desktop/DesktopPlatform.h"
#include "market_dialogue_fixture.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
static const char* output;
class MusicResourceCounter final : public Platform::IResourceStore {
public:
    explicit MusicResourceCounter(Platform::IResourceStore& backing): backing_(backing) {}
    bool mount() override { return backing_.mount(); }
    size_t totalBytes() const override { return backing_.totalBytes(); }
    size_t usedBytes() const override { return backing_.usedBytes(); }
    Platform::ResourceFile open(const char* path) override {
        if(path&&std::strstr(path,"bgm_market.smonaudio"))++marketOpens;
        return backing_.open(path);
    }
    unsigned marketOpens=0;
private:
    Platform::IResourceStore& backing_;
};
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
    static void run(AmoledApp& app,FaultStore& blobs,MusicResourceCounter& resources) {
        using Page=Ui::MarketShopUi::Page;
        using Advice=Game::MarketSupply::Advice;
        auto& s=app.gameState;s={};s.oobeDone=true;s.teamCount=2;s.coins=30000;
        s.team[0]=Game::MonsterFactory::create(25,20);s.team[1]=Game::MonsterFactory::create(133,20);
        app.syncHomeActors(0);uint32_t t=0;
        auto advance=[&](int ms){while(ms>0){const int step=std::min(ms,33);t+=step;app.update(t);if(app.market_)assert(AudioManager::ins().requestedMusic()==MusicTrack::MARKET);ms-=step;}};
        auto send=[&](TouchEventType type,int x,int y,int dt){t+=dt;TouchEvent e{};e.x=x;e.y=y;e.timestampMs=t;e.type=type;app.handleTouch(e);};
        auto tap=[&](int x,int y){send(TouchEventType::DOWN,x,y,1);send(TouchEventType::UP,x,y,60);app.update(t);};
        auto shot=[&](const char* name){app.render(PixelRenderer::canvas());capture(name);};
        auto day=[&](uint32_t d){s.gameMinutesTotal=d*1440+540;app.gameClock.set(t,s.gameMinutesTotal);app.lastCareMs=t;};
        day(1);assert(app.startMarket(t));assert(Game::MarketSchedule::daysUntilOpen(s)==0);app.closeMarket(t);
        day(0);assert(app.startMarket(t));advance(4000);assert(resources.marketOpens>0);const auto musicOpens=resources.marketOpens;
        auto enter=[&](int species){
            int target=-1;for(int i=0;i<app.market_->data().residentCount;++i)if(app.market_->actor(i).species==species)target=i;
            assert(target>=0&&marketStandNear(*app.market_,target));app.marketUi_.chatTarget=target;app.marketUi_.refresh(*app.market_);tap(184,399);
            for(int i=0;i<60&&!app.marketServiceActive();++i)advance(100);
            assert(app.marketServiceActive());advance(500);
        };
        auto& ui=app.marketShopUi_;
        auto shop=[&](){enter(352);if(ui.page()==Page::GREET){tap(184,380);advance(500);}assert(ui.page()==Page::MENU);advance(2200);};
        auto leave=[&](){assert(ui.page()==Page::MENU);tap(184,100);advance(600);assert(!ui.active()&&!app.market_->conversing());};
        auto shelf=[&](){assert(ui.page()==Page::MENU);tap(184,280);assert(ui.page()==Page::SHELF);};
        auto home=[&](){assert(ui.page()==Page::SHELF);tap(184,100);assert(ui.page()==Page::MENU);};
        auto scroll=[&](int desired){const int delta=desired-ui.scrollOffset();if(!delta)return;
            send(TouchEventType::DOWN,184,410,1);send(TouchEventType::MOVE,184,410-delta,100);
            send(TouchEventType::UP,184,410-delta,200);app.update(t);assert(ui.scrollOffset()==desired);};
        auto select=[&](uint8_t slot){assert(ui.page()==Page::SHELF);const int offset=std::clamp(int(slot)*72-72,0,352);scroll(offset);tap(184,216+slot*72-offset+36);};
        SaveManager manager;Game::GameState loaded;MainSceneViewState view;
        auto verifySaved=[&](){assert(manager.load(loaded,view));assert(!std::memcmp(&loaded.marketSupply,&s.marketSupply,sizeof(s.marketSupply)));
            assert(!std::memcmp(&loaded.marketSeason,&s.marketSeason,sizeof(s.marketSeason)));};
        auto advice=[&](Advice a){assert(ui.page()==Page::MENU);const auto before=s.marketSeason;tap(184,354);assert(ui.page()==Page::ADVICE);
            tap(184,244+(int(a)-1)*54);assert(ui.page()==Page::ADVICE_DONE);assert(s.marketSupply.pendingAdvice==uint8_t(a));
            assert(!std::memcmp(&before,&s.marketSeason,sizeof(before)));verifySaved();tap(184,414);assert(ui.page()==Page::MENU);};
        shop();shot("01-home");shelf();shot("02-shelf");
        // Existing motion remains driven by real application ticks.
        send(TouchEventType::DOWN,184,410,1);send(TouchEventType::MOVE,184,375,30);send(TouchEventType::MOVE,184,340,30);send(TouchEventType::UP,184,325,15);
        const int released=ui.scrollOffset();advance(33);assert(ui.scrollOffset()>released);
        tap(184,300);const int braked=ui.scrollOffset();advance(100);assert(ui.scrollOffset()==braked&&ui.page()==Page::SHELF);
        // Legacy empty rows remain readable and cannot be selected or refilled.
        const auto rareItem=s.marketSeason.items[Game::MarketShop::RARE],rareStock=s.marketSeason.stock[Game::MarketShop::RARE];
        s.marketSeason.items[Game::MarketShop::RARE]=255;s.marketSeason.stock[Game::MarketShop::RARE]=0;
        select(Game::MarketShop::RARE);assert(ui.page()==Page::SHELF);shot("02b-legacy-empty-rare");
        assert(!Game::MarketShop::ensurePeriod(s)&&!s.marketSeason.stock[Game::MarketShop::RARE]);
        s.marketSeason.items[Game::MarketShop::RARE]=rareItem;s.marketSeason.stock[Game::MarketShop::RARE]=rareStock;
        const auto nutrient=Game::MarketShop::item(s,Game::MarketShop::NUTRIENT);
        const auto berry=Game::MarketShop::item(s,Game::MarketShop::RESET_BERRY);
        assert(nutrient>=Game::ItemId::HP_UP&&nutrient<=Game::ItemId::CARBOS);
        assert(berry>=Game::ItemId::POMEG_BERRY&&berry<=Game::ItemId::TAMATO_BERRY);
        select(Game::MarketShop::NUTRIENT);assert(ui.page()==Page::CONFIRM);shot("03-nutrient-detail");
        const auto coins=s.coins;const auto stock=s.marketSeason.stock[Game::MarketShop::NUTRIENT];
        app.saveWritesBlocked=true;tap(272,414);assert(ui.page()==Page::CONFIRM&&s.coins==coins&&s.marketSeason.stock[Game::MarketShop::NUTRIENT]==stock);shot("04-purchase-save-failed");
        app.saveWritesBlocked=false;blobs.failKey="snapshots";tap(272,414);assert(ui.page()==Page::CONFIRM&&s.coins==coins);
        blobs.failKey="state";tap(272,414);blobs.failKey=nullptr;
        assert(ui.page()==Page::RESULT&&s.coins==coins-Game::MarketShop::price(nutrient)&&Game::ItemInventory::count(s,nutrient)==1);
        assert(s.marketSeason.stock[Game::MarketShop::NUTRIENT]==stock-1);shot("05-purchase-result");tap(184,414);
        select(Game::MarketShop::RESET_BERRY);assert(ui.page()==Page::CONFIRM);shot("06-berry-detail");
        app.encounterHistory.add(150);app.encounterHistoryDirty=true;blobs.failKey="encounters";tap(272,414);
        assert(ui.page()==Page::RESULT&&app.encounterHistoryDirty&&Game::ItemInventory::count(s,berry)==1);
        blobs.failKey=nullptr;assert(app.saveState()&&!app.encounterHistoryDirty);tap(184,414);
        select(Game::MarketShop::CANDY);tap(272,414);assert(ui.page()==Page::RESULT&&s.marketSeason.stock[Game::MarketShop::CANDY]==0);tap(184,414);
        select(Game::MarketShop::CANDY);assert(ui.page()==Page::SHELF);shot("07-sold-out");
        select(Game::MarketShop::BOX);assert(ui.page()==Page::CONFIRM);shot("08-box-detail");tap(96,414);home();
        const auto current=s.marketSeason;tap(184,354);assert(ui.page()==Page::ADVICE);shot("09-advice");
        app.saveWritesBlocked=true;tap(184,244);assert(ui.page()==Page::ADVICE&&!s.marketSupply.pendingAdvice);shot("10-advice-save-failed");
        app.saveWritesBlocked=false;blobs.failKey="snapshots";tap(184,244);assert(ui.page()==Page::ADVICE&&!s.marketSupply.pendingAdvice);
        blobs.failKey="state";tap(184,244);blobs.failKey=nullptr;assert(ui.page()==Page::ADVICE_DONE&&s.marketSupply.pendingAdvice==1);shot("11-advice-done");
        assert(!std::memcmp(&current,&s.marketSeason,sizeof(current)));verifySaved();tap(184,414);
        tap(184,354);shot("12-selected-advice");tap(184,100);assert(ui.page()==Page::MENU);leave();
        day(3);shop();const uint8_t explore[]={4,4,1,1,2,2,1,1};assert(!std::memcmp(s.marketSeason.stock,explore,8)&&s.marketSupply.stockPlan==1&&!s.marketSupply.pendingAdvice);
        verifySaved();advice(Advice::GROWTH);leave();
        day(7);shop();const uint8_t growth[]={2,2,1,3,4,2,1,1};assert(!std::memcmp(s.marketSeason.stock,growth,8)&&s.marketSupply.stockPlan==2&&!s.marketSupply.pendingAdvice);
        advice(Advice::RARE);leave();
        // Choose a real following opening with two boxes (fixed fixture, no draw overrides).
        uint32_t rareDay=10;for(;;++rareDay){if(!Game::MarketSchedule::isOpenDay(rareDay))continue;
            auto probe=s;probe.gameMinutesTotal=rareDay*1440+540;Game::MarketShop::ensurePeriod(probe);if(probe.marketSeason.stock[Game::MarketShop::BOX]==2)break;}
        day(rareDay);shop();assert(s.marketSupply.stockPlan==4||s.marketSupply.stockPlan==5);
        int total=0;for(auto n:s.marketSeason.stock)total+=n;assert(total==16&&!s.marketSupply.pendingAdvice);
        const auto firstRoll=s.marketSeason.boxRoll,secondRoll=s.marketSupply.boxRoll2;assert(firstRoll&&secondRoll);
        const auto first=Game::MarketShop::boxReward(s);shelf();select(Game::MarketShop::BOX);assert(ui.page()==Page::CONFIRM);
        // Whole reward refusal neither charges nor advances the frozen ordinal.
        const auto oldCount=Game::ItemInventory::count(s,first.item);assert(Game::ItemInventory::add(s,first.item,Game::ITEM_STACK_CAP-oldCount));
        const auto beforeBoxCoins=s.coins;tap(272,414);assert(ui.page()==Page::CONFIRM&&s.coins==beforeBoxCoins&&s.marketSeason.stock[Game::MarketShop::BOX]==2);
        assert(Game::ItemInventory::remove(s,first.item,Game::ITEM_STACK_CAP-oldCount));
        app.saveWritesBlocked=true;tap(272,414);assert(ui.page()==Page::CONFIRM&&s.marketSeason.stock[Game::MarketShop::BOX]==2&&s.marketSeason.boxRoll==firstRoll&&s.marketSupply.boxRoll2==secondRoll);
        app.saveWritesBlocked=false;tap(272,414);assert(ui.page()==Page::RESULT&&s.marketSeason.stock[Game::MarketShop::BOX]==1);
        assert(Game::ItemInventory::count(s,first.item)==oldCount+first.amount&&Game::MarketShop::boxRoll(s)==secondRoll);shot("13-first-box-result");verifySaved();
        tap(184,414);select(Game::MarketShop::BOX);shot("14-second-box-detail");const auto second=Game::MarketShop::boxReward(s);const auto beforeSecond=Game::ItemInventory::count(s,second.item);
        tap(272,414);assert(ui.page()==Page::RESULT&&s.marketSeason.stock[Game::MarketShop::BOX]==0&&Game::ItemInventory::count(s,second.item)==beforeSecond+second.amount);
        verifySaved();tap(184,414);select(Game::MarketShop::BOX);assert(ui.page()==Page::SHELF);shot("15-boxes-sold-out");home();leave();
        // No repeated advice means the following opening returns to default.
        uint32_t next=rareDay+1;while(!Game::MarketSchedule::isOpenDay(next))++next;day(next);shop();const uint8_t defaults[]={3,3,1,2,3,2,1,1};
        assert(!std::memcmp(s.marketSeason.stock,defaults,8)&&s.marketSupply.stockPlan==0&&!s.marketSupply.boxRoll2);leave();
        // The three real Gengar member pages all render the added prompt.
        enter(94);assert(s.marketTutorGifted&&s.bag.heartScale==1);tap(184,380);advance(500);
        auto& tutor=app.marketTutorUi_;assert(tutor.page()==Ui::MarketTutorUi::Page::MENU);
        tap(184,244);assert(tutor.page()==Ui::MarketTutorUi::Page::TUTOR);shot("16-gengar-learn-member");tap(184,100);
        tap(184,310);assert(tutor.page()==Ui::MarketTutorUi::Page::RECALL_MEMBER);shot("17-gengar-recall-member");tap(184,100);
        tap(184,380);assert(tutor.page()==Ui::MarketTutorUi::Page::TRAIN_MEMBER);shot("18-gengar-train-member");tap(184,100);tap(184,100);advance(600);
        assert(!app.marketServiceActive()&&!app.market_->conversing());
        // Preserve real App once-only, retry, full-stack and partial-write gift coverage.
        enter(94);assert(s.bag.heartScale==1);app.marketTutorUi_.close();app.closeMarketTutor();
        s.marketTutorGifted=0;s.bag.heartScale=0;blobs.failKey="snapshots";enter(94);
        assert(!s.marketTutorGifted&&!s.bag.heartScale);shot("19-gengar-gift-retry");
        app.marketTutorUi_.close();app.closeMarketTutor();blobs.failKey="slots";enter(94);blobs.failKey=nullptr;
        assert(s.marketTutorGifted&&s.bag.heartScale==1);assert(manager.load(loaded,view)&&loaded.marketTutorGifted&&loaded.bag.heartScale==1);
        app.marketTutorUi_.close();app.closeMarketTutor();s.marketTutorGifted=0;s.bag.heartScale=99;enter(94);
        assert(!s.marketTutorGifted&&s.bag.heartScale==99);app.marketTutorUi_.close();app.closeMarketTutor();
        s.bag.heartScale=98;enter(94);assert(s.marketTutorGifted&&s.bag.heartScale==99);app.marketTutorUi_.close();app.closeMarketTutor();
        day(next+1); // Both valid opening offsets are followed by closed days.
        for(int species:{352,94,113}) {
            int target=-1;for(int i=0;i<app.market_->data().residentCount;++i)if(app.market_->actor(i).species==species)target=i;
            assert(target>=0&&marketStandNear(*app.market_,target));app.marketUi_.chatTarget=target;app.marketUi_.refresh(*app.market_);tap(184,399);advance(6000);
            assert(app.market_->conversing()&&!app.marketTutorPending_&&app.marketServiceActive());
            assert(species!=352||app.marketShopUi_.active());assert(species!=94||app.marketTutorUi_.active());assert(species!=113||app.marketCareUi_.active());
            app.closeMarketTutor();
        }
        assert(resources.marketOpens==musicOpens);app.closeMarket(t);
        std::puts("market-procurement-app: concrete purchases, save rollback/partial commit, next-opening explore/growth/rare/default16, advice durability, two frozen boxes and three Gengar prompts OK");
    }
};
}
int main(int argc,char**argv){
    assert(argc==2||argc==3);output=argc==3?argv[2]:nullptr;
    DesktopPlatform desktop(argv[1]);auto& base=desktop.serviceBundle();FaultStore blobs(base.blobs);
    MusicResourceCounter resources(base.resources);
    Platform::Services services{base.lifecycle,base.clock,base.logger,base.input,base.display,base.audio,base.imu,base.power,blobs,resources,base.memory,base.peers};
    Platform::bind(services);assert(desktop.begin());assert(Platform::resources().mount());assert(GameAssets::begin());assert(FontResource::ins().begin());
    std::vector<uint16_t> pixels(368*448);auto& c=PixelRenderer::canvas();c.attach({pixels.data(),368,448,false});c.setCoordinateScale(1);c.setLayoutScale(1);c.setAssetScale(1);c.setNativeText(true);
    AmoledV1::AmoledApp app;app.begin(0);AmoledV1::AmoledAppTestAccess::run(app,blobs,resources);
}
