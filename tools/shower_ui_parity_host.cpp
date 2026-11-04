#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "platform/desktop/DesktopPlatform.h"
#include "assets/PokemonSprites.h"
#include "game/GameRandom.h"
#include "game/MonsterFactory.h"
#include "game/CareTicker.h"
#include "presentation/PixelRenderer.h"
#include "core/UiStrings.h"
#define private public
#include "hardware/Hal.h"
#include "core/GameEngine.h"
#include "scenes/ShowerScene.h"
#undef private

#if STICKMON_SHOWER_UI_BASELINE
#define VISUAL(s) (s)
#else
#define VISUAL(s) ((s).ui.state)
#endif
#if STICKMON_UI_COMPACT_TOUCH && !STICKMON_SHOWER_UI_BASELINE
constexpr bool directDrag = true;
#else
constexpr bool directDrag = false;
#endif
using Mode=ShowerScene::Mode;
using Stage=Game::BathService::Stage;
using Touch=Platform::IInputDevice::TouchType;
class Input final : public Platform::IInputDevice {
public:
    bool hold=false;
    void update() override {}
    bool pressed(Platform::InputButton button) const override {
        return hold && button==Platform::InputButton::PRIMARY;
    }
    bool touchAvailable() const override { return STICKMON_UI_COMPACT_TOUCH; }
};
class Imu final : public Platform::IImuDevice {
public:
    bool available=true;
    float x=0,y=0,z=1;
    unsigned reads=0;
    bool readAcceleration(float& ax,float& ay,float& az) override {
        ++reads;ax=x;ay=y;az=z;return available;
    }
};

