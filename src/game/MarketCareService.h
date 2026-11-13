#pragma once

#include <cstdint>
#include "game/BondSystem.h"
#include "game/ContactRoster.h"
#include "game/GameState.h"
#include "game/MarketSchedule.h"

// Chansey stall (暖心小屋): paid affection massage and partner tea, plus a free
// relationship check. Two paid services per game day for the whole stall and
// one per monster, shared between both services and bound to the stable
// identity used by care rewards. Pure rules; the caller saves and restores
// its copy on failure.
namespace Game::MarketCare {

constexpr uint16_t STALL_SPECIES = 113;
constexpr uint16_t MASSAGE_PRICE = 150, TEA_PRICE = 200;
constexpr uint8_t AFFECTION_GAIN = 10, MOOD_GAIN = 10, BOND_GAIN = 5;
constexpr uint8_t AFFECTION_MAX = 255, MOOD_MAX = 100;
constexpr uint8_t ADVICE_AFFECTION = 220; // matches the friendship evolution threshold

enum class Result : uint8_t { OK, STALE, BAD_TARGET, VISITOR, FAINTED, FULL, FIRST_INVITE, USED, QUOTA_FULL, NO_COINS };
enum class Service : uint8_t { MASSAGE, TEA };
enum class Advice : uint8_t { REST, INVITE, TEA, MASSAGE, EXPLORE, BEST };

// A team slot or a roster (storage) slot that is not in the team.
struct Target {
    enum Kind : uint8_t { TEAM, CONTACT } kind = TEAM;
    uint8_t slot = 0;
    bool operator==(const Target& o) const { return kind == o.kind && slot == o.slot; }
};

inline uint32_t periodDay(const GameState& state) { return MarketSchedule::periodDay(state); }

inline const MonsterRuntime* monster(const GameState& state, Target t) {
    if (t.kind == Target::TEAM) return t.slot < state.teamCount && t.slot < TEAM_CAP ? &state.team[t.slot] : nullptr;
    if (t.slot >= state.storageCount || t.slot >= STORAGE_CAP) return nullptr;
    // Roster entries that mirror a team member are listed as that member.
    return ContactRoster::teamSlotForContact(state, t.slot) < 0 ? &state.storage[t.slot] : nullptr;
}
inline MonsterRuntime* monster(GameState& state, Target t) {
    return const_cast<MonsterRuntime*>(monster(static_cast<const GameState&>(state), t));
}

// Team members first (visitors excluded), then roster partners not in the team.
inline uint8_t targets(const GameState& state, Target* out, bool teamOnly = false) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < state.teamCount && i < TEAM_CAP; ++i)
        if (state.team[i].origin != Origin::VISITOR) out[n++] = {Target::TEAM, i};
    if (teamOnly) return n;
    for (uint8_t i = 0; i < state.storageCount && i < STORAGE_CAP; ++i)
        if (monster(state, {Target::CONTACT, i}) && state.storage[i].origin != Origin::VISITOR) out[n++] = {Target::CONTACT, i};
    return n;
}
constexpr uint8_t TARGET_CAP = TEAM_CAP + STORAGE_CAP;

inline uint8_t used(const GameState& state) {
    return state.marketCare.periodDay == periodDay(state) ? state.marketCare.used : 0;
}
inline uint8_t remaining(const GameState& state) {
    return used(state) >= MARKET_CARE_PERIOD_CAP ? 0 : uint8_t(MARKET_CARE_PERIOD_CAP - used(state));
}
inline bool servedThisPeriod(const GameState& state, const MonsterRuntime& m) {
    const auto& care = state.marketCare;
    if (care.periodDay != periodDay(state)) return false;
    for (uint8_t i = 0; i < care.used && i < MARKET_CARE_PERIOD_CAP; ++i)
        if (care.served[i].ivPacked == m.ivPacked && care.served[i].metAt == m.metAt) return true;
    return false;
}

