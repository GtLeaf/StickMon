#pragma once
#include "ui/round_touch/MarketTutorUi.h"
using Tutor=Ui::RoundMarketTutorUi;
using TutorPage=Tutor::Page;

static void tutorOpen(Journey& j,bool initialized=true){
    int keeper=-1;for(int i=0;i<j.scene.market_->data().residentCount;++i)if(j.scene.market_->actor(i).species==94)keeper=i;
    assert(keeper>=0&&marketStandNear(*j.scene.market_,keeper));j.scene.marketUi_.chatTarget=keeper;j.scene.marketUi_.refresh(*j.scene.market_);
    j.tap(180,304);assert(j.scene.marketServicePending_&&!j.scene.marketUi_.dialogue().active());j.advance(1000);
    if(!initialized){assert(j.scene.marketTutorUi_.page()==TutorPage::ERROR);return;}
    assert(j.scene.marketTutorUi_.page()==TutorPage::GREET);
    j.advance(1100);j.shot("tutor-01-greeting");j.finishGreeting(j.scene.marketTutorUi_);
}
static void tutorLeave(Journey& j){j.tap(180,100);j.advance(600);assert(!j.scene.marketTutorUi_.active()&&!j.scene.market_->conversing());}
static void tutorJourney(DesktopPlatform& desktop,GameEngine& engine,Blobs& blobs,Display& display,const std::string& out){
    auto& state=engine.gameState();state.teamCount=2;state.team[0]=Game::MonsterFactory::create(25,50);state.team[1]=Game::MonsterFactory::create(133,50);
    state.team[0].satiety=state.team[1].satiety=100;state.coins=20000;state.marketTutor={};state.marketTutorGifted=0;
    Game::ItemInventory::remove(state,Game::ItemId::HEART_SCALE,Game::ItemInventory::count(state,Game::ItemId::HEART_SCALE));assert(engine.saveNow());
    auto& scene=*static_cast<ExploreScene*>(engine.currentScene.get());assert(scene.beginMarket());Journey j{desktop,engine,scene,out};j.advance(1000);
    assert(Game::ItemInventory::add(state,Game::ItemId::HEART_SCALE,Game::ITEM_STACK_CAP));assert(engine.saveNow());
    tutorOpen(j);assert(!state.marketTutorGifted&&Game::ItemInventory::count(state,Game::ItemId::HEART_SCALE)==Game::ITEM_STACK_CAP);tutorLeave(j);
    assert(Game::ItemInventory::remove(state,Game::ItemId::HEART_SCALE,Game::ITEM_STACK_CAP));state.marketTutor={};assert(engine.saveNow());
    blobs.failAll=true;tutorOpen(j,false);assert(!state.marketTutorGifted&&Game::ItemInventory::count(state,Game::ItemId::HEART_SCALE)==0);j.shot("tutor-00-init-failed");j.tap(180,308);j.advance(600);blobs.failAll=false;
    tutorOpen(j);assert(state.marketTutorGifted&&Game::ItemInventory::count(state,Game::ItemId::HEART_SCALE)==1);savedMatches(engine);const auto lesson=state.marketTutor.move;assert(lesson);
    j.shot("tutor-02-menu");renderContract(scene.marketTutorUi_,state);tutorLeave(j);tutorOpen(j);assert(state.marketTutor.move==lesson&&Game::ItemInventory::count(state,Game::ItemId::HEART_SCALE)==1);
    // Learning: select a compatible real party member and replacement slot.
    j.tap(180,178);assert(scene.marketTutorUi_.page()==TutorPage::TUTOR);j.shot("tutor-03-lesson");renderContract(scene.marketTutorUi_,state);
    int member=-1;for(int i=0;i<2;++i)if(Game::MarketTutor::eligibility(state.team[i],lesson)==Game::MarketTutor::Eligibility::CAN_LEARN)member=i;assert(member>=0);
    j.tap(member?236:124,254);assert(scene.marketTutorUi_.page()==TutorPage::SLOT);j.shot("tutor-04-replace");j.tap(180,210);assert(scene.marketTutorUi_.page()==TutorPage::CONFIRM);j.shot("tutor-05-confirm");renderContract(scene.marketTutorUi_,state);
    const auto coins=state.coins;blobs.failAll=true;j.tap(180,308);assert(scene.marketTutorUi_.page()==TutorPage::CONFIRM&&state.coins==coins&&!state.marketTutor.taught);j.shot("tutor-06-save-failed");blobs.failAll=false;
    blobs.delay=350;j.tap(180,308);blobs.delay=0;assert(scene.marketTutorUi_.page()==TutorPage::TEACHING&&state.marketTutor.taught&&state.coins==coins-Game::MarketTutor::lessonPrice(lesson));
    assert(scene.marketCoins_.frame().rolling&&scene.marketCoins_.frame().tags[0].elapsed==0);savedMatches(engine);j.event(Type::UP,180,308,0);assert(scene.marketTutorUi_.page()==TutorPage::TEACHING);
    j.advance(2000);j.shot("tutor-07-learning");renderContract(scene.marketTutorUi_,state);j.advance(3600);assert(scene.marketTutorUi_.page()==TutorPage::RESULT);j.shot("tutor-08-learned");renderContract(scene.marketTutorUi_,state);tutorLeave(j);
    // Training: the selected row controls the price, then only a durable save
    // can raise proficiency and show the native two-actor performance.
    tutorOpen(j);j.tap(180,270);assert(scene.marketTutorUi_.page()==TutorPage::TRAIN_MEMBER);j.shot("tutor-09-members");j.tap(124,254);assert(scene.marketTutorUi_.page()==TutorPage::TRAIN_MOVE);j.shot("tutor-10-training-list");renderContract(scene.marketTutorUi_,state);
    j.tap(180,254);const int slot=2;const auto p=state.team[0].moveProficiency[slot];const auto trainCoins=state.coins;
    blobs.failAll=true;j.tap(180,308);assert(scene.marketTutorUi_.page()==TutorPage::TRAIN_MOVE&&state.coins==trainCoins&&state.team[0].moveProficiency[slot]==p);blobs.failAll=false;
    j.tap(180,308);assert(scene.marketTutorUi_.page()==TutorPage::TRAINING&&state.coins==trainCoins-Game::MarketTutor::trainStep(p).cost);savedMatches(engine);
    j.advance(1500);j.shot("tutor-11-training");scene.market_->pause(true);j.advance(10000);assert(scene.marketTutorUi_.page()==TutorPage::TRAINING);scene.market_->pause(false);
    j.advance(3300);assert(scene.marketTutorUi_.page()==TutorPage::RESULT&&state.team[0].moveProficiency[slot]==Game::MarketTutor::trainedProficiency(p));j.shot("tutor-12-trained");renderContract(scene.marketTutorUi_,state);tutorLeave(j);
    // Recall: a long real move list scrolls without selecting on DOWN/drag;
    // braking consumes one contact, returning keeps the list position.
    tutorOpen(j);j.tap(180,224);assert(scene.marketTutorUi_.page()==TutorPage::RECALL_MEMBER);j.tap(236,254);assert(scene.marketTutorUi_.page()==TutorPage::RECALL_LIST);
    j.event(Type::DOWN,180,280);j.event(Type::MOVE,180,210,40);j.event(Type::UP,180,190,20);const auto released=scene.marketTutorUi_.scrollOffset();assert(released>0&&scene.marketTutorUi_.page()==TutorPage::RECALL_LIST);
    j.advance(66);assert(scene.marketTutorUi_.scrollOffset()>released);j.tap(180,190);assert(scene.marketTutorUi_.page()==TutorPage::RECALL_LIST);j.shot("tutor-13-recall-list");renderContract(scene.marketTutorUi_,state);
    j.tap(180,176);assert(scene.marketTutorUi_.page()==TutorPage::SLOT);j.tap(180,210);assert(scene.marketTutorUi_.page()==TutorPage::CONFIRM);j.shot("tutor-14-recall-confirm");renderContract(scene.marketTutorUi_,state);
    const auto scales=Game::ItemInventory::count(state,Game::ItemId::HEART_SCALE);const auto recallCoins=state.coins;
    blobs.failAll=true;j.tap(180,308);assert(scene.marketTutorUi_.page()==TutorPage::CONFIRM&&Game::ItemInventory::count(state,Game::ItemId::HEART_SCALE)==scales);blobs.failAll=false;
    j.tap(180,308);assert(scene.marketTutorUi_.page()==TutorPage::RESULT&&Game::ItemInventory::count(state,Game::ItemId::HEART_SCALE)==scales-1&&state.coins==recallCoins&&!scene.marketCoins_.busy());savedMatches(engine);j.shot("tutor-15-recalled");tutorLeave(j);
    // A stale period/member request and a power-key UP cannot apply a service.
    tutorOpen(j);const auto before=state;Tutor::Request stale;stale.kind=Tutor::Request::TRAIN;stale.member=1;stale.slot=1;stale.period=state.marketTutor.periodDay+1;stale.iv=state.team[1].ivPacked;stale.metAt=state.team[1].metAt;stale.move=state.team[1].move2Id;scene.commitMarketTutor(stale,desktop.millis());assert(state.coins==before.coins);
    stale.period=state.marketTutor.periodDay;++stale.iv;scene.commitMarketTutor(stale,desktop.millis());assert(state.coins==before.coins);
    j.tap(180,270);j.tap(236,254);assert(scene.marketTutorUi_.page()==TutorPage::TRAIN_MOVE);
    const auto paidCoins=state.coins;blobs.failHistory=true;engine.encounterHistoryDirty=true;
    j.tap(180,308);assert(scene.marketTutorUi_.page()==TutorPage::TRAINING&&state.coins<paidCoins&&engine.saveCoordinator.dirty());savedMatches(engine);blobs.failHistory=false;
    const auto lockedCoins=state.coins;
    j.event(Type::DOWN,180,270);display.isLocked=true;scene.cancelTouch();assert(!scene.marketTutorUi_.active()&&!scene.market_->conversing());display.isLocked=false;j.event(Type::UP,180,270);assert(state.coins==lockedCoins&&!scene.marketUi_.dragging);savedMatches(engine);assert(engine.saveNow());
    scene.onExit();std::puts("round-tutor: actual gift/period restart, learn/train/recall, failed/slow saves, inertia, stale identity, pause/lock, clip/swap OK");
}

