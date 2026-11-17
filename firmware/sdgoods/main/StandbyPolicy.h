#pragma once
#include <algorithm>
#include <cstdint>
namespace Sdgoods::StandbyPolicy {
inline constexpr uint64_t limits[]={300000,3600000,86400000,259200000};
inline constexpr uint32_t intervals[]={30000,300000,7200000,43200000,86400000};
inline unsigned tier(uint64_t ageMs){unsigned i=0;while(i<4&&ageMs>=limits[i])++i;return i;}
inline uint32_t intervalMs(uint64_t ageMs){return intervals[tier(ageMs)];}
inline uint32_t waitMs(uint64_t ageMs){const auto i=tier(ageMs);return i<4?uint32_t(std::min<uint64_t>(intervals[i],limits[i]-ageMs)):intervals[i];}
inline bool deep(uint64_t ageMs){return tier(ageMs)>=2;}
}
