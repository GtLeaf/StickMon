#pragma once
#include <cstddef>
#include <cstdint>
#include "game/GameState.h"

namespace Game::HomeFurniture {
enum class Slot:uint8_t { WALL_RIGHT,BED,RUG,COUNT };
enum class Result:uint8_t { OK,ALREADY,NOT_OWNED,UNKNOWN_ID };
struct Entry {uint8_t id;Slot slot;const char* name;};
// IDs are save bits, never list positions. ID0 is the existing workshop gift.
inline constexpr Entry CATALOG[]={{0,Slot::WALL_RIGHT,"相框"}};
inline constexpr size_t COUNT=sizeof(CATALOG)/sizeof(CATALOG[0]);
inline const Entry* entry(uint16_t id){for(const auto& e:CATALOG)if(e.id==id)return &e;return nullptr;}
inline uint16_t bit(uint16_t id){return id<16?uint16_t(1u<<id):0;}
inline bool owned(const GameState& s,uint16_t id){return entry(id)&&(s.room.ownedFurniture&bit(id));}
inline bool placed(const GameState& s,uint16_t id){return owned(s,id)&&(s.room.placedFurniture&bit(id));}
inline bool replacing(const GameState& s,uint16_t id){const auto* e=entry(id);if(!e)return false;for(const auto& other:CATALOG)if(other.id!=id&&other.slot==e->slot&&placed(s,other.id))return true;return false;}
inline uint16_t visibleMask(const GameState& s) {
    uint16_t mask = 0;
    for (const auto& e : CATALOG)
        if (placed(s, e.id)) mask |= bit(e.id);
    return mask;
}
inline Result setPlaced(GameState& s,uint16_t id,bool place){
    const auto* selected=entry(id);if(!selected)return Result::UNKNOWN_ID;
    if(!owned(s,id))return Result::NOT_OWNED;
    if(placed(s,id)==place)return Result::ALREADY;
    if(place)for(const auto& other:CATALOG)if(other.slot==selected->slot)s.room.placedFurniture&=uint16_t(~bit(other.id));
    if(place)s.room.placedFurniture|=bit(id);else s.room.placedFurniture&=uint16_t(~bit(id));
    return Result::OK;
}
template<class Save> bool commit(GameState& s,uint16_t id,bool place,Save save){
    const auto before=s;const auto result=setPlaced(s,id,place);
    if(result==Result::ALREADY)return true;
    if(result!=Result::OK)return false;
    if(save())return true;
    s=before;
    return false;
}
}
