#pragma once
#include "game/GameState.h"

namespace Game::MarketWorkshop {
inline constexpr uint16_t STALL_SPECIES=115;
// Stable furniture ID0. Ownership doubles as the once-per-save gift marker.
inline constexpr uint16_t PHOTO_FRAME=uint16_t(1u<<0);
enum class Result : uint8_t { OK,ALREADY };
inline bool hasFrame(const GameState& state){return (state.room.ownedFurniture&PHOTO_FRAME)!=0;}
inline Result grantFrame(GameState& state){
    if(hasFrame(state))return Result::ALREADY;
    state.room.ownedFurniture|=PHOTO_FRAME;
    return Result::OK;
}
}
