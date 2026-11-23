#include <memory>
#include <new>
#pragma once
#include <cstddef>
#include <cstdint>
#include "game/StandbyCare.h"
#include "core/SaveCodec.h"
namespace Sdgoods {
inline void advanceStandbyCare(Game::GameState& state,Game::StandbyCare::Context& ctx,
                               uint64_t& applied,uint64_t now){
    if(now<applied)return;
    const auto ms=(now-applied)/1000;
    const auto speed=1u<<std::min<unsigned>(3,state.settings.speedIndex);
    Game::StandbyCare::advance(state,ctx,ms,speed,true,true,true);
    applied+=ms*1000;
    ContactRoster::syncTeamContacts(state);
}

inline uint32_t standbySnapshotHash(const Game::GameState& state,const MainSceneViewState& view){
    std::unique_ptr<uint8_t[]> bytes(new (std::nothrow) uint8_t[SaveCodec::MAX_ENCODED_BYTES]);size_t count=0;
    if(!bytes||!SaveCodec::encode(state,view,0,bytes.get(),SaveCodec::MAX_ENCODED_BYTES,count))return 0;
    uint32_t h=2166136261u;for(size_t i=0;i<count;++i)h=(h^bytes[i])*16777619u;return h;
}
struct StandbyCheckpoint {
    uint32_t magic=0;
    uint32_t version=1;
    uint64_t enteredUs=0,appliedUs=0;
    Game::StandbyCare::Context context{};
    uint32_t snapshotHash=0,checksum=0;
    uint32_t digest()const{
        uint32_t h=2166136261u;const auto* p=reinterpret_cast<const uint8_t*>(this);
        for(size_t i=0;i<offsetof(StandbyCheckpoint,checksum);++i) h=(h^p[i])*16777619u;
        return h;
    }
    void seal(){magic=0x53474231;version=1;checksum=digest();}
    bool valid(uint32_t snapshot,bool deepWake,uint64_t now)const{
        if(!snapshotHash||!snapshot||!deepWake||magic!=0x53474231||version!=1||checksum!=digest()||snapshotHash!=snapshot||
           now<appliedUs||appliedUs<enteredUs||context.realMinuteMs>=60000)return false;
        for(auto n:context.care.satietyFraction)if(n>=Game::StandbyCare::FRACTION)return false;
        return true;
    }
};
}
