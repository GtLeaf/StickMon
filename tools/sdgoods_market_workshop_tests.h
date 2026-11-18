#pragma once
#if __has_include("ui/common/MarketWorkshopUi.h")
#include "ui/common/MarketWorkshopUi.h"
#define HAS_FRAME_GIFT 1
#endif

static void workshopControllerTests(const std::string& out){
#if HAS_FRAME_GIFT
    using W=Ui::MarketWorkshopUi;using P=W::Page;using R=W::Request;
    Game::GameState state{};state.coins=0;state.room.ownedFurniture=0x4000;state.room.placedFurniture=0x4000;
    const auto before=state;W ui({360,360,true});ui.open(state);ui.update(400);
    assert(ui.page()==P::DIALOGUE&&ui.dialogueIndex()==0);
    uint32_t time=1000;
    const auto event=[&](Type type,int x=180,int y=260){time+=40;return ui.touch({type,int16_t(x),int16_t(y),time},state);};
    const auto tap=[&](){event(Type::DOWN);return event(Type::UP);};
    assert(event(Type::UP)==R::NONE&&ui.dialogueIndex()==0);
    assert(tap()==R::NONE&&ui.dialogueIndex()==1);renderContract(ui,state);
    assert(tap()==R::NONE&&ui.dialogueIndex()==2);
    assert(tap()==R::GRANT&&ui.pending()&&!Game::MarketWorkshop::hasFrame(state));
    assert(tap()==R::NONE&&ui.pending());ui.committed(false);assert(ui.page()==P::DIALOGUE&&!ui.pending());
    assert(tap()==R::GRANT);
    const auto failed=Game::MarketTransaction::commit(state,[](auto& s){return Game::MarketWorkshop::grantFrame(s);},[]{return false;},[](auto){assert(false);});
    assert(failed.status==Game::MarketTransaction::Status::SAVE_FAILED&&state.room.ownedFurniture==before.room.ownedFurniture);
    ui.committed(false);assert(tap()==R::GRANT);
    const auto paid=Game::MarketTransaction::commit(state,[](auto& s){return Game::MarketWorkshop::grantFrame(s);},[]{return true;},[](auto){assert(false);});
    assert(paid.status==Game::MarketTransaction::Status::COMMITTED&&Game::MarketWorkshop::hasFrame(state));
    assert(state.coins==0&&state.room.placedFurniture==before.room.placedFurniture&&(state.room.ownedFurniture&0x4000));
    ui.committed(true);assert(ui.page()==P::REWARD);SfxCue cue;assert(ui.takeSfx(cue)&&cue==SfxCue::ITEM_GET);assert(!ui.takeSfx(cue));
    assert(event(Type::UP)==R::NONE&&ui.page()==P::REWARD);ui.update(60000);assert(ui.page()==P::REWARD);renderContract(ui,state);
    event(Type::DOWN);event(Type::MOVE,180,220);event(Type::UP);assert(ui.page()==P::REWARD);
    event(Type::DOWN);ui.cancelTouch();event(Type::UP);assert(ui.page()==P::REWARD);
    tap();assert(!ui.active());ui.open(state);ui.update(400);assert(ui.dialogueIndex()==2);assert(tap()==R::NONE&&!ui.active());
    assert(Game::MarketWorkshop::grantFrame(state)==Game::MarketWorkshop::Result::ALREADY);
    W rectangular;rectangular.open(state);rectangular.update(400);assert(rectangular.dialogueIndex()==2);
    (void)out;
    std::puts("workshop: three dialogues, durable once-only ownership, zero coins, failed save/retry, old-UP/drag/cancel/repeat and clip/swap OK");
#else
    (void)out;assert(false&&"Kangaskhan first-visit photo-frame gift is not implemented");
#endif
}

