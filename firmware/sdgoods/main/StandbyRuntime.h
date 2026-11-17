#pragma once
#include "StandbyCheckpoint.h"
#include "StandbyPolicy.h"

namespace Sdgoods {
// Board waits and persistence are injected so the real lifecycle can also run
// under a deterministic clock without ESP hardware or a second implementation.
template<class Board,class Session,class Engine,class Retention>
class StandbyRuntime {
public:
    StandbyRuntime(Board& b,Session& s,Engine& e,Retention& r)
        :board_(b),session_(s),engine_(e),retention_(r){}
    void beforePowerActions(){
        if(!active_)return;
        if(board_.powerKeyActive()||!board_.locked())reconcile();
        if(!board_.locked()){
            if(offline_)engine_.adaptiveStandbyRestore(context_,false);
            engine_.adaptiveStandbyEnd();active_=offline_=false;retention_.clear();
        }
    }
    void poll(){
        const auto now=board_.standbyMicros();
        if(!active_){
            engine_.adaptiveStandbyBegin();active_=true;enteredUs_=appliedUs_=dueUs_=now;
            offline_=engine_.adaptiveStandbySafe();
            if(offline_)capture();
        }
        if(board_.powerKeyActive()||now<dueUs_)return;
        if(!offline_){
            session_.poll();
            if(engine_.adaptiveStandbySafe()){
                offline_=true;capture();
            }
        }
        reconcile();const auto age=(board_.standbyMicros()-enteredUs_)/1000;
        if(offline_&&StandbyPolicy::deep(age)){
            if(session_.save()){
                StandbyCheckpoint next{};next.enteredUs=enteredUs_;next.appliedUs=appliedUs_;next.context=context_;
                next.snapshotHash=standbySnapshotHash(engine_.gameState(),engine_.mainSceneViewState());next.seal();retention_.store(next);
                board_.enterDeepSleep(uint64_t(StandbyPolicy::waitMs(age))*1000,true);
            }
            dueUs_=board_.standbyMicros()+1000000;return;
        }
        dueUs_=board_.standbyMicros()+uint64_t(offline_?StandbyPolicy::waitMs(age):1000)*1000;
    }
    void wait(){
        const auto now=board_.standbyMicros();
        const auto ms=dueUs_>now?std::min<uint64_t>((dueUs_-now+999)/1000,300000):1;
        board_.waitStandby(uint32_t(ms),active_&&offline_);
    }
private:
    Board& board_;Session& session_;Engine& engine_;Retention& retention_;
    bool active_=false,offline_=false;
    uint64_t enteredUs_=0,appliedUs_=0,dueUs_=0;
    Game::StandbyCare::Context context_{};
    void capture(){
        // Pair RTC time with the engine capture, after any running-scene/save work.
        appliedUs_=board_.standbyMicros();
        context_=engine_.adaptiveStandbyContext();
        engine_.adaptiveStandbyRestore(context_,true);
    }
    void reconcile(){
        if(!offline_)return;
        advanceStandbyCare(engine_.gameState(),context_,appliedUs_,board_.standbyMicros());
        engine_.adaptiveStandbyRestore(context_,board_.locked());
    }
};
}
