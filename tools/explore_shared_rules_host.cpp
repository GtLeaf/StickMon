#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "game/ExploreAreaCatalog.h"
#include "game/ExplorePickupRules.h"
#include "game/ExploreRunRules.h"
#include "game/GameRandom.h"

namespace {
unsigned checks = 0;
unsigned randomCalls = 0;

uint32_t countRandom(uint32_t low, uint32_t) {
    ++randomCalls;
    return low;
}

unsigned number() {
    unsigned value = 0;
    assert(std::scanf("%u", &value) == 1);
    return value;
}

void check(bool condition) {
    assert(condition);
    ++checks;
}
}  // namespace

int main() {
    GameRandom::setRangeProvider(countRandom);
    check(Game::EXPLORE_AREA_COUNT == number());
    for (uint8_t area = 0; area < Game::EXPLORE_AREA_COUNT; ++area) {
        const unsigned minMaps = number(), maxMaps = number();
        const unsigned chance = number(), minCoin = number(), maxCoin = number();
        const unsigned count = number();
        std::vector<std::array<unsigned, 2>> entries;
        for (unsigned i = 0; i < count; ++i) entries.push_back({number(), number()});
        const auto& table = ExplorePickupRules::tableForArea(area);
        check(ExploreAreaCatalog::minMapCount(area) == minMaps);
        check(ExploreAreaCatalog::maxMapCount(area) == maxMaps);
        check(ExploreAreaCatalog::encounterChance(area) == chance);
        check(table.count == count && table.minCoin == minCoin && table.maxCoin == maxCoin);
        for (unsigned i = 0; i < count; ++i) {
            check(table.entries[i].id == entries[i][0] && table.entries[i].weight == entries[i][1]);
        }
        for (unsigned roll = 0; roll <= 255; ++roll) {
            unsigned expected = minMaps;
            if (maxMaps == minMaps + 1) expected += roll >= 50;
            if (maxMaps > minMaps + 1) expected = roll < 25 ? minMaps : (roll < 75 ? minMaps + 1 : maxMaps);
            check(ExploreRunRules::mapCountForRoll(minMaps, maxMaps, roll) == expected);
        }
        for (uint16_t steps : {uint16_t(0), uint16_t(4999), uint16_t(5000), uint16_t(65535)}) {
            unsigned total = 0;
            std::vector<unsigned> expectedRolls;
            for (const auto& entry : entries) {
                if (entry[0] == 5 && steps < 5000) continue;
                total += entry[1];
                expectedRolls.insert(expectedRolls.end(), entry[1], entry[0]);
            }
            check(ExplorePickupRules::weightTotal(table, steps) == total);
            for (unsigned roll = 0; roll < total; ++roll) {
                check(ExplorePickupRules::forRoll(table, steps, roll) == expectedRolls[roll]);
            }
            check(ExplorePickupRules::forRoll(table, steps, total) == ExplorePickupRules::NONE);
            check(ExplorePickupRules::forRoll(table, steps, 65535) == ExplorePickupRules::NONE);
        }
    }
    for (unsigned area = Game::EXPLORE_AREA_COUNT; area <= 255; ++area) {
        check(&ExplorePickupRules::tableForArea(area) == &ExplorePickupRules::TABLES[5]);
    }
    for (unsigned id = 0; id <= 255; ++id) {
        for (uint16_t steps : {uint16_t(0), uint16_t(4999), uint16_t(5000), uint16_t(65535)}) {
            check(ExplorePickupRules::available(id, steps) == (id != 5 || steps >= 5000));
        }
    }
    const Game::ItemId items[] = {
        Game::ItemId::COUNT, Game::ItemId::COUNT, Game::ItemId::POTION,
        Game::ItemId::SUPER_POTION, Game::ItemId::ANTIDOTE, Game::ItemId::CANDY,
        Game::ItemId::MAX_POTION, Game::ItemId::FULL_RESTORE, Game::ItemId::FULL_HEAL,
        Game::ItemId::REVIVE, Game::ItemId::MAX_REPEL, Game::ItemId::HONEY,
        Game::ItemId::NUGGET, Game::ItemId::BIG_PEARL, Game::ItemId::STAR_PIECE,
        Game::ItemId::HEART_SCALE,
    };
    for (unsigned id = 0; id <= 255; ++id) {
        check(ExplorePickupRules::itemId(id) == (id < 16 ? items[id] : Game::ItemId::COUNT));
    }
    check(ExploreRunRules::COOLDOWN_STEPS == number());
    check(ExploreRunRules::MAX_ENCOUNTERS_PER_MAP == number());
    check(ExploreRunRules::MAP_PICKUP_CHANCE == number());
    check(ExploreRunRules::MAP_GENERATION_SAFE_SEED == number());
    for (auto salt : ExploreRunRules::MAP_GENERATION_RETRY_SALTS) check(salt == number());
    for (unsigned cooldown = 0; cooldown <= 255; ++cooldown) {
        check(ExploreRunRules::cooldownAfterCompletedStep(cooldown) == (cooldown ? cooldown - 1 : 0));
        for (unsigned value = 0; value <= 255; ++value) {
            check(ExploreRunRules::cooldownAfterCompletedSteps(cooldown, value) ==
                  (cooldown > value ? cooldown - value : 0));
            const bool open = cooldown == 0 && value < 2;
            check(ExploreRunRules::encounterGateOpen(cooldown, value) == open);
            check(ExploreRunRules::encounterGateAllows(cooldown, value, false) == open);
            check(ExploreRunRules::encounterGateAllows(cooldown, value, true));
            const bool schedule = value >= 3 && cooldown + 1 <= value - 2;
            check(ExploreRunRules::canScheduleGuaranteedEncounter(value, cooldown) == schedule);
            if (schedule) {
                unsigned expected = value * 2 / 3;
                if (expected < cooldown + 1) expected = cooldown + 1;
                if (expected > value - 2) expected = value - 2;
                check(ExploreRunRules::guaranteedEncounterIndex(value, cooldown) == expected);
            }
        }
    }
    for (bool guaranteed : {false, true}) {
        for (bool repel : {false, true}) {
            check(ExploreRunRules::repelAllowsEncounter(guaranteed, repel) == (guaranteed || !repel));
        }
    }
    const ExplorePickupRules::Table empty{nullptr, 0, 0, 0};
    check(ExplorePickupRules::weightTotal(empty, 5000) == 0);
    check(ExplorePickupRules::forRoll(empty, 5000, 0) == ExplorePickupRules::NONE);
    check(randomCalls == 0);
    GameRandom::setRangeProvider(nullptr);
    std::printf("shared-rules-checks=%u rng-calls=%u\n", checks, randomCalls);
}
