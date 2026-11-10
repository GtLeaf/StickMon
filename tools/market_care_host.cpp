// Host checks for the Chansey stall rules (暖心小屋) and their save round trip.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include "core/SaveCodec.h"
#include "game/MarketCareService.h"
#include "save_schema1_fabricator.h"

using namespace Game;
namespace Care = Game::MarketCare;
using Target = Care::Target;

static MonsterRuntime mon(uint16_t species, uint32_t iv, uint32_t met, uint8_t affection, int8_t bond) {
    MonsterRuntime m{};
    m.speciesId = species; m.ivPacked = iv; m.metAt = met; m.hpCur = m.hpMax = 30;
    m.affection = affection; m.bond = bond; m.mood = 95; m.origin = Origin::BEFRIENDED;
    m.lastExploredAt = met + 1; // already explored together
    return m;
}

static GameState fixture(uint32_t day) {
    GameState s{};
    s.coins = 1000; s.gameMinutesTotal = day * 1440 + 9 * 60;
    s.teamCount = 2;
    s.team[0] = mon(25, 0x1111, 10, 250, 100);
    s.team[1] = mon(133, 0x2222, 20, 100, 40);
    s.team[1].origin = Origin::VISITOR;
    s.storageCount = 5;
    s.storage[0] = s.team[0];                   // roster mirror of the leader
    s.storage[1] = mon(4, 0x3333, 30, 50, 23);  // partner, bond 23 (初识)
    s.storage[2] = mon(7, 0x4444, 40, 50, Bond::NEW_CONTACT_VALUE);
    s.storage[2].lastExploredAt = 0;            // never invited: guaranteed first invite
    s.storage[3] = mon(1, 0x5555, 50, 50, Bond::MAX_VALUE);
    s.storage[4] = mon(10, 0x6666, 60, 50, 30);
    s.storage[4].fainted = 1; s.storage[4].hpCur = 0;
    return s;
}

