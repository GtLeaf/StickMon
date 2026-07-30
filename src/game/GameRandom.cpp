#include "game/GameRandom.h"

namespace {
uint32_t gState = 0x6D2B79F5U;
GameRandom::RangeProvider gRangeProvider = nullptr;
}

namespace GameRandom {

void seed(uint32_t value) {
    gState = value == 0 ? 0x6D2B79F5U : value;
}

void setRangeProvider(RangeProvider provider) {
    gRangeProvider = provider;
}

uint32_t next() {
    uint32_t value = gState;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    gState = value;
    return value;
}

uint32_t range(uint32_t minimumInclusive, uint32_t maximumExclusive) {
    if (maximumExclusive <= minimumInclusive) return minimumInclusive;
    if (gRangeProvider) {
        return gRangeProvider(minimumInclusive, maximumExclusive);
    }
    return minimumInclusive +
           next() % (maximumExclusive - minimumInclusive);
}

}  // namespace GameRandom
