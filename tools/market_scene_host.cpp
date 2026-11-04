#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "platform/desktop/DesktopPlatform.h"
#include "game/MonsterFactory.h"
#include "game/ExploreDestinations.h"
#include "core/AudioManager.h"
#include "core/RoomResource.h"
#include "ui/common/MarketView.h"
#define private public
#include "core/GameEngine.h"
#include "scenes/ExploreScene.h"
#include "scenes/MainScene.h"
#undef private
#include "market_dialogue_fixture.h"

class MarketTestPower final : public Platform::IPowerDevice {
public:
    int batteryLevel() override { return 73; }
    Platform::WakeReason wakeReason() const override { return Platform::WakeReason::NORMAL; }
    uint32_t hardwareRandom() override { return 38; }
    size_t externalMemorySize() const override { return 8*1024*1024; }
    size_t externalMemoryFree() const override { return 7*1024*1024; }
    void restart() override { assert(false); }
    void enterDeepSleep(uint64_t,bool) override { assert(false); }
};

static void capture(const char* output,const char* name,const Ui::MarketView::Viewport& view) {
    const auto path=std::string(output)+"/"+name+".ppm";
    auto* file=std::fopen(path.c_str(),"wb");assert(file);
    std::fprintf(file,"P6\n%d %d\n255\n",view.width,view.height);
    for(int y=0;y<view.height;++y)for(int x=0;x<view.width;++x) {
        const int dx=2*x-view.width+1,dy=2*y-view.height+1;
        const auto color=view.round&&dx*dx+dy*dy>=view.width*view.width?0:PixelRenderer::canvas().readPixel(x,y);
        const unsigned char rgb[]={uint8_t(((color>>11)&31)*255/31),uint8_t(((color>>5)&63)*255/63),uint8_t((color&31)*255/31)};
        std::fwrite(rgb,1,3,file);
    }
    assert(std::fclose(file)==0);
}

