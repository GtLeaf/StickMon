#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "platform/desktop/DesktopPlatform.h"
#include "game/GameRandom.h"
#include "game/MonsterFactory.h"
#include "presentation/PixelRenderer.h"
#define private public
#include "hardware/Hal.h"
#include "core/GameEngine.h"
#include "scenes/SocialScene.h"
#undef private

#if STICKMON_SOCIAL_UI_BASELINE
#define VISUAL(s) (s)
#else
#define VISUAL(s) ((s).ui.state)
#endif
using State = Communication::VisitSessionService::State;
constexpr uint8_t PEER[6] = {2,4,6,8,10,12};
void put16(uint8_t* p, uint16_t v) { p[0]=uint8_t(v);p[1]=uint8_t(v>>8); }
uint16_t get16(const uint8_t* p) { return uint16_t(p[0]) | uint16_t(p[1])<<8; }
void wire(DesktopPlatform& desktop, LinkMessageType type, uint8_t room, uint16_t seq=0, bool accept=false) {
    uint8_t packet[9] = {0xa5,0x5a,3,uint8_t(type),4,room,0,0,uint8_t(accept)};
    put16(packet+6,seq);assert(desktop.send(PEER,packet,sizeof(packet)));
}
void session(DesktopPlatform& desktop, LinkMessageType type, uint16_t seq, uint16_t id,
             const void* payload=nullptr, uint8_t size=0) {
    uint8_t packet[33] = {0xa5,0x5a,3,uint8_t(type)};
    put16(packet+4,seq);put16(packet+6,id);packet[8]=size;
    if(size)std::memcpy(packet+9,payload,size);
    assert(desktop.send(PEER,packet,sizeof(packet)));
}
Platform::PeerPacket take(DesktopPlatform& desktop, LinkMessageType type) {
    Platform::PeerPacket packet;
    while(desktop.receive(packet))if(packet.length>=4 && packet.payload[3]==uint8_t(type))return packet;
    assert(false && "expected protocol packet");return {};
}
void drain(DesktopPlatform& desktop) { Platform::PeerPacket packet;while(desktop.receive(packet)){} }
void ack(DesktopPlatform& desktop, const Platform::PeerPacket& packet) {
    session(desktop,LinkMessageType::SESSION_ACK,get16(packet.payload+4),get16(packet.payload+6));
}