int main() {
    const Target leader{Target::TEAM, 0}, visitor{Target::TEAM, 1};
    const Target mirror{Target::CONTACT, 0}, partner{Target::CONTACT, 1}, first{Target::CONTACT, 2},
                 full{Target::CONTACT, 3}, fainted{Target::CONTACT, 4};
    auto s = fixture(3);
    const uint32_t day = Care::periodDay(s);

    // Listing: visitors and roster mirrors of team members are left out.
    Target list[Care::TARGET_CAP];
    assert(Care::targets(s, list, true) == 1 && list[0] == leader);
    const uint8_t n = Care::targets(s, list);
    assert(n == 5 && list[0] == leader && list[1] == partner && list[4] == fainted);
    assert(!Care::monster(s, mirror));

    // Refusals never charge or use quota.
    assert(Care::check(s, Care::Service::MASSAGE, partner, day) == Care::Result::BAD_TARGET);
    assert(Care::check(s, Care::Service::MASSAGE, visitor, day) == Care::Result::VISITOR);
    assert(Care::check(s, Care::Service::TEA, full, day) == Care::Result::FULL);
    assert(Care::check(s, Care::Service::TEA, fainted, day) == Care::Result::FAINTED);
    assert(Care::check(s, Care::Service::TEA, first, day) == Care::Result::FIRST_INVITE);
    assert(Care::check(s, Care::Service::MASSAGE, leader, day + 1) == Care::Result::STALE);
    assert(Care::apply(s, Care::Service::TEA, first, day) == Care::Result::FIRST_INVITE);
    assert(s.coins == 1000 && Care::remaining(s) == 2 && s.marketCare.periodDay == MARKET_PERIOD_NONE);
    {
        auto poor = s; poor.coins = 149;
        assert(Care::check(poor, Care::Service::MASSAGE, leader, day) == Care::Result::NO_COINS);
    }

    // Massage near the caps grants the remaining amount and syncs the roster.
    assert(Care::affectionAfter(s.team[0]) == 255 && Care::moodAfter(s.team[0]) == 100);
    assert(Care::apply(s, Care::Service::MASSAGE, leader, day) == Care::Result::OK);
    assert(s.team[0].affection == 255 && s.team[0].mood == 100 && s.coins == 850);
    assert(s.storage[0].affection == 255 && s.storage[0].mood == 100);
    assert(s.team[0].hpCur == 30 && s.team[0].bond == 100);
    assert(Care::remaining(s) == 1 && Care::servedThisPeriod(s, s.team[0]));
    // One service per monster per period, shared across both services.
    assert(Care::check(s, Care::Service::MASSAGE, leader, day) == Care::Result::FULL);
    s.team[0].affection = 200;
    assert(Care::check(s, Care::Service::MASSAGE, leader, day) == Care::Result::USED);
    // Identity survives team reordering and evolution.
    {
        auto moved = s; moved.team[0].speciesId = 26; std::swap(moved.team[0], moved.team[1]);
        assert(Care::check(moved, Care::Service::MASSAGE, {Target::TEAM, 1}, day) == Care::Result::USED);
    }

    // Tea on a roster partner: bond +5, level crosses 初识 -> 熟悉.
    assert(Care::bondAfter(s.storage[1]) == 28);
    assert(Bond::levelFor(23) == Bond::Level::ACQUAINTED && Bond::levelFor(28) == Bond::Level::FAMILIAR);
    assert(Care::inviteChance(partner, 23) == 35 && Care::inviteChance(partner, 28) == 60);
    assert(Care::inviteChance(leader, 28) == 0);
    const uint8_t lockMarker = s.storage[1].petCountToday = Bond::inviteLockMarker(Bond::invitationDay(s.gameMinutesTotal));
    assert(Care::apply(s, Care::Service::TEA, partner, day) == Care::Result::OK);
    assert(s.storage[1].bond == 28 && s.coins == 650 && s.storage[1].petCountToday == lockMarker);
    assert(s.teamCount == 2 && Care::remaining(s) == 0);
    assert(Care::check(s, Care::Service::TEA, {Target::CONTACT, 1}, day) == Care::Result::USED);
    // Stall quota: two paid services per period.
    s.storage[4].fainted = 0; s.storage[4].hpCur = 10;
    assert(Care::check(s, Care::Service::TEA, fainted, day) == Care::Result::QUOTA_FULL);
    assert(!Care::anyEligible(s, Care::Service::TEA) && !Care::anyEligible(s, Care::Service::MASSAGE));

    // Next game day resets the stall but not the earlier gains.
    s.gameMinutesTotal += 24 * 60;
    assert(Care::remaining(s) == 2 && !Care::servedThisPeriod(s, s.team[0]));
    assert(Care::anyEligible(s, Care::Service::TEA));
    assert(Care::apply(s, Care::Service::TEA, fainted, day + 1) == Care::Result::OK);
    assert(s.marketCare.periodDay == day + 1 && s.marketCare.used == 1 && s.storage[4].bond == 35);

    // Advice priorities.
    {
        auto a = fixture(1);
        assert(Care::advice(a, fainted) == Care::Advice::REST);
        assert(Care::advice(a, first) == Care::Advice::INVITE);
        assert(Care::advice(a, partner) == Care::Advice::TEA);
        a.team[0].affection = 100; assert(Care::advice(a, leader) == Care::Advice::MASSAGE);
        a.team[0].affection = 230; a.team[0].bond = 60; assert(Care::advice(a, leader) == Care::Advice::EXPLORE);
        assert(Care::advice(a, full) == Care::Advice::BEST);
    }

    // Save codec: schema 6 round trip, schema 5 blobs decode to an empty stall.
    {
        uint8_t encoded[SaveCodec::MAX_ENCODED_BYTES] = {};
        size_t length = 0;
        MainSceneViewState view;
        s.marketShop.periodDay = 7; s.marketShop.items[0] = 3; s.marketShop.stock[0] = 1;
        assert(SaveCodec::encode(s, view, 4, encoded, sizeof(encoded), length));
        SaveCodec::Snapshot decoded;
        assert(SaveCodec::decode(encoded, length, decoded));
        assert(decoded.state.marketCare.periodDay == day + 1 && decoded.state.marketCare.used == 1);
        assert(decoded.state.marketCare.served[0].ivPacked == 0x6666 && decoded.state.marketCare.served[0].metAt == 60);
        std::vector<uint8_t> legacy;
        assert(SaveTestUtil::fabricateSchema5Blob(encoded, length, legacy));
        SaveCodec::Snapshot old;
        assert(SaveCodec::decode(legacy.data(), legacy.size(), old));
        assert(old.state.marketCare.periodDay == MARKET_PERIOD_NONE && old.state.marketCare.used == 0);
        assert(old.state.marketShop.periodDay == 7 && old.state.marketShop.items[0] == 3);
        assert(old.state.version == SAVE_VERSION && old.state.coins == s.coins);
    }
    std::puts("market-care: listing refusals caps sync identity quota period advice save OK");
}