int main(int argc,char** argv) {
    assert(argc==3);(void)argv[2];
    DesktopPlatform desktop(argv[1]);auto bundle=desktop.serviceBundle();MarketTestPower power;
    Platform::Services services{bundle.lifecycle,bundle.clock,bundle.logger,bundle.input,bundle.display,
        bundle.audio,bundle.imu,power,bundle.blobs,bundle.resources,bundle.memory,bundle.peers};
    Platform::bind(services);assert(desktop.begin());
    auto& engine=GameEngine::ins();assert(engine.begin());
    auto& state=engine.gameState();state={};state.oobeDone=true;state.teamCount=2;
    state.team[0]=Game::MonsterFactory::create(25,10);state.team[1]=Game::MonsterFactory::create(133,10);
    for(int i=0;i<6;++i){state.explorePoolRerollCounts[i]=i+1;state.normalBossMissCount[i]=i+2;}
    engine.switchScene(SceneID::EXPLORE);
    auto& selectionStart=*static_cast<ExploreScene*>(engine.currentScene.get());selectionStart.prepareSurface();
    unsigned speciesCount=0;
    for(uint16_t sid=1;sid<=386;++sid) {
        PokemonSprites::WalkingAnimation animation{};
        if(!PokemonSprites::walkingAnimation(sid,PokemonSprites::WalkDirection::DOWN,animation))continue;
        assert(PokemonSprites::preloadDynamicSpecies(&sid,1));++speciesCount;
        for(int direction=0;direction<8;++direction) {
            assert(PokemonSprites::walkingAnimation(sid,PokemonSprites::WalkDirection(direction),animation));
            for(int frame=0;frame<animation.frameCount;++frame)
                assert(PokemonSprites::findCachedSpeciesSprite(sid,PokemonSprites::SpriteKind(uint16_t(animation.base)+frame)));
        }
    }
    assert(speciesCount==68);
    PokemonSprites::WalkingAnimation diagonal{};
    assert(PokemonSprites::walkingAnimation(25,PokemonSprites::WalkDirection::DOWN_RIGHT,diagonal));
    assert(diagonal.base==PokemonSprites::SpriteKind::PIKACHU_WALKING_DOWN_LEFT_0&&diagonal.flipX);
    assert(PokemonSprites::walkingAnimation(133,PokemonSprites::WalkDirection::UP_RIGHT,diagonal));
    assert(diagonal.base==PokemonSprites::SpriteKind::EEVEE_WALKING_UP_RIGHT_0&&!diagonal.flipX);
    assert(!PokemonSprites::walkingAnimation(25,PokemonSprites::WalkDirection(8),diagonal));
    // The resource sweep replaces the dynamic cache. Restore the actual party
    // before the room plans routes using its sprite footprint geometry.
    assert(engine.syncSpriteCache());
    const auto before=state;
    auto frame=selectionStart.exploreRouteFrame();
    assert(frame.visibleAreaCount==7&&frame.marketPreview);
    assert(std::strcmp(frame.areaNames[0],Ui::MarketText::NAME)==0);
    assert(selectionStart.activateExplore(Ui::ExploreAction::DEPART,0));
    assert(engine.currentId==SceneID::MAIN&&engine.exploreTravel==ExploreTravelPhase::DEPARTING);
    assert(engine.pendingExploreArea()==ExploreDestinations::MARKET);
    bool walk=false,cross=false,second=false,black=false,fadeIn=false;
    for(int i=0;i<1800&&(engine.currentId!=SceneID::EXPLORE||engine.sceneFadeActive());++i) {
        desktop.advanceMs(33);
        const auto now=desktop.millis();
        engine.updateSceneFade(now);
        if(engine.currentId==SceneID::MAIN) {
            auto& home=*static_cast<MainScene*>(engine.currentScene.get());
            walk|=home.doorTransition==MainScene::DoorTransitionMode::EXIT_ROUTE;
            cross|=home.doorTransition==MainScene::DoorTransitionMode::EXIT_CROSS;
            second|=home.doorTransition==MainScene::DoorTransitionMode::EXIT_SECOND_ROUTE||
                    home.doorTransition==MainScene::DoorTransitionMode::EXIT_SECOND_CROSS;
        } else {
            assert(static_cast<ExploreScene*>(engine.currentScene.get())->market_);
            fadeIn|=engine.sceneFadeInActive();
        }
        if(engine.sceneFade!=GameEngine::SceneFadePhase::HOLD)engine.currentScene->update(now,0.033f);
        engine.render(now);
        if(engine.sceneFade==GameEngine::SceneFadePhase::HOLD) {
            black=true;
            auto& canvas=PixelRenderer::canvas();
            for(int y=0;y<canvas.height();++y)for(int x=0;x<canvas.width();++x)assert(canvas.readPixel(x,y)==0);
        }
    }
    std::fprintf(stderr,"market departure walk=%d cross=%d companion=%d black=%d fadeIn=%d scene=%u travel=%u\n",
        walk,cross,second,black,fadeIn,unsigned(engine.currentId),unsigned(engine.exploreTravel));
    if(!cross||!second)std::fprintf(stderr,"%s\n",desktop.logs().c_str());
    assert(walk&&cross&&second&&black&&fadeIn);
    assert(engine.currentId==SceneID::EXPLORE&&engine.exploreTravel==ExploreTravelPhase::ACTIVE);
    auto& scene=*static_cast<ExploreScene*>(engine.currentScene.get());
    assert(scene.market_);
    assert(std::memcmp(&state,&before,sizeof(state))==0);
    scene.render();
    using Type=Platform::IInputDevice::TouchType;
    assert(scene.market_->manual());
    assert(AudioManager::ins().requestedMusic()==MusicTrack::MARKET);
    const auto view=Ui::MarketWalkUi::viewport();
    assert(view.scale==1&&view.footer()==0&&view.mapHeight()==view.height);
    uint32_t simulationTime=desktop.millis();
    const auto advance=[&](int ticks){while(ticks--){simulationTime+=100;scene.update(simulationTime,0.1f);}};
    advance(70);assert(scene.market_&&!scene.market_->exiting());
#if STICKMON_UI_ROUND_TOUCH
    marketCheckShiftedCache(*scene.market_,view,state.gameMinutesTotal);
#endif
    scene.render();
    uint32_t touchTime=100;
    const auto touch=[&](Type type,int x,int y) {
        scene.onTouch({type,int16_t(x),int16_t(y),touchTime});touchTime+=50;
    };
    const int mx=view.round?268:view.width-26,my=view.round?78:26;
    const int jx=view.width/2,jy=view.height/2;
    assert(!scene.marketUi_.dragging);
    capture(argv[2],"market-idle",view);
    // With no active contact/NPC, the map is unobstructed by any controls.
    std::vector<uint16_t> idlePixels;
    for(int y=0;y<view.height;++y)for(int x=0;x<view.width;++x)
        idlePixels.push_back(PixelRenderer::canvas().readPixel(x,y));
    Ui::MarketView::render(*scene.market_,view,scene.market_->timeMs(),state.gameMinutesTotal);
    for(int y=0;y<view.height;++y)for(int x=0;x<view.width;++x)
        assert(idlePixels[y*view.width+x]==PixelRenderer::canvas().readPixel(x,y));
    touch(Type::DOWN,jx,jy);assert(scene.marketUi_.dragging&&scene.marketUi_.direction==-1);
    assert(scene.marketUi_.joystick().x+34==jx&&scene.marketUi_.joystick().y+34==jy);
    touch(Type::MOVE,jx+7,jy);assert(scene.marketUi_.direction==-1);
    touch(Type::MOVE,jx,jy-24);assert(scene.marketUi_.direction==2);
    touch(Type::MOVE,jx+22,jy-24);assert(scene.marketUi_.direction==6);
    // Sector boundaries retain the current direction until five degrees past.
    touch(Type::MOVE,jx+11,jy-24);assert(scene.marketUi_.direction==6);
    touch(Type::MOVE,jx+7,jy-24);assert(scene.marketUi_.direction==2);
    touch(Type::MOVE,jx+11,jy-24);assert(scene.marketUi_.direction==2);
    touch(Type::MOVE,jx+14,jy-24);assert(scene.marketUi_.direction==6);
    for(int direction=0;direction<8;++direction) {
        touch(Type::MOVE,jx+24*Market::DIRECTION_X[direction],jy+24*Market::DIRECTION_Y[direction]);
        assert(scene.marketUi_.direction==direction);
    }
    touch(Type::MOVE,jx+24,jy);assert(scene.marketUi_.direction==3);
    scene.render();capture(argv[2],"market-joystick-center",view);
    touch(Type::UP,jx+24,jy);assert(!scene.marketUi_.dragging&&scene.marketUi_.direction==-1);
    // The next contact gets its own center, well outside the old fixed pad.
    const int ax=view.width*3/4,ay=view.height*2/3;
    touch(Type::DOWN,ax,ay);assert(scene.marketUi_.dragging&&scene.marketUi_.direction==-1);
    assert(scene.marketUi_.joystick().x+34==ax&&scene.marketUi_.joystick().y+34==ay);
    touch(Type::MOVE,ax-50,ay);assert(scene.marketUi_.direction==1&&scene.marketUi_.knobX==-26);
    assert(scene.marketUi_.joystick().x+34==ax&&scene.marketUi_.joystick().y+34==ay);
    scene.render();capture(argv[2],"market-joystick-right",view);
    touch(Type::CANCEL,ax-50,ay);assert(!scene.marketUi_.dragging&&scene.marketUi_.direction==-1);
    touch(Type::MOVE,ax,ay);touch(Type::UP,ax,ay);assert(!scene.marketUi_.dragging);
    // Keep the exact press center at the rim; clipping must remain safe.
    touch(Type::DOWN,3,jy);assert(scene.marketUi_.dragging);
    assert(scene.marketUi_.joystick().x+34==3);
    touch(Type::MOVE,27,jy);assert(scene.marketUi_.direction==3);
    scene.render();capture(argv[2],"market-joystick-edge",view);
    touch(Type::MOVE,view.round?0:-1,view.round?0:jy);
    assert(!scene.marketUi_.dragging&&scene.marketUi_.direction==-1);
    touch(Type::UP,jx,jy);assert(!scene.marketUi_.dragging);
    // The former top-right menu area is ordinary joystick space now.
    touch(Type::DOWN,mx,my);assert(scene.marketUi_.dragging);
    assert(scene.marketUi_.joystick().x+34==mx&&scene.marketUi_.joystick().y+34==my);
    touch(Type::MOVE,mx-24,my);assert(scene.marketUi_.direction==1);
    scene.render();capture(argv[2],"market-former-menu",view);
    touch(Type::UP,mx-24,my);assert(!scene.marketUi_.dragging);
    touch(Type::DOWN,jx+20,jy);scene.cancelTouch();assert(scene.marketUi_.direction==-1);
    touch(Type::UP,jx+20,jy);assert(scene.marketUi_.direction==-1);
    touch(Type::DOWN,jx,jy-20);touch(Type::MOVE,-1,-1);assert(scene.marketUi_.direction==-1);
    touch(Type::UP,-1,-1);
    // External pause/lock semantics remain independent of the removed menu.
    scene.market_->pause(true);const auto frozen=scene.market_->playerPose();
    touch(Type::DOWN,jx,jy);touch(Type::MOVE,jx+24,jy);touch(Type::UP,jx+24,jy);
    assert(!scene.marketUi_.dragging&&scene.marketUi_.direction==-1);
    advance(2);assert(scene.update(simulationTime,0.1f).nextUpdateDelayMs==SceneUpdateResult::NO_UPDATE);
    assert(scene.market_->playerPose().x==frozen.x&&scene.market_->playerPose().y==frozen.y);
    scene.market_->pause(false);advance(80);
    const auto idle=scene.market_->playerPose();
    touch(Type::DOWN,view.width/2,view.height/2);touch(Type::UP,view.width/2,view.height/2);
    advance(20);assert(!scene.market_->exiting());
    assert(scene.market_->playerPose().x==idle.x&&scene.market_->playerPose().y==idle.y);
    scene.marketUi_.chatTarget=0;
    const auto chat=Ui::MarketWalkUi::chat(view),hit=Ui::MarketWalkUi::chatHit(view);
    assert(chat.w==36&&chat.h==36&&hit.w==44&&hit.h==44);
    assert(chat.x+chat.w/2==view.width/2);
    scene.render();capture(argv[2],"market-chat-icon",view);
    // The actual UI pack must supply the generated icon (not its text fallback).
    assert(GameAssets::drawCentered(GameAssets::Kind::MARKET_CHAT,chat.x+chat.w/2,chat.y+chat.h/2));
    touch(Type::DOWN,chat.x+chat.w/2,chat.y+chat.h/2);assert(!scene.marketUi_.dragging);
    touch(Type::UP,chat.x+chat.w/2,chat.y+chat.h/2);
    assert(!scene.marketUi_.dragging);
    // Four extra pixels preserve a comfortable touch target around the icon.
    touch(Type::DOWN,hit.x+1,hit.y+hit.h/2);assert(!scene.marketUi_.dragging);
    touch(Type::UP,hit.x+1,hit.y+hit.h/2);
    touch(Type::DOWN,view.width/2-30,chat.y+chat.h/2);assert(scene.marketUi_.dragging);
    touch(Type::UP,view.width/2-30,chat.y+chat.h/2);
    touch(Type::DOWN,chat.x+chat.w/2,chat.y+chat.h/2);
    touch(Type::MOVE,jx,jy);assert(!scene.marketUi_.dragging);touch(Type::UP,jx,jy);
    // A walking contact cannot be stolen when an NPC's chat button appears.
    scene.marketUi_.chatTarget=-1;touch(Type::DOWN,ax,ay);scene.marketUi_.chatTarget=0;
    touch(Type::MOVE,chat.x+chat.w/2,chat.y+chat.h/2);assert(scene.marketUi_.dragging);
    touch(Type::UP,chat.x+chat.w/2,chat.y+chat.h/2);assert(!scene.marketUi_.dragging);
    assert(!scene.market_->exiting()&&std::memcmp(&state,&before,sizeof(state))==0);
#if STICKMON_UI_ROUND_TOUCH
    {
        const auto saved=*scene.market_;
        assert(marketStandNear(*scene.market_,0));
        scene.marketUi_.chatTarget=0;scene.marketUi_.refresh(*scene.market_);
        assert(scene.marketUi_.chatTarget==0);
        touch(Type::DOWN,180,304);touch(Type::UP,180,304);
        assert(scene.market_->conversing()&&scene.marketUi_.dialogue().active());
        const auto frozenTime=scene.market_->timeMs();
        for(int n=0;n<30&&!scene.marketUi_.dialogue().visible();++n)advance(1);
        assert(scene.marketUi_.dialogue().visible());advance(2);
        scene.render();capture(argv[2],"market-dialogue-partial",view);
        char text[256],frozen[256];scene.marketUi_.dialogue().visibleText(text,sizeof(text));
        scene.market_->pause(true);advance(10);scene.marketUi_.dialogue().visibleText(frozen,sizeof(frozen));
        assert(std::strcmp(text,frozen)==0);scene.market_->pause(false);advance(15);
        assert(scene.marketUi_.dialogue().complete()&&scene.market_->timeMs()==frozenTime);
        marketCheckShiftedCache(*scene.market_,view,state.gameMinutesTotal,0);
        marketCheckShiftedCache(*scene.market_,view,0,0);
        scene.render();capture(argv[2],"market-dialogue-complete",view);
        const auto box=Ui::MarketDialogueUi::layout(view);
        const auto inCircle=[&](int x,int y) {const int dx=2*x-view.width,dy=2*y-view.height;return dx*dx+dy*dy<view.width*view.width;};
        for(int x:{box.textX,box.textX+box.textWidth})for(int y:{box.textY,box.textY+box.lineHeight+box.glyph})
            assert(inCircle(x,y));
        for(int x:{box.arrowX,box.arrowX+Ui::MarketDialogueUi::ARROW_WIDTH-1})
            for(int y:{box.arrowY,box.arrowY+Ui::MarketDialogueUi::ARROW_HEIGHT+Ui::MarketDialogueUi::ARROW_BOB-1})
                assert(inCircle(x,y));
        for(int n=0;n<64&&scene.marketUi_.dialogue().active();++n){touch(Type::DOWN,180,300);touch(Type::UP,180,300);}
        assert(!scene.market_->conversing()&&!scene.marketUi_.dialogue().active());
        touch(Type::MOVE,140,200);touch(Type::UP,140,200);assert(!scene.marketUi_.dragging);
        int pair=-1;
        for(int n=0;n<1800&&pair<0;++n){advance(1);pair=marketPairNear(*scene.market_);}
        assert(pair>=0);scene.marketUi_.chatTarget=pair;scene.marketUi_.refresh(*scene.market_);
        touch(Type::DOWN,180,304);touch(Type::UP,180,304);
        assert(scene.market_->conversationActor(1)==pair&&scene.market_->conversationActor(2)==pair+1);
        for(int n=0;n<30&&!scene.marketUi_.dialogue().visible();++n)advance(1);
        advance(20);scene.render();capture(argv[2],"market-dialogue-pair",view);
        bool otherSpoke=false;
        for(int n=0;n<64&&scene.marketUi_.dialogue().active();++n) {
            otherSpoke|=scene.market_->pose(pair+1).speaking;
            touch(Type::DOWN,180,300);touch(Type::UP,180,300);
        }
        assert(otherSpoke&&!scene.market_->conversing());
        *scene.market_=saved;scene.market_->update(simulationTime,false);scene.marketUi_.refresh(*scene.market_);
    }
#endif
    // Walk back through the actual authored exit using the joystick alone.
    const auto& data=scene.market_->data();
    const auto player=scene.market_->playerPose();
    assert(std::abs(player.x-float(data.entry%Market::WIDTH*13))+std::abs(player.y-float(data.entry/Market::WIDTH*13))==13);
    const int dx[4]={0,-1,0,1},dy[4]={1,0,-1,0};
    scene.marketUi_.chatTarget=-1;
    touch(Type::DOWN,jx,jy);touch(Type::MOVE,jx+24*dx[data.outward],jy+24*dy[data.outward]);
    advance(1);assert(scene.market_->exiting()&&!scene.marketUi_.dragging);
    scene.render();capture(argv[2],"market-walkout",view);
    touch(Type::UP,jx,jy);
    for(int i=0;i<1000&&!engine.sceneFadeActive();++i)advance(1);
    assert(scene.market_&&scene.market_->finished()&&scene.phase==Ui::ExplorePhase::ENDING);
    assert(engine.exploreTravel==ExploreTravelPhase::RETURNING&&engine.sceneFadeTarget==SceneID::MAIN);
    assert(std::memcmp(&state,&before,sizeof(state))==0);
    // The actual engine fades the market, swaps at black, then fades the room.
    while(engine.sceneFade==GameEngine::SceneFadePhase::OUT) {
        engine.updateSceneFade(engine.sceneFadeLastStepMs+33);
        engine.render(0);
        if(engine.sceneFadeProgressMs>100&&engine.sceneFadeProgressMs<150)capture(argv[2],"market-fade-out",view);
    }
    assert(engine.currentId==SceneID::EXPLORE&&scene.market_);
    capture(argv[2],"market-black",view);
    for(int y=0;y<view.height;++y)for(int x=0;x<view.width;++x)assert(PixelRenderer::canvas().readPixel(x,y)==0);
    engine.updateSceneFade(engine.sceneFadeStartedMs+500);
    assert(engine.currentId==SceneID::MAIN&&engine.sceneFadeInActive());
    assert(RoomResource::ins().available());
    while(engine.sceneFadeActive()) {
        engine.render(0);
        if(engine.sceneFadeProgressMs>100&&engine.sceneFadeProgressMs<150)capture(argv[2],"room-fade-in",view);
        engine.updateSceneFade(engine.sceneFadeLastStepMs+33);
    }
    assert(engine.exploreTravel==ExploreTravelPhase::RETURNING);
    uint32_t homeTime=desktop.millis();
    for(int t=0;t<1000&&engine.exploreTravel!=ExploreTravelPhase::NONE;++t) {
        homeTime+=33;engine.currentScene->update(homeTime,0.033f);engine.render(homeTime);
    }
    assert(engine.exploreTravel==ExploreTravelPhase::NONE);
    engine.render(homeTime);capture(argv[2],"market-return-room",view);
    // Selecting the second display item must still depart to persistent area 0.
    engine.finishExploreReturn();engine.switchScene(SceneID::EXPLORE);
    auto& selection=*static_cast<ExploreScene*>(engine.currentScene.get());
    selection.ui.state.areaCursor=1;
    selection.activateExplore(Ui::ExploreAction::DEPART,1);
    assert(engine.pendingExploreArea()==0);
    std::puts("market-scene: eight-way sprites joystick hysteresis ownership pause walkout full-screen fade room-return progress OK");
}
