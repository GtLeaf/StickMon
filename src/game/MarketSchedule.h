#pragma once

#include <cstdint>
#include "core/BuildConfig.h"
#include "game/GameState.h"

// Open for one game day at a time, beginning on day zero for discovery.
// Opening gaps alternate 3 and 4 days: 0, 3, 7, 10, 14, ... .
namespace Game::MarketSchedule {

inline constexpr uint32_t gameDay(const GameState& state) {
    return state.gameMinutesTotal / 1440U;
}

inline constexpr bool isOpenDay(uint32_t day) {
    const auto offset = day % 7U;
    return offset == 0U || offset == 3U;
}

// Keep the actual opening game day as the saved period key, so an existing
// shelf/quota from that day is retained when the schedule is introduced.
inline constexpr uint32_t openingDay(uint32_t day) {
    const auto offset = day % 7U;
    return day - offset + (offset >= 3U ? 3U : 0U);
}

inline constexpr uint32_t nextOpenDay(uint32_t day) {
    const auto offset = day % 7U;
    if (offset == 0U || offset == 3U) return day;
    // UINT32_MAX is itself an opening day (remainder 3), so every closed
    // uint32 day has a representable following opening; addition cannot wrap.
    return day + (offset < 3U ? 3U - offset : 7U - offset);
}

inline constexpr uint32_t daysUntilOpen(uint32_t day) {
    return nextOpenDay(day) - day;
}

inline constexpr bool isOpen(const GameState& state) {
    // Debug bypasses access restrictions, not persistent shelf/quota periods.
    return BuildConfig::DEBUG_FEATURES || isOpenDay(gameDay(state));
}

inline constexpr uint32_t daysUntilOpen(const GameState& state) {
    return isOpen(state) ? 0U : daysUntilOpen(gameDay(state));
}

inline constexpr uint32_t periodDay(const GameState& state) {
    return openingDay(gameDay(state));
}

} // namespace Game::MarketSchedule
