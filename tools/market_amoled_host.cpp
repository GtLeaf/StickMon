#include "AmoledApp.h"
#include "game/MonsterFactory.h"
#include "core/FontResource.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/PixelRenderer.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "market_dialogue_fixture.h"

static void capture(const char* output,const char* name) {
    if(!output)return;
    const auto path=std::string(output)+"/"+name+".ppm";
    auto* file=std::fopen(path.c_str(),"wb");assert(file);
    std::fprintf(file,"P6\n368 448\n255\n");
    for(int y=0;y<448;++y)for(int x=0;x<368;++x) {
        const auto c=PixelRenderer::canvas().readPixel(x,y);
        const unsigned char rgb[]={uint8_t(((c>>11)&31)*255/31),uint8_t(((c>>5)&63)*255/63),uint8_t((c&31)*255/31)};
        std::fwrite(rgb,1,3,file);
    }
    assert(std::fclose(file)==0);
}

namespace AmoledV1 {
struct AmoledAppTestAccess {
    static void run(AmoledApp& app,const char* output) {
        auto& state=app.gameState;state={};state.oobeDone=true;state.teamCount=2;
        state.team[0]=Game::MonsterFactory::create(25,10);state.team[1]=Game::MonsterFactory::create(133,10);
        app.syncHomeActors(0);
        assert(app.homeMainActor.active&&app.homeCompanionActor.active);
        for(int i=0;i<6;++i){state.explorePoolRerollCounts[i]=i+1;state.normalBossMissCount[i]=i+2;}
        app.sceneFlow.enter(AppSceneFlow::Scene::EXPLORE_AREAS);
        app.selectedExploreDestination_=ExploreDestinations::MARKET;
        app.loadExplorePreview(0);
        assert(app.selectedExploreListIndex()==0);
        const auto before=state;
        uint32_t simulationTime=0;
        const auto depart=[&](bool pair) {
            app.sceneFlow.enter(AppSceneFlow::Scene::EXPLORE_AREAS);
            app.selectedExploreDestination_=ExploreDestinations::MARKET;
            TouchEvent event{};event.x=98;event.y=412;event.timestampMs=simulationTime;
            event.type=TouchEventType::DOWN;app.handleTouch(event);
            event.type=TouchEventType::UP;app.handleTouch(event);
            assert(app.pendingExpedition&&app.pendingExpeditionArea==ExploreDestinations::MARKET);
            assert(!app.market_&&!app.exploreSessionActive&&app.sceneFlow.current()==AppSceneFlow::Scene::HOME);
            bool walk=false,cross=false,companion=false,companionCross=false,out=false,black=false,in=false;
            for(int n=0;n<3600&&(app.pendingExpedition||app.expeditionDeparturePhase!=AmoledApp::ExpeditionDeparturePhase::NONE);++n) {
                simulationTime+=33;app.update(simulationTime);
                const auto phase=app.expeditionDeparturePhase;
                if(phase==AmoledApp::ExpeditionDeparturePhase::WALK_TO_DOOR&&!walk) {
                    walk=true;app.render(PixelRenderer::canvas());capture(output,"market-depart-room");
                }
                cross|=phase==AmoledApp::ExpeditionDeparturePhase::CROSS_DOOR;
                companion|=phase==AmoledApp::ExpeditionDeparturePhase::WALK_COMPANION_TO_DOOR;
                companionCross|=phase==AmoledApp::ExpeditionDeparturePhase::CROSS_COMPANION_DOOR;
                if(app.sceneFadeActive()) {
                    app.render(PixelRenderer::canvas());
                    if(!app.sceneFadeInward()) {
                        assert(!app.market_&&app.sceneFlow.current()==AppSceneFlow::Scene::HOME);
                        out|=app.sceneFadeAlpha()>0&&app.sceneFadeAlpha()<255;
                        if(app.sceneFadeAlpha()==255) {
                            black=true;
                            for(int y=0;y<448;++y)for(int x=0;x<368;++x)assert(PixelRenderer::canvas().readPixel(x,y)==0);
                        }
                    } else {
                        assert(app.market_&&app.sceneFlow.current()==AppSceneFlow::Scene::MARKET);
                        assert(app.market_->timeMs()==0);
                        in|=app.sceneFadeAlpha()>0&&app.sceneFadeAlpha()<255;
                    }
                    app.markRendered();
                }
            }
            assert(walk&&cross&&companion==pair&&companionCross==pair&&out&&black&&in);
            assert(app.market_&&!app.pendingExpedition&&!app.exploreSessionActive&&!app.sceneFadeActive());
            assert(state.team[0].lastExploredAt==before.team[0].lastExploredAt);
        };
        depart(true);
        assert(app.sceneFlow.current()==AppSceneFlow::Scene::MARKET);
        assert(app.market_->manual());
        const auto view=app.marketViewport();
        assert(view.freeWalk&&view.scale==1.5f&&view.pixel(26)==39&&view.footer()==0&&view.mapHeight()==448);
        const auto advance=[&](int ticks) {
            while(ticks--){
                simulationTime+=100;app.update(simulationTime);
                if(app.sceneFadeActive()) {
                    app.render(PixelRenderer::canvas());app.markRendered();
                }
            }
        };
        advance(70);assert(app.market_&&!app.market_->exiting());
        const auto entranceSession=*app.market_;
        const auto idle=app.market_->playerPose();
        marketCheckShiftedCache(*app.market_,view,state.gameMinutesTotal);
        advance(20);
        assert(app.market_->playerPose().x==idle.x&&app.market_->playerPose().y==idle.y);
        app.render(PixelRenderer::canvas());
        capture(output,"market-idle");
        assert(app.marketUi_.chatTarget==-1&&!app.marketUi_.dragging);
        std::vector<uint16_t> idlePixels;
        for(int y=0;y<448;++y)for(int x=0;x<368;++x)
            idlePixels.push_back(PixelRenderer::canvas().readPixel(x,y));
        Ui::MarketView::render(*app.market_,view,app.market_->timeMs(),state.gameMinutesTotal);
        for(int y=0;y<448;++y)for(int x=0;x<368;++x)
            assert(idlePixels[y*368+x]==PixelRenderer::canvas().readPixel(x,y));
        uint32_t touchTime=100;
        const auto touch=[&](TouchEventType type,int x,int y) {
            TouchEvent e{};e.type=type;e.x=x;e.y=y;e.timestampMs=touchTime;
            app.handleTouch(e);touchTime+=50;
        };
        // The old footer is ordinary walking space, without pause/back taps.
        touch(TouchEventType::DOWN,60,420);
        assert(app.marketUi_.dragging&&app.marketUi_.direction==-1);
        touch(TouchEventType::MOVE,67,420);assert(app.marketUi_.direction==-1);
        touch(TouchEventType::MOVE,60,396);assert(app.marketUi_.direction==2);
        touch(TouchEventType::MOVE,82,396);assert(app.marketUi_.direction==6);
        app.render(PixelRenderer::canvas());capture(output,"market-joystick");
        advance(4);
        assert(app.market_->playerPose().x!=idle.x||app.market_->playerPose().y!=idle.y);
        touch(TouchEventType::UP,60,396);
        assert(!app.marketUi_.dragging&&app.marketUi_.direction==-1&&!app.market_->exiting());
        advance(5);const auto stopped=app.market_->playerPose();advance(10);
        assert(app.market_->playerPose().x==stopped.x&&app.market_->playerPose().y==stopped.y);
        for(int x : {60,184,310}) {
            touch(TouchEventType::DOWN,x,420);touch(TouchEventType::UP,x,420);
            assert(!app.market_->paused()&&!app.market_->exiting()&&!app.marketUi_.dragging);
        }
        touch(TouchEventType::DOWN,60,420);app.cancelTouch();
        touch(TouchEventType::MOVE,60,380);touch(TouchEventType::UP,60,420);
        assert(!app.marketUi_.dragging&&app.marketUi_.direction==-1);
        touch(TouchEventType::DOWN,3,200);touch(TouchEventType::MOVE,27,200);
        assert(app.marketUi_.joystick().x+34==3&&app.marketUi_.direction==3);
        app.render(PixelRenderer::canvas());capture(output,"market-joystick-edge");
        touch(TouchEventType::MOVE,-1,200);assert(!app.marketUi_.dragging);
        touch(TouchEventType::UP,3,200);
        touch(TouchEventType::DOWN,280,200);touch(TouchEventType::MOVE,330,200);
        assert(app.marketUi_.joystick().x+34==280&&app.marketUi_.knobX==26);
        const auto locked=app.market_->playerPose();
        app.setDisplayLocked(true);
        assert(!app.marketUi_.dragging&&app.marketUi_.direction==-1);
        advance(5);
        assert(!app.marketUi_.dragging&&app.marketUi_.direction==-1);
        assert(app.market_->playerPose().x==locked.x&&app.market_->playerPose().y==locked.y);
        touch(TouchEventType::DOWN,100,200);assert(!app.marketUi_.dragging);
        app.setDisplayLocked(false);touch(TouchEventType::MOVE,150,200);touch(TouchEventType::UP,150,200);
        advance(10);assert(app.market_->playerPose().x==locked.x&&app.market_->playerPose().y==locked.y);
        app.market_->pause(true);
        touch(TouchEventType::DOWN,100,200);touch(TouchEventType::MOVE,150,200);
        assert(!app.marketUi_.dragging);touch(TouchEventType::UP,150,200);
        app.market_->pause(false);

        // Place the player on authored open ground near a resident to verify
        // the actual distance/occlusion query and production button rendering.
        const auto saved=*app.market_;
        auto& player=const_cast<Market::Actor&>(app.market_->actor(app.market_->data().residentCount));
        assert(marketStandNear(*app.market_,0));
        app.marketUi_.chatTarget=0;app.marketUi_.refresh(*app.market_);
        assert(app.marketUi_.chatTarget==0);
        const auto chat=Ui::MarketWalkUi::chat(view),hit=Ui::MarketWalkUi::chatHit(view);
        assert(chat.x+chat.w/2==184&&chat.y==363&&chat.w==72&&chat.h==72&&hit.w==88&&hit.h==88);
        app.render(PixelRenderer::canvas());capture(output,"market-chat");
        assert(GameAssets::drawCentered(GameAssets::Kind::MARKET_CHAT,184,399,2));
        touch(TouchEventType::DOWN,184,399);assert(!app.marketUi_.dragging);
        touch(TouchEventType::UP,184,399);assert(!app.marketUi_.dragging);
        assert(app.market_->conversing()&&app.marketUi_.dialogue().active());
        const auto frozenTime=app.market_->timeMs();
        for(int n=0;n<30&&!app.marketUi_.dialogue().visible();++n)advance(1);
        assert(app.market_->conversationReady()&&app.marketUi_.dialogue().visible());
        advance(2);app.render(PixelRenderer::canvas());capture(output,"market-dialogue-partial");
        char beforeLock[256],afterLock[256];
        app.marketUi_.dialogue().visibleText(beforeLock,sizeof(beforeLock));
        app.setDisplayLocked(true);advance(10);
        app.marketUi_.dialogue().visibleText(afterLock,sizeof(afterLock));
        assert(std::strcmp(beforeLock,afterLock)==0);
        app.setDisplayLocked(false);
        app.setDisplayStandby(true,simulationTime);advance(10);
        app.marketUi_.dialogue().visibleText(afterLock,sizeof(afterLock));
        assert(app.displayStandby()&&std::strcmp(beforeLock,afterLock)==0);
        app.setDisplayStandby(false,simulationTime);advance(12);
        assert(app.marketUi_.dialogue().complete()&&app.market_->timeMs()==frozenTime);
        marketCheckShiftedCache(*app.market_,view,state.gameMinutesTotal,0);
        marketCheckShiftedCache(*app.market_,view,0,0);
        app.render(PixelRenderer::canvas());capture(output,"market-dialogue-complete");
        // Touches on the map and drags into the panel cannot advance or walk.
        app.marketUi_.dialogue().visibleText(beforeLock,sizeof(beforeLock));
        touch(TouchEventType::DOWN,100,200);touch(TouchEventType::MOVE,184,399);touch(TouchEventType::UP,184,399);
        app.marketUi_.dialogue().visibleText(afterLock,sizeof(afterLock));
        assert(!app.marketUi_.dragging&&std::strcmp(beforeLock,afterLock)==0);
        for(int n=0;n<64&&app.marketUi_.dialogue().active();++n) {
            touch(TouchEventType::DOWN,184,399);touch(TouchEventType::UP,184,399);
        }
        assert(!app.marketUi_.dialogue().active()&&!app.market_->conversing());
        assert(app.market_->timeMs()==frozenTime);
        touch(TouchEventType::MOVE,140,220);touch(TouchEventType::UP,140,220);
        assert(!app.marketUi_.dragging); // close cannot become a new joystick press
        touch(TouchEventType::DOWN,hit.x+1,399);assert(!app.marketUi_.dragging);
        touch(TouchEventType::MOVE,100,200);touch(TouchEventType::UP,100,200);
        assert(!app.marketUi_.dragging&&app.marketUi_.direction==-1);
        touch(TouchEventType::DOWN,100,200);touch(TouchEventType::MOVE,184,399);
        assert(app.marketUi_.dragging);touch(TouchEventType::UP,184,399);
        // A real visitor may have arrived near the original player while
        // controls were tested. Find genuinely distant ground for hiding.
        bool hidden=false;
        for(int n=0;n<Market::NODES&&!hidden;++n) {
            if(!app.market_->data().pedestrian.has(n))continue;
            player.from=player.to=uint16_t(n);
            app.marketUi_.refresh(*app.market_);
            hidden=app.marketUi_.chatTarget==-1;
        }
        assert(hidden);
        *app.market_=saved;app.marketUi_.refresh(*app.market_);

        int pair=-1;
        assert(marketStandNear(*app.market_,0));
        for(int n=0;n<1800&&pair<0;++n){advance(1);pair=marketPairNear(*app.market_);}
        assert(pair>=0);app.marketUi_.chatTarget=pair;app.marketUi_.refresh(*app.market_);
        touch(TouchEventType::DOWN,184,399);touch(TouchEventType::UP,184,399);
        assert(app.market_->conversationActor(1)==pair&&app.market_->conversationActor(2)==pair+1);
        for(int n=0;n<30&&!app.marketUi_.dialogue().visible();++n)advance(1);
        advance(20);app.render(PixelRenderer::canvas());capture(output,"market-dialogue-pair");
        bool otherSpoke=false;
        for(int n=0;n<64&&app.marketUi_.dialogue().active();++n) {
            otherSpoke|=app.market_->pose(pair+1).speaking;
            touch(TouchEventType::DOWN,184,399);touch(TouchEventType::UP,184,399);
        }
        assert(otherSpoke&&!app.market_->conversing());
        *app.market_=saved;app.market_->update(simulationTime,false);app.marketUi_.refresh(*app.market_);

        // Return using only the actual doorway's outward joystick direction.
        const int dx[]={0,-1,0,1},dy[]={1,0,-1,0};
        for(int lane : {-1,0,1}) {
            if(!app.market_)assert(app.startMarket(simulationTime));
            *app.market_=entranceSession;
            app.market_->update(simulationTime,false);app.marketUi_.reset();
            const int outward=app.market_->data().outward;
            if(lane) {
                const int tangent=lane<0?1:3;
                touch(TouchEventType::DOWN,184,220);
                touch(TouchEventType::MOVE,184+24*dx[tangent],220+24*dy[tangent]);
                advance(1);touch(TouchEventType::UP,184,220);advance(3);
                assert(app.market_->playerPose().x==idle.x+13*lane);
            }
            touch(TouchEventType::DOWN,184,220);
            touch(TouchEventType::MOVE,184+24*dx[outward],220+24*dy[outward]);
            advance(1);assert(app.market_&&app.market_->exiting());
            advance(1);assert(!app.marketUi_.dragging&&app.marketUi_.direction==-1);
            if(lane==1){app.render(PixelRenderer::canvas());capture(output,"market-walkout");}
            bool fadeOut=false,black=false,fadeIn=false;
            for(int t=0;t<1200&&(app.market_||app.expeditionDeparturePhase!=AmoledApp::ExpeditionDeparturePhase::NONE);++t) {
                advance(1);
                assert(app.sceneFlow.current()!=AppSceneFlow::Scene::EXPLORE_AREAS);
                if(app.sceneFadeActive()) {
                    const auto alpha=app.sceneFadeAlpha();
                    if(!app.sceneFadeInward()) {
                        assert(app.market_&&app.sceneFlow.current()==AppSceneFlow::Scene::MARKET);
                        if(alpha>0&&alpha<255) {
                            fadeOut=true;if(lane==0)capture(output,"market-fade-out");
                        }
                        if(alpha==255) {
                            black=true;
                            for(int y=0;y<448;++y)for(int x=0;x<368;++x)assert(PixelRenderer::canvas().readPixel(x,y)==0);
                            if(lane==0)capture(output,"market-black");
                        }
                        touch(TouchEventType::DOWN,184,220);touch(TouchEventType::MOVE,208,220);
                        assert(!app.marketUi_.dragging);
                    } else {
                        assert(!app.market_&&app.sceneFlow.current()==AppSceneFlow::Scene::HOME);
                        if(alpha>0&&alpha<255) {
                            fadeIn=true;if(lane==0)capture(output,"room-fade-in");
                        }
                    }
                }
            }
            assert(!app.market_&&fadeOut&&black&&fadeIn);
            assert(app.sceneFlow.current()==AppSceneFlow::Scene::HOME&&!app.sceneFadeActive());
            app.render(PixelRenderer::canvas());if(lane==0)capture(output,"market-return-room");
        }
        assert(app.sceneFlow.current()==AppSceneFlow::Scene::HOME);
        assert(std::memcmp(state.explorePoolRerollCounts,before.explorePoolRerollCounts,6)==0);
        assert(std::memcmp(state.normalBossMissCount,before.normalBossMissCount,6)==0);
        app.selectExploreDestination(1,120000);
        assert(app.selectedExploreArea==0&&app.selectedExploreDestination_==0&&app.selectedExploreListIndex()==1);
        app.render(PixelRenderer::canvas());
        // Market departures remain free even for a hungry solo party; a touch
        // during the room walk cancels without leaving a queued market behind.
        state.teamCount=1;state.team[0].satiety=0;app.syncHomeActors(simulationTime);
        assert(app.queueExploreDeparture(ExploreDestinations::MARKET,false));
        advance(2);assert(app.expeditionDeparturePhase==AmoledApp::ExpeditionDeparturePhase::WALK_TO_DOOR);
        TouchEvent cancel{};cancel.x=184;cancel.y=100;cancel.timestampMs=simulationTime;
        cancel.type=TouchEventType::DOWN;app.handleTouch(cancel);
        cancel.type=TouchEventType::UP;app.handleTouch(cancel);
        assert(!app.pendingExpedition&&!app.market_&&app.expeditionDeparturePhase==AmoledApp::ExpeditionDeparturePhase::NONE);
        depart(false);assert(state.team[0].satiety==0);
    }
};
}
int main(int argc,char** argv) {
    assert(argc==2||argc==3);DesktopPlatform desktop(argv[1]);Platform::bind(desktop.serviceBundle());assert(desktop.begin());
    assert(Platform::resources().mount());assert(GameAssets::begin());FontResource::ins().begin();
    std::vector<uint16_t> pixels(368*448);auto& c=PixelRenderer::canvas();c.attach({pixels.data(),368,448,false});
    c.setCoordinateScale(1);c.setLayoutScale(1);c.setAssetScale(1);c.setNativeText(true);
    AmoledV1::AmoledApp app;app.begin(0);AmoledV1::AmoledAppTestAccess::run(app,argc==3?argv[2]:nullptr);
    std::puts("market-amoled: eight-way joystick lock chat walkout full-screen fade black room-return progress OK");
}
