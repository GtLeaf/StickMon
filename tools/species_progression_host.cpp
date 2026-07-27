#include <array>
#include <cstdint>

#include "game/BattleSystem.h"
#include "game/ExploreBoss.h"
#include "game/FriendshipSystem.h"
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

constexpr uint16_t EXPECTED_BOSS_EXP
    [ExploreBoss::AREA_COUNT][ExploreBoss::CANDIDATE_COUNT] = {
        {494, 414, 348, 320},
        {688, 918, 732, 732},
        {1458, 1820, 1650, 1796},
        {2016, 2700, 2616, 2892},
        {2786, 3406, 3648, 2650},
        {5168, 4594, 4172, 4612},
    };

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
        if (species.catchRate == 0) return 60;

        uint8_t previousLevel = 0;
        for (uint16_t entryIndex = 0; entryIndex < learnset->count; ++entryIndex) {
            const LearnsetEntry* entry = learnsetEntryForSpecies(species, entryIndex);
            if (!entry || !findMove(entry->moveId)) return 9;
            if (entryIndex > 0 && entry->level < previousLevel) return 10;
            previousLevel = entry->level;
        }
        if (learnsetEntryForSpecies(species, learnset->count) != nullptr) return 11;

        if (species.evolveTo != 0) {
            const Species* target = findSpecies(species.evolveTo);
            if (!target || target->id == species.id) return 36;
            if (target->growthRate != species.growthRate) return 37;
            if (species.evolveMethod == EvolutionMethod::LEVEL &&
                (species.evolveLevel < 2 || species.evolveLevel > Game::LEVEL_MAX)) {
                return 38;
            }
        }
    }

    const Species* bulbasaur = findSpecies(1);
    const Species* metapod = findSpecies(11);
    const Species* magikarp = findSpecies(129);
    if (!bulbasaur || !metapod || !magikarp) return 12;
    if (bulbasaur->baseExp != 64 || metapod->baseExp != 72 || magikarp->baseExp != 40) {
        return 30;
    }
    if (bulbasaur->catchRate != 45 || metapod->catchRate != 120 ||
        magikarp->catchRate != 255) {
        return 61;
    }
    const Species* latias = findSpecies(380);
    if (!latias || latias->catchRate != 3) return 62;

    Game::MonsterRuntime friendshipTarget;
    friendshipTarget.level = 25;
    friendshipTarget.majorStatus = Game::MajorStatus::NONE;
    uint16_t normalChance = FriendshipSystem::offerChancePermille(
        *bulbasaur, friendshipTarget, false);
    friendshipTarget.majorStatus = Game::MajorStatus::SLEEP;
    uint16_t sleepChance = FriendshipSystem::offerChancePermille(
        *bulbasaur, friendshipTarget, false);
    friendshipTarget.majorStatus = Game::MajorStatus::NONE;
    uint16_t bossChance = FriendshipSystem::offerChancePermille(
        *bulbasaur, friendshipTarget, true);
    uint16_t legendaryChance = FriendshipSystem::offerChancePermille(
        *latias, friendshipTarget, false);
    uint16_t fedChance = FriendshipSystem::offerChancePermille(
        *bulbasaur, friendshipTarget, false,
        FriendshipSystem::FOOD_BOND_MAX);
    uint16_t easyFedChance = FriendshipSystem::offerChancePermille(
        *magikarp, friendshipTarget, false,
        FriendshipSystem::FOOD_BOND_MAX);
    if (normalChance < 24 || normalChance > 30 ||
        sleepChance <= normalChance || sleepChance > 60 ||
        bossChance >= normalChance || legendaryChance > 3 ||
        fedChance < normalChance * 2 - 1 ||
        fedChance > normalChance * 2 + 1 ||
        easyFedChance != FriendshipSystem::OFFER_CHANCE_MAX_PERMILLE) {
        return 63;
    }
    uint16_t lowRolls[FriendshipSystem::SHAKE_CHECK_COUNT] = {};
    uint16_t highRolls[FriendshipSystem::SHAKE_CHECK_COUNT] = {
        65535, 65535, 65535, 65535,
    };
    if (!FriendshipSystem::passesOfferChecks(
            *magikarp, friendshipTarget, false, 0, lowRolls) ||
        FriendshipSystem::passesOfferChecks(
            *magikarp, friendshipTarget, false,
            FriendshipSystem::OFFER_GATE_PERMILLE, lowRolls) ||
        !FriendshipSystem::passesOfferChecks(
            *magikarp, friendshipTarget, false, 200, lowRolls,
            FriendshipSystem::FOOD_BOND_MAX) ||
        FriendshipSystem::passesOfferChecks(
            *magikarp, friendshipTarget, false,
            FriendshipSystem::OFFER_CHANCE_MAX_PERMILLE, lowRolls,
            FriendshipSystem::FOOD_BOND_MAX) ||
        FriendshipSystem::passesOfferChecks(
            *bulbasaur, friendshipTarget, false, 0, highRolls)) {
        return 64;
    }
    if (FriendshipSystem::addFoodBond(0, FriendshipSystem::NORMAL_FOOD_BOND_GAIN) != 30 ||
        FriendshipSystem::addFoodBond(90, FriendshipSystem::NORMAL_FOOD_BOND_GAIN) !=
            FriendshipSystem::FOOD_BOND_MAX ||
        !FriendshipSystem::acceptsNormalFood(false, 49) ||
        FriendshipSystem::acceptsNormalFood(false, 50) ||
        !FriendshipSystem::acceptsNormalFood(true, 29) ||
        FriendshipSystem::acceptsNormalFood(true, 30)) {
        return 65;
    }
    if (BattleSystem::experienceReward(*magikarp, 7) != 40 ||
        BattleSystem::experienceReward(*magikarp, 12) != 68 ||
        BattleSystem::experienceReward(*magikarp, 17) != 97) {
        return 31;
    }
    if (BattleSystem::RESERVE_EXP_PERCENT != 50) return 39;
    BattleSystem::ExperienceAwards soloAwards = BattleSystem::experienceAwards(97, false);
    BattleSystem::ExperienceAwards teamAwards = BattleSystem::experienceAwards(97, true);
    if (soloAwards.active != 97 || soloAwards.reserve != 0 ||
        teamAwards.active != 97 || teamAwards.reserve != 48) {
        return 40;
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

    mon = {};
    mon.speciesId = bulbasaur->id;
    mon.level = 15;
    if (levelUpEvolutionTarget(*bulbasaur, mon) != nullptr) return 41;
    mon.level = 16;
    const Species* ivysaur = levelUpEvolutionTarget(*bulbasaur, mon);
    if (!ivysaur || ivysaur->id != 2) return 42;
    mon.speciesId = ivysaur->id;
    mon.level = 31;
    if (levelUpEvolutionTarget(*ivysaur, mon) != nullptr) return 43;
    mon.level = 32;
    const Species* venusaur = levelUpEvolutionTarget(*ivysaur, mon);
    if (!venusaur || venusaur->id != 3 ||
        levelUpEvolutionTarget(*venusaur, mon) != nullptr) {
        return 44;
    }

    const Species* azurill = findSpecies(298);
    const Species* pikachu = findSpecies(25);
    const Species* graveler = findSpecies(75);
    const Species* eevee = findSpecies(133);
    if (!azurill || !pikachu || !graveler || !eevee) return 45;
    mon = {};
    mon.speciesId = azurill->id;
    mon.level = 10;
    mon.affection = FRIENDSHIP_EVOLUTION_THRESHOLD - 1;
    if (levelUpEvolutionTarget(*azurill, mon) != nullptr) return 46;
    mon.affection = FRIENDSHIP_EVOLUTION_THRESHOLD;
    const Species* marill = levelUpEvolutionTarget(*azurill, mon);
    if (!marill || marill->id != 183) return 47;
    mon.level = Game::LEVEL_MAX;
    mon.affection = 255;
    if (levelUpEvolutionTarget(*pikachu, mon) != nullptr ||
        levelUpEvolutionTarget(*graveler, mon) != nullptr ||
        levelUpEvolutionTarget(*eevee, mon) != nullptr) {
        return 48;
    }

    bool testedInheritedMove = false;
    for (uint8_t speciesIndex = 0; speciesIndex < speciesCount(); ++speciesIndex) {
        const Species& source = speciesTable()[speciesIndex];
        const Species* target = findSpecies(source.evolveTo);
        if (!target) continue;
        const uint16_t count = learnsetEntryCountForSpecies(source);
        for (uint16_t entryIndex = 0; entryIndex < count; ++entryIndex) {
            const LearnsetEntry* entry = learnsetEntryForSpecies(source, entryIndex);
            if (!entry || !canLearnAsSpecialMove(source, entry->moveId) ||
                moveLearnLevelForSpecies(*target, entry->moveId) != 0) {
                continue;
            }
            if (!canRetainSpecialMove(*target, entry->moveId, Game::LEVEL_MAX)) return 49;
            testedInheritedMove = true;
            break;
        }
        if (testedInheritedMove) break;
    }
    if (!testedInheritedMove) return 50;

    if (ExploreBoss::SPAWN_CHANCE != 4000 ||
        ExploreBoss::SPAWN_ROLL_MAX != 10000) {
        return 51;
    }
    if (ExploreBoss::victoryCoinReward(false) != 0 ||
        ExploreBoss::victoryCoinReward(true) !=
            ExploreBoss::VICTORY_COIN_REWARD) {
        return 59;
    }
    uint8_t previousBossLevel = 0;
    for (uint8_t area = 0; area < ExploreBoss::AREA_COUNT; ++area) {
        const ExploreBoss::Config& boss = ExploreBoss::configForArea(area);
        if (boss.level <= previousBossLevel || boss.level > Game::LEVEL_MAX) return 53;
        if (boss.experiencePercent != 200) return 54;
        for (uint8_t candidate = 0;
             candidate < ExploreBoss::CANDIDATE_COUNT;
             ++candidate) {
            uint16_t speciesId = boss.speciesIds[candidate];
            const Species* bossSpecies = findSpecies(speciesId);
            if (!bossSpecies ||
                ExploreBoss::speciesForRoll(area, candidate) != speciesId ||
                ExploreBoss::speciesForRoll(
                    area, candidate + ExploreBoss::CANDIDATE_COUNT) != speciesId) {
                return 52;
            }
            for (uint8_t previous = 0; previous < candidate; ++previous) {
                if (boss.speciesIds[previous] == speciesId) return 58;
            }
            uint16_t baseReward = BattleSystem::experienceReward(
                *bossSpecies, boss.level);
            uint16_t bossReward = BattleSystem::scaledExperienceReward(
                baseReward, boss.experiencePercent);
            if (bossReward != EXPECTED_BOSS_EXP[area][candidate]) return 55;
            BattleSystem::ExperienceAwards awards = BattleSystem::experienceAwards(
                bossReward, true);
            if (awards.active != bossReward || awards.reserve != baseReward) return 56;
        }
        previousBossLevel = boss.level;
    }
    if (!ExploreBoss::canPlaceOnPath(3) ||
        ExploreBoss::routeIndex(3) != 1 ||
        ExploreBoss::routeIndex(12) != 10) {
        return 57;
    }

    return 0;
}
