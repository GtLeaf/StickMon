#include <cassert>
#include <cstdio>

#include "game/FriendshipPity.h"

int main() {
    using namespace FriendshipPity;
    using namespace ExploreSpecial;
    constexpr uint16_t futureLegendary = 249;

    assert(!usesFriendshipPity(configFor(Kind::NONE), MEW));
    assert(!usesFriendshipPity(configFor(Kind::FIRST_SNORLAX), SNORLAX));
    assert(!usesFriendshipPity(configFor(Kind::FIRST_LATIAS), LATIAS));
    assert(!usesFriendshipPity(configFor(Kind::FIRST_LATIOS), LATIOS));
    assert(usesFriendshipPity(configFor(Kind::ROAMING_LATIAS), LATIAS));
    assert(usesFriendshipPity(configFor(Kind::ROAMING_LATIOS), LATIOS));
    assert(usesFriendshipPity(configFor(Kind::MEW_EVENT), MEW));
    assert(!usesFriendshipPity(configFor(Kind::MEW_EVENT), SNORLAX));

    Game::FriendshipPityState pity{};
    assert(!recordFailure(pity, 0));
    assert(recordFailure(pity, MEW));
    assert(recordFailure(pity, LATIAS));
    assert(recordFailure(pity, LATIOS));
    for (uint8_t failures = 2; failures <= 5; ++failures) {
        assert(recordFailure(pity, MEW));
        assert(failuresFor(pity, MEW) == failures);
    }
    assert(!recordFailure(pity, MEW));
    assert(recordFailure(pity, LATIAS));
    assert(recordFailure(pity, MEW));  // Touch a capped entry that is no longer newest.
    assert(recordFailure(pity, futureLegendary));
    assert(failuresFor(pity, LATIOS) == 0);
    assert(failuresFor(pity, MEW) == 5);
    assert(failuresFor(pity, LATIAS) == 2);
    assert(failuresFor(pity, futureLegendary) == 1);

    assert(clear(pity, MEW));
    assert(!clear(pity, MEW));
    assert(pity.entries[0].speciesId == futureLegendary);
    assert(pity.entries[1].speciesId == LATIAS);
    assert(pity.entries[2].speciesId == 0);
    assert(recordFailure(pity, LATIOS));  // Reuses vacancy without eviction.
    assert(failuresFor(pity, LATIAS) == 2);

    uint8_t legacy[Game::LEGACY_FRIENDSHIP_PITY_COUNT]{};
    legacy[0] = legacy[34] = 5; // Former ordinary and Snorlax counts.
    legacy[35] = 3;
    legacy[37] = 255;
    pity = migrateLegacy(legacy);
    assert(pity.entries[0].speciesId == MEW && pity.entries[0].failures == 3);
    assert(pity.entries[1].speciesId == LATIOS && pity.entries[1].failures == 5);
    assert(pity.entries[2].speciesId == 0);

    pity.entries[0] = {0, 3};
    pity.entries[1] = {LATIOS, 255};
    pity.entries[2] = {LATIOS, 2};
    assert(sanitize(pity));
    assert(pity.entries[0].speciesId == LATIOS && pity.entries[0].failures == 5);
    assert(pity.entries[1].speciesId == 0 && pity.entries[2].speciesId == 0);
    assert(!sanitize(pity));

    assert(chanceWithBonus(8, 0) == 8);
    assert(chanceWithBonus(8, 1) == 18);
    assert(chanceWithBonus(8, 5) == 58);
    assert(chanceWithBonus(8, 200) == 58);
    assert(chanceWithBonus(240, 5) == 250);
    assert(conditionalBonusPermille(8, 58) == 50);
    std::puts("Legendary eligibility, LRU, cap, clearing, and old counts passed");
}
