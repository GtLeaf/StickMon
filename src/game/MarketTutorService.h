#pragma once

#include <cstdint>
#include "game/ExploreCostService.h"
#include "game/GameState.h"
#include "game/ItemInventory.h"
#include "game/MoveManagementService.h"
#include "game/Species.h"

// Gengar stall (秘技小屋): one TM-style lesson per game day drawn from the
// current team's learnable moves, plus proficiency training. Pure rules only;
// callers persist the mutated GameState and restore their copy on failure.
namespace Game::MarketTutor {

constexpr uint16_t STALL_SPECIES = 94;
constexpr uint8_t POOL_CAP = 96;

enum class Eligibility : uint8_t { CAN_LEARN, KNOWN, INCOMPATIBLE, FAINTED, HUNGRY, VISITOR };
enum class Result : uint8_t {
    OK, NO_CLASS, ALREADY_TAUGHT, BAD_MEMBER, BAD_SLOT, INCOMPATIBLE, KNOWN,
    FAINTED, HUNGRY, NO_COINS, NO_HEART_SCALE, MAXED, ALREADY_TRAINED, QUOTA_FULL,
};
struct TrainStep { uint16_t cost; uint8_t gain; };

inline uint32_t periodDay(const GameState& state) { return state.gameMinutesTotal / (24U * 60U); }

// Price tiers are data, not derived from damage alone: status moves sit in
// the middle tier because control effects can matter as much as power.
inline uint16_t lessonPrice(MoveId move) {
    const MoveInfo* info = findMove(move);
    if (!info) return 0;
    if (info->damageClass == DamageClass::STATUS) return 400;
    return info->power >= 86 ? 600 : info->power >= 61 ? 400 : 200;
}

inline TrainStep trainStep(uint8_t proficiency) {
    if (proficiency >= MOVE_PROFICIENCY_MAX) return {0, 0};
    if (proficiency < 40) return {100, 10};
    if (proficiency < 80) return {200, 8};
    return {300, 5};
}
inline uint8_t trainedProficiency(uint8_t proficiency) {
    const auto step = trainStep(proficiency);
    return uint8_t(std::min<int>(MOVE_PROFICIENCY_MAX, proficiency + step.gain));
}

inline bool knows(const MonsterRuntime& m, MoveId move) {
    return move && (m.move1Id == move || m.move2Id == move || m.move3Id == move);
}

// TM compatibility or a level-up move the monster cannot recall yet. Moves the
// heart-scale recall already offers are excluded so the two prices never sell
// the same service.
inline bool teachable(const Species& species, const MonsterRuntime& m, MoveId move) {
    if (!move || knows(m, move) || isBasicFirstMoveForSpecies(species, move) ||
        !isMoveBattleSupported(move) || canRetainLevelMove(species, move, m.level)) return false;
    return canLearnByTutor(species, move) || canLearnAsSpecialMove(species, move);
}

inline uint8_t collectPool(const MonsterRuntime& m, MoveId* out, uint8_t capacity) {
    const Species* species = findSpecies(m.speciesId);
    if (!species || !out) return 0;
    uint8_t count = 0;
    const auto add = [&](MoveId move) {
        if (count >= capacity || !teachable(*species, m, move)) return;
        for (uint8_t i = 0; i < count; ++i) if (out[i] == move) return;
        out[count++] = move;
    };
    for (uint16_t i = 0; i < tutorMoveCountForSpecies(*species); ++i) add(tutorMoveForSpecies(*species, i));
    for (uint16_t i = 0; i < learnsetEntryCountForSpecies(*species); ++i) {
        const LearnsetEntry* e = learnsetEntryForSpecies(*species, i);
        if (e) add(e->moveId);
    }
    return count;
}

inline Eligibility eligibility(const MonsterRuntime& m, MoveId move) {
    if (m.origin == Origin::VISITOR) return Eligibility::VISITOR;
    const Species* species = findSpecies(m.speciesId);
    if (!species) return Eligibility::INCOMPATIBLE;
    if (knows(m, move)) return Eligibility::KNOWN;
    if (!teachable(*species, m, move)) return Eligibility::INCOMPATIBLE;
    if (!ExplorePartyService::healthy(m)) return Eligibility::FAINTED;
    if (ExploreCost::needsFood(m) && m.satiety < ExploreCost::MIN_DEPARTURE_SATIETY) return Eligibility::HUNGRY;
    return Eligibility::CAN_LEARN;
}

inline uint32_t mix(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x ? x : 1;
}

// Draw once per period with a private seed; never touches GameRandom.
// Returns true when the stored period changed (caller should persist).
inline bool ensurePeriod(GameState& state) {
    auto& t = state.marketTutor;
    const uint32_t day = periodDay(state);
    if (t.periodDay == day) return false;
    t = MarketTutorState{};
    t.periodDay = day;
    uint32_t seed = mix(day ^ 0x47454e47U);
    MoveId pools[TEAM_CAP][POOL_CAP];
    uint8_t counts[TEAM_CAP] = {}, members[TEAM_CAP], memberCount = 0;
    for (uint8_t i = 0; i < state.teamCount && i < TEAM_CAP; ++i) {
        seed = mix(seed ^ state.team[i].ivPacked ^ state.team[i].metAt);
        if (state.team[i].origin == Origin::VISITOR) continue;
        counts[i] = collectPool(state.team[i], pools[i], POOL_CAP);
        if (counts[i]) members[memberCount++] = i;
    }
    if (!memberCount) return true; // 休课 this period
    const uint8_t member = members[seed % memberCount];
    seed = mix(seed + 0x9e3779b9U);
    t.move = pools[member][seed % counts[member]];
    t.drawnSpecies = state.team[member].speciesId;
    return true;
}

inline int trainedIndex(const GameState& state, const MonsterRuntime& m) {
    const auto& t = state.marketTutor;
    for (uint8_t i = 0; i < t.trainCount && i < MARKET_TUTOR_TRAIN_CAP; ++i)
        if (t.trained[i].ivPacked == m.ivPacked && t.trained[i].metAt == m.metAt) return i;
    return -1;
}
inline bool trainedThisPeriod(const GameState& state, const MonsterRuntime& m) {
    return state.marketTutor.periodDay == periodDay(state) && trainedIndex(state, m) >= 0;
}

inline Result checkMember(const GameState& state, uint8_t slot) {
    if (slot >= state.teamCount || slot >= TEAM_CAP) return Result::BAD_MEMBER;
    const auto& m = state.team[slot];
    if (m.origin == Origin::VISITOR) return Result::BAD_MEMBER;
    if (!ExplorePartyService::healthy(m)) return Result::FAINTED;
    if (ExploreCost::needsFood(m) && m.satiety < ExploreCost::MIN_DEPARTURE_SATIETY) return Result::HUNGRY;
    return Result::OK;
}

inline Result checkTeach(const GameState& state, uint8_t slot, uint8_t replacement) {
    const auto& t = state.marketTutor;
    if (t.periodDay != periodDay(state) || !t.move) return Result::NO_CLASS;
    if (t.taught) return Result::ALREADY_TAUGHT;
    if (replacement < MoveManagementService::SPECIAL_MOVE_SLOT_FIRST || replacement >= MOVE_SLOT_COUNT) return Result::BAD_SLOT;
    const Result member = checkMember(state, slot);
    if (member != Result::OK) return member;
    const auto e = eligibility(state.team[slot], t.move);
    if (e == Eligibility::KNOWN) return Result::KNOWN;
    if (e != Eligibility::CAN_LEARN) return Result::INCOMPATIBLE;
    if (state.coins < lessonPrice(t.move)) return Result::NO_COINS;
    return Result::OK;
}

inline Result teach(GameState& state, uint8_t slot, uint8_t replacement) {
    const Result r = checkTeach(state, slot, replacement);
    if (r != Result::OK) return r;
    auto& t = state.marketTutor;
    auto& m = state.team[slot];
    state.coins -= lessonPrice(t.move);
    (replacement == 1 ? m.move2Id : m.move3Id) = t.move;
    m.moveProficiency[replacement] = 0;
    t.taught = 1;
    return Result::OK;
}

inline Result checkTrain(const GameState& state, uint8_t slot, uint8_t moveSlot) {
    if (moveSlot < MoveManagementService::SPECIAL_MOVE_SLOT_FIRST || moveSlot >= MOVE_SLOT_COUNT) return Result::BAD_SLOT;
    const Result member = checkMember(state, slot);
    if (member != Result::OK) return member;
    const auto& m = state.team[slot];
    if (!(moveSlot == 1 ? m.move2Id : m.move3Id)) return Result::BAD_SLOT;
    if (state.marketTutor.periodDay != periodDay(state)) return Result::NO_CLASS;
    if (trainedThisPeriod(state, m)) return Result::ALREADY_TRAINED;
    if (state.marketTutor.trainCount >= MARKET_TUTOR_TRAIN_CAP) return Result::QUOTA_FULL;
    const auto step = trainStep(m.moveProficiency[moveSlot]);
    if (!step.gain) return Result::MAXED;
    if (state.coins < step.cost) return Result::NO_COINS;
    return Result::OK;
}

inline Result train(GameState& state, uint8_t slot, uint8_t moveSlot) {
    const Result r = checkTrain(state, slot, moveSlot);
    if (r != Result::OK) return r;
    auto& m = state.team[slot];
    auto& t = state.marketTutor;
    state.coins -= trainStep(m.moveProficiency[moveSlot]).cost;
    m.moveProficiency[moveSlot] = trainedProficiency(m.moveProficiency[moveSlot]);
    t.trained[t.trainCount++] = {m.ivPacked, m.metAt};
    return Result::OK;
}

inline uint8_t trainableMembers(const GameState& state) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < state.teamCount && i < TEAM_CAP; ++i)
        if (state.team[i].origin != Origin::VISITOR && !trainedThisPeriod(state, state.team[i])) ++n;
    return n;
}

// Recall keeps the existing heart-scale rule and service.
inline Result recall(GameState& state, uint8_t slot, MoveId move, uint8_t replacement) {
    if (slot >= state.teamCount || state.team[slot].origin == Origin::VISITOR) return Result::BAD_MEMBER;
    if (!ItemInventory::count(state, ItemId::HEART_SCALE)) return Result::NO_HEART_SCALE;
    return MoveManagementService::recallMove(state, slot, move, replacement) ? Result::OK : Result::INCOMPATIBLE;
}

} // namespace Game::MarketTutor