int main(int argc,char** argv) {
    assert(argc==3);DesktopPlatform desktop(argv[1]);Platform::bind(desktop.serviceBundle());
    assert(desktop.begin());auto& engine=GameEngine::ins();assert(engine.begin());
    auto& game=engine.gameState();auto& service=engine.visitService();auto& link=EspNowLink::ins();
    std::unique_ptr<SocialScene> scene;unsigned checks=0;
    const ButtonEvent a{0,BtnAction::PRESSED},b{1,BtnAction::PRESSED};
    const ButtonEvent longA{0,BtnAction::LONG_PRESS},longB{1,BtnAction::LONG_PRESS};
    auto setup=[&]() -> SocialScene& {
        scene.reset();engine.currentScene.reset();service.stop();link.end();drain(desktop);
        engine.currentId=SceneID::SOCIAL;engine.prevId=SceneID::MENU;
        engine.contactVisit={};engine.exploreTravel=ExploreTravelPhase::NONE;
        engine.sceneFade=GameEngine::SceneFadePhase::NONE;
        engine.evolutionEventHead=engine.evolutionEventCount=0;
        engine.moveReplacementEventHead=engine.moveReplacementEventCount=0;
        engine.debugBattleRequested=engine.debugMenuReturnRequested=false;
        engine.clearMainSceneViewState();GameRandom::seed(1234);
        game=Game::GameState{};game.oobeDone=true;game.tutorialFlags=0xff;
        game.teamCount=1;game.team[0]=Game::MonsterFactory::create(1,25);
        game.gameMinutesTotal=420;engine.gameClock.set(desktop.millis(),420);
        engine.saveCoordinator.reset(desktop.millis());link.roomId=0;link.seq=1;
        scene=std::make_unique<SocialScene>();scene->onEnter();return *scene;
    };
    auto advance=[&](uint32_t ms){desktop.advanceMs(ms);return scene->update(desktop.millis(),ms*.001f);};
    auto capture=[&](const std::string& name){
        PixelRenderer::clear(0);scene->render();const auto fb=desktop.frameBuffer();
        FILE* file=std::fopen((std::string(argv[2])+"/"+name+".rgb565").c_str(),"wb");assert(file);
        assert(std::fwrite(fb.pixels,sizeof(uint16_t),fb.width*fb.height,file)==size_t(fb.width)*fb.height);
        std::fclose(file);
    };
    auto rooms=[&](int count){
        link.roomCountValue=count;
        for(int i=0;i<count;++i){auto& room=link.rooms[i];room={};room.roomId=i==3?255:uint8_t(i*17+1);
            room.purpose=EspNowLink::RoomPurpose::VISIT;std::memcpy(room.mac,PEER,6);
            room.mac[5]+=i;room.lastSeenMs=desktop.millis();}
    };
    auto hostRequest=[&](){
        assert(scene->onButton(a));assert(service.state()==State::HOSTING);
        wire(desktop,LinkMessageType::JOIN_REQ,service.viewModel().roomId,71);
        service.update(desktop.millis());assert(service.state()==State::WAITING_HOST_DECISION);
        advance(0);assert(VISUAL(*scene).cursor==0);
    };
    auto searchRoom=[&](){
        scene->onButton(b);scene->onButton(a);assert(service.state()==State::SEARCHING);
        wire(desktop,LinkMessageType::HELLO,93);service.update(desktop.millis());
        assert(service.state()==State::ROOM_LIST);advance(0);
    };
    desktop.advanceMs(5000);
    for(int state=0;state<12;++state)for(unsigned age:{0u,1u,499u,500u,501u,999u,1000u,1499u,1500u}){
        setup();service.state_=State(state);service.localIsHost_=true;
        VISUAL(*scene).stateStartedMs=desktop.millis()-age;rooms(4);
        capture("state-"+std::to_string(state)+"-age-"+std::to_string(age));
    }
    for(int cursor=0;cursor<3;++cursor){setup();VISUAL(*scene).cursor=cursor;capture("menu-"+std::to_string(cursor));}
    for(int count=0;count<=4;++count)for(int cursor=0;cursor<=count;++cursor)
        for(int error=0;error<4;++error){
            setup();service.state_=State::ROOM_LIST;rooms(count);VISUAL(*scene).roomCursor=cursor;
            const char* errors[]={nullptr,"JOIN DECLINED","JOIN TIMEOUT","OTHER"};service.error_=errors[error];
            capture("rooms-"+std::to_string(count)+"-cursor-"+std::to_string(cursor)+"-error-"+std::to_string(error));
        }
    for(int count:{0,1,3}){setup();service.state_=State::ROOM_LIST;rooms(count);VISUAL(*scene).roomCursor=4;capture("room-shrink-"+std::to_string(count));}
    for(int cursor:{0,1}){setup();service.state_=State::WAITING_HOST_DECISION;VISUAL(*scene).cursor=cursor;capture("decision-"+std::to_string(cursor));}
    for(bool host:{false,true}){setup();service.state_=State::ACTIVE;service.localIsHost_=host;capture("active-host-"+std::to_string(host));}
    for(int room:{0,1,9,10,99,100,255}){setup();service.state_=State::HOSTING;service.localIsHost_=true;link.roomId=room;capture("host-room-"+std::to_string(room));}
    const char* terminalErrors[]={nullptr,"NO PET","TEAM NOT SOLO","LINK UNAVAILABLE","HOST TIMEOUT","SEARCH TIMEOUT","LINK TIMEOUT","LINK LOST","HOST FULL","HOST TEAM BUSY","HOST NO PET","AN ERROR MESSAGE LONGER THAN THE SCREEN WIDTH"};
    for(int state:{10,11})for(const char* error:terminalErrors){
        setup();service.state_=State(state);service.error_=error;
        capture("terminal-"+std::to_string(state)+"-error-"+std::to_string(error?std::strlen(error):0)+"-"+std::to_string(error?unsigned(error[0]):0));
    }
    setup();VISUAL(*scene).stateStartedMs=0xffffff00u;service.state_=State::SYNCING;
    capture("dots-clock-wrap");

    setup();{const auto d=advance(20);assert(!d.redraw && d.nextUpdateDelayMs==SceneUpdateResult::NO_UPDATE);}++checks;
    setup();{const auto before=game;assert(!scene->onButton({0,BtnAction::DOWN}) && !scene->onButton({1,BtnAction::UP}));
        assert(!scene->onButton({2,BtnAction::LONG_PRESS}));assert(scene->onButton({2,BtnAction::PRESSED}));assert(std::memcmp(&before,&game,sizeof(game))==0);}++checks;
    setup();for(int i=1;i<=3;++i){assert(scene->onButton(b));assert(VISUAL(*scene).cursor==i%3);}++checks;
    setup();assert(scene->onButton(a) && service.state()==State::HOSTING && link.isEnabled());++checks;
    setup();game.teamCount=0;scene->onButton(a);assert(service.state()==State::FAILED && std::strcmp(service.viewModel().error,"NO PET")==0);++checks;
    setup();game.teamCount=2;scene->onButton(a);assert(service.state()==State::FAILED && std::strcmp(service.viewModel().error,"TEAM NOT SOLO")==0);++checks;
    setup();searchRoom();rooms(4);for(int i=1;i<=5;++i){assert(scene->onButton(b));assert(VISUAL(*scene).roomCursor==i%5);}++checks;
    setup();searchRoom();assert(scene->onButton(a) && service.state()==State::JOINING);
    {auto request=take(desktop,LinkMessageType::JOIN_REQ);assert(request.payload[5]==93 && get16(request.payload+6)!=0);}++checks;
    setup();searchRoom();scene->onButton(a);{auto request=take(desktop,LinkMessageType::JOIN_REQ);
        wire(desktop,LinkMessageType::JOIN_ACK,93,get16(request.payload+6),false);service.update(desktop.millis());
        assert(service.state()==State::ROOM_LIST && std::strcmp(service.viewModel().error,"JOIN DECLINED")==0);}++checks;
    setup();searchRoom();scene->onButton(a);drain(desktop);desktop.advanceMs(Communication::VisitSessionService::JOIN_TIMEOUT_MS);
    service.update(desktop.millis());assert(service.state()==State::SEARCHING && std::strcmp(service.viewModel().error,"JOIN TIMEOUT")==0);++checks;
    setup();searchRoom();scene->onButton(b);assert(scene->onButton(a) && service.state()==State::IDLE && link.isEnabled());++checks;
    setup();hostRequest();assert(scene->onButton(a) && service.state()==State::SYNCING);assert(take(desktop,LinkMessageType::JOIN_ACK).payload[8]==1);++checks;
    setup();hostRequest();scene->onButton(b);assert(scene->onButton(a) && service.state()==State::HOSTING);assert(take(desktop,LinkMessageType::JOIN_ACK).payload[8]==0);++checks;
    setup();hostRequest();service.stop();assert(service.state()==State::IDLE && take(desktop,LinkMessageType::JOIN_ACK).payload[8]==0);++checks;
    setup();hostRequest();drain(desktop);desktop.advanceMs(Communication::VisitSessionService::HOST_DECISION_TIMEOUT_MS);
    service.update(desktop.millis());assert(service.state()==State::HOSTING && take(desktop,LinkMessageType::JOIN_ACK).payload[8]==0);++checks;
    setup();scene->onButton(b);scene->onButton(a);desktop.advanceMs(Communication::VisitSessionService::SEARCH_TIMEOUT_MS);
    service.update(desktop.millis());assert(service.state()==State::FAILED && std::strcmp(service.viewModel().error,"SEARCH TIMEOUT")==0);++checks;
    setup();hostRequest();scene->onButton(a);desktop.advanceMs(Communication::VisitSessionService::HANDSHAKE_TIMEOUT_MS);
    service.update(desktop.millis());assert(service.state()==State::FAILED && std::strcmp(service.viewModel().error,"LINK TIMEOUT")==0);++checks;
    for(int state:{1,2,4,6,7,10,11}){setup();service.state_=State(state);assert(scene->onButton(a) && service.state()==State::IDLE);}++checks;
    setup();service.state_=State::ENDING;assert(scene->onButton(a) && service.state()==State::ENDING);++checks;
    for(int state:{1,2,4,6,7,8,9,10,11}){setup();service.state_=State(state);assert(!scene->onButton(b) && service.state()==State(state));}++checks;
    for(int state=0;state<12;++state){setup();service.state_=State(state);scene->onExit();
        const bool preserved=state==8 || state==9;assert(link.isEnabled()==preserved);assert(service.state()==(preserved?State(state):State::IDLE));}++checks;
    setup();service.state_=State::SEARCHING;{auto d=advance(0);assert(d.redraw && d.nextUpdateDelayMs==66);
        d=advance(1);assert(!d.redraw && d.nextUpdateDelayMs==66);
        const auto until=500-desktop.millis()%500;d=advance(until);assert(d.redraw && d.nextUpdateDelayMs==66);}++checks;
    setup();service.state_=State::ROOM_LIST;scene->lastState=State::ROOM_LIST;rooms(4);VISUAL(*scene).roomCursor=4;
    {auto d=advance(0);assert(d.redraw && d.nextUpdateDelayMs==200);d=advance(0);assert(!d.redraw && d.nextUpdateDelayMs==200);
        rooms(1);d=advance(0);assert(d.redraw && VISUAL(*scene).roomCursor==4);}++checks;
    setup();VISUAL(*scene).cursor=2;VISUAL(*scene).roomCursor=3;service.state_=State::SEARCHING;
    advance(0);assert(VISUAL(*scene).roomCursor==0 && VISUAL(*scene).cursor==2 && VISUAL(*scene).stateStartedMs==desktop.millis());
    service.state_=State::WAITING_HOST_DECISION;advance(0);assert(VISUAL(*scene).cursor==0);++checks;
    setup();{const auto before=game;const auto model=service.viewModel();scene->render();scene->render();
        assert(std::memcmp(&before,&game,sizeof(game))==0 && service.state()==model.state && link.isEnabled());}++checks;
    setup();{const auto before=game;using Touch=Platform::IInputDevice::TouchType;
        for(int x:{10,100,Hal::DISPLAY_W-10}){assert(scene->touchTarget(x,20)==-1 && !scene->onTap(0));
            assert(!scene->onTouch({Touch::DOWN,int16_t(x),20,desktop.millis()}));scene->cancelTouch();
            assert(!scene->onTouch({Touch::UP,int16_t(x),20,desktop.millis()+20}));}
        assert(std::memcmp(&before,&game,sizeof(game))==0);}++checks;
    for(int exit=0;exit<3;++exit){setup();scene.reset();engine.currentId=SceneID::MENU;engine.requestScene(SceneID::SOCIAL);
        auto* owned=static_cast<SocialScene*>(engine.currentScene.get());assert(owned);
        if(exit==2){owned->onButton(b);owned->onButton(b);assert(owned->onButton(a));}
        else assert(owned->onButton(exit==0?longA:longB));
        assert(engine.currentId==SceneID::MENU && service.state()==State::IDLE && !link.isEnabled());}++checks;
    for(bool host:{false,true}){setup();scene.reset();engine.currentId=SceneID::MENU;engine.requestScene(SceneID::SOCIAL);
        auto* owned=static_cast<SocialScene*>(engine.currentScene.get());service.state_=State::ACTIVE;service.localIsHost_=host;
        const auto d=owned->update(desktop.millis(),0);assert(d.redraw && engine.currentId==SceneID::MAIN && service.active() && link.isEnabled());}++checks;
    setup();hostRequest();scene->onButton(a);drain(desktop);{VisitSyncPayload pet{25,20,0,50,40,30};
        session(desktop,LinkMessageType::VISIT_SYNC,501,71,&pet,sizeof(pet));service.update(desktop.millis());
        auto accept=take(desktop,LinkMessageType::VISIT_ACCEPT);assert(accept.payload[9]==1 && game.teamCount==2);
        ack(desktop,accept);service.update(desktop.millis());assert(service.active() && game.team[1].origin==Game::Origin::VISITOR);
        assert(scene->onButton(a) && service.state()==State::ENDING && game.teamCount==1);
        service.update(desktop.millis());auto end=take(desktop,LinkMessageType::VISIT_END);ack(desktop,end);service.update(desktop.millis());assert(service.state()==State::ENDED);}++checks;
    setup();searchRoom();scene->onButton(a);{auto request=take(desktop,LinkMessageType::JOIN_REQ);auto id=get16(request.payload+6);
        wire(desktop,LinkMessageType::JOIN_ACK,93,id,true);service.update(desktop.millis());assert(service.state()==State::WAITING_ACCEPT);
        auto sync=take(desktop,LinkMessageType::VISIT_SYNC);assert(sync.payload[8]==sizeof(VisitSyncPayload));ack(desktop,sync);
        VisitAcceptPayload accepted{1,0};session(desktop,LinkMessageType::VISIT_ACCEPT,601,id,&accepted,sizeof(accepted));
        service.update(desktop.millis());assert(service.active());auto ping=take(desktop,LinkMessageType::VISIT_PING);ack(desktop,ping);service.update(desktop.millis());
        const auto pet=game.team[0];assert(scene->onButton(a) && service.state()==State::ENDING);
        service.update(desktop.millis());auto recall=take(desktop,LinkMessageType::VISIT_RECALL);ack(desktop,recall);service.update(desktop.millis());
        assert(service.state()==State::ENDED && std::memcmp(&pet,&game.team[0],sizeof(pet))==0);}++checks;
    for(bool host:{false,true}){setup();service.state_=State::ACTIVE;service.localIsHost_=host;link.mode=EspNowLink::Mode::CONNECTED;link.sessionIdValue=71;
        assert(scene->onButton(a) && service.state()==State::ENDING && service.queuedMessage_);
        assert(service.queuedType_==(host?LinkMessageType::VISIT_END:LinkMessageType::VISIT_RECALL));}++checks;
    for(int state=0;state<12;++state){setup();service.state_=State(state);VISUAL(*scene).cursor=2;scene->onEnter();
        assert(VISUAL(*scene).cursor==0 && VISUAL(*scene).roomCursor==0 && VISUAL(*scene).lastVisualTick==UINT32_MAX);
        assert(service.state()==(state==11?State::IDLE:State(state)));}++checks;
    engine.currentScene.reset();scene.reset();service.stop();link.end();
    assert(checks==32);std::printf("social-checkpoints=%u\n",checks);
}