int main(int argc,char** argv) {
    assert(argc==3 || argc==4);
    DesktopPlatform desktop(argv[1]);const auto bundle=desktop.serviceBundle();
    Input input;Imu imu;
    Platform::Services services{bundle.lifecycle,bundle.clock,bundle.logger,input,
        bundle.display,bundle.audio,imu,bundle.power,bundle.blobs,bundle.resources,
        bundle.memory,bundle.peers};
    Platform::bind(services);assert(desktop.begin());
    auto& engine=GameEngine::ins();assert(engine.begin());
    auto& state=engine.gameState();std::unique_ptr<ShowerScene> scene;
    unsigned checks=0;
    const ButtonEvent a{0,BtnAction::PRESSED},b{1,BtnAction::PRESSED};
    const ButtonEvent longA{0,BtnAction::LONG_PRESS},longB{1,BtnAction::LONG_PRESS};
    auto setup=[&](uint16_t species=1) -> ShowerScene& {
        scene.reset();engine.currentScene.reset();
        engine.currentId=SceneID::SHOWER;engine.prevId=SceneID::MAIN;
        engine.visitSession.stop();engine.contactVisit={};
        engine.exploreTravel=ExploreTravelPhase::NONE;
        engine.sceneFade=GameEngine::SceneFadePhase::NONE;
        engine.evolutionEventHead=engine.evolutionEventCount=0;
        engine.moveReplacementEventHead=engine.moveReplacementEventCount=0;
        engine.debugBattleRequested=engine.debugMenuReturnRequested=false;
        engine.clearMainSceneViewState();GameRandom::seed(1234);
        state=Game::GameState{};state.oobeDone=true;state.tutorialFlags=0xff;
        state.teamCount=1;state.team[0]=Game::MonsterFactory::create(species,25);
        state.team[0].mood=20;
        for(auto& stock:state.bag.soap)stock=5;
        state.gameMinutesTotal=420;
        Game::resetDailyCareCounters(state);
        engine.gameClock.set(desktop.millis(),state.gameMinutesTotal);
        assert(PokemonSprites::syncTeamCache(&species,1));
        imu.available=true;imu.x=imu.y=0;imu.z=1;imu.reads=0;input.hold=false;
        scene=std::make_unique<ShowerScene>();scene->onEnter();
        engine.saveCoordinator.reset(desktop.millis());
        return *scene;
    };
    auto capture=[&](const std::string& name) {
        PixelRenderer::clear(0);scene->render();const auto fb=desktop.frameBuffer();
        FILE* file=std::fopen((std::string(argv[2])+"/"+name+".rgb565").c_str(),"wb");assert(file);
        assert(std::fwrite(fb.pixels,sizeof(uint16_t),fb.width*fb.height,file)==size_t(fb.width)*fb.height);
        std::fclose(file);
    };
    auto advance=[&](uint32_t ms) {desktop.advanceMs(ms);return scene->update(desktop.millis(),ms*.001f);};
    auto same=[&](const Game::GameState& before){
        if(std::memcmp(&state,&before,sizeof(state))!=0){std::fprintf(stderr,"state mismatch after checkpoints=%u\n",checks);assert(false);}
    };
    auto foamCount=[&](){int count=0;for(const auto& f:scene->bath.foam)count+=f.active;return count;};
    auto waterCount=[&](){int count=0;for(const auto& d:VISUAL(*scene).water)count+=d.active;return count;};
    auto fillFoam=[&](int count,int stage=0) {
        const auto layout=scene->bathLayout();
        for(auto& f:scene->bath.foam)f={};
        for(int i=0;i<count;++i){auto& f=scene->bath.foam[i];f.active=true;f.stage=stage;
            f.x=layout.centerX+(i%4-1.5f)*18;f.y=layout.centerY+(i/4)*26;
            f.restX=f.x;f.restYOffset=-float(i);f.bornMs=desktop.millis();}
    };
    auto ownedIncomplete=[&]() {
        setup();scene.reset();engine.currentId=SceneID::MAIN;
        engine.requestScene(SceneID::SHOWER);
        assert(engine.currentId==SceneID::SHOWER);
        auto* owned=static_cast<ShowerScene*>(engine.currentScene.get());
        owned->soapUsed=true;
        assert(owned->onButton(longB) && owned->mode==Mode::EXIT_CONFIRM);
        assert(owned->onButton(b) && owned->onButton(a) && owned->mode==Mode::INCOMPLETE);
        desktop.advanceMs(699);const auto waiting=owned->update(desktop.millis(),.699f);
        assert(!waiting.redraw && waiting.nextUpdateDelayMs==1);
        desktop.advanceMs(1);owned->update(desktop.millis(),.001f);
        assert(engine.currentId==SceneID::MENU);
    };
    if(argc==4){assert(std::strcmp(argv[3],"incomplete-uaf")==0);ownedIncomplete();return 0;}
    desktop.advanceMs(5000);

    // Preserve complete production frames at state and animation boundaries.
    for(uint16_t species:{1,25,92,143,147}){
        auto& s=setup(species);
        for(int mode=0;mode<8;++mode){s.mode=Mode(mode);VISUAL(s).soapCursor=mode%3;
            s.soapUsed=mode>1;s.completionHearts=3;
            std::snprintf(s.completionMessage,sizeof(s.completionMessage),Ui::Shower::COMPLETE_MESSAGE_FMT,engine.activeSpecies().name);
            capture("mode-species-"+std::to_string(species)+"-"+std::to_string(mode));}
    }
    for(float cursor:{-.75f,-.5f,-.25f,0.f,.25f,.49f,.5f,1.f,1.5f,2.f,2.5f,3.f,3.5f,3.75f,4.f,4.5f}){
        auto& s=setup();VISUAL(s).menuAnimCursor=cursor;capture("menu-cursor-"+std::to_string(int(cursor*100)));
    }
    for(int soap=0;soap<3;++soap)for(int stock:{0,1,99}){
        auto& s=setup();s.mode=Mode::SOAP_SELECT;VISUAL(s).soapCursor=soap;state.bag.soap[soap]=stock;
        capture("soap-picker-"+std::to_string(soap)+"-"+std::to_string(stock));
        s.mode=Mode::SOAPING;VISUAL(s).soapX=53+(Hal::DISPLAY_W-240)/3;VISUAL(s).soapY=38+(Hal::DISPLAY_H-135)/2;
        capture("soap-tool-"+std::to_string(soap)+"-"+std::to_string(stock));
    }
    for(uint16_t species:{1,143,147})for(int stage=0;stage<4;++stage){
        auto& s=setup(species);s.mode=Mode::BRUSHING;fillFoam(8,stage);
        for(auto& f:s.bath.foam)f.y=s.foamRestY(stage)+f.restYOffset;
        capture("foam-stage-"+std::to_string(species)+"-"+std::to_string(stage));
        VISUAL(s).brushX=s.bath.foam[0].x;VISUAL(s).brushY=s.bath.foam[0].y;
        capture("foam-tool-overlap-"+std::to_string(species)+"-"+std::to_string(stage));
    }
    for(int count=0;count<=8;++count){
        auto& s=setup();s.mode=Mode::BRUSHING;fillFoam(count);
        for(int i=0;i<count;++i)s.bath.foam[i].stage=(7-i)%4;
        capture("foam-layered-count-"+std::to_string(count));
    }
    for(uint16_t species:{1,143,147})for(int alpha:{0,1,64,127,254,255}){
        auto& s=setup(species);s.mode=Mode::BRUSHING;VISUAL(s).atmosphereAlpha=alpha;
        capture("atmosphere-"+std::to_string(species)+"-"+std::to_string(alpha));
    }
    for(int count:{0,1,63,64})for(unsigned elapsed:{0u,1u,100u,228u,500u}){
        auto& s=setup();s.mode=Mode::RINSING;s.modeStartedMs=desktop.millis()-elapsed;
        for(int i=0;i<count;++i){auto& d=VISUAL(s).water[i];d.active=true;
            d.x=(i*17)%175+(Hal::DISPLAY_W-240)/3;d.y=float((i*13)%160)-20;
            d.vy=i%2?181:180;d.vx=0;}
        capture("water-count-"+std::to_string(count)+"-"+std::to_string(elapsed));
    }
    for(unsigned elapsed:{0u,1u,79u,159u,160u,319u,320u}){
        auto& s=setup(147);VISUAL(s).hopStartMs=desktop.millis()-elapsed;VISUAL(s).hopHeightPx=10;
        capture("hop-"+std::to_string(elapsed));
    }
    for(unsigned elapsed:{0u,69u,70u,139u,140u,209u,219u,220u}){
        auto& s=setup(25);VISUAL(s).wiggleStartMs=desktop.millis()-elapsed;
        VISUAL(s).turnFlip=true;VISUAL(s).turnUntilMs=desktop.millis()+2600;
        capture("wiggle-over-turn-"+std::to_string(elapsed));
    }
    for(unsigned remaining:{0u,1u,2599u,2600u}){
        auto& s=setup(25);VISUAL(s).turnFlip=true;VISUAL(s).turnUntilMs=desktop.millis()+remaining;
        capture("turn-remaining-"+std::to_string(remaining));
    }
    for(int count=0;count<=3;++count)for(unsigned elapsed:{0u,90u,450u}){
        auto& s=setup();s.mode=Mode::COMPLETE;s.completionHearts=count;s.modeStartedMs=desktop.millis()-elapsed;
        std::snprintf(s.completionMessage,sizeof(s.completionMessage),Ui::Shower::COMPLETE_MESSAGE_FMT,engine.activeSpecies().name);
        capture("completion-hearts-"+std::to_string(count)+"-"+std::to_string(elapsed));
    }
    for(unsigned age:{0u,499u,500u,501u,1099u,1100u}){
        auto& s=setup();
        for(int slot=0;slot<4;++slot){auto& f=VISUAL(s).expFloats[slot];f.active=true;
            f.x=60+slot*22;f.y=60+(Hal::DISPLAY_H-135)/2;f.bornMs=desktop.millis()-age;
            f.heart=slot%2;std::snprintf(f.text,sizeof(f.text),Ui::Shower::EXP_GAIN_FMT,25u+slot);}
        capture("float-age-"+std::to_string(age));
    }
    for(const auto* text:{Ui::Shower::NO_SOAP,Ui::Shower::ALREADY_SOAPED}){
        auto& s=setup();VISUAL(s).toast=text;VISUAL(s).toastUntilMs=desktop.millis()+1200;
        capture(text==Ui::Shower::NO_SOAP?"toast-no-soap":"toast-already-used");
    }
    for(bool yes:{false,true}){auto& s=setup();s.mode=Mode::EXIT_CONFIRM;VISUAL(s).exitConfirmYes=yes;fillFoam(4,2);
        capture("exit-confirm-"+std::to_string(yes));s.mode=Mode::INCOMPLETE;capture("incomplete-choice-"+std::to_string(yes));}
    for(bool motion:{false,true})for(int mode:{2,3,4}){
        auto& s=setup();s.mode=Mode(mode);s.soapUsed=true;fillFoam(8);
        imu.available=motion;input.hold=!motion;s.modeStartedMs=desktop.millis();
        for(int frame=0;frame<25;++frame){imu.x=frame%2?1.3f:-1.3f;imu.y=frame%3?.7f:-.7f;
            advance(50);if(frame%5==0)capture("motion-"+std::to_string(motion)+"-mode-"+std::to_string(mode)+"-frame-"+std::to_string(frame));}
    }

    // Real button and motion journeys, with game and save-state assertions.
    setup();const auto invalid=state;
    assert(!scene->onButton({0,BtnAction::DOWN}) && !scene->onButton({1,BtnAction::UP}));
    assert(!scene->onButton({2,BtnAction::PRESSED}));same(invalid);++checks;

    setup();const auto menuBefore=state;for(int cursor=1;cursor<=4;++cursor){assert(scene->onButton(b));assert(VISUAL(*scene).menuCursor==cursor%4);}
    same(menuBefore);++checks;

    setup();VISUAL(*scene).menuCursor=3;const auto wrap=advance(20);
#if STICKMON_UI_COMPACT_TOUCH && !STICKMON_SHOWER_UI_BASELINE
    assert(!wrap.redraw && wrap.nextUpdateDelayMs==SceneUpdateResult::NO_UPDATE && VISUAL(*scene).menuAnimCursor==3.f);
    VISUAL(*scene).menuAnimCursor=3.75f;VISUAL(*scene).menuCursor=0;advance(20);assert(VISUAL(*scene).menuAnimCursor==0.f);
#else
    assert(wrap.redraw && wrap.nextUpdateDelayMs==66 && VISUAL(*scene).menuAnimCursor==-0.25f);
    VISUAL(*scene).menuAnimCursor=3.75f;VISUAL(*scene).menuCursor=0;advance(20);assert(VISUAL(*scene).menuAnimCursor==3.8125f);
#endif
    ++checks;

    setup();{const auto parked=advance(20);assert(!parked.redraw && parked.nextUpdateDelayMs==SceneUpdateResult::NO_UPDATE && imu.reads==(directDrag ? 0u : 1u));}++checks;

    setup();assert(scene->onButton(longA) && scene->mode==Mode::SOAP_SELECT && VISUAL(*scene).soapCursor==0);
    {const auto d=advance(20);assert(!d.redraw && d.nextUpdateDelayMs==SceneUpdateResult::NO_UPDATE);}
    assert(scene->onButton(b) && VISUAL(*scene).soapCursor==1);assert(scene->onButton(b) && VISUAL(*scene).soapCursor==2);
    assert(scene->onButton(b) && VISUAL(*scene).soapCursor==0);++checks;

    setup();state.bag.soap[0]=state.bag.soap[1]=0;assert(scene->onButton(a) && VISUAL(*scene).soapCursor==2);
    assert(scene->onButton(b) && VISUAL(*scene).soapCursor==2);assert(scene->onButton(longB) && scene->mode==Mode::MENU);
    assert(state.bag.soap[2]==5 && !scene->soapUsed);++checks;

    setup();for(auto& n:state.bag.soap)n=0;assert(scene->onButton(a) && scene->mode==Mode::MENU);
    assert(std::strcmp(VISUAL(*scene).toast,Ui::Shower::NO_SOAP)==0);const auto toastStart=desktop.millis();
    advance(1199);assert(VISUAL(*scene).toast);advance(1);assert(!VISUAL(*scene).toast);
    assert(desktop.millis()==toastStart+1200);++checks;

    setup();scene->onButton(a);const auto stock=state.bag.soap[0];assert(scene->onButton(longA));
    assert(scene->mode==Mode::SOAPING && scene->soapUsed && state.bag.soap[0]==stock-1);
    {Game::GameState loaded;MainSceneViewState view;assert(engine.saveManager.load(loaded,view));assert(loaded.bag.soap[0]==stock-1);}++checks;

    setup();scene->onButton(a);for(auto& n:state.bag.soap)n=0;const auto depleted=state;
    assert(scene->onButton(a) && scene->mode==Mode::SOAP_SELECT && !scene->soapUsed);
    assert(std::strcmp(VISUAL(*scene).toast,Ui::Shower::NO_SOAP)==0);same(depleted);++checks;

    setup();scene->soapUsed=true;assert(scene->onButton(a) && scene->mode==Mode::MENU);
    assert(std::strcmp(VISUAL(*scene).toast,Ui::Shower::ALREADY_SOAPED)==0);++checks;

    for(int mode:{2,3,4}){setup();scene->mode=Mode(mode);scene->soapUsed=true;
        for(auto& d:VISUAL(*scene).water)d.active=true;
        assert(scene->onButton(a) && scene->mode==Mode(mode));
        assert(scene->onButton(longB) && scene->mode==Mode::MENU && waterCount()==0 && scene->soapUsed);}++checks;

    setup();scene->onButton(b);assert(scene->onButton(a) && scene->mode==Mode::BRUSHING);
    {const auto d=advance(20);if(directDrag)assert(!d.redraw && d.nextUpdateDelayMs==SceneUpdateResult::NO_UPDATE);
     else assert(d.redraw && d.nextUpdateDelayMs==66);}assert(foamCount()==0);++checks;

    setup();scene->onButton(b);scene->onButton(b);assert(scene->onButton(a) && scene->mode==Mode::RINSING);
    assert(waterCount()==0);advance(50);assert(waterCount()==4);++checks;

    setup();assert(scene->onButton(longB) && engine.currentId==SceneID::MENU);++checks;

    setup();scene->soapUsed=true;assert(scene->onButton(longB) && scene->mode==Mode::EXIT_CONFIRM && !VISUAL(*scene).exitConfirmYes);
    assert(scene->onButton(longB) && !VISUAL(*scene).exitConfirmYes);
    {const auto d=advance(20);assert(!d.redraw && d.nextUpdateDelayMs==SceneUpdateResult::NO_UPDATE);}
    assert(scene->onButton(longA) && scene->mode==Mode::MENU && scene->soapUsed);
    scene->onButton(longB);assert(scene->onButton(b) && VISUAL(*scene).exitConfirmYes);
    assert(scene->onButton(longA) && scene->mode==Mode::INCOMPLETE);
    {const auto d=advance(699);assert(!d.redraw && d.nextUpdateDelayMs==1);}++checks;

    if (directDrag) {
        setup();scene->mode=Mode::SOAPING;scene->soapUsed=true;input.hold=true;imu.x=2;
        for(int i=0;i<20;++i)advance(200);
        assert(foamCount()==0 && !scene->soapRewarded && imu.reads==0);
        checks+=4;
    } else {
    setup();scene->mode=Mode::SOAPING;scene->soapUsed=true;advance(50);assert(foamCount()==0);
    imu.x=2;advance(200);assert(foamCount()>0 && scene->soapRewarded && imu.reads==2);
    {const auto exp=state.team[0].exp;advance(50);assert(state.team[0].exp==exp);}++checks;

    setup();scene->mode=Mode::SOAPING;scene->soapUsed=true;imu.available=false;advance(200);assert(foamCount()==0);
    input.hold=true;advance(200);assert(foamCount()>0 && scene->soapRewarded);++checks;

    setup();scene->mode=Mode::SOAPING;scene->soapUsed=true;imu.available=true;input.hold=true;
    advance(250);assert(foamCount()==0 && !scene->soapRewarded);++checks;

    setup();scene->mode=Mode::SOAPING;scene->soapUsed=true;imu.available=false;input.hold=true;
    for(int i=0;i<20;++i)advance(200);assert(foamCount()==8);
    {const auto exp=state.team[0].exp;advance(200);assert(foamCount()==8 && state.team[0].exp==exp);}++checks;

    }

    setup();scene->mode=Mode::BRUSHING;fillFoam(2);auto& source=scene->bath.foam[0];auto& partner=scene->bath.foam[1];
    source.x=partner.x=scene->bathLayout().centerX;source.y=partner.y=scene->bathLayout().centerY;
    scene->rubFoamAt(source.x,source.y,desktop.millis());assert(source.brushProgress==1 && foamCount()==2);
    scene->rubFoamAt(source.x,source.y,desktop.millis());assert(foamCount()==1 && source.stage==2 && VISUAL(*scene).hopStartMs==desktop.millis());++checks;

    setup();scene->mode=Mode::BRUSHING;fillFoam(2,2);
    for(auto& f:scene->bath.foam)if(f.active){f.y=scene->foamRestY(2);f.brushProgress=4;}
    assert(scene->tryMergeFoam(scene->bath.foam[0],desktop.millis()));assert(foamCount()==1 && scene->bath.foam[0].stage==3);++checks;

    setup();fillFoam(8,0);scene->checkAtmosphereThreshold(desktop.millis());assert(!scene->bath.atmosphereTarget && foamCount()==8);
    scene->bath.foam[0].stage=2;for(auto& f:scene->bath.foam)if(f.active)f.y=scene->bathLayout().lowerY;
    scene->checkAtmosphereThreshold(desktop.millis());assert(scene->bath.atmosphereTarget && foamCount()==0 && scene->brushRewarded);
    {const auto exp=state.team[0].exp;scene->checkAtmosphereThreshold(desktop.millis());assert(state.team[0].exp==exp);}++checks;

    setup();scene->bath.atmosphereTarget=true;advance(1000);assert(VISUAL(*scene).atmosphereAlpha==15.f);
    scene->bath.atmosphereTarget=false;advance(1000);assert(VISUAL(*scene).atmosphereAlpha==0.f);++checks;

    setup();scene->mode=Mode::BRUSHING;fillFoam(1,2);auto& floating=scene->bath.foam[0];
    floating.y=scene->foamRestY(2)-10;floating.restX=floating.x+10;
    {const auto y=floating.y,x=floating.x;advance(50);assert(floating.y>y && floating.x>x);}++checks;

    setup();scene->mode=Mode::RINSING;scene->modeStartedMs=desktop.millis();scene->lastRinseFoamMs=desktop.millis();fillFoam(1,3);
    advance(349);assert(scene->bath.foam[0].stage==3);advance(1);assert(scene->bath.foam[0].stage==2);
    advance(350);assert(scene->bath.foam[0].stage==1);advance(350);assert(scene->bath.foam[0].stage==0);
    advance(350);assert(foamCount()==0);++checks;

    setup();scene->mode=Mode::RINSING;scene->modeStartedMs=desktop.millis();VISUAL(*scene).lastRinseShakeMs=desktop.millis();
    for(auto& d:VISUAL(*scene).water){d.active=true;d.x=10;d.y=100;d.vy=0;d.vx=0;}
    advance(50);assert(waterCount()==64);++checks;

    setup();scene->mode=Mode::RINSING;scene->modeStartedMs=desktop.millis();VISUAL(*scene).lastRinseShakeMs=desktop.millis();
    {auto& drop=VISUAL(*scene).water[0];drop.active=true;drop.x=scene->bathLayout().centerX;drop.y=120+(Hal::DISPLAY_H-135)/2;drop.vy=200;
    advance(50);assert(drop.active && drop.vy<0);}++checks;

    for(int score=1;score<=3;++score){setup();scene->soapUsed=score>=2;scene->soapRewarded=score>=2;scene->brushRewarded=score==3;
        state.team[0].hpCur=1;const auto maximum=state.team[0].hpMax;
        scene->beginRinse(desktop.millis());advance(1799);assert(scene->mode==Mode::RINSING && state.team[0].hpCur==1);
        advance(1);assert(scene->mode==Mode::COMPLETE && scene->completionHearts==score && !scene->soapUsed && scene->rinseRewarded);
        const auto expected=std::min<uint32_t>(maximum,1+(maximum*Game::BathService::hpRecoveryPercentForScore(score)+99)/100);
        assert(state.team[0].hpCur==expected && engine.saveCoordinator.dirty());
        const auto awarded=state;advance(1499);same(awarded);advance(1);same(awarded);
        assert(scene->mode==Mode::MENU && !scene->soapRewarded && !scene->brushRewarded && !scene->rinseRewarded && scene->completionHearts==0);
        assert(engine.saveNow());Game::GameState loaded;MainSceneViewState view;assert(engine.saveManager.load(loaded,view));
        assert(loaded.team[0].hpCur==state.team[0].hpCur && loaded.team[0].exp==state.team[0].exp && loaded.bag.soap[0]==state.bag.soap[0]);}++checks;

    setup();scene->awardBathStage(Stage::SOAP);const auto reward=state;
    scene->awardBathStage(Stage::SOAP);same(reward);assert(scene->soapRewarded);++checks;

    setup();state.team[0].origin=Game::Origin::VISITOR;const auto visitor=state;
    scene->awardBathStage(Stage::SOAP);scene->awardBathStage(Stage::BRUSH);scene->awardBathStage(Stage::RINSE);same(visitor);++checks;

    setup();VISUAL(*scene).hopStartMs=desktop.millis();VISUAL(*scene).hopHeightPx=10;
    advance(319);assert(VISUAL(*scene).hopStartMs);{const auto d=advance(1);assert(d.redraw && !VISUAL(*scene).hopStartMs);}++checks;

    setup();VISUAL(*scene).wiggleStartMs=desktop.millis();VISUAL(*scene).turnUntilMs=desktop.millis()+2600;
    advance(220);assert(!VISUAL(*scene).wiggleStartMs && VISUAL(*scene).turnUntilMs);
    advance(2380);assert(!VISUAL(*scene).turnUntilMs);++checks;

    setup();for(auto& f:VISUAL(*scene).expFloats){f.active=true;f.bornMs=desktop.millis();}
    advance(1099);for(const auto& f:VISUAL(*scene).expFloats)assert(f.active);
    {const auto d=advance(1);assert(d.redraw);for(const auto& f:VISUAL(*scene).expFloats)assert(!f.active);}++checks;

#if STICKMON_UI_COMPACT_TOUCH && !STICKMON_SHOWER_UI_BASELINE
    auto touch=[&](Touch type,int x,int y,uint32_t elapsed=20) {
        desktop.advanceMs(elapsed);
        return scene->onTouch({type,int16_t(x),int16_t(y),desktop.millis()});
    };
    auto tap=[&](int x,int y) {touch(Touch::DOWN,x,y);return touch(Touch::UP,x,y);};
    setup();const auto untouched=state;
    // All four tools keep their position, regardless of the selected index.
    for(int cursor=0;cursor<4;++cursor){VISUAL(*scene).menuCursor=cursor;advance(20);
        for(int i=0;i<4;++i)assert(scene->touchTarget(254,48+i*48)==100+i);}
    assert(!touch(Touch::DOWN,254,48));same(untouched);
    scene->cancelTouch();assert(!touch(Touch::UP,254,48));same(untouched);
    touch(Touch::DOWN,254,48);touch(Touch::MOVE,230,48);assert(!touch(Touch::UP,254,48));same(untouched);
    touch(Touch::DOWN,254,48);assert(!touch(Touch::UP,254,48,1001));same(untouched);
    touch(Touch::DOWN,254,48);touch(Touch::CANCEL,254,48);assert(!touch(Touch::UP,254,48));same(untouched);
    touch(Touch::DOWN,254,48);scene->onButton(b);assert(!touch(Touch::UP,254,48));same(untouched);
    touch(Touch::DOWN,254,48);assert(!touch(Touch::UP,254,96));same(untouched);
    assert(tap(254,48) && scene->mode==Mode::SOAP_SELECT);same(untouched);
    assert(!tap(148,78) && scene->mode==Mode::SOAP_SELECT);same(untouched);
    assert(scene->touchTarget(232,135)==122);
    assert(!scene->onTap(101) && scene->mode==Mode::SOAP_SELECT);same(untouched);
    assert(tap(148,135) && scene->mode==Mode::SOAPING && state.bag.soap[1]==4);
    const auto consumed=state;
    assert(!touch(Touch::UP,148,135));same(consumed);
    assert(tap(254,48) && scene->mode==Mode::SOAPING);same(consumed);
    // Touch motion is the only scrub input, with pickup, release and cancellation boundaries.
    input.hold=true;imu.x=2;
    assert(touch(Touch::DOWN,100,130));advance(200);assert(foamCount()==0);
    assert(touch(Touch::MOVE,140,130));advance(200);
    assert(foamCount()>0 && scene->soapRewarded && VISUAL(*scene).soapX==140 && VISUAL(*scene).soapY==130);
    const auto spawnStopped=scene->bath.spawnCursor;
    advance(200);assert(scene->bath.spawnCursor==spawnStopped);
    assert(touch(Touch::UP,140,130));advance(200);assert(scene->bath.spawnCursor==spawnStopped);
    capture("touch-soap-drag");
    touch(Touch::DOWN,100,130);touch(Touch::MOVE,140,130);scene->cancelTouch();advance(200);
    assert(scene->bath.spawnCursor==spawnStopped);
    touch(Touch::MOVE,110,130);touch(Touch::UP,110,130);advance(200);assert(scene->bath.spawnCursor==spawnStopped);
    touch(Touch::DOWN,100,130);touch(Touch::MOVE,140,130);scene->onButton(a);advance(200);
    assert(scene->bath.spawnCursor==spawnStopped);
    touch(Touch::DOWN,100,130);touch(Touch::MOVE,254,96);touch(Touch::UP,254,96);
    assert(scene->mode==Mode::SOAPING);
    // Button clicks remain valid while an active tool's update runs between down and up.
    touch(Touch::DOWN,254,96);advance(200);assert(touch(Touch::UP,254,96) && scene->mode==Mode::BRUSHING);
    fillFoam(2);for(auto& f:scene->bath.foam)if(f.active){f.x=120;f.y=140;f.restX=120;}
    touch(Touch::DOWN,120,140);advance(200);
    assert(scene->bath.foam[0].brushProgress==0);
    touch(Touch::MOVE,130,140);advance(200);assert(scene->bath.foam[0].brushProgress==1);
    advance(200);assert(scene->bath.foam[0].brushProgress==1);
    touch(Touch::MOVE,120,140);advance(600);assert(foamCount()==1 && scene->bath.foam[0].stage==2);
    touch(Touch::UP,120,140);capture("touch-brush-drag");
    input.hold=false;
    const auto scrubbed=state;
    assert(tap(254,96) && scene->mode==Mode::BRUSHING);same(scrubbed);
    assert(tap(254,144) && scene->mode==Mode::RINSING);advance(20);assert(waterCount()>0);
    const auto rinseStarted=scene->modeStartedMs;
    assert(tap(254,144) && scene->modeStartedMs==rinseStarted);
    assert(tap(254,96) && scene->mode==Mode::BRUSHING && waterCount()==0);
    assert(tap(254,192) && scene->mode==Mode::EXIT_CONFIRM && !VISUAL(*scene).exitConfirmYes);
    assert(!tap(254,144) && scene->mode==Mode::EXIT_CONFIRM);same(scrubbed);
    assert(tap(157,90) && scene->mode==Mode::MENU && scene->soapUsed);
    assert(tap(254,192) && scene->mode==Mode::EXIT_CONFIRM);
    assert(tap(90,90) && scene->mode==Mode::INCOMPLETE);
    assert(!tap(254,96) && scene->mode==Mode::INCOMPLETE);
    setup();scene->mode=Mode::COMPLETE;assert(!tap(254,96) && scene->mode==Mode::COMPLETE);
    setup();for(auto& n:state.bag.soap)n=0;
    assert(tap(254,48) && scene->mode==Mode::MENU && !scene->soapUsed);
    // The picker consumes the released card directly, exactly once; every variant is reachable.
    for(int index=0;index<Game::SOAP_VARIANT_COUNT;++index){
        setup();assert(tap(254,48) && scene->mode==Mode::SOAP_SELECT);
        const auto stock=state.bag.soap[index];
        touch(Touch::DOWN,64+index*84,135);assert(state.bag.soap[index]==stock && !scene->soapUsed);
        assert(touch(Touch::UP,64+index*84,135) && scene->mode==Mode::SOAPING);
        assert(VISUAL(*scene).soapCursor==index && state.bag.soap[index]==stock-1);
        const auto used=state;assert(!touch(Touch::UP,64+index*84,135));same(used);
        Game::GameState loaded;MainSceneViewState view;assert(engine.saveManager.load(loaded,view));
        assert(loaded.bag.soap[index]==stock-1);
    }
    setup();state.bag.soap[1]=0;assert(tap(254,48));const auto beforePicker=state;
    capture("touch-soap-picker-empty");
    assert(scene->touchTarget(148,135)==-1 && !tap(148,135));same(beforePicker);
    assert(!scene->onTap(121) && scene->mode==Mode::SOAP_SELECT);same(beforePicker);
    // Blank panel space and the old USE area never activate a tool or consume stock.
    assert(!tap(148,78) && !tap(186,195) && !tap(254,192));same(beforePicker);
    touch(Touch::DOWN,64,135);touch(Touch::MOVE,148,135);assert(!touch(Touch::UP,64,135));same(beforePicker);
    touch(Touch::DOWN,64,135);touch(Touch::CANCEL,64,135);assert(!touch(Touch::UP,64,135));same(beforePicker);
    touch(Touch::DOWN,64,135);scene->cancelTouch();assert(!touch(Touch::UP,64,135));same(beforePicker);
    touch(Touch::DOWN,64,135);assert(!touch(Touch::UP,64,135,1001));same(beforePicker);
    touch(Touch::DOWN,64,135);scene->onButton(b);assert(!touch(Touch::UP,64,135));same(beforePicker);
    touch(Touch::DOWN,64,135);state.bag.soap[0]=0;
    const auto depletedPicker=state;assert(!touch(Touch::UP,64,135));same(depletedPicker);
    capture("touch-soap-picker-one-owned");
    assert(tap(256,60) && scene->mode==Mode::MENU);same(depletedPicker);
    assert(tap(254,48) && VISUAL(*scene).soapCursor==2);
    assert(tap(285,144) && scene->mode==Mode::MENU);same(depletedPicker);
    assert(!touch(Touch::UP,285,144) && scene->mode==Mode::MENU);
    assert(tap(254,48));assert(scene->onButton(longB) && scene->mode==Mode::MENU);same(depletedPicker);
    // The engine owns and destroys the scene synchronously on a clean exit.
    setup();scene.reset();engine.currentId=SceneID::MAIN;engine.requestScene(SceneID::SHOWER);
    auto* owned=static_cast<ShowerScene*>(engine.currentScene.get());
    owned->onTouch({Touch::DOWN,254,192,desktop.millis()});
    assert(owned->onTouch({Touch::UP,254,192,desktop.millis()+20}));
    assert(engine.currentId==SceneID::MENU);
#endif

    setup();const auto beforeTouch=state;
    for(int x:{10,100,Hal::DISPLAY_W-10}){assert(scene->touchTarget(x,20)==-1 && !scene->onTap(0));
        assert(!scene->onTouch({Touch::DOWN,int16_t(x),20,desktop.millis()}));scene->cancelTouch();
        assert(!scene->onTouch({Touch::UP,int16_t(x),20,desktop.millis()+20}));}same(beforeTouch);++checks;

    setup();fillFoam(8,2);const auto rendered=state;const auto bath=scene->bath;
    scene->render();scene->render();same(rendered);assert(std::memcmp(&bath,&scene->bath,sizeof(bath))==0);++checks;
#if !STICKMON_SHOWER_UI_BASELINE
    ownedIncomplete();
#endif
    engine.currentScene.reset();scene.reset();std::printf("shower-checkpoints=%u\n",checks);
}
