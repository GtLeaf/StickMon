#pragma once

#include <cstdint>

namespace GameRandom {

using RangeProvider = uint32_t (*)(uint32_t minimumInclusive,
                                   uint32_t maximumExclusive);

void seed(uint32_t value);
void setRangeProvider(RangeProvider provider);
uint32_t next();
uint32_t range(uint32_t minimumInclusive, uint32_t maximumExclusive);

}  // namespace GameRandom
