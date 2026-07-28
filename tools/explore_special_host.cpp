#include <cstdint>
#include <cstdio>

#include "game/ExploreSpecialEncounter.h"

namespace {

int fail(int code, const char* what, uint32_t detail = 0) {
    std::printf("[explore_special_host] FAIL %d: %s (%lu)\n",
                code, what, static_cast<unsigned long>(detail));
    return code;
}

}

int main() {
    using namespace ExploreSpecial;

    const uint8_t noRerolls[ROAMER_COUNT] = {};
    if (kindForArea(SNORLAX_AREA, 0, 0, noRerolls, false) !=
            Kind::FIRST_SNORLAX ||
        kindForArea(LATIAS_AREA, SNORLAX_DEFEATED, 0, noRerolls, false) !=
            Kind::FIRST_LATIAS ||
        kindForArea(LATIOS_AREA,
                    SNORLAX_DEFEATED | LATIAS_DEFEATED,
                    0, noRerolls, false) != Kind::FIRST_LATIOS) {
        return fail(1, "first bosses must override all later activity");
    }

    if (configFor(Kind::FIRST_SNORLAX).level != 27 ||
        configFor(Kind::FIRST_LATIAS).level != 53 ||
        configFor(Kind::FIRST_LATIOS).level != 67 ||
        configFor(Kind::ROAMING_LATIAS).level != 70 ||
        configFor(Kind::ROAMING_LATIOS).level != 70 ||
        configFor(Kind::MEW_EVENT).level != 70) {
        return fail(2, "special encounter levels");
    }
    if (configFor(Kind::FIRST_LATIAS).allowsFriendship ||
        configFor(Kind::FIRST_LATIOS).allowsFriendship ||
        !configFor(Kind::ROAMING_LATIAS).allowsFriendship ||
        !configFor(Kind::MEW_EVENT).allowsFriendship) {
        return fail(3, "friendship gates");
    }
    if (!configFor(Kind::ROAMING_LATIAS).optional ||
        !configFor(Kind::ROAMING_LATIOS).optional ||
        !configFor(Kind::MEW_EVENT).optional ||
        configFor(Kind::FIRST_SNORLAX).optional) {
        return fail(4, "optional encounter rules");
    }

    const uint16_t roamingSpecies[ROAMER_COUNT] = {LATIAS, LATIOS};
    for (uint32_t slot = 0; slot < 200; ++slot) {
        for (uint16_t species : roamingSpecies) {
            uint8_t previous = roamingArea(slot, species, 0);
            if (previous >= AREA_COUNT) return fail(5, "invalid roaming area");
            for (uint16_t reroll = 1; reroll <= 255; ++reroll) {
                uint8_t current = roamingArea(
                    slot, species, static_cast<uint8_t>(reroll));
                if (current >= AREA_COUNT || current == previous) {
                    return fail(6, "roamer must move to a different area",
                                reroll);
                }
                previous = current;
            }
        }
    }

    const uint8_t bothDefeated =
        SNORLAX_DEFEATED | LATIAS_DEFEATED | LATIOS_DEFEATED;
    uint8_t rerolls[ROAMER_COUNT] = {7, 11};
    for (uint32_t slot = 0; slot < 1000; ++slot) {
        uint8_t first[ROAMER_COUNT] = {};
        uint8_t second[ROAMER_COUNT] = {};
        resolveRoamingAreas(bothDefeated, slot, rerolls, first);
        resolveRoamingAreas(bothDefeated, slot, rerolls, second);
        if (first[0] != second[0] || first[1] != second[1]) {
            return fail(7, "roaming positions must be deterministic");
        }
        if (first[0] == first[1]) {
            return fail(8, "roamers must not share an area");
        }
    }

    uint32_t activeWindows = 0;
    constexpr uint32_t WINDOW_SAMPLES = 10000;
    for (uint32_t slot = 0; slot < WINDOW_SAMPLES; ++slot) {
        if (mewWindowActive(bothDefeated, slot, false)) ++activeWindows;
        if (mewWindowActive(0, slot, false) ||
            mewWindowActive(bothDefeated, slot, true)) {
            return fail(9, "Mew window unlock/ownership gate");
        }
    }
    uint32_t windowPercent = activeWindows * 100 / WINDOW_SAMPLES;
    if (windowPercent < 18 || windowPercent > 22) {
        return fail(10, "Mew window should be approximately 20%",
                    windowPercent);
    }

    for (uint32_t minutes = 0; minutes < 4000; ++minutes) {
        if (slotIndexFor(minutes) != minutes / SLOT_PERIOD_MINUTES) {
            return fail(11, "eight-hour slot index", minutes);
        }
    }

    std::printf("[explore_special_host] all tests passed\n");
    return 0;
}
