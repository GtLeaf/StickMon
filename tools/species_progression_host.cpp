#include <array>
#include <cstdint>

#include "game/BattleSystem.h"
#include "game/Species.h"

namespace {

struct GrowthExpectation {
    GrowthRate rate;
    uint32_t level100Exp;
};

constexpr std::array<GrowthExpectation, 6> EXPECTED_GROWTH = {{
    {GrowthRate::MEDIUM, 1000000},
    {GrowthRate::ERRATIC, 600000},
    {GrowthRate::FLUCTUATING, 1640000},
    {GrowthRate::PARABOLIC, 1059860},
    {GrowthRate::FAST, 800000},
    {GrowthRate::SLOW, 1250000},
}};

bool hasLearnsetEntry(const Species& species, uint8_t level, Game::MoveId moveId) {
    const uint16_t count = learnsetEntryCountForSpecies(species);
    for (uint16_t index = 0; index < count; ++index) {
        const LearnsetEntry* entry = learnsetEntryForSpecies(species, index);
        if (entry && entry->level == level && entry->moveId == moveId) return true;
    }
    return false;
}

} // namespace

int main() {
    static_assert(sizeof(Game::MoveId) == 2, "official move ids require 16 bits");
    if (Game::LEVEL_MAX != 100) return 1;

    for (const GrowthExpectation& expected : EXPECTED_GROWTH) {
        if (minimumExpForLevel(expected.rate, Game::LEVEL_MAX) != expected.level100Exp) {
            return 2;
        }
        uint32_t previous = 0;
        for (uint8_t level = 1; level <= Game::LEVEL_MAX; ++level) {
            uint32_t current = minimumExpForLevel(expected.rate, level);
            if (level > 1 && current <= previous) return 3;
            if (levelForExp(expected.rate, current) != level) return 4;
            if (level < Game::LEVEL_MAX) {
                uint32_t next = minimumExpForLevel(
                    expected.rate, static_cast<uint8_t>(level + 1));
                if (levelForExp(expected.rate, next - 1) != level) return 5;
            }
            previous = current;
        }
    }

    if (speciesCount() != 64) return 6;
    for (uint8_t speciesIndex = 0; speciesIndex < speciesCount(); ++speciesIndex) {
        const Species& species = speciesTable()[speciesIndex];
        const SpeciesLearnset* learnset = findLearnset(species.id);
        if (!learnset || learnset->count == 0) return 7;
        if (!findMove(basicMoveIdForSpecies(species))) return 8;
        if (species.baseExp == 0) return 29;

        uint8_t previousLevel = 0;
        for (uint16_t entryIndex = 0; entryIndex < learnset->count; ++entryIndex) {
            const LearnsetEntry* entry = learnsetEntryForSpecies(species, entryIndex);
            if (!entry || !findMove(entry->moveId)) return 9;
            if (entryIndex > 0 && entry->level < previousLevel) return 10;
            previousLevel = entry->level;
        }
        if (learnsetEntryForSpecies(species, learnset->count) != nullptr) return 11;
    }

    const Species* bulbasaur = findSpecies(1);
    const Species* metapod = findSpecies(11);
    const Species* magikarp = findSpecies(129);
    if (!bulbasaur || !metapod || !magikarp) return 12;
    if (bulbasaur->baseExp != 64 || metapod->baseExp != 72 || magikarp->baseExp != 40) {
        return 30;
    }
    if (BattleSystem::experienceReward(*magikarp, 7) != 40 ||
        BattleSystem::experienceReward(*magikarp, 12) != 68 ||
        BattleSystem::experienceReward(*magikarp, 17) != 97) {
        return 31;
    }
    if (basicMoveIdForSpecies(*bulbasaur) != 33) return 13;
    if (!hasLearnsetEntry(*bulbasaur, 7, 73)) return 14;   // Leech Seed
    if (!hasLearnsetEntry(*bulbasaur, 9, 22)) return 15;   // Vine Whip
    if (!hasLearnsetEntry(*bulbasaur, 37, 402)) return 16; // Seed Bomb
    if (canLearnAsSpecialMove(*bulbasaur, 73)) return 17;
    if (!canLearnAsSpecialMove(*bulbasaur, 45)) return 33;
    if (!canLearnAsSpecialMove(*bulbasaur, 22)) return 18;
    if (moveLearnLevelForSpecies(*bulbasaur, 402) != 37) return 19;

    const MoveInfo* tackle = findMove(33);
    const MoveInfo* leechLife = findMove(141);
    const MoveInfo* seedBomb = findMove(402);
    if (!tackle || tackle->power != 50 || tackle->damageClass != DamageClass::PHYSICAL) {
        return 20;
    }
    if (!leechLife || leechLife->power != 20) return 21;
    if (!seedBomb || seedBomb->type != TypeId::GRASS || !seedBomb->battleSupported) {
        return 22;
    }
    const MoveInfo* fireFang = findMove(424);
    const MoveInfo* wrap = findMove(35);
    const MoveInfo* harden = findMove(106);
    if (!fireFang || fireFang->effectCount != 2 || !wrap || !harden) return 34;
    const MoveEffectSpec* fireFangBurn = moveEffectFor(*fireFang, 0);
    const MoveEffectSpec* fireFangFlinch = moveEffectFor(*fireFang, 1);
    const MoveEffectSpec* wrapBind = moveEffectFor(*wrap, 0);
    const MoveEffectSpec* hardenBoost = moveEffectFor(*harden, 0);
    if (!fireFangBurn || fireFangBurn->kind != MoveEffectKind::MAJOR_STATUS ||
        !fireFangFlinch || fireFangFlinch->kind != MoveEffectKind::FLINCH ||
        !wrapBind || wrapBind->kind != MoveEffectKind::BIND ||
        !hardenBoost || hardenBoost->kind != MoveEffectKind::STAT_STAGE) {
        return 35;
    }

    Game::MonsterRuntime mon;
    mon.speciesId = bulbasaur->id;
    mon.level = 8;
    mon.moveProficiency[0] = 12;
    mon.moveProficiency[1] = 34;
    mon.moveProficiency[2] = 56;
    resetMovesForLevel(mon, *bulbasaur);
    if (mon.move1Id != 33 || mon.move2Id != 45 || mon.move3Id != 0) return 23;
    if (mon.moveProficiency[0] != Game::MOVE_PROFICIENCY_MAX ||
        mon.moveProficiency[1] != 0 || mon.moveProficiency[2] != 0) return 32;
    mon.level = 9;
    resetMovesForLevel(mon, *bulbasaur);
    if (mon.move1Id != 33 || mon.move2Id != 45 || mon.move3Id != 22) return 24;
    mon.level = 37;
    resetMovesForLevel(mon, *bulbasaur);
    if (mon.move2Id != 45 || mon.move3Id != 402) return 25;

    mon.speciesId = metapod->id;
    mon.level = 7;
    resetMovesForLevel(mon, *metapod);
    if (mon.move1Id != 106 || mon.move2Id != 0 || mon.move3Id != 0) return 26;

    mon.speciesId = magikarp->id;
    mon.level = 14;
    resetMovesForLevel(mon, *magikarp);
    if (mon.move1Id != 150 || mon.move2Id != 0) return 27;
    mon.level = 15;
    resetMovesForLevel(mon, *magikarp);
    if (mon.move1Id != 150 || mon.move2Id != 33) return 28;

    return 0;
}
