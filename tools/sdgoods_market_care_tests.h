#pragma once
#include "ui/round_touch/MarketCareUi.h"
#include "game/MarketMapData.h"
#include "game/ExploreDestinations.h"
using Care=Ui::RoundMarketCareUi;
using CarePage=Care::Page;
static void careOpen(Journey& j){
    int target=-1;for(int i=0;i<j.scene.market_->data().residentCount;++i)if(j.scene.market_->actor(i).species==113)target=i;
    assert(target>=0&&marketStandNear(*j.scene.market_,target));j.scene.marketUi_.chatTarget=target;j.scene.marketUi_.refresh(*j.scene.market_);
    j.tap(180,304);assert(j.scene.marketServicePending_&&!j.scene.marketUi_.dialogue().active());j.advance(4000);assert(j.scene.marketCareUi_.page()==CarePage::MENU);
}
static void careRelease(DesktopPlatform& desktop,GameEngine& engine){
    auto& state=engine.gameState();state.gameMinutesTotal=1440;
    engine.switchScene(SceneID::EXPLORE,false);auto& scene=*static_cast<ExploreScene*>(engine.currentScene.get());
    scene.ui.state.areaCursor=ExploreDestinations::visibleIndex(ExploreDestinations::MARKET,state);
    const auto route=scene.exploreRouteFrame();assert(route.marketPreview&&!route.marketOpen&&route.marketDaysUntilOpen==2);
    assert(!route.areaUnlocked[route.areaCursor]&&!scene.beginMarket());
    assert(!scene.ui.selection.tap(scene,scene.ui.state,Ui::RoundExplore::DEPART));
    state.gameMinutesTotal=3*1440;assert(scene.beginMarket());
    Journey j{desktop,engine,scene,"."};j.advance(1000);j.open();assert(scene.marketShopUi_.page()==Page::MENU);
    j.tap(180,100);j.advance(600);assert(!scene.marketStallActive());
    const auto coins=state.coins;const auto affection=state.team[0].affection;Care::Request forged;forged.kind=Care::Request::MASSAGE;forged.target={Game::MarketCare::Target::TEAM,0};forged.period=Game::MarketCare::periodDay(state);forged.iv=state.team[0].ivPacked;forged.metAt=state.team[0].metAt;
    scene.commitMarketCare(forged,desktop.millis());assert(state.coins==coins&&state.team[0].affection==affection&&!scene.marketCareUi_.active());
    Care ui;ui.open(state);assert(!ui.active());
    auto& session=*scene.market_;int target=-1;
    for(int i=0;i<session.data().residentCount;++i)if(session.actor(i).species==113)target=i;
    assert(target>=0&&marketStandNear(session,target));scene.marketUi_.chatTarget=target;scene.marketUi_.refresh(session);
    j.tap(180,304);assert(scene.marketUi_.dialogue().active()&&!scene.marketServicePending_&&!scene.marketCareUi_.active());
    engine.currentScene.reset();
    std::puts("round-care release: closed countdown/disabled departure, open market/Kecleon, Chansey ordinary dialogue, no forged payment OK");
}
static void careJourney(DesktopPlatform& desktop,GameEngine& engine,Blobs& blobs,Display& display,const std::string& out){
    auto& state=engine.gameState();state.teamCount=2;state.coins=20000;state.marketCare={};
    for(int i=0;i<2;++i){state.team[i]=Game::MonsterFactory::create(i?133:25,30);state.team[i].satiety=100;state.team[i].affection=100;state.team[i].mood=50;state.team[i].bond=30;state.team[i].ivPacked=200+i;state.team[i].metAt=100+i;}
    state.storageCount=10;state.storage[0]=state.team[0];
    for(int i=1;i<10;++i){state.storage[i]=Game::MonsterFactory::create(i%2?133:25,25);state.storage[i].bond=20;state.storage[i].ivPacked=300+i;state.storage[i].metAt=20+i;state.storage[i].lastExploredAt=100;}
    state.storage[2].bond=Game::Bond::NEW_CONTACT_VALUE;state.storage[2].lastExploredAt=state.storage[2].metAt;state.storage[3].origin=Game::Origin::VISITOR;state.storage[4].fainted=1;state.storage[4].hpCur=0;
    assert(engine.saveNow());auto& scene=*static_cast<ExploreScene*>(engine.currentScene.get());assert(scene.beginMarket());Journey j{desktop,engine,scene,out};j.advance(1000);careOpen(j);j.shot("care-01-menu");renderContract(scene.marketCareUi_,state);
    Game::MarketCare::Target targets[Game::MarketCare::TARGET_CAP];const int n=Game::MarketCare::targets(state,targets);assert(n==10);
    for(int i=0;i<n;++i)assert(!(targets[i].kind==Game::MarketCare::Target::CONTACT&&(targets[i].slot==0||targets[i].slot==3)));
    // Free evaluation and recommendation use the same selected partner without
    // paying or consuming a reservation until the explicit confirmation.
    const auto evalCoins=state.coins;j.tap(180,270);assert(scene.marketCareUi_.page()==CarePage::LIST);j.tap(180,180);assert(scene.marketCareUi_.page()==CarePage::EVAL&&state.coins==evalCoins&&Game::MarketCare::used(state)==0);j.shot("care-02-evaluation");renderContract(scene.marketCareUi_,state);
    j.tap(180,308);assert(scene.marketCareUi_.page()==CarePage::CONFIRM);j.shot("care-03-confirm");renderContract(scene.marketCareUi_,state);
    // Frozen target identity rejects a roster change between display and UP.
    const auto original=state;std::swap(state.team[0],state.team[1]);j.tap(180,308);assert(scene.marketCareUi_.page()==CarePage::CONFIRM&&state.coins==evalCoins&&Game::MarketCare::used(state)==0);std::swap(state.team[0],state.team[1]);
    state.coins=0;j.tap(180,308);assert(!scene.marketCareUi_.pending()&&Game::MarketCare::used(state)==0);state.coins=evalCoins;
    blobs.failAll=true;j.tap(180,308);assert(scene.marketCareUi_.page()==CarePage::CONFIRM&&state.team[0].affection==original.team[0].affection&&state.coins==evalCoins);j.shot("care-04-save-failed");blobs.failAll=false;
    blobs.delay=350;j.tap(180,308);blobs.delay=0;assert(scene.marketCareUi_.page()==CarePage::SHOW&&state.coins==evalCoins-Game::MarketCare::MASSAGE_PRICE&&state.team[0].affection==110&&state.team[0].mood==60&&Game::MarketCare::used(state)==1);savedMatches(engine);
    assert(scene.marketCoins_.frame().rolling&&scene.marketCoins_.frame().tags[0].elapsed==0);j.event(Type::UP,180,308,0);assert(!scene.marketCareUi_.showDone());j.advance(300);j.shot("care-05-massage");renderContract(scene.marketCareUi_,state,&scene.marketCoins_);
    scene.market_->pause(true);j.advance(5000);assert(!scene.marketCareUi_.showDone());scene.market_->pause(false);j.advance(900);assert(scene.marketCareUi_.showDone());j.shot("care-06-massaged");renderContract(scene.marketCareUi_,state);j.tap(180,308);assert(scene.marketCareUi_.page()==CarePage::MENU);
    // Paid roster tea: scrolling must not select; protected first invitation
    // and used/fainted entries remain disabled even after an inertial brake.
    j.tap(180,224);assert(scene.marketCareUi_.page()==CarePage::LIST);j.shot("care-07-tea-list");j.tap(180,180);assert(scene.marketCareUi_.page()==CarePage::LIST);
    j.event(Type::DOWN,180,280);j.event(Type::MOVE,180,220,40);j.event(Type::UP,180,200,20);const int release=scene.marketCareUi_.scrollOffset();assert(release>0&&scene.marketCareUi_.page()==CarePage::LIST);j.advance(66);assert(scene.marketCareUi_.scrollOffset()>release);j.tap(180,200);assert(scene.marketCareUi_.page()==CarePage::LIST);
    // Return/reenter resets the list; a measured112px drag reveals protected
    // contact2 as row1, and the ordinary contact1 as row0.
    j.tap(180,100);j.tap(180,224);j.event(Type::DOWN,180,280);j.event(Type::MOVE,180,168,100);j.event(Type::UP,180,168,200);assert(scene.marketCareUi_.scrollOffset()==112);j.tap(180,236);assert(scene.marketCareUi_.page()==CarePage::LIST);j.shot("care-08-protected-roster");renderContract(scene.marketCareUi_,state);
    j.tap(180,180);assert(scene.marketCareUi_.page()==CarePage::CONFIRM&&scene.marketCareUi_.target().kind==Game::MarketCare::Target::CONTACT&&scene.marketCareUi_.target().slot==1);j.shot("care-09-tea-confirm");renderContract(scene.marketCareUi_,state);
    const auto teaCoins=state.coins;blobs.failAll=true;j.tap(180,308);assert(state.coins==teaCoins&&state.storage[1].bond==20);blobs.failAll=false;
    blobs.failHistory=true;engine.encounterHistoryDirty=true;j.tap(180,308);blobs.failHistory=false;assert(scene.marketCareUi_.page()==CarePage::SHOW&&state.storage[1].bond==25&&state.coins==teaCoins-Game::MarketCare::TEA_PRICE&&Game::MarketCare::used(state)==2&&engine.saveCoordinator.dirty());savedMatches(engine);
    j.advance(300);j.shot("care-10-tea");renderContract(scene.marketCareUi_,state);
    auto finished=scene.marketCareUi_;for(int i=0;i<4;++i)finished.update(250);assert(finished.showDone());renderContract(finished,state);
    scene.render();finished.render(state,&scene.marketCoins_);capture(out,"care-10b-tea-result");
    const auto paid=state.coins;display.isLocked=true;scene.cancelTouch();assert(!scene.marketCareUi_.active()&&!scene.market_->conversing()&&!scene.marketCoins_.busy());display.isLocked=false;j.event(Type::UP,180,308);assert(state.coins==paid);assert(engine.saveNow());
    careOpen(j);j.tap(180,178);assert(scene.marketCareUi_.page()==CarePage::MENU);j.shot("care-11-quota-full");j.tap(180,270);j.tap(180,180);assert(scene.marketCareUi_.page()==CarePage::EVAL&&state.coins==paid);j.shot("care-12-free-after-quota");renderContract(scene.marketCareUi_,state);
    // Outside returns one level and closes the home panel, retaining saved
    // reservations across re-entry. A stale emitted period never charges.
    j.tap(180,100);j.tap(180,100);Care::Request stale;stale.kind=Care::Request::TEA;stale.target={Game::MarketCare::Target::TEAM,1};stale.period=Game::MarketCare::periodDay(state)+1;stale.iv=state.team[1].ivPacked;stale.metAt=state.team[1].metAt;scene.commitMarketCare(stale,desktop.millis());assert(state.coins==paid);
    j.tap(180,100);j.advance(600);assert(!scene.marketStallActive()&&!scene.market_->conversing());careOpen(j);assert(Game::MarketCare::used(state)==2);scene.onExit();savedMatches(engine);
    std::puts("round-care: real evaluation/massage/roster tea, frozen identity, filters/protection/quota, save faults/readback, inertia/pause/lock, clip/swap OK");
}
