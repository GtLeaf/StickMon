#include <cassert>
#include <cstring>
#include <cstdio>
#include <functional>
#include "game/GameState.h"
#include "game/MonsterFactory.h"
#include "platform/desktop/DesktopPlatform.h"
#include <memory>
#include <vector>
#define private public
#include "core/GameEngine.h"
#include "scenes/MainScene.h"
#undef private
#if __has_include("game/StandbyCare.h")
#include "game/StandbyCare.h"
#include "StandbyPolicy.h"
#include "StandbyCheckpoint.h"
#include "StandbyRuntime.h"
#include "StandbyBootFlow.h"
#include "core/ContentStore.h"
#include "core/ContentValidation.h"
#define HAS_STANDBY 1
#endif
extern "C" unsigned uzlib_crc32(const void*,unsigned,unsigned){assert(false);return 0;}
extern "C" unsigned uzlib_adler32(const void*,unsigned,unsigned){assert(false);return 0;}
class FailingBlobs:public Platform::IBlobStore {
public:
    explicit FailingBlobs(Platform::IBlobStore& base):base_(base){}
    bool fail=false;
    bool initialize()override{return base_.initialize();}
    size_t blobSize(const char*n,const char*k)override{return base_.blobSize(n,k);}
    bool readBlob(const char*n,const char*k,void*d,size_t s)override{return base_.readBlob(n,k,d,s);}
    bool writeBlob(const char*n,const char*k,const void*d,size_t s)override{return !fail&&base_.writeBlob(n,k,d,s);}
    bool removeBlob(const char*n,const char*k)override{return !fail&&base_.removeBlob(n,k);}
    bool clearNamespace(const char*n)override{return !fail&&base_.clearNamespace(n);}
private:Platform::IBlobStore& base_;
};
int main(int argc,char** argv){
    assert(argc==2);DesktopPlatform desktop(argv[1]);Platform::bind(desktop.serviceBundle());assert(desktop.begin());
#if STICKMON_EXTERNAL_CONTENT
    assert(Platform::resources().mount());ContentStore catalog;ContentData::bind(&catalog);
    assert(validateContentCatalog()&&catalog.healthy());
#endif
    auto& engine=GameEngine::ins();assert(engine.begin());
#ifndef HAS_STANDBY
    assert(false&&"adaptive standby policy and chronological care are missing");
#else
    using namespace Sdgoods;
    struct Board {
        uint64_t now=1000000;bool asleep=true,key=false,light=false;uint64_t deepUs=0;uint32_t waited=0;
        uint64_t standbyMicros()const{return now;}
        bool locked()const{return asleep;}bool powerKeyActive()const{return key;}
        void waitStandby(uint32_t ms,bool allowed){waited=ms;light=allowed;}
        void enterDeepSleep(uint64_t us,bool external){assert(external);deepUs=us;}
    } board;
    struct Session {bool ok=true;int saves=0,polls=0;std::function<void()> onPoll;bool save(){++saves;return ok;}void poll(){++polls;if(onPoll)onPoll();}} session;
    struct Engine {
        Game::GameState s{};Game::StandbyCare::Context c{};MainSceneViewState view{};
        bool safe=true,offline=false;int begins=0,ends=0;
        void adaptiveStandbyBegin(){++begins;}void adaptiveStandbyEnd(){++ends;}
        bool adaptiveStandbySafe()const{return safe;}
        Game::StandbyCare::Context adaptiveStandbyContext(){return c;}
        void adaptiveStandbyRestore(const Game::StandbyCare::Context& n,bool off){c=n;offline=off;}
        Game::GameState& gameState(){return s;}const MainSceneViewState& mainSceneViewState()const{return view;}
    } fake;
    struct Retention {
        StandbyCheckpoint checkpoint{};int writes=0,clears=0;
        void store(const StandbyCheckpoint& c){checkpoint=c;++writes;}void clear(){checkpoint={};++clears;}
    } retained;
    StandbyRuntime<Board,Session,Engine,Retention> runtime(board,session,fake,retained);
    runtime.poll();runtime.wait();assert(board.waited==30000&&board.light&&fake.begins==1);
    board.now+=17ULL*1000000;board.key=true;runtime.beforePowerActions();
    assert(fake.c.gameMs==17000);runtime.poll();assert(!board.deepUs);
    board.asleep=false;runtime.beforePowerActions();assert(fake.ends==1&&!fake.offline);
    board.key=false;board.asleep=true;runtime.poll();runtime.wait();assert(board.waited==30000&&fake.begins==2);
    board.now+=300000000;runtime.poll();runtime.wait();assert(board.waited==300000&&board.light);
    board.now+=3300000000ULL;session.ok=false;runtime.poll();
    assert(!board.deepUs&&!retained.writes&&session.saves==1);
    board.now+=1000000;session.ok=true;runtime.poll();
    assert(board.deepUs==7200000000ULL&&retained.writes==1);
    assert(retained.checkpoint.valid(standbySnapshotHash(fake.s,fake.view),true,board.now));
    // Unsafe scenes keep a short running cadence and cannot enter deep sleep.
    Board busyBoard;Session busySession;Engine busyEngine;Retention busyRetained;busyEngine.safe=false;
    StandbyRuntime<Board,Session,Engine,Retention> busy(busyBoard,busySession,busyEngine,busyRetained);
    busy.poll();busy.wait();assert(busyBoard.waited==1000&&!busyBoard.light);
    busyBoard.now+=4ULL*86400000000;busy.poll();assert(!busyBoard.deepUs&&busySession.polls==2);

    Board slowBoard;Session slowSession;Engine slowEngine;Retention slowRetained;slowEngine.safe=false;
    slowSession.onPoll=[&]{slowBoard.now+=2000000;slowEngine.c.gameMs+=2000;slowEngine.safe=true;};
    StandbyRuntime<Board,Session,Engine,Retention> slow(slowBoard,slowSession,slowEngine,slowRetained);
    slow.poll();assert(slowEngine.c.gameMs==2000);
    struct BootStorage {
        Game::GameState durable{};MainSceneViewState view{};bool good=true,fail=false;
        int writes=0,begins=0;
        bool begin(){++begins;return good;}
        bool load(Game::GameState& s,MainSceneViewState& v){s=durable;v=view;return good;}
        bool save(const Game::GameState& s,const MainSceneViewState& v){++writes;if(fail)return false;durable=s;view=v;return true;}
    } storage;
    struct BootRetention:Retention {StandbyCheckpoint read()const{return checkpoint;}} bootRetained;
    Board bootBoard;bootBoard.now=1000000;
    storage.durable=engine.gameState();storage.durable.oobeDone=true;storage.durable.teamCount=1;
    storage.durable.team[0]=Game::MonsterFactory::create(25,20);storage.durable.team[0].satiety=100;
    bootRetained.checkpoint.enteredUs=bootRetained.checkpoint.appliedUs=bootBoard.now;
    bootRetained.checkpoint.context.gameMs=7ULL*60*60000;
    storage.durable.gameMinutesTotal=7*60;
    bootRetained.checkpoint.snapshotHash=standbySnapshotHash(storage.durable,storage.view);bootRetained.checkpoint.seal();
    const auto originalCheckpoint=bootRetained.checkpoint;
    const auto originalDurable=storage.durable;
    bootBoard.now+=7200000000ULL;storage.fail=true;
    auto failedWake=resumeStandbyBootFlow(bootBoard,storage,bootRetained,StandbyWake::KEY,true);
    assert(failedWake.restored&&failedWake.pendingState);
    assert(!std::memcmp(&bootRetained.checkpoint,&originalCheckpoint,sizeof(originalCheckpoint)));
    assert(failedWake.pendingState->gameMinutesTotal==9*60&&failedWake.pendingState->team[0].satiety<100);
    storage.fail=false;
    assert(storage.save(*failedWake.pendingState,storage.view));bootRetained.clear();
    assert(storage.durable.gameMinutesTotal==9*60);
    // A damaged timer anchor stays dark; no catalog load or snapshot write may replay it.
    BootRetention invalidRetained;Board invalidBoard;BootStorage invalidStorage;
    auto invalid=resumeStandbyBootFlow(invalidBoard,invalidStorage,invalidRetained,StandbyWake::TIMER,true);
    assert(!invalid.restored&&invalidBoard.deepUs&&invalidStorage.writes==0);
    Board coldBoard;BootStorage coldStorage;BootRetention stale;stale.checkpoint=originalCheckpoint;
    auto cold=resumeStandbyBootFlow(coldBoard,coldStorage,stale,StandbyWake::TIMER,false);
    assert(!cold.restored&&!coldBoard.deepUs&&!coldStorage.begins);
    // Failed timer commits keep the original anchor so the next attempt replays once.
    Board timerBoard;timerBoard.now=bootBoard.now;BootStorage timerStorage;timerStorage.durable=originalDurable;
    timerStorage.durable.gameMinutesTotal=7*60;timerStorage.durable.team[0].satiety=100;
    BootRetention timerRetention;timerRetention.checkpoint=originalCheckpoint;timerStorage.fail=true;
    auto timer=resumeStandbyBootFlow(timerBoard,timerStorage,timerRetention,StandbyWake::TIMER,true);
    assert(!timer.restored&&timerBoard.deepUs&&timerRetention.checkpoint.appliedUs==originalCheckpoint.appliedUs);
    timerStorage.fail=false;timerBoard.deepUs=0;
    timer=resumeStandbyBootFlow(timerBoard,timerStorage,timerRetention,StandbyWake::TIMER,true);
    assert(!timer.restored&&timerBoard.deepUs&&timerStorage.durable.gameMinutesTotal==9*60);
    assert(timerRetention.checkpoint.enteredUs==originalCheckpoint.enteredUs);
    // A physical key pressed during a timer boot takes the normal wake path.
    Board interceptedBoard;interceptedBoard.now=bootBoard.now;interceptedBoard.key=true;
    BootStorage interceptedStorage;interceptedStorage.durable=originalDurable;
    BootRetention interceptedRetention;interceptedRetention.checkpoint=originalCheckpoint;
    auto intercepted=resumeStandbyBootFlow(interceptedBoard,interceptedStorage,interceptedRetention,StandbyWake::TIMER,true);
    assert(intercepted.restored&&!interceptedBoard.deepUs&&interceptedRetention.clears==1);
    for(bool corruptSave:{false,true}){
        Board blockedBoard;blockedBoard.now=bootBoard.now;
        BootStorage blockedStorage;blockedStorage.durable=originalDurable;
        if(corruptSave)++blockedStorage.durable.coins;else blockedStorage.good=false;
        BootRetention blockedRetention;blockedRetention.checkpoint=originalCheckpoint;
        auto blocked=resumeStandbyBootFlow(blockedBoard,blockedStorage,blockedRetention,StandbyWake::TIMER,true);
        assert(!blocked.restored&&blockedBoard.deepUs&&!blockedStorage.writes);
    }
    const uint64_t ages[]={0,300000,3600000,86400000,259200000};
    const uint32_t waits[]={30000,300000,7200000,43200000,86400000};
    for(int i=0;i<5;++i){assert(StandbyPolicy::tier(ages[i])==i);assert(StandbyPolicy::intervalMs(ages[i])==waits[i]);if(i)assert(StandbyPolicy::tier(ages[i]-1)==i-1);}
    assert(StandbyPolicy::waitMs(299999)==1&&StandbyPolicy::waitMs(3599999)==1);
    assert(StandbyPolicy::intervalMs(UINT64_MAX)==86400000);
    auto seed=engine.gameState();seed.oobeDone=true;seed.teamCount=1;
    seed.team[0]=Game::MonsterFactory::create(25,20);seed.team[0].satiety=100;seed.gameMinutesTotal=7*60;seed.room.bowlCount=0;
    Game::StandbyCare::Context a{},b{};a.gameMs=b.gameMs=uint64_t(seed.gameMinutesTotal)*60000;
    auto normal=seed,dark=seed;
    Game::StandbyCare::advance(normal,a,12*3600000ULL,1,false,true,false);
    Game::StandbyCare::advance(dark,b,12*3600000ULL,1,true,true,false);
    assert(normal.team[0].satiety==0&&dark.team[0].satiety==50);
    // Fractional hunger survives sub-minute lock cycles and day/night changes.
    auto whole=seed,split=seed;Game::StandbyCare::Context x{},y{};x.gameMs=y.gameMs=uint64_t(seed.gameMinutesTotal)*60000;
    Game::StandbyCare::advance(whole,x,864000,1,true,true,false);
    for(int i=0;i<8640;++i)Game::StandbyCare::advance(split,y,100,1,true,true,false);
    assert(whole.team[0].satiety==99&&split.team[0].satiety==99);
    assert(x.care.satietyFraction[0]==y.care.satietyFraction[0]&&x.gameMs==y.gameMs);
    seed.teamCount=2;seed.team[1]=Game::MonsterFactory::create(133,20);seed.team[1].satiety=50;seed.team[0].satiety=40;
    seed.room.bowlFood=1;seed.room.bowlCount=1;seed.room.bowlBitesRemaining=5;
    whole=split=seed;x={};y={};x.gameMs=y.gameMs=uint64_t(seed.gameMinutesTotal)*60000;
    const auto stock=seed.room.food[1];
    Game::StandbyCare::advance(whole,x,3*86400000ULL,8,true,true,true);
    uint64_t remaining=3*86400000ULL;
    while(remaining){auto n=std::min<uint64_t>(37001,remaining);Game::StandbyCare::advance(split,y,n,8,true,true,true);remaining-=n;}
    assert(!std::memcmp(&whole,&split,sizeof(whole))&&x.gameMs==y.gameMs);
    assert(whole.room.bowlCount==0&&whole.room.food[1]==stock&&whole.coins==seed.coins);
    assert(!std::memcmp(x.care.satietyFraction,y.care.satietyFraction,sizeof(x.care.satietyFraction)));
    // Scheduled sleepers leave food alone until the shared urgent threshold;
    // visitors, battle-sleep status and fixed cocoons must not take a bite.
    auto eligible=seed;eligible.teamCount=1;eligible.gameMinutesTotal=23*60;
    eligible.team[0].nature=0;eligible.team[0].satiety=36;
    auto bites=eligible.room.bowlBitesRemaining;
    assert(!Game::StandbyCare::feed(eligible)&&eligible.room.bowlBitesRemaining==bites);
    eligible.team[0].satiety=34;assert(Game::StandbyCare::feed(eligible));
    for(int category=0;category<4;++category){
        auto excluded=seed;excluded.teamCount=1;excluded.team[0].satiety=10;
        if(category==0)excluded.team[0].origin=Game::Origin::VISITOR;
        if(category==1)excluded.team[0].majorStatus=Game::MajorStatus::SLEEP;
        if(category==2)excluded.team[0].speciesId=11;
        if(category==3)excluded.team[0].fainted=true;
        assert(!Game::StandbyCare::feed(excluded)&&excluded.room.bowlBitesRemaining==seed.room.bowlBitesRemaining);
    }
    auto contact=seed;contact.teamCount=1;contact.team[0].origin=Game::Origin::VISITOR;
    Game::StandbyCare::Context visitorContext{};visitorContext.gameMs=7ULL*60*60000;
    Game::StandbyCare::advance(contact,visitorContext,86400000,1,true,true,true);
    assert(contact.team[0].satiety==seed.team[0].satiety);
    StandbyCheckpoint cp{};cp.enteredUs=100;cp.appliedUs=1000;cp.context=x;cp.snapshotHash=123;cp.seal();
    assert(cp.valid(123,true,1000)&&cp.valid(123,true,1000+86400000000ULL));
    assert(!cp.valid(123,false,1000)&&!cp.valid(124,true,1000)&&!cp.valid(123,true,999));
    auto corrupt=cp;corrupt.context.gameMs++;assert(!corrupt.valid(123,true,1000));
    engine.gameState()=seed;engine.switchScene(SceneID::MAIN,false);
    engine.adaptiveStandbyBegin();assert(engine.adaptiveStandbySafe());
    auto bridge=engine.adaptiveStandbyContext();
    Game::StandbyCare::advance(engine.gameState(),bridge,7200000,1,true,true,true);
    engine.adaptiveStandbyRestore(bridge,true);const auto frozen=engine.gameMinutesTotal();desktop.advanceMs(2000);
    assert(engine.gameMinutesTotal()==frozen&&engine.saveNow());
    engine.adaptiveStandbyRestore(bridge,false);engine.adaptiveStandbyEnd();desktop.advanceMs(60000);
    assert(engine.gameMinutesTotal()>frozen);
    // A lock that ends between core ticks must charge the dark portion at half rate.
    engine.gameState()=seed;engine.gameState().room.bowlCount=0;engine.gameState().team[0].satiety=100;
    Game::StandbyCare::Context boundary{};boundary.gameMs=7ULL*60*60000;
    engine.adaptiveStandbyRestore(boundary,false);engine.adaptiveStandbyBegin();
    desktop.advanceMs(500000);engine.adaptiveStandbyEnd();
    desktop.advanceMs(100000);engine.syncGameClock(desktop.millis());engine.tickCare(desktop.millis());
    assert(engine.gameState().team[0].satiety==100);
    assert(engine.careAcc.satietyFraction[0]==1400000);
    auto& main=*static_cast<MainScene*>(engine.currentScene.get());
    main.pairInteractionPhase=MainScene::PairInteractionPhase::NONE;
    main.pairInteraction=MainScene::PairInteraction::NONE;
    assert(engine.adaptiveStandbySafe());
    main.progressionModal=MainScene::ProgressionModal::LEVEL_UP;
    assert(!engine.adaptiveStandbySafe());
    main.progressionModal=MainScene::ProgressionModal::NONE;
    // Canonical fingerprint must survive the real save/load path. Failed writes
    // leave the old checkpoint usable and replay from its durable snapshot.
    auto original=desktop.serviceBundle();FailingBlobs blobs(original.blobs);
    Platform::Services services{original.lifecycle,original.clock,original.logger,original.input,original.display,
        original.audio,original.imu,original.power,blobs,original.resources,original.memory,original.peers};
    Platform::bind(services);SaveManager saver;assert(saver.begin());
    MainSceneViewState view{},loadedView{};auto durable=seed;durable.teamCount=1;
    assert(saver.saveSnapshot(durable,view));Game::GameState loaded{};
    assert(saver.load(loaded,loadedView));
    assert(standbySnapshotHash(durable,view)==standbySnapshotHash(loaded,loadedView));
    StandbyCheckpoint durableCp{};durableCp.enteredUs=durableCp.appliedUs=1000000;
    durableCp.context.gameMs=uint64_t(loaded.gameMinutesTotal)*60000;
    durableCp.snapshotHash=standbySnapshotHash(loaded,loadedView);durableCp.seal();
    auto candidate=durableCp;advanceStandbyCare(loaded,candidate.context,candidate.appliedUs,1000000+7200000000ULL);
    auto firstAttempt=loaded;blobs.fail=true;assert(!saver.saveSnapshot(loaded,loadedView));blobs.fail=false;
    assert(saver.load(loaded,loadedView)&&durableCp.valid(standbySnapshotHash(loaded,loadedView),true,candidate.appliedUs));
    auto replay=durableCp;advanceStandbyCare(loaded,replay.context,replay.appliedUs,candidate.appliedUs);
    assert(standbySnapshotHash(loaded,loadedView)==standbySnapshotHash(firstAttempt,loadedView));
    assert(saver.saveSnapshot(loaded,loadedView));replay.snapshotHash=standbySnapshotHash(loaded,loadedView);replay.seal();
    assert(saver.load(loaded,loadedView)&&replay.valid(standbySnapshotHash(loaded,loadedView),true,replay.appliedUs));
    assert(!durableCp.valid(standbySnapshotHash(loaded,loadedView),true,replay.appliedUs));
    Platform::bind(desktop.serviceBundle());
    // Offline eating applies XP and leaves canonical level-up work for UI wake.
    auto xpState=seed;xpState.teamCount=1;for(auto& reward:xpState.careRewards)reward={};xpState.team[0].satiety=30;
    const auto* species=findSpecies(xpState.team[0].speciesId);
    xpState.team[0].exp=minimumExpForLevel(species->growthRate,xpState.team[0].level+1)-1;
    const auto oldLevel=xpState.team[0].level;assert(Game::StandbyCare::feed(xpState));
    assert(xpState.team[0].level>oldLevel);
    assert(Game::CareExperience::recordFor(xpState,xpState.team[0])->pendingOldLevel==oldLevel);
    engine.gameState()=xpState;engine.adaptiveStandbyEnd();assert(engine.hasPendingLevelUp());
    engine.switchScene(SceneID::MENU,false);assert(!engine.adaptiveStandbySafe());
    std::puts("standby: exact tiers/boundaries, half satiety, subminute/day-night batch parity, finite bowl, snapshot-bound RTC guards, actual lifecycle/failed-save replay/progression OK");
#endif
    engine.currentScene.reset();
}
