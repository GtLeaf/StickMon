#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "platform/desktop/DesktopPlatform.h"
#define private public
#include "core/GameEngine.h"
#include "scenes/ExploreScene.h"
#undef private
#include "market_dialogue_fixture.h"
#include "core/SaveManager.h"
#include "core/FontResource.h"
#include "core/ContentStore.h"
#include "core/ContentValidation.h"
#include "game/MonsterFactory.h"
#include "game/MarketShopService.h"
#include "game/MarketTransaction.h"
#include "ui/SceneSurface.h"
#include "presentation/PixelRenderer.h"
#if __has_include("ui/round_touch/MarketShopUi.h")
#include "ui/round_touch/MarketShopUi.h"
#define HAS_ROUND_MARKET 1
#else
#define HAS_ROUND_MARKET 0
#endif
// Production only consumes raw DEFLATE; envelope checksum entrypoints are unused.
extern "C" unsigned int uzlib_crc32(const void*, unsigned int, unsigned int) { assert(false); return 0; }
extern "C" unsigned int uzlib_adler32(const void*, unsigned int, unsigned int) { assert(false); return 0; }
using Type=Platform::IInputDevice::TouchType;
using Event=Platform::IInputDevice::TouchEvent;
class Blobs final:public Platform::IBlobStore {
public:
    explicit Blobs(Platform::IBlobStore& b):b(b){}
    bool failAll=false,failHistory=false,failPrimary=false;int failedHistory=0;uint32_t delay=0;DesktopPlatform* clock=nullptr;
    bool initialize()override{return b.initialize();}
    size_t blobSize(const char* n,const char* k)override{return b.blobSize(n,k);}
    bool readBlob(const char*n,const char*k,void*d,size_t s)override{return b.readBlob(n,k,d,s);}
    bool writeBlob(const char*n,const char*k,const void*d,size_t s)override{
        if(delay&&clock)clock->advanceMs(delay);
        if(failHistory&&std::strcmp(k,"encounters")==0){++failedHistory;return false;}
        if(failPrimary&&(std::strcmp(k,"state_a")==0||std::strcmp(k,"state_b")==0))return false;
        return !failAll&&b.writeBlob(n,k,d,s);
    }
    bool removeBlob(const char*n,const char*k)override{return b.removeBlob(n,k);}
    bool clearNamespace(const char*n)override{return b.clearNamespace(n);}
private:Platform::IBlobStore& b;
};
class Display final:public Platform::IDisplayDevice {
public:
    explicit Display(Platform::IDisplayDevice& d):d(d){}
    bool isLocked=false;
    Platform::FrameBuffer565 frameBuffer()override{return d.frameBuffer();}
    void present()override{d.present();}
    void setBrightness(uint8_t b)override{d.setBrightness(b);}
    uint8_t brightness()const override{return d.brightness();}
    void sleep()override{isLocked=true;}
    bool locked()const override{return isLocked;}
private:Platform::IDisplayDevice& d;
};
static void savedMatches(GameEngine& engine){
    SaveManager saves;assert(saves.begin());Game::GameState loaded;MainSceneViewState view{};assert(saves.load(loaded,view));
    const auto& current=engine.gameState();
    assert(loaded.coins==current.coins);
    assert(loaded.marketTutorGifted==current.marketTutorGifted);
    assert(loaded.marketCare.periodDay==current.marketCare.periodDay&&loaded.marketCare.used==current.marketCare.used);
    for(int i=0;i<current.marketCare.used;++i)assert(loaded.marketCare.served[i].ivPacked==current.marketCare.served[i].ivPacked&&loaded.marketCare.served[i].metAt==current.marketCare.served[i].metAt);
    for(int i=0;i<current.storageCount;++i)assert(loaded.storage[i].affection==current.storage[i].affection&&loaded.storage[i].mood==current.storage[i].mood&&loaded.storage[i].bond==current.storage[i].bond);
    assert(std::memcmp(&loaded.marketTutor,&current.marketTutor,sizeof(loaded.marketTutor))==0);
    for(int id=0;id<int(Game::ItemId::COUNT);++id)assert(Game::ItemInventory::count(loaded,Game::ItemId(id))==Game::ItemInventory::count(current,Game::ItemId(id)));
    for(int i=0;i<current.teamCount;++i){
        assert(loaded.team[i].affection==current.team[i].affection&&loaded.team[i].mood==current.team[i].mood&&loaded.team[i].bond==current.team[i].bond);
        assert(loaded.team[i].move1Id==current.team[i].move1Id&&loaded.team[i].move2Id==current.team[i].move2Id&&loaded.team[i].move3Id==current.team[i].move3Id);
        assert(!std::memcmp(loaded.team[i].moveProficiency,current.team[i].moveProficiency,sizeof(current.team[i].moveProficiency)));
    }
    assert(loaded.marketSeason.periodDay==current.marketSeason.periodDay&&loaded.marketSeason.boxRoll==current.marketSeason.boxRoll);
    assert(!std::memcmp(loaded.marketSeason.items,current.marketSeason.items,sizeof(current.marketSeason.items)));
    assert(!std::memcmp(loaded.marketSeason.stock,current.marketSeason.stock,sizeof(current.marketSeason.stock)));
}
using UI=Ui::RoundMarketShopUi;
using Page=UI::Page;
struct UiDriver {
    Game::GameState state{}; UI ui; uint32_t time=1000;
    UiDriver(){state.coins=20000;Game::MarketShop::ensurePeriod(state);ui.open(state);advance(4000);}
    void advance(int ms){while(ms){const auto step=std::min(ms,66);ui.update(step);ms-=step;}}
    UI::Request event(Type type,int x,int y,int dt=40){time+=dt;return ui.touch({type,int16_t(x),int16_t(y),time},state);}
    UI::Request tap(int x,int y){event(Type::DOWN,x,y);return event(Type::UP,x,y);}
    void shelf(){tap(180,190);assert(ui.page()==Page::SHELF);}
    void box(){shelf();event(Type::DOWN,180,280);event(Type::MOVE,180,-80,100);event(Type::UP,180,160,200);
        // Leaving the circle cancels this gesture rather than selecting a row.
        assert(ui.page()==Page::SHELF);
        for(int n=0;n<4;++n){event(Type::DOWN,180,280);event(Type::MOVE,180,160,100);event(Type::UP,180,160,200);advance(1000);}
        assert(ui.scrollOffset()==360);tap(180,270);assert(ui.page()==Page::CONFIRM&&ui.selected()==Game::MarketShop::BOX);}
};
static void controllerTests(){
    UiDriver d;assert(d.ui.page()==Page::MENU);d.event(Type::UP,180,190);assert(d.ui.page()==Page::MENU);
    d.shelf();d.event(Type::DOWN,180,280);d.event(Type::MOVE,180,220,40);d.event(Type::UP,180,200,20);
    const int release=d.ui.scrollOffset();assert(d.ui.page()==Page::SHELF&&release>0);d.advance(66);assert(d.ui.scrollOffset()>release);
    d.tap(180,200);assert(d.ui.page()==Page::SHELF); // inertia brake owns whole contact
    d.tap(180,100);assert(d.ui.page()==Page::MENU);d.tap(180,258);assert(d.ui.page()==Page::ADVICE);
    for(int i=0;i<3;++i){auto r=d.tap(180,185+46*i);assert(r.kind==UI::Request::ADVISE&&uint8_t(r.advice)==i+1);
        assert(d.event(Type::UP,180,185+46*i).kind==UI::Request::NONE);d.ui.adviceCommitted(false);}
    d.ui.adviceCommitted(true);assert(d.ui.page()==Page::ADVICE_DONE);d.tap(180,308);assert(d.ui.page()==Page::MENU);
    d.tap(0,0);assert(d.ui.page()==Page::MENU);d.tap(180,100);d.advance(600);assert(!d.ui.active());
    d.ui.open(d.state,false);d.advance(600);assert(d.ui.page()==Page::ERROR);d.tap(180,190);assert(d.ui.page()==Page::ERROR);d.tap(180,308);d.advance(600);assert(!d.ui.active());
    UiDriver refused;refused.shelf();refused.state.marketSeason.stock[0]=0;refused.tap(180,182);assert(refused.ui.page()==Page::SHELF);
    refused.state.marketSeason.stock[0]=1;refused.tap(180,182);assert(refused.ui.page()==Page::CONFIRM);
    const auto item=Game::MarketShop::item(refused.state,0);refused.state.coins=0;
    assert(refused.tap(232,308).kind==UI::Request::NONE&&!refused.ui.pending());refused.state.coins=20000;
    assert(Game::ItemInventory::add(refused.state,item,uint8_t(Game::ITEM_STACK_CAP-Game::ItemInventory::count(refused.state,item))));assert(refused.tap(232,308).kind==UI::Request::NONE&&!refused.ui.pending());
    ++refused.state.marketSeason.periodDay;assert(refused.tap(232,308).kind==UI::Request::NONE&&!refused.ui.pending());
    UiDriver b;b.box();auto r=b.tap(232,308);assert(r.kind==UI::Request::BUY);b.ui.committed(false);assert(b.ui.page()==Page::CONFIRM);
    assert(b.tap(232,308).kind==UI::Request::BUY);Game::MarketShop::Reward reward;
    assert(Game::MarketShop::buy(b.state,r.slot,r.period,&reward)==Game::MarketShop::Result::OK);const auto paid=b.state;
    b.ui.committed(true,reward.item,reward.amount);assert(b.ui.page()==Page::BOX_ARRIVE);b.event(Type::UP,232,308);
    b.advance(599);assert(b.ui.page()==Page::BOX_ARRIVE);b.advance(1);assert(b.ui.page()==Page::BOX_WAIT);
    b.event(Type::UP,180,232);b.tap(180,100);b.advance(60000);assert(b.ui.page()==Page::BOX_WAIT);
    b.event(Type::DOWN,180,232);b.event(Type::MOVE,180,200);b.event(Type::UP,180,232);assert(b.ui.page()==Page::BOX_WAIT);
    b.event(Type::DOWN,180,232);b.ui.cancelTouch();b.event(Type::UP,180,232);assert(b.ui.page()==Page::BOX_WAIT);
    b.tap(180,232);assert(b.ui.page()==Page::BOX_OPEN);b.advance(1249);assert(b.ui.page()==Page::BOX_OPEN);b.advance(1);assert(b.ui.page()==Page::RESULT);
    assert(!std::memcmp(&paid,&b.state,sizeof(paid)));b.event(Type::UP,180,308);assert(b.ui.page()==Page::RESULT);b.tap(180,308);assert(b.ui.page()==Page::SHELF&&b.ui.scrollOffset()==360);
    std::puts("round-market: native UI inertia/brake/circle, advice, failed save, explicit box and once-only state OK");
}
static void capture(const std::string& out,const char* name){
    auto* f=std::fopen((out+"/"+name+".ppm").c_str(),"wb");assert(f);std::fputs("P6\n360 360\n255\n",f);
    for(int y=0;y<360;++y)for(int x=0;x<360;++x){auto p=PixelRenderer::canvas().readPixel(x,y);
        if((2*x-359)*(2*x-359)+(2*y-359)*(2*y-359)>=360*360)p=0;
        const unsigned char rgb[]={uint8_t(((p>>11)&31)*255/31),uint8_t(((p>>5)&63)*255/63),uint8_t((p&31)*255/31)};std::fwrite(rgb,1,3,f);
    }assert(!std::fclose(f));
}
struct Journey {
    DesktopPlatform& desktop;GameEngine& engine;ExploreScene& scene;std::string out;
    void advance(int ms){while(ms>0){const int step=std::min(ms,66);desktop.advanceMs(step);scene.update(desktop.millis(),step/1000.f);ms-=step;}}
    void event(Type type,int x,int y,int dt=40){desktop.advanceMs(dt);Event e{type,int16_t(x),int16_t(y),desktop.millis()};
        scene.prepareSurface();Ui::SceneSurface::InputScope scope(desktop.frameBuffer());assert(Ui::SceneSurface::mapTouch(e));scene.onTouch(e);}
    void tap(int x,int y){event(Type::DOWN,x,y);event(Type::UP,x,y);}
    void shot(const char* name){scene.prepareSurface();assert(Ui::SceneSurface::begin(desktop.frameBuffer()));scene.render();Ui::SceneSurface::finish();capture(out,name);}
    void open(bool initialized=true){int keeper=-1;for(int i=0;i<scene.market_->data().residentCount;++i)if(scene.market_->actor(i).species==Game::MarketShop::STALL_SPECIES)keeper=i;
        assert(keeper>=0&&marketStandNear(*scene.market_,keeper));scene.marketUi_.chatTarget=keeper;scene.marketUi_.refresh(*scene.market_);
        tap(180,304);assert(scene.marketServicePending_&&scene.market_->conversing()&&!scene.marketUi_.dragging);
        advance(4000);assert(scene.marketShopUi_.page()==(initialized?Page::MENU:Page::ERROR)&&!scene.marketServicePending_);}
    void shelf(){tap(180,190);assert(scene.marketShopUi_.page()==Page::SHELF);}
    void toBox(){for(int n=0;n<4;++n){event(Type::DOWN,180,280);event(Type::MOVE,180,160,100);event(Type::UP,180,160,200);advance(1000);}assert(scene.marketShopUi_.scrollOffset()==360);tap(180,270);assert(scene.marketShopUi_.selected()==Game::MarketShop::BOX&&scene.marketShopUi_.page()==Page::CONFIRM);}
};
template<class Stall> static void renderContract(Stall& ui,const Game::GameState& state,const Ui::MarketCoinMotion* coins=nullptr){
    auto& c=PixelRenderer::canvas();const auto old=Platform::display().frameBuffer();std::vector<uint16_t> pixels(360*360),expected(360*360);
    for(bool swapped:{false,true}){
        c.attach({pixels.data(),360,360,swapped});c.setCoordinateScale(1);c.setLayoutScale(1);c.setAssetScale(1);c.setNativeText(false);c.clearClipRect();c.fillSprite(0x31a5);ui.render(state,coins);
        for(int y=0;y<360;++y)for(int x=0;x<360;++x){const auto p=c.readPixel(x,y);if(!swapped)expected[y*360+x]=p;else assert(p==expected[y*360+x]);}
        // Reconstruct the same complete image in clipped bands. Renderers must
        // preserve each caller's clip so later components stay in that band.
        c.fillSprite(0x31a5);
        for(int y=0;y<360;y+=13){const int h=std::min(13,360-y);c.setClipRect(0,y,360,h);ui.render(state,coins);assert(c.clipTop()==y&&c.clipBottom()==y+h);}
        c.clearClipRect();for(int y=0;y<360;++y)for(int x=0;x<360;++x)assert(c.readPixel(x,y)==expected[y*360+x]);
    }c.attach(old);c.setCoordinateScale(1);c.setLayoutScale(1);c.setAssetScale(1);
}
static void journeyTests(DesktopPlatform& desktop,GameEngine& engine,Blobs& blobs,Display& display,const std::string& out){
    engine.switchScene(SceneID::EXPLORE,false);auto& scene=*static_cast<ExploreScene*>(engine.currentScene.get());assert(scene.beginMarket());Journey j{desktop,engine,scene,out};auto& state=engine.gameState();
    j.advance(1000);
    state.marketSeason.periodDay=Game::MARKET_PERIOD_NONE;assert(engine.saveNow());
    blobs.failAll=true;j.open(false);assert(state.marketSeason.periodDay==Game::MARKET_PERIOD_NONE);j.shot("00-init-failed");
    j.tap(180,190);assert(scene.marketShopUi_.page()==Page::ERROR);j.tap(180,308);j.advance(600);assert(!scene.market_->conversing());blobs.failAll=false;
    j.open();j.shot("01-home");renderContract(scene.marketShopUi_,state);
    j.shelf();j.shot("02-shelf");renderContract(scene.marketShopUi_,state);
    j.tap(180,182);assert(scene.marketShopUi_.page()==Page::CONFIRM);j.shot("03-detail");renderContract(scene.marketShopUi_,state);
    const auto before=state;const auto id=Game::MarketShop::item(state,0);const auto qty=Game::ItemInventory::count(state,id);const auto price=Game::MarketShop::priceForSlot(state,0);
    blobs.failAll=true;j.tap(232,308);assert(scene.marketShopUi_.page()==Page::CONFIRM&&state.coins==before.coins&&state.marketSeason.stock[0]==before.marketSeason.stock[0]);
    j.shot("04-save-failed");blobs.failAll=false;
    blobs.delay=350;j.tap(232,308);blobs.delay=0;assert(scene.marketShopUi_.page()==Page::RESULT&&state.coins==before.coins-price&&Game::ItemInventory::count(state,id)==qty+1);
    assert(scene.marketCoins_.frame().rolling&&scene.marketCoins_.frame().tags[0].elapsed==0);savedMatches(engine);j.event(Type::UP,232,308,0);assert(state.coins==before.coins-price);
    j.advance(66);assert(scene.marketCoins_.frame().rolling&&scene.marketCoins_.frame().tags[0].elapsed==66);j.shot("05-purchased");renderContract(scene.marketShopUi_,state,&scene.marketCoins_);
    j.advance(1000);j.tap(180,308);assert(scene.marketShopUi_.page()==Page::SHELF);j.tap(180,100);j.tap(180,258);assert(scene.marketShopUi_.page()==Page::ADVICE);j.shot("06-advice");renderContract(scene.marketShopUi_,state);
    j.tap(180,278);assert(scene.marketShopUi_.page()==Page::ADVICE_DONE&&state.marketSupply.pendingAdvice==3);savedMatches(engine);assert(!scene.marketCoins_.busy());j.shot("07-advice-done");
    j.tap(180,308);j.shelf();j.toBox();j.shot("08-box-detail");const auto boxBefore=state;const auto reward=Game::MarketShop::boxReward(state);const auto rewardBefore=Game::ItemInventory::count(state,reward.item);
    j.tap(232,308);assert(scene.marketShopUi_.page()==Page::BOX_ARRIVE&&state.coins==boxBefore.coins-Game::MarketShop::BOX_PRICE);savedMatches(engine);j.shot("09-box-arrival");
    j.advance(600);assert(scene.marketShopUi_.page()==Page::BOX_WAIT);j.shot("10-box-wait");renderContract(scene.marketShopUi_,state,&scene.marketCoins_);
    scene.market_->pause(true);const auto frozen=scene.marketCoins_.frame().tags[0].elapsed;j.advance(10000);assert(scene.marketShopUi_.page()==Page::BOX_WAIT&&scene.marketCoins_.frame().tags[0].elapsed==frozen);scene.market_->pause(false);
    j.event(Type::UP,180,232);assert(scene.marketShopUi_.page()==Page::BOX_WAIT);j.tap(180,232);j.advance(500);j.shot("11-box-stars");renderContract(scene.marketShopUi_,state,&scene.marketCoins_);
    j.advance(750);assert(scene.marketShopUi_.page()==Page::RESULT&&Game::ItemInventory::count(state,reward.item)==rewardBefore+reward.amount);j.shot("12-box-reward");renderContract(scene.marketShopUi_,state);
    const auto paid=state;scene.cancelTouch();j.event(Type::UP,180,308);assert(scene.marketShopUi_.page()==Page::RESULT);j.tap(180,308);j.tap(180,100);j.tap(180,100);j.advance(600);
    assert(!scene.marketShopUi_.active()&&!scene.market_->conversing()&&!scene.marketUi_.dragging);j.event(Type::MOVE,180,280);j.event(Type::UP,180,280);assert(!scene.marketUi_.dragging);
    j.open();j.shelf();assert(state.marketSeason.stock[Game::MarketShop::BOX]==paid.marketSeason.stock[Game::MarketShop::BOX]);j.shot("13-stock-retained");
    const auto beforeLock=state;
    j.tap(180,182);assert(scene.marketShopUi_.page()==Page::CONFIRM);j.event(Type::DOWN,232,308);
    display.isLocked=true;scene.cancelTouch();
    assert(!scene.marketShopUi_.active()&&!scene.market_->conversing());j.advance(1000);
    assert(!scene.marketShopUi_.active()&&!scene.market_->conversing()&&!scene.marketCoins_.busy());
    display.isLocked=false;j.event(Type::UP,232,308);assert(state.coins==beforeLock.coins);
    // Other stalls still hand over to ordinary dialogue in this batch.
    for(const auto species:{113}){
        int target=-1;for(int i=0;i<scene.market_->data().residentCount;++i)if(scene.market_->actor(i).species==species)target=i;
        assert(target>=0&&marketStandNear(*scene.market_,target));scene.marketUi_.chatTarget=target;scene.marketUi_.refresh(*scene.market_);j.tap(180,304);
        assert(!scene.marketUi_.dialogue().active()&&scene.marketServicePending_);
        scene.resetMarketStall();scene.market_->endConversation();scene.marketUi_.reset();scene.marketUi_.enableDialogue();
    }
    scene.onExit();assert(!scene.marketShopUi_.active()&&!scene.marketCoins_.busy()&&!scene.marketServicePending_);savedMatches(engine);
    std::puts("round-market: actual engine chat/facing/menu/purchase/advice/box, slow and failed saves, pause/exit/reentry, clipped/swapped native renders OK");
}

