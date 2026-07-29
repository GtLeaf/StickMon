#include <cstdint>
#include <cstdio>

#include "game/ExploreEncounters.h"
#include "game/FriendshipPity.h"

namespace {

int fail(int code, const char* what, uint32_t detail = 0) {
    std::printf("[friendship_pity_host] FAIL %d: %s (%lu)\n",
                code, what, static_cast<unsigned long>(detail));
    return code;
}

} // namespace

int main() {
    bool seen[Game::FRIENDSHIP_PITY_TRACKED_COUNT] = {};
    for (uint8_t i = 0; i < Game::FRIENDSHIP_PITY_TRACKED_COUNT; ++i) {
        uint16_t speciesId = FriendshipPity::TRACKED[i].speciesId;
        int8_t mapped = FriendshipPity::indexFor(speciesId);
        if (mapped < 0 || mapped >= Game::FRIENDSHIP_PITY_TRACKED_COUNT) {
            return fail(1, "tracked species has no valid index", speciesId);
        }
        if (seen[mapped]) {
            return fail(2, "duplicate tracked species/index", speciesId);
        }
        seen[mapped] = true;
    }

    if (FriendshipPity::indexFor(10) != -1 ||
        FriendshipPity::indexFor(42) != -1 ||
        FriendshipPity::indexFor(169) != -1) {
        return fail(3, "untracked species entered pity table");
    }

    struct Area {
        const ExploreEncounters::Entry* entries;
        uint8_t count;
    };
#define ENTRY_COUNT(entries) \
    static_cast<uint8_t>(sizeof(entries) / sizeof(entries[0]))
    const Area areas[] = {
        {ExploreEncounters::GRASS_PATH,
         ENTRY_COUNT(ExploreEncounters::GRASS_PATH)},
        {ExploreEncounters::CREEK_SLOPE,
         ENTRY_COUNT(ExploreEncounters::CREEK_SLOPE)},
        {ExploreEncounters::TALL_GRASS_PARK,
         ENTRY_COUNT(ExploreEncounters::TALL_GRASS_PARK)},
        {ExploreEncounters::FROST_CRYSTAL_CAVE,
         ENTRY_COUNT(ExploreEncounters::FROST_CRYSTAL_CAVE)},
        {ExploreEncounters::MIST_FOREST_PATH,
         ENTRY_COUNT(ExploreEncounters::MIST_FOREST_PATH)},
        {ExploreEncounters::ANCIENT_WATERFALL_VALLEY,
         ENTRY_COUNT(ExploreEncounters::ANCIENT_WATERFALL_VALLEY)},
    };
#undef ENTRY_COUNT
    for (uint8_t area = 0;
         area < static_cast<uint8_t>(sizeof(areas) / sizeof(areas[0]));
         ++area) {
        for (uint8_t i = 0; i < areas[area].count; ++i) {
            const ExploreEncounters::Entry& entry = areas[area].entries[i];
            bool shouldTrack =
                static_cast<uint8_t>(entry.rarity) >=
                static_cast<uint8_t>(ExplorePool::Rarity::UNCOMMON);
            int8_t index = FriendshipPity::indexFor(entry.speciesId);
            if (shouldTrack != (index >= 0)) {
                return fail(4, "rarity/pity whitelist mismatch",
                            entry.speciesId);
            }
            if (index >= 0) {
                FriendshipPity::Tier expected =
                    entry.rarity == ExplorePool::Rarity::UNCOMMON
                    ? FriendshipPity::Tier::UNCOMMON
                    : FriendshipPity::Tier::RARE;
                if (FriendshipPity::tierAt(
                        static_cast<uint8_t>(index)) != expected) {
                    return fail(5, "rarity/pity tier mismatch",
                                entry.speciesId);
                }
            }
        }
    }

    if (FriendshipPity::bonusPermille(
            FriendshipPity::Tier::UNCOMMON, 5) != 25 ||
        FriendshipPity::bonusPermille(
            FriendshipPity::Tier::RARE, 5) != 50 ||
        FriendshipPity::bonusPermille(
            FriendshipPity::Tier::SPECIAL, 5) != 75 ||
        FriendshipPity::bonusPermille(
            FriendshipPity::Tier::SPECIAL, 200) != 75) {
        return fail(6, "tier bonus curve");
    }

    if (FriendshipPity::chanceWithBonus(
            26, FriendshipPity::Tier::RARE, 5) != 76 ||
        FriendshipPity::chanceWithBonus(
            240, FriendshipPity::Tier::RARE, 5) != 250) {
        return fail(7, "final chance bonus/cap");
    }

    uint16_t conditional =
        FriendshipPity::conditionalBonusPermille(26, 76);
    uint32_t reconstructed =
        26U + (1000U - 26U) * conditional / 1000U;
    if (reconstructed < 75 || reconstructed > 77) {
        return fail(8, "conditional bonus reconstruction", reconstructed);
    }

    std::printf("[friendship_pity_host] all tests passed\n");
    return 0;
}
