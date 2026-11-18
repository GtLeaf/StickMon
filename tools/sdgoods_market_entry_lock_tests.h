#pragma once
#include "PowerActions.h"
#include "DisplayLockRequest.h"
#include "game/ExploreDestinations.h"
#include "ui/round_touch/ExploreSelectUi.h"
#include "presentation/FrameDamage.h"

struct SelectorHost final:Ui::ExploreHost {
    Ui::ExploreRouteFrame route;Ui::ExploreInputFrame input;int departures=0;
    Ui::ExploreInputFrame exploreInputFrame()const override{return input;}
    Ui::ExploreRouteFrame exploreRouteFrame()const override{return route;}
    Ui::ExploreBattleFrame exploreBattleFrame()const override{return {};}
    Ui::ExploreTutorialFrame exploreTutorialFrame()const override{return {};}
    bool activateExplore(Ui::ExploreAction a,uint8_t=0)override{if(a==Ui::ExploreAction::DEPART)++departures;return true;}
    bool handleExploreProgressionButton(const ButtonEvent&)override{return false;}
    void renderExploreProgression()override{}
};
static void selectorContract(const Ui::ExploreRouteFrame& frame,const std::string& out,const char* name){
    auto& c=PixelRenderer::canvas();const auto old=Platform::display().frameBuffer();std::vector<uint16_t> pixels(360*360),expected(360*360);Ui::RoundExplore::Selection ui;
    for(bool swapped:{false,true}){
        c.attach({pixels.data(),360,360,swapped});c.setCoordinateScale(1);c.setLayoutScale(1);c.setAssetScale(1);c.setNativeText(false);c.clearClipRect();c.fillSprite(0x31a5);ui.render(frame);
        for(int y=0;y<360;++y)for(int x=0;x<360;++x){if(!swapped)expected[y*360+x]=c.readPixel(x,y);else assert(expected[y*360+x]==c.readPixel(x,y));}
        if(!swapped)capture(out,name);
        c.fillSprite(0x31a5);for(int y=0;y<360;y+=13){const int h=std::min(13,360-y);c.setClipRect(0,y,360,h);ui.render(frame);assert(c.clipTop()==y&&c.clipBottom()==y+h);}
        c.clearClipRect();for(int y=0;y<360;++y)for(int x=0;x<360;++x)assert(c.readPixel(x,y)==expected[y*360+x]);
    }c.attach(old);c.setCoordinateScale(1);c.setLayoutScale(1);c.setAssetScale(1);
}
static void marketSelectorTests(GameEngine& engine,const std::string& out){
    SelectorHost host;host.input.visibleAreaCount=2;host.route.visibleAreaCount=2;host.route.marketPreview=true;host.route.marketOpen=true;host.route.areaNames[0]=Ui::MarketText::NAME;host.route.areaNames[1]=Ui::Explore::GRASS_PATH;host.route.areaUnlocked[0]=host.route.areaUnlocked[1]=true;
    selectorContract(host.route,out,"entry-01-open");Ui::RoundExplore::Selection ui;Ui::ExploreUiState state;
    assert(ui.tap(host,state,Ui::RoundExplore::DEPART)&&host.departures==1);host.route.marketOpen=false;host.route.marketDaysUntilOpen=3;
    assert(!ui.tap(host,state,Ui::RoundExplore::DEPART)&&host.departures==1);selectorContract(host.route,out,"entry-02-closed");
    host.route.marketDaysUntilOpen=1;selectorContract(host.route,out,"entry-03-next-day");
    // Real model and parked-scene wake refresh status from the shared calendar.
    engine.switchScene(SceneID::EXPLORE,false);auto& scene=*static_cast<ExploreScene*>(engine.currentScene.get());scene.ui.state.areaCursor=ExploreDestinations::visibleIndex(ExploreDestinations::MARKET,engine.gameState());
    const auto frame=scene.exploreRouteFrame();assert(frame.marketPreview&&frame.marketOpen&&frame.marketDaysUntilOpen==0&&frame.areaPreviewCount==MarketMapData::get().residentCount);
    scene.update(Platform::clock().millis(),0);const auto day=scene.marketCountdownDay_;engine.gameState().gameMinutesTotal+=1440;
    const auto changed=scene.update(Platform::clock().millis()+1,0);assert(changed.redraw&&scene.marketCountdownDay_!=day);
    scene.onExit();std::puts("market-entry: actual cover warm open/closed/day state, depart guard, parked clock refresh and clip/swap OK");
}
struct NativeLockBoard {
    DesktopPlatform& desktop;Display& display;GameEngine& engine;Sdgoods::DisplayLockRequest pending;
    bool unlock=false,save=false;int locks=0;
    bool takeSaveRequest(){const bool v=save;save=false;return v;}
    bool takeUnlockRequest(){const bool v=unlock;unlock=false;return v;}
    bool takeShutdownRequest(){return false;}
    bool lockRequestReady()const{return pending.ready(desktop.millis());}
    void completeLockRequest(){assert(engine.currentId==SceneID::MAIN&&engine.exploreTravelPhase()==ExploreTravelPhase::NONE&&!engine.sceneFadeActive());pending.cancel();display.isLocked=true;++locks;}
    void powerOff(){assert(false);}
};
struct NativeLockSession {
    GameEngine& engine;Blobs& blobs;bool rejectFinalOnce=false;int saves=0;
    bool save(){++saves;const bool reject=rejectFinalOnce&&engine.currentId==SceneID::MAIN&&engine.exploreTravelPhase()==ExploreTravelPhase::NONE;if(reject){rejectFinalOnce=false;blobs.failAll=true;}
        const bool ok=engine.saveNow();if(reject)blobs.failAll=false;return ok;}
};
static ExploreScene& departMarket(DesktopPlatform& desktop,GameEngine& engine){
    engine.exploreTravel=ExploreTravelPhase::NONE;engine.switchScene(SceneID::MAIN,false);assert(engine.saveNow());engine.switchScene(SceneID::EXPLORE,false);assert(engine.beginExploreDeparture(ExploreDestinations::MARKET));
    for(int i=0;i<1500;++i){desktop.advanceMs(66);engine.run();if(engine.currentId==SceneID::EXPLORE&&static_cast<ExploreScene*>(engine.currentScene.get())->market_&&!engine.sceneFadeActive())return *static_cast<ExploreScene*>(engine.currentScene.get());}
    std::fprintf(stderr,"departure timeout scene=%u travel=%u fade=%u clock=%u\n",unsigned(engine.currentId),unsigned(engine.exploreTravelPhase()),unsigned(engine.sceneFade),desktop.millis());
    assert(false&&"market departure did not complete");return *static_cast<ExploreScene*>(engine.currentScene.get());
}
static void marketLockTests(DesktopPlatform& desktop,GameEngine& engine,Blobs& blobs,Display& display,const std::string& out){
    auto& state=engine.gameState();state.storageCount=0;state.settings.idleTimeoutIndex=4;
    for(int mode=0;mode<3;++mode){
        state.coins=30000;state.teamCount=2;state.team[0]=Game::MonsterFactory::create(25,30);state.team[1]=Game::MonsterFactory::create(133,30);for(auto& m:state.team){m.satiety=100;m.affection=100;m.bond=30;}
        state.marketSeason={};Game::MarketShop::ensurePeriod(state);state.marketTutor={};state.marketCare={};assert(engine.saveNow());
        auto& scene=departMarket(desktop,engine);Journey j{desktop,engine,scene,out};
        if(mode==0){j.open();j.shelf();j.toBox();j.tap(232,308);assert(scene.marketShopUi_.page()==Page::BOX_ARRIVE);}
        else if(mode==1){tutorOpen(j);j.tap(180,270);j.tap(124,254);j.tap(180,308);assert(scene.marketTutorUi_.page()==TutorPage::TRAINING);}
        else {careOpen(j);j.tap(180,178);j.tap(180,180);j.tap(180,308);assert(scene.marketCareUi_.page()==CarePage::SHOW);}
        const auto paid=state.coins;savedMatches(engine);NativeLockBoard board{desktop,display,engine};NativeLockSession session{engine,blobs};session.rejectFinalOnce=mode==0;board.pending.request(desktop.millis());
        Sdgoods::processPowerActions(board,session,engine);assert(!display.isLocked&&!scene.marketStallActive()&&engine.exploreTravelPhase()==ExploreTravelPhase::RETURNING);
        // The same native main loop advances the market fade and door arrival;
        // no sleeps, manual scene switch or early forced home state.
        for(int i=0;i<1500&&!display.isLocked;++i){desktop.advanceMs(66);engine.run();Sdgoods::processPowerActions(board,session,engine);}
        assert(display.isLocked&&board.locks==1&&state.coins==paid);
        if(mode==0)assert(session.saves==1&&!session.rejectFinalOnce&&engine.saveCoordinator.dirty());
        savedMatches(engine);
        // A failed final snapshot must not veto the lock; retry while dark.
        if(mode==0){desktop.advanceMs(2100);engine.run();assert(display.isLocked&&!engine.saveCoordinator.dirty());savedMatches(engine);}
        display.isLocked=false;board.unlock=true;Sdgoods::processPowerActions(board,session,engine);desktop.advanceMs(66);engine.run();assert(engine.currentId==SceneID::MAIN&&!engine.sceneFadeActive());
        if(mode==0){engine.render(desktop.millis());capture(out,"entry-04-home-after-lock");}
    }
    // A second short press cancels a queued lock while the safe return still
    // finishes. A request made during departure also reaches home before dark.
    engine.switchScene(SceneID::EXPLORE,false);assert(engine.beginExploreDeparture(ExploreDestinations::MARKET));NativeLockBoard board{desktop,display,engine};NativeLockSession session{engine,blobs};board.pending.request(desktop.millis());
    for(int i=0;i<2000&&!display.isLocked;++i){desktop.advanceMs(66);Sdgoods::processPowerActions(board,session,engine);engine.run();}
    assert(display.isLocked&&engine.exploreTravelPhase()==ExploreTravelPhase::NONE);display.isLocked=false;board.unlock=true;Sdgoods::processPowerActions(board,session,engine);
    auto& scene=departMarket(desktop,engine);board.pending.request(desktop.millis());Sdgoods::processPowerActions(board,session,engine);board.pending.toggle(desktop.millis());
    for(int i=0;i<1500&&engine.exploreTravelPhase()!=ExploreTravelPhase::NONE;++i){desktop.advanceMs(66);engine.run();Sdgoods::processPowerActions(board,session,engine);}
    assert(!display.isLocked&&!board.pending.pending()&&engine.currentId==SceneID::MAIN&&engine.exploreTravelPhase()==ExploreTravelPhase::NONE);(void)scene;
    std::puts("market-lock: three paid stalls→fade/door/home/lock despite save failure, retry while dark, wake/departure/cancel and durable rewards OK");
}

