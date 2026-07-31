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

int32_t random(int32_t maximumExclusive) {
    return random(0, maximumExclusive);
}

int32_t random(int32_t minimumInclusive, int32_t maximumExclusive) {
    if (maximumExclusive <= minimumInclusive) return minimumInclusive;
    if (minimumInclusive >= 0) {
        return static_cast<int32_t>(range(
            static_cast<uint32_t>(minimumInclusive),
            static_cast<uint32_t>(maximumExclusive)));
    }
    uint32_t span = static_cast<uint32_t>(
        static_cast<int64_t>(maximumExclusive) - minimumInclusive);
    return minimumInclusive + static_cast<int32_t>(next() % span);
}

}  // namespace GameRandom
