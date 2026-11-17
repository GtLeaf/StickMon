#pragma once
#include "game/CareTicker.h"
#include "game/HomeCare.h"
#include "game/MonsterMind.h"
#include "game/ExperienceService.h"

namespace Game::StandbyCare {
constexpr uint32_t FRACTION=1728000;
struct Context {
    CareTickAccumulators care{};
    uint64_t gameMs=0;
    uint32_t realMinuteMs=0;
};
// Caller splits at game-minute boundaries, so scheduled sleep is authoritative.
inline bool hunger(GameState& state,CareTickAccumulators& acc,uint64_t ms,bool dark){
    bool changed=false;
    for(unsigned i=0;i<state.teamCount&&i<TEAM_CAP;++i){
        auto& m=state.team[i];if(m.origin==Origin::VISITOR)continue;
        const auto profile=speciesCareProfileFor(m.speciesId);
        if(!profile.satietyDecays){changed|=m.satiety!=100;m.satiety=100;acc.satietyFraction[i]=0;continue;}
        const bool sleeping=m.majorStatus==MajorStatus::SLEEP||isSleepCareTime(state.gameMinutesTotal,m.nature);
        const unsigned rate=dark?(sleeping?1:2):(sleeping?2:4);
        const uint64_t total=acc.satietyFraction[i]+ms*rate;
        const auto drop=std::min<uint64_t>(100,total/FRACTION);acc.satietyFraction[i]=uint32_t(total%FRACTION);
        const auto next=uint8_t(m.satiety>drop?m.satiety-drop:0);changed|=next!=m.satiety;m.satiety=next;
    }return changed;
}
inline bool feed(GameState& state){
    bool changed=false;
    for(unsigned i=0;i<state.teamCount&&i<TEAM_CAP&&state.room.bowlCount;++i){
        auto& m=state.team[i];const auto profile=speciesCareProfileFor(m.speciesId);
        if(!profile.needsFood||m.origin==Origin::VISITOR||m.fainted||!m.hpCur||m.majorStatus==MajorStatus::SLEEP)continue;
        if(profile.usesBed&&isSleepCareTime(state.gameMinutesTotal,m.nature)&&!monsterShouldWakeForFood(m.satiety))continue;
        while(state.room.bowlCount&&m.satiety<MONSTER_FEED_TARGET_SATIETY){
            const auto bite=HomeCare::consumeBowlFood(state,uint8_t(i));
            if(!bite.consumed)break;
            const auto* species=findSpecies(m.speciesId);
            if(species&&bite.careExp){
                const auto xp=ExperienceService::add(m,*species,bite.careExp);
                if(xp.leveledUp)if(auto* record=CareExperience::recordFor(state,m))
                    if(!record->pendingOldLevel)record->pendingOldLevel=xp.oldLevel;
            }
            changed=true;
        }
    }return changed;
}
// Work is bounded by real/game minutes and hunger changes, independent of UI
// frames or wake frequency. A 24h interval at 8x requires 11520 small care steps.
inline bool advance(GameState& state,Context& ctx,uint64_t elapsedMs,unsigned speed,
                    bool dark=true,bool home=true,bool autoFeed=true){
    if(!elapsedMs)return false;
    speed=std::clamp(speed,1u,8u);bool changed=false;
    state.gameMinutesTotal=uint32_t(std::min<uint64_t>(UINT32_MAX,ctx.gameMs/60000));
    if(autoFeed&&home)changed|=feed(state);
    while(elapsedMs){
        uint64_t step=std::min<uint64_t>(elapsedMs,60000-ctx.realMinuteMs);
        step=std::min(step,(60000-ctx.gameMs%60000+speed-1)/speed);
        // Hunger threshold is also an event, so inserting wake boundaries cannot
        // change which pet gets a finite bite or the meal's daily care reward.
        for(unsigned i=0;i<state.teamCount&&i<TEAM_CAP;++i){
            const auto& m=state.team[i];if(!speciesCareProfileFor(m.speciesId).satietyDecays||m.origin==Origin::VISITOR)continue;
            const bool asleep=m.majorStatus==MajorStatus::SLEEP||isSleepCareTime(state.gameMinutesTotal,m.nature);
            const unsigned rate=dark?(asleep?1:2):(asleep?2:4);
            step=std::min<uint64_t>(step,(FRACTION-ctx.care.satietyFraction[i]+rate-1)/rate);
        }
        changed|=hunger(state,ctx.care,step,dark);
        const auto oldMinute=ctx.gameMs/60000;ctx.gameMs+=step*speed;
        ctx.realMinuteMs+=uint32_t(step);elapsedMs-=step;
        const auto realMinutes=ctx.realMinuteMs/60000;ctx.realMinuteMs%=60000;
        const auto newMinute=ctx.gameMs/60000;
        state.gameMinutesTotal=uint32_t(std::min<uint64_t>(UINT32_MAX,newMinute));
        if(realMinutes||newMinute!=oldMinute){
            changed|=resetDailyCareCounters(state);
            changed|=applyCareMinutes(state,ctx.care,realMinutes,uint32_t(newMinute-oldMinute),home,false).stateChanged;
        }
        if(autoFeed&&home)changed|=feed(state);
    }return changed;
}
}