static void workshopJourney(DesktopPlatform& desktop,GameEngine& engine,Blobs& blobs,Display& display,const std::string& out){
    using W=Ui::MarketWorkshopUi;using P=W::Page;
    auto& state=engine.gameState();const auto original=state;
    state.coins=0;state.room.ownedFurniture=0x4000;state.room.placedFurniture=0x4000;
    // The first game's opening day is valid in both Release and Debug.
    state.gameMinutesTotal=0;assert(engine.saveNow());
    engine.switchScene(SceneID::EXPLORE,false);auto& scene=*static_cast<ExploreScene*>(engine.currentScene.get());
    scene.beginMarket();assert(scene.market_);Journey j{desktop,engine,scene,out};j.advance(800);
    const auto open=[&](){
        int keeper=-1;for(int i=0;i<scene.market_->data().residentCount;++i)if(scene.market_->actor(i).species==115)keeper=i;
        assert(keeper>=0&&marketStandNear(*scene.market_,keeper));scene.marketUi_.chatTarget=keeper;scene.marketUi_.refresh(*scene.market_);
        j.tap(180,304);assert(scene.marketServicePending_&&!scene.marketUi_.dialogue().active());j.advance(1500);
        assert(scene.marketWorkshopUi_.active()&&scene.market_->conversing());
    };
    open();assert(scene.marketWorkshopUi_.dialogueIndex()==0&&!Game::MarketWorkshop::hasFrame(state));j.shot("workshop-01-welcome");
    // Lock before receiving the gift preserves first-visit eligibility.
    j.event(Type::DOWN,180,260);display.isLocked=true;j.advance(66);assert(!scene.marketStallActive()&&!scene.market_->conversing());
    display.isLocked=false;j.event(Type::UP,180,260);open();assert(scene.marketWorkshopUi_.dialogueIndex()==0);
    j.tap(180,260);assert(scene.marketWorkshopUi_.dialogueIndex()==1);j.shot("workshop-02-gift");
    j.tap(180,260);assert(scene.marketWorkshopUi_.dialogueIndex()==2&&!Game::MarketWorkshop::hasFrame(state));j.shot("workshop-03-opening");
    blobs.failAll=true;j.tap(180,260);blobs.failAll=false;
    assert(scene.marketWorkshopUi_.page()==P::DIALOGUE&&!Game::MarketWorkshop::hasFrame(state));j.shot("workshop-04-save-failed");
    blobs.delay=350;j.tap(180,260);blobs.delay=0;
    assert(scene.marketWorkshopUi_.page()==P::REWARD&&state.room.ownedFurniture==0x4001&&state.room.placedFurniture==0x4000&&state.coins==0);savedMatches(engine);
    j.event(Type::UP,180,260);assert(scene.marketWorkshopUi_.page()==P::REWARD);j.advance(5000);j.shot("workshop-05-received");
    j.event(Type::DOWN,180,260);scene.cancelTouch();j.event(Type::UP,180,260);assert(scene.marketWorkshopUi_.page()==P::REWARD);
    j.tap(180,100);assert(!scene.marketStallActive()&&!scene.market_->conversing());j.shot("workshop-06-dismissed");
    open();assert(scene.marketWorkshopUi_.dialogueIndex()==2);j.tap(180,260);assert(!scene.marketStallActive()&&!scene.market_->conversing());
    // A fresh SaveManager decode and a later market period retain ownership.
    SaveManager saves;assert(saves.begin());Game::GameState loaded;MainSceneViewState view{};assert(saves.load(loaded,view));
    state=loaded;state.gameMinutesTotal+=10*1440;scene.resetMarketStall();scene.market_->endConversation();
    scene.marketWorkshopUi_.open(state);scene.marketWorkshopUi_.update(400);assert(scene.marketWorkshopUi_.dialogueIndex()==2);
    display.isLocked=true;j.advance(66);display.isLocked=false;assert(!scene.marketStallActive()&&state.room.ownedFurniture==0x4001);
    scene.onExit();state=original;assert(engine.saveNow());
    std::puts("workshop actual round engine: service entry, three dialogues, failed/slow durable save, lock/cancel, dismissal, repeat/reload and period persistence OK");
}
