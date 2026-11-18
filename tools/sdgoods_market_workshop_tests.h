#pragma once
#if __has_include("ui/common/MarketWorkshopUi.h")
#include "ui/common/MarketWorkshopUi.h"
#include "ui/round_touch/ExploreRouteUi.h"
#define HAS_FRAME_GIFT 1
#endif

static void workshopControllerTests(const std::string& out){
#if HAS_FRAME_GIFT
    using W=Ui::MarketWorkshopUi;using P=W::Page;using R=W::Request;
    Game::GameState state{};state.coins=0;state.room.ownedFurniture=0x4000;state.room.placedFurniture=0x4000;
    const auto before=state;W ui({360,360,true});ui.open(state);ui.update(400);
    assert(ui.page()==P::DIALOGUE&&ui.dialogueIndex()==0);
    uint32_t time=1000;
    const auto event=[&](Type type,int x=180,int y=285){time+=40;return ui.touch({type,int16_t(x),int16_t(y),time},state);};
    const auto tap=[&](){event(Type::DOWN);return event(Type::UP);};
    assert(event(Type::UP)==R::NONE&&ui.dialogueIndex()==0);
    assert(tap()==R::NONE&&ui.dialogueIndex()==0&&"First tap finishes the typewriter");
    const auto next=[&](unsigned index){
        for(int i=0;i<8&&ui.dialogueIndex()==index;++i)assert(tap()==R::NONE);
        assert(ui.dialogueIndex()==index+1);
    };
    const auto grant=[&](){
        for(int i=0;i<8;++i)if(tap()==R::GRANT)return true;
        return false;
    };
    next(0);renderContract(ui,state);next(1);
    assert(grant()&&ui.pending()&&!Game::MarketWorkshop::hasFrame(state));
    assert(tap()==R::NONE&&ui.pending());ui.committed(false);assert(ui.page()==P::DIALOGUE&&!ui.pending());
    assert(grant());
    const auto failed=Game::MarketTransaction::commit(state,[](auto& s){return Game::MarketWorkshop::grantFrame(s);},[]{return false;},[](auto){assert(false);});
    assert(failed.status==Game::MarketTransaction::Status::SAVE_FAILED&&state.room.ownedFurniture==before.room.ownedFurniture);
    ui.committed(false);assert(grant());
    const auto paid=Game::MarketTransaction::commit(state,[](auto& s){return Game::MarketWorkshop::grantFrame(s);},[]{return true;},[](auto){assert(false);});
    assert(paid.status==Game::MarketTransaction::Status::COMMITTED&&Game::MarketWorkshop::hasFrame(state));
    assert(state.coins==0&&state.room.placedFurniture==before.room.placedFurniture&&(state.room.ownedFurniture&0x4000));
    ui.committed(true);assert(ui.page()==P::DIALOGUE&&ui.active());
    ui.update(180);assert(ui.page()==P::DIALOGUE);renderContract(ui,state);
    ui.update(180);assert(ui.page()==P::REWARD);SfxCue cue;assert(ui.takeSfx(cue)&&cue==SfxCue::ITEM_GET);assert(!ui.takeSfx(cue));
    assert(event(Type::UP)==R::NONE&&ui.page()==P::REWARD);ui.update(60000);assert(ui.page()==P::REWARD);renderContract(ui,state);
    event(Type::DOWN);event(Type::MOVE,180,220);event(Type::UP);assert(ui.page()==P::REWARD);
    event(Type::DOWN);ui.cancelTouch();event(Type::UP);assert(ui.page()==P::REWARD);
    tap();assert(ui.active());ui.update(180);assert(ui.active());ui.update(180);assert(!ui.active());
    ui.open(state);ui.update(400);assert(ui.dialogueIndex()==2);
    for(int i=0;i<8&&!ui.exiting();++i)assert(tap()==R::NONE);
    assert(ui.exiting()&&ui.active());ui.update(180);assert(ui.active());ui.update(180);assert(!ui.active());
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
    j.shot("market-menu-button");
    const auto menuInk=PixelRenderer::rgb(247,246,233);
    assert(PixelRenderer::canvas().readPixel(224,34)==menuInk&&"The round market must show the explore menu button");
    const auto walkingView=Ui::MarketWalkUi::viewport();
    const auto worldBand=[](){
        std::vector<uint16_t> pixels;
        for(int y=80;y<160;++y)for(int x=60;x<300;++x)
            pixels.push_back(PixelRenderer::canvas().readPixel(x,y));
        return pixels;
    };
    {
        auto session=std::make_unique<Market::Session>(*scene.market_);
        assert(marketStandNear(*session,0));
        Ui::MarketWalkUi ordinary;
        ordinary.enableDialogue();
        ordinary.refresh(*session);
        assert(ordinary.chatTarget>=0);
        const auto now=desktop.millis();
        ordinary.touchSession(*session,{Type::DOWN,180,304,now},walkingView);
        ordinary.touchSession(*session,{Type::UP,180,304,now+40},walkingView);
        for(uint32_t delta=66;delta<=1320;delta+=66)ordinary.update(*session,now+delta);
        assert(ordinary.dialogue().visible());
        assert(ordinary.dialogue().textLayout().options().font==BitmapText::Font::PX20);
        assert(ordinary.dialogue().pageLineCount()==2);
        Ui::MarketView::render(*session,walkingView,session->timeMs(),state.gameMinutesTotal,ordinary.dialogue().environmentMs());
        ordinary.render(*session,walkingView,state.gameMinutesTotal);
        capture(j.out,"market-npc-dialogue");
        auto lowView=walkingView;
        const auto box=Ui::MarketDialogueUi::layout(lowView);
        lowView.cameraOffsetY=lowView.pixel(session->playerPose().y)-lowView.cameraY(*session)-box.y+10;
        Ui::MarketView::render(*session,lowView,session->timeMs(),state.gameMinutesTotal,ordinary.dialogue().environmentMs());
        const auto walkingPixels=worldBand();
        ordinary.render(*session,lowView,state.gameMinutesTotal);
        assert(worldBand()==walkingPixels&&"Ordinary NPC dialogue keeps the world camera even near the box");
    }
    const auto menuTap=[&](int row){
        j.tap(220,35);
        assert(!scene.marketUi_.dragging);
        const auto r=Ui::RoundExplore::routeMenuItem(row);
        j.tap(r.x+r.width/2,r.y+r.height/2);
    };
    menuTap(1);assert(scene.exploreSubViewOpen);j.shot("market-menu-team");scene.exploreSubView.resetNavigation();assert(scene.finishSubViewInput());
    j.tap(220,35); // Close the retained menu before reopening it.
    menuTap(2);assert(scene.exploreSubViewOpen);j.shot("market-menu-bag");scene.exploreSubView.resetNavigation();assert(scene.finishSubViewInput());j.tap(220,35);
    menuTap(0);assert(scene.photo_.viewfinder());j.shot("market-menu-viewfinder");
    const auto beforePhoto=scene.market_->timeMs();
    j.advance(400);assert(scene.market_->timeMs()>beforePhoto);
    scene.renderMarketPhotoWorld();
    auto& canvas=PixelRenderer::canvas();
    const auto center=canvas.readPixel(180,180);
    j.tap(180,324);assert(scene.photo_.page()==Ui::PhotoFlow::Page::PREVIEW);j.shot("market-menu-photo");
    assert(scene.photo_.image()[96*Photos::WIDTH+96]==center);
    const auto frozen=scene.market_->timeMs();
    j.advance(500);assert(scene.market_->timeMs()==frozen);
    j.tap(228,310);assert(!scene.photo_.active());
    Photos::Library photos;assert(photos.refresh()==Photos::Result::OK&&photos.count()==1&&photos.entry(0).area==Photos::MARKET_AREA);
    const auto open=[&](){
        int keeper=-1;for(int i=0;i<scene.market_->data().residentCount;++i)if(scene.market_->actor(i).species==115)keeper=i;
        assert(keeper>=0&&marketStandNear(*scene.market_,keeper));scene.marketUi_.chatTarget=keeper;scene.marketUi_.refresh(*scene.market_);
        j.tap(180,304);assert(scene.marketServicePending_&&!scene.marketUi_.dialogue().active());j.shot("workshop-menu-hidden");
        assert(PixelRenderer::canvas().readPixel(224,34)!=menuInk);
        j.advance(1500);
        assert(scene.marketWorkshopUi_.active()&&scene.market_->conversing());
    };
    open();assert(scene.marketWorkshopUi_.dialogueIndex()==0&&!Game::MarketWorkshop::hasFrame(state));j.shot("workshop-01-welcome");
    assert(canvas.readPixel(180,266)==PixelRenderer::rgb(248,248,224));
    assert(canvas.readPixel(180,259)!=PixelRenderer::rgb(248,248,224)&&"The round stall must use the low NPC dialogue box");
    const auto dialogueBand=worldBand();
    Ui::MarketView::render(*scene.market_,walkingView,scene.market_->timeMs(),state.gameMinutesTotal);
    assert(worldBand()==dialogueBand&&"Workshop dialogue must keep the walking camera position");
    Ui::RoundMarketShopUi cameraShop;
    cameraShop.open(state);cameraShop.update(180);
    assert(cameraShop.sceneShift(*scene.market_,walkingView)==0&&cameraShop.sceneShiftX(*scene.market_,walkingView)==0);
    cameraShop.update(180);
    for(uint32_t time=1000;time<1600&&cameraShop.page()==Page::GREET;time+=80){
        cameraShop.touch({Type::DOWN,180,285,time},state);
        cameraShop.touch({Type::UP,180,285,time+40},state);
    }
    assert(cameraShop.page()==Page::MENU);
    const int settledShift=Ui::RoundMarketStall::sceneShift(*scene.market_,walkingView);
    assert(settledShift>0&&cameraShop.sceneShift(*scene.market_,walkingView)==0);
    cameraShop.update(210);
    const int slidingShift=cameraShop.sceneShift(*scene.market_,walkingView);
    assert(slidingShift>0&&slidingShift<settledShift);
    cameraShop.update(210);
    assert(cameraShop.sceneShift(*scene.market_,walkingView)==settledShift);
    // Lock before receiving the gift preserves first-visit eligibility.
    j.event(Type::DOWN,180,260);display.isLocked=true;j.advance(66);assert(!scene.marketStallActive()&&!scene.market_->conversing());
    display.isLocked=false;j.event(Type::UP,180,260);open();assert(scene.marketWorkshopUi_.dialogueIndex()==0);
    const auto next=[&](unsigned index){
        for(int i=0;i<8&&scene.marketWorkshopUi_.dialogueIndex()==index;++i)j.tap(180,285);
        assert(scene.marketWorkshopUi_.dialogueIndex()==index+1);
        j.advance(900);
    };
    next(0);j.shot("workshop-02-gift");
    next(1);assert(!Game::MarketWorkshop::hasFrame(state));j.shot("workshop-03-opening");
    blobs.failAll=true;j.tap(180,260);blobs.failAll=false;
    assert(scene.marketWorkshopUi_.page()==P::DIALOGUE&&scene.marketWorkshopUi_.error_&&!Game::MarketWorkshop::hasFrame(state));
    j.advance(1000);j.shot("workshop-04-save-failed");
    blobs.delay=350;j.tap(180,285);blobs.delay=0;
    assert(scene.marketWorkshopUi_.page()==P::DIALOGUE&&state.room.ownedFurniture==0x4001&&state.room.placedFurniture==0x4000&&state.coins==0);savedMatches(engine);
    j.advance(180);assert(scene.marketWorkshopUi_.page()==P::DIALOGUE);j.shot("workshop-gift-dialogue-out");
    j.advance(180);assert(scene.marketWorkshopUi_.page()==P::REWARD);
    j.event(Type::UP,180,260);assert(scene.marketWorkshopUi_.page()==P::REWARD);j.advance(5000);j.shot("workshop-05-received");
    j.event(Type::DOWN,180,260);scene.cancelTouch();j.event(Type::UP,180,260);assert(scene.marketWorkshopUi_.page()==P::REWARD);
    const auto rewardCenter=PixelRenderer::canvas().readPixel(180,180);
    j.tap(180,100);assert(scene.marketStallActive()&&scene.market_->conversing());
    j.shot("workshop-reward-dismissed");
    assert(PixelRenderer::canvas().readPixel(180,180)!=rewardCenter&&"The reward frame must disappear on tap");
    j.advance(180);j.shot("workshop-reward-out");
    assert(scene.marketStallActive()&&scene.market_->conversing());j.advance(180);
    assert(!scene.marketStallActive()&&!scene.market_->conversing());j.shot("workshop-06-dismissed");
    open();assert(scene.marketWorkshopUi_.dialogueIndex()==2);
    for(int i=0;i<8&&!scene.marketWorkshopUi_.exiting();++i)j.tap(180,285);
    assert(scene.marketWorkshopUi_.exiting());j.advance(180);
    assert(scene.marketStallActive()&&scene.market_->conversing());j.shot("workshop-repeat-out");j.advance(180);
    assert(!scene.marketStallActive()&&!scene.market_->conversing());
    // A fresh SaveManager decode and a later market period retain ownership.
    SaveManager saves;assert(saves.begin());Game::GameState loaded;MainSceneViewState view{};assert(saves.load(loaded,view));
    state=loaded;state.gameMinutesTotal+=10*1440;scene.resetMarketStall();scene.market_->endConversation();
    scene.marketWorkshopUi_.open(state);scene.marketWorkshopUi_.update(400);assert(scene.marketWorkshopUi_.dialogueIndex()==2);
    display.isLocked=true;j.advance(66);display.isLocked=false;assert(!scene.marketStallActive()&&state.room.ownedFurniture==0x4001);
    menuTap(3);assert(scene.market_->exiting());
    scene.onExit();state=original;assert(engine.saveNow());
    std::puts("workshop actual round engine: reusable menu, team/bag, clean market camera/save/exit, menu hidden, dialogue/reward exits, failed/slow save and lock/reload OK");
}
