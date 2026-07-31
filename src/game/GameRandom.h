#pragma once

#include <cstdint>

namespace GameRandom {

using RangeProvider = uint32_t (*)(uint32_t minimumInclusive,
                                   uint32_t maximumExclusive);

void seed(uint32_t value);
void setRangeProvider(RangeProvider provider);
uint32_t next();
uint32_t range(uint32_t minimumInclusive, uint32_t maximumExclusive);
int32_t random(int32_t maximumExclusive);
int32_t random(int32_t minimumInclusive, int32_t maximumExclusive);

}  // namespace GameRandom