#include "ui/round_touch/ExploreSelectUi.h"
#include "sdgoods_market_tutor_tests.h"
#include "sdgoods_market_care_tests.h"
#include "sdgoods_market_entry_lock_tests.h"
#include "sdgoods_explore_card_tests.h"

int main(int argc,char**argv){
    assert(argc==5);assert(std::string(argv[4])==(STICKMON_EXTERNAL_CONTENT?"external":"embedded"));DesktopPlatform desktop(argv[1]);auto services=desktop.serviceBundle();Blobs blobs(services.blobs);blobs.clock=&desktop;Display display(services.display);
    Platform::Services bundle{services.lifecycle,services.clock,services.logger,services.input,display,services.audio,services.imu,services.power,blobs,services.resources,services.memory,services.peers};
    Platform::bind(bundle);assert(desktop.begin());
#if STICKMON_EXTERNAL_CONTENT
    assert(Platform::resources().mount());ContentStore catalog;ContentData::bind(&catalog);
    assert(validateContentCatalog() && catalog.healthy());
#endif
    auto& engine=GameEngine::ins();assert(engine.begin());
    auto& state=engine.gameState();state.oobeDone=true;state.coins=10000;state.teamCount=1;state.team[0]=Game::MonsterFactory::create(25,25);state.team[0].satiety=100;state.settings.idleTimeoutIndex=4;
    blobs.failHistory=true;
    assert(engine.saveNow() && "A durable game snapshot must survive encounter-history failure");
    assert(blobs.failedHistory>0&&engine.saveCoordinator.dirty());savedMatches(engine);
    blobs.failHistory=false;assert(engine.saveNow()&&!engine.saveCoordinator.dirty());
    if(!BuildConfig::DEBUG_FEATURES){careRelease(desktop,engine);return 0;}
    Game::MarketShop::ensurePeriod(state);assert(engine.saveNow());
    const auto before=state.coins;blobs.failAll=true;
    const auto rejected=Game::MarketTransaction::commit(state,[&](Game::GameState& s){return Game::MarketShop::buy(s,0,s.marketSeason.periodDay);},[&](){return engine.saveNow();},[](auto){assert(false);});
    assert(rejected.status==Game::MarketTransaction::Status::SAVE_FAILED&&state.coins==before);
    blobs.failAll=false;
    blobs.failPrimary=true;engine.encounterHistoryDirty=true;blobs.failHistory=true;int notices=0;
    const auto committed=Game::MarketTransaction::commit(state,[&](Game::GameState& s){return Game::MarketShop::buy(s,0,s.marketSeason.periodDay);},[&](){return engine.saveNow();},[&](auto){++notices;});
    assert(committed.status==Game::MarketTransaction::Status::COMMITTED&&notices==1&&engine.saveCoordinator.dirty());savedMatches(engine);
    blobs.failHistory=false;blobs.failPrimary=false;assert(engine.saveNow());
    std::puts("round-market: snapshot/history durability and total-write rollback OK");
    if(std::string(argv[3])=="save"){engine.currentScene.reset();return 0;}
#if HAS_ROUND_MARKET
    for(int id=int(Game::ItemId::HP_UP);id<=int(Game::ItemId::LUM_BERRY);++id)
        assert(GameAssets::drawItemIcon48(Game::ItemId(id),180,180));
    assert(GameAssets::drawCentered(GameAssets::Kind::MARKET_MYSTERY_BOX,180,180));
    assert(Ui::RoundExplore::PREVIOUS_BOUNDS.y+Ui::RoundExplore::PREVIOUS_BOUNDS.height/2==180&&Ui::RoundExplore::NEXT_BOUNDS.y+Ui::RoundExplore::NEXT_BOUNDS.height/2==180);
    exploreCardTests(desktop,engine,argv[2]);
    controllerTests();journeyTests(desktop,engine,blobs,display,argv[2]);
    tutorJourney(desktop,engine,blobs,display,argv[2]);
    careJourney(desktop,engine,blobs,display,argv[2]);
    marketSelectorTests(engine,argv[2]);
    marketLockTests(desktop,engine,blobs,display,argv[2]);
    // Prior nonmarket pixel baseline is intentionally superseded by the approved
    // all-scene card design; its frozen sources remain in batch4 artifacts.
#ifdef STICKMON_TUTOR_BASELINE
    tutorPerformanceParity(argv[2]);
#endif
    // Destroy platform-bound scene resources before the local service bundle.
    engine.currentScene.reset();
#else
    assert(false&&"Round Kecleon service is not connected yet");
#endif
}