#ifdef STICKMON_TUTOR_BASELINE
#include STICKMON_TUTOR_BASELINE
static void tutorPerformanceParity(const std::string& out){
    using P=Ui::MarketTutorPerformance::Style;using B=TutorPerformanceBaseline::Style;
    auto& c=PixelRenderer::canvas();const auto old=Platform::display().frameBuffer();std::vector<uint16_t> pixels(368*448),expected(368*448);
    for(int style=0;style<6;++style)for(uint32_t ms:{0U,200U,450U,1000U,1800U,2700U,3500U,4200U,4800U,5400U}){
        c.attach({pixels.data(),368,448,false});c.setCoordinateScale(1);c.setLayoutScale(1);c.setAssetScale(1);c.setNativeText(true);c.fillSprite(0xffdc);
        TutorPerformanceBaseline::render(B(style),ms,25);expected=pixels;c.fillSprite(0xffdc);Ui::MarketTutorPerformance::render(P(style),ms,25);assert(expected==pixels);
        c.attach({pixels.data(),360,360,false});c.setNativeText(false);c.fillSprite(0xffdc);Ui::MarketTutorPerformance::render(P(style),ms,25,{.76f,40,-18});
        for(int y=0;y<360;++y)for(int x=0;x<360;++x)if(c.readPixel(x,y)!=0xffdc){assert(x>=60&&x<300&&y>=168&&y<280);}
        if(ms==1800){capture(out,(std::string("performance-")+std::to_string(style)).c_str());}
    }
    c.attach(old);c.setCoordinateScale(1);c.setLayoutScale(1);c.setAssetScale(1);c.setNativeText(false);
    std::puts("round-tutor:60 six-style samples preserve baseline AMOLED pixels and fit native round stage");
}
#endif
