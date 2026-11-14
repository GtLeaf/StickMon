#include "game/MarketSchedule.h"
#include "game/ExploreDestinations.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

using namespace Game;

int main() {
    // Catch an off-by-one opening or confusing elapsed days with period indices.
    struct DayCase { uint32_t day, previous, next; bool open; };
    const DayCase cases[] = {
        {0, 0, 0, true}, {1, 0, 3, false}, {2, 0, 3, false},
        {3, 3, 3, true}, {4, 3, 7, false}, {5, 3, 7, false},
        {6, 3, 7, false}, {7, 7, 7, true}, {8, 7, 10, false},
        {9, 7, 10, false}, {10, 10, 10, true}, {11, 10, 14, false},
        {12, 10, 14, false}, {13, 10, 14, false}, {14, 14, 14, true},
    };
    for (const auto& c : cases) {
        assert(MarketSchedule::isOpenDay(c.day) == c.open);
        assert(MarketSchedule::openingDay(c.day) == c.previous);
        assert(MarketSchedule::nextOpenDay(c.day) == c.next);
        assert(MarketSchedule::daysUntilOpen(c.day) == c.next - c.day);
        for (uint32_t minute = 0; minute < 1440; ++minute) {
            GameState state{};
            state.gameMinutesTotal = c.day * 1440 + minute;
            state.coins = 1234;
            state.marketShop.periodDay = 987;
            const auto before = state;
            assert(MarketSchedule::gameDay(state) == c.day);
            assert(MarketSchedule::isOpen(state) == (BuildConfig::DEBUG_FEATURES || c.open));
            assert(MarketSchedule::daysUntilOpen(state) == (BuildConfig::DEBUG_FEATURES ? 0U : c.next - c.day));
            assert(MarketSchedule::periodDay(state) == c.previous);
            // Debug visits bypass closing days without changing saved periods
            // or any ordinary area's unlock behavior.
            assert(ExploreDestinations::visible(ExploreDestinations::MARKET, state) == BuildConfig::DEBUG_FEATURES);
            assert(ExploreDestinations::unlocked(ExploreDestinations::MARKET, state) == BuildConfig::DEBUG_FEATURES);
            assert(ExploreDestinations::unlocked(0, state));
            assert(std::memcmp(&before, &state, sizeof(state)) == 0);
        }
    }
    GameState initial{};
    assert(MarketSchedule::gameDay(initial) == 0);
    assert(MarketSchedule::isOpen(initial));

    // Independent walk through alternating opening gaps catches drift, missing
    // openings, multi-day openings and a countdown that skips the current day.
    uint32_t previous = 0, next = 0, gap = 3;
    for (uint32_t day = 0; day < 1000000; ++day) {
        const bool open = day == next;
        if (open) {
            previous = day;
            next += gap;
            gap = gap == 3 ? 4 : 3;
        }
        assert(MarketSchedule::isOpenDay(day) == open);
        assert(MarketSchedule::openingDay(day) == previous);
        const auto upcoming = open ? day : next;
        assert(MarketSchedule::nextOpenDay(day) == upcoming);
        assert(MarketSchedule::daysUntilOpen(day) == upcoming - day);
    }

    // All GameState minute values remain in range, including UINT32_MAX.
    GameState last{};
    last.gameMinutesTotal = std::numeric_limits<uint32_t>::max();
    assert(MarketSchedule::gameDay(last) == 2982616);
    assert(MarketSchedule::isOpen(last));
    assert(MarketSchedule::periodDay(last) == 2982616);
    assert(MarketSchedule::nextOpenDay(2982616) == 2982616);
    assert(MarketSchedule::daysUntilOpen(2982616) == 0);

    // The last representable day is itself an opening; nearby closed days
    // must advance to it without wrapping back to the initial opening.
    const DayCase limits[] = {
        {4294967291U, 4294967288U, 4294967292U, false},
        {4294967292U, 4294967292U, 4294967292U, true},
        {4294967293U, 4294967292U, 4294967295U, false},
        {4294967294U, 4294967292U, 4294967295U, false},
        {4294967295U, 4294967295U, 4294967295U, true},
    };
    for (const auto& c : limits) {
        assert(MarketSchedule::isOpenDay(c.day) == c.open);
        assert(MarketSchedule::openingDay(c.day) == c.previous);
        assert(MarketSchedule::nextOpenDay(c.day) == c.next);
        assert(MarketSchedule::daysUntilOpen(c.day) == c.next - c.day);
    }
    std::puts("market-schedule: day-zero discovery, one-day openings, alternating 3/4-day gaps, countdown, immutable queries and uint32 limits OK");
}