#ifdef STICKMON_SELECTOR_BASELINE
#include STICKMON_SELECTOR_BASELINE
static void marketSelectorBaseline(){
    auto& c=PixelRenderer::canvas();const auto old=Platform::display().frameBuffer();std::vector<uint16_t> pixels(360*360),reference(360*360);
    const bool partial=FrameDamage::allowed;FrameDamage::allowed=false;
    for(bool swapped:{false,true})for(int scenario=0;scenario<4;++scenario){
        Ui::ExploreRouteFrame f;f.visibleAreaCount=2;f.areaNames[0]=Ui::Explore::GRASS_PATH;f.areaNames[1]=Ui::Explore::MIST_FOREST_PATH;f.areaCursor=scenario%2;f.areaUnlocked[0]=f.areaUnlocked[1]=scenario!=0;
        if(scenario>=2){f.areaPreviewCount=1;f.areaPreviewFrames[0]=PokemonSprites::findSpeciesSprite(25,PokemonSprites::SpriteKind::FRONT);}
        if(scenario==3)f.toast=Ui::MarketText::CLOSED;
        Ui::RoundExploreBefore::Selection before;Ui::RoundExplore::Selection after;
        c.attach({pixels.data(),360,360,swapped});c.setCoordinateScale(1);c.setLayoutScale(1);c.setAssetScale(1);c.setNativeText(false);c.clearClipRect();before.render(f);
        for(int y=0;y<360;++y)for(int x=0;x<360;++x)reference[y*360+x]=c.readPixel(x,y);
        after.render(f);for(int y=0;y<360;++y)for(int x=0;x<360;++x)assert(reference[y*360+x]==c.readPixel(x,y));
    }
    FrameDamage::allowed=partial;c.attach(old);c.setCoordinateScale(1);c.setLayoutScale(1);c.setAssetScale(1);
    std::puts("market-entry:8 nonmarket/header scenarios match pre-batch pixels exactly");
}
#endif
