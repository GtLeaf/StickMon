#pragma once

#include <cstdint>

namespace Market {
// Preserve the authored four-direction values; append the diagonals.
inline constexpr int DIRECTION_X[8]={0,-1,0,1,-1,-1,1,1};
inline constexpr int DIRECTION_Y[8]={1,0,-1,0,1,-1,-1,1};
inline constexpr int DIRECTION_RING[8]={0,4,1,5,2,6,3,7};
inline constexpr int DIRECTION_SECTOR[8]={0,2,4,6,1,3,5,7};
inline int directionForDelta(int dx,int dy) {
    if(dx&&dy)return dx<0?(dy<0?5:4):(dy<0?6:7);
    return dx?(dx<0?1:3):(dy<0?2:0);
}
inline int turnDirection(int direction,int eighths) {
    return DIRECTION_RING[(DIRECTION_SECTOR[direction]+eighths+8)%8];
}
inline uint16_t walkDurationMs(int direction) { return direction<4?200:283; }
} // namespace Market