// A partner met in the wild keeps a guaranteed first invitation while its bond
// is still the initial value; tea would cost it that guarantee.
inline bool firstInvitePending(const GameState& state, Target t) {
    const MonsterRuntime* m = monster(state, t);
    return m && t.kind == Target::CONTACT && m->bond == Bond::NEW_CONTACT_VALUE && m->lastExploredAt <= m->metAt;
}
inline uint8_t inviteChance(Target t, Bond::Value bond) {
    return t.kind == Target::CONTACT ? Bond::inviteChance(bond, false) : 0;
}

inline uint8_t affectionAfter(const MonsterRuntime& m) {
    return uint8_t(m.affection + AFFECTION_GAIN > AFFECTION_MAX ? AFFECTION_MAX : m.affection + AFFECTION_GAIN);
}
inline uint8_t moodAfter(const MonsterRuntime& m) {
    return uint8_t(m.mood + MOOD_GAIN > MOOD_MAX ? MOOD_MAX : m.mood + MOOD_GAIN);
}
inline Bond::Value bondAfter(const MonsterRuntime& m) { return Bond::increase(m.bond, BOND_GAIN); }
inline uint16_t price(Service s) { return s == Service::MASSAGE ? MASSAGE_PRICE : TEA_PRICE; }

// `period` is the day the confirmation page showed; a rollover while it is
// open never charges the old quote against the new period.
inline Result check(const GameState& state, Service s, Target t, uint32_t period) {
    if (!MarketSchedule::isOpen(state)) return Result::STALE;
    if (period != periodDay(state)) return Result::STALE;
    if (s == Service::MASSAGE && t.kind != Target::TEAM) return Result::BAD_TARGET;
    const MonsterRuntime* m = monster(state, t);
    if (!m) return Result::BAD_TARGET;
    if (m->origin == Origin::VISITOR) return Result::VISITOR;
    if (m->fainted || m->hpCur == 0) return Result::FAINTED;
    if (s == Service::MASSAGE ? m->affection >= AFFECTION_MAX : m->bond >= Bond::MAX_VALUE) return Result::FULL;
    if (s == Service::TEA && firstInvitePending(state, t)) return Result::FIRST_INVITE;
    if (servedThisPeriod(state, *m)) return Result::USED;
    if (!remaining(state)) return Result::QUOTA_FULL;
    if (state.coins < price(s)) return Result::NO_COINS;
    return Result::OK;
}

inline Result apply(GameState& state, Service s, Target t, uint32_t period) {
    const Result r = check(state, s, t, period);
    if (r != Result::OK) return r;
    auto& care = state.marketCare;
    if (care.periodDay != periodDay(state)) { care = MarketCareState{}; care.periodDay = periodDay(state); }
    MonsterRuntime& m = *monster(state, t);
    care.served[care.used++] = {m.ivPacked, m.metAt};
    state.coins -= price(s);
    if (s == Service::MASSAGE) { m.affection = affectionAfter(m); m.mood = moodAfter(m); }
    else m.bond = bondAfter(m);
    if (t.kind == Target::TEAM) ContactRoster::syncTeamContacts(state);
    return Result::OK;
}

// Is any listed monster able to take this service now (ignoring coins)?
inline bool anyEligible(const GameState& state, Service s) {
    Target list[TARGET_CAP];
    const uint8_t n = targets(state, list, s == Service::MASSAGE);
    for (uint8_t i = 0; i < n; ++i) {
        const Result r = check(state, s, list[i], periodDay(state));
        if (r == Result::OK || r == Result::NO_COINS) return true;
    }
    return false;
}

inline Advice advice(const GameState& state, Target t) {
    const MonsterRuntime* m = monster(state, t);
    if (!m || m->fainted || m->hpCur == 0) return Advice::REST;
    if (firstInvitePending(state, t)) return Advice::INVITE;
    if (m->bond < 25) return Advice::TEA;
    if (t.kind == Target::TEAM && m->affection < ADVICE_AFFECTION) return Advice::MASSAGE;
    if (m->bond < 75) return Advice::EXPLORE;
    return Advice::BEST;
}

} // namespace Game::MarketCare
