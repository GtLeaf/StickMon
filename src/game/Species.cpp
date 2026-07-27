#include "game/Species.h"
#include "core/UiStrings.h"
#include "game/FoodTuning.h"

#include <cstddef>

namespace {
constexpr uint16_t EV(uint8_t hp, uint8_t atk, uint8_t def, uint8_t spa, uint8_t spd, uint8_t spe) {
    return (hp & 0x3) | ((atk & 0x3) << 2) | ((def & 0x3) << 4) |
           ((spa & 0x3) << 6) | ((spd & 0x3) << 8) | ((spe & 0x3) << 10);
}

// Base Exp values follow Pokemon Essentials' Gen 6 PBS data.
const Species SPECIES[] = {
    {1, Ui::SpeciesName::BULBASAUR, TypeId::GRASS, TypeId::POISON, {45, 49, 49, 65, 65, 45}, GrowthRate::PARABOLIC, EV(0, 0, 0, 1, 0, 0), 2, EvolutionMethod::LEVEL, 16, 64, 45},
    {2, Ui::SpeciesName::IVYSAUR, TypeId::GRASS, TypeId::POISON, {60, 62, 63, 80, 80, 60}, GrowthRate::PARABOLIC, EV(0, 0, 0, 1, 1, 0), 3, EvolutionMethod::LEVEL, 32, 142, 45},
    {3, Ui::SpeciesName::VENUSAUR, TypeId::GRASS, TypeId::POISON, {80, 82, 83, 100, 100, 80}, GrowthRate::PARABOLIC, EV(0, 0, 0, 2, 1, 0), 0, EvolutionMethod::NONE, 0, 236, 45},
    {4, Ui::SpeciesName::CHARMANDER, TypeId::FIRE, TypeId::NONE, {39, 52, 43, 60, 50, 65}, GrowthRate::PARABOLIC, EV(0, 0, 0, 0, 0, 1), 5, EvolutionMethod::LEVEL, 16, 62, 45},
    {5, Ui::SpeciesName::CHARMELEON, TypeId::FIRE, TypeId::NONE, {58, 64, 58, 80, 65, 80}, GrowthRate::PARABOLIC, EV(0, 0, 0, 1, 0, 1), 6, EvolutionMethod::LEVEL, 36, 142, 45},
    {6, Ui::SpeciesName::CHARIZARD, TypeId::FIRE, TypeId::FLYING, {78, 84, 78, 109, 85, 100}, GrowthRate::PARABOLIC, EV(0, 0, 0, 3, 0, 0), 0, EvolutionMethod::NONE, 0, 240, 45},
    {7, Ui::SpeciesName::SQUIRTLE, TypeId::WATER, TypeId::NONE, {44, 48, 65, 50, 64, 43}, GrowthRate::PARABOLIC, EV(0, 0, 1, 0, 0, 0), 8, EvolutionMethod::LEVEL, 16, 63, 45},
    {8, Ui::SpeciesName::WARTORTLE, TypeId::WATER, TypeId::NONE, {59, 63, 80, 65, 80, 58}, GrowthRate::PARABOLIC, EV(0, 0, 1, 0, 1, 0), 9, EvolutionMethod::LEVEL, 36, 142, 45},
    {9, Ui::SpeciesName::BLASTOISE, TypeId::WATER, TypeId::NONE, {79, 83, 100, 85, 105, 78}, GrowthRate::PARABOLIC, EV(0, 0, 0, 0, 3, 0), 0, EvolutionMethod::NONE, 0, 239, 45},
    {10, Ui::SpeciesName::CATERPIE, TypeId::BUG, TypeId::NONE, {45, 30, 35, 20, 20, 45}, GrowthRate::MEDIUM, EV(0, 0, 0, 0, 0, 1), 11, EvolutionMethod::LEVEL, 7, 39, 255},
    {11, Ui::SpeciesName::METAPOD, TypeId::BUG, TypeId::NONE, {50, 20, 55, 25, 25, 30}, GrowthRate::MEDIUM, EV(0, 0, 2, 0, 0, 0), 12, EvolutionMethod::LEVEL, 10, 72, 120},
    {12, Ui::SpeciesName::BUTTERFREE, TypeId::BUG, TypeId::FLYING, {60, 45, 50, 90, 80, 70}, GrowthRate::MEDIUM, EV(0, 0, 0, 2, 1, 0), 0, EvolutionMethod::NONE, 0, 173, 45},
    {16, Ui::SpeciesName::PIDGEY, TypeId::NORMAL, TypeId::FLYING, {40, 45, 40, 35, 35, 56}, GrowthRate::PARABOLIC, EV(0, 0, 0, 0, 0, 1), 17, EvolutionMethod::LEVEL, 18, 50, 255},
    {17, Ui::SpeciesName::PIDGEOTTO, TypeId::NORMAL, TypeId::FLYING, {63, 60, 55, 50, 50, 71}, GrowthRate::PARABOLIC, EV(0, 0, 0, 0, 0, 2), 18, EvolutionMethod::LEVEL, 36, 122, 120},
    {18, Ui::SpeciesName::PIDGEOT, TypeId::NORMAL, TypeId::FLYING, {83, 80, 75, 70, 70, 101}, GrowthRate::PARABOLIC, EV(0, 0, 0, 0, 0, 3), 0, EvolutionMethod::NONE, 0, 211, 45},
    {151, Ui::SpeciesName::MEW, TypeId::PSYCHIC, TypeId::NONE, {100, 100, 100, 100, 100, 100}, GrowthRate::PARABOLIC, EV(3, 0, 0, 0, 0, 0), 0, EvolutionMethod::NONE, 0, 270, 45},
    {161, Ui::SpeciesName::SENTRET, TypeId::NORMAL, TypeId::NONE, {35, 46, 34, 35, 45, 20}, GrowthRate::MEDIUM, EV(0, 1, 0, 0, 0, 0), 162, EvolutionMethod::LEVEL, 15, 43, 255},
    {162, Ui::SpeciesName::FURRET, TypeId::NORMAL, TypeId::NONE, {85, 76, 64, 45, 55, 90}, GrowthRate::MEDIUM, EV(0, 0, 0, 0, 0, 2), 0, EvolutionMethod::NONE, 0, 145, 90},
    {298, Ui::SpeciesName::AZURILL, TypeId::NORMAL, TypeId::FAIRY, {50, 20, 40, 20, 40, 20}, GrowthRate::FAST, EV(1, 0, 0, 0, 0, 0), 183, EvolutionMethod::FRIENDSHIP, 0, 38, 150},
    {183, Ui::SpeciesName::MARILL, TypeId::WATER, TypeId::FAIRY, {70, 20, 50, 20, 50, 40}, GrowthRate::FAST, EV(2, 0, 0, 0, 0, 0), 184, EvolutionMethod::LEVEL, 18, 88, 190},
    {184, Ui::SpeciesName::AZUMARILL, TypeId::WATER, TypeId::FAIRY, {100, 50, 80, 60, 80, 50}, GrowthRate::FAST, EV(3, 0, 0, 0, 0, 0), 0, EvolutionMethod::NONE, 0, 189, 75},
    {194, Ui::SpeciesName::WOOPER, TypeId::WATER, TypeId::GROUND, {55, 45, 45, 25, 25, 15}, GrowthRate::MEDIUM, EV(1, 0, 0, 0, 0, 0), 195, EvolutionMethod::LEVEL, 20, 42, 255},
    {195, Ui::SpeciesName::QUAGSIRE, TypeId::WATER, TypeId::GROUND, {95, 85, 85, 65, 65, 35}, GrowthRate::MEDIUM, EV(2, 0, 0, 0, 0, 0), 0, EvolutionMethod::NONE, 0, 151, 90},
    {285, Ui::SpeciesName::SHROOMISH, TypeId::GRASS, TypeId::NONE, {60, 40, 60, 40, 60, 35}, GrowthRate::FLUCTUATING, EV(1, 0, 0, 0, 0, 0), 286, EvolutionMethod::LEVEL, 23, 59, 255},
    {286, Ui::SpeciesName::BRELOOM, TypeId::GRASS, TypeId::FIGHTING, {60, 130, 80, 60, 60, 70}, GrowthRate::FLUCTUATING, EV(0, 2, 0, 0, 0, 0), 0, EvolutionMethod::NONE, 0, 161, 90},
    {322, Ui::SpeciesName::NUMEL, TypeId::FIRE, TypeId::GROUND, {60, 60, 40, 65, 45, 35}, GrowthRate::MEDIUM, EV(0, 0, 0, 1, 0, 0), 323, EvolutionMethod::LEVEL, 33, 61, 255},
    {323, Ui::SpeciesName::CAMERUPT, TypeId::FIRE, TypeId::GROUND, {70, 100, 70, 105, 75, 40}, GrowthRate::MEDIUM, EV(0, 1, 0, 1, 0, 0), 0, EvolutionMethod::NONE, 0, 161, 150},
    {361, Ui::SpeciesName::SNORUNT, TypeId::ICE, TypeId::NONE, {50, 50, 50, 50, 50, 50}, GrowthRate::MEDIUM, EV(1, 0, 0, 0, 0, 0), 362, EvolutionMethod::LEVEL, 42, 60, 190},
    {362, Ui::SpeciesName::GLALIE, TypeId::ICE, TypeId::NONE, {80, 80, 80, 80, 80, 80}, GrowthRate::MEDIUM, EV(2, 0, 0, 0, 0, 0), 0, EvolutionMethod::NONE, 0, 168, 75},
    {280, Ui::SpeciesName::RALTS, TypeId::PSYCHIC, TypeId::FAIRY, {28, 25, 25, 45, 35, 40}, GrowthRate::SLOW, EV(0, 0, 0, 1, 0, 0), 281, EvolutionMethod::LEVEL, 20, 40, 235},
    {281, Ui::SpeciesName::KIRLIA, TypeId::PSYCHIC, TypeId::FAIRY, {38, 35, 35, 65, 55, 50}, GrowthRate::SLOW, EV(0, 0, 0, 2, 0, 0), 282, EvolutionMethod::LEVEL, 30, 97, 120},
    {282, Ui::SpeciesName::GARDEVOIR, TypeId::PSYCHIC, TypeId::FAIRY, {68, 65, 65, 125, 115, 80}, GrowthRate::SLOW, EV(0, 0, 0, 3, 0, 0), 0, EvolutionMethod::NONE, 0, 233, 45},
    {41, Ui::SpeciesName::ZUBAT, TypeId::POISON, TypeId::FLYING, {40, 45, 35, 30, 40, 55}, GrowthRate::MEDIUM, EV(0, 0, 0, 0, 0, 1), 42, EvolutionMethod::LEVEL, 22, 49, 255},
    {42, Ui::SpeciesName::GOLBAT, TypeId::POISON, TypeId::FLYING, {75, 80, 70, 65, 75, 90}, GrowthRate::MEDIUM, EV(0, 0, 0, 0, 0, 2), 169, EvolutionMethod::FRIENDSHIP, 0, 159, 90},
    {169, Ui::SpeciesName::CROBAT, TypeId::POISON, TypeId::FLYING, {85, 90, 80, 70, 80, 130}, GrowthRate::MEDIUM, EV(0, 0, 0, 0, 0, 3), 0, EvolutionMethod::NONE, 0, 241, 90},
    {261, Ui::SpeciesName::POOCHYENA, TypeId::DARK, TypeId::NONE, {35, 55, 35, 30, 30, 35}, GrowthRate::MEDIUM, EV(0, 1, 0, 0, 0, 0), 262, EvolutionMethod::LEVEL, 18, 44, 255},
    {262, Ui::SpeciesName::MIGHTYENA, TypeId::DARK, TypeId::NONE, {70, 90, 70, 60, 60, 70}, GrowthRate::MEDIUM, EV(0, 2, 0, 0, 0, 0), 0, EvolutionMethod::NONE, 0, 147, 127},
    {278, Ui::SpeciesName::WINGULL, TypeId::WATER, TypeId::FLYING, {40, 30, 30, 55, 30, 85}, GrowthRate::MEDIUM, EV(0, 0, 0, 0, 0, 1), 279, EvolutionMethod::LEVEL, 25, 54, 190},
    {279, Ui::SpeciesName::PELIPPER, TypeId::WATER, TypeId::FLYING, {60, 50, 100, 95, 70, 65}, GrowthRate::MEDIUM, EV(0, 0, 2, 0, 0, 0), 0, EvolutionMethod::NONE, 0, 151, 45},
    {172, Ui::SpeciesName::PICHU, TypeId::ELECTRIC, TypeId::NONE, {20, 40, 15, 35, 35, 60}, GrowthRate::MEDIUM, EV(0, 0, 0, 0, 0, 1), 25, EvolutionMethod::FRIENDSHIP, 0, 41, 190},
    {25, Ui::SpeciesName::PIKACHU, TypeId::ELECTRIC, TypeId::NONE, {35, 55, 40, 50, 50, 90}, GrowthRate::MEDIUM, EV(0, 0, 0, 0, 0, 2), 26, EvolutionMethod::STONE, 0, 112, 190},
    {26, Ui::SpeciesName::RAICHU, TypeId::ELECTRIC, TypeId::NONE, {60, 90, 55, 90, 80, 110}, GrowthRate::MEDIUM, EV(0, 0, 0, 0, 0, 3), 0, EvolutionMethod::NONE, 0, 214, 75},
    {74, Ui::SpeciesName::GEODUDE, TypeId::ROCK, TypeId::GROUND, {40, 80, 100, 30, 30, 20}, GrowthRate::PARABOLIC, EV(0, 0, 1, 0, 0, 0), 75, EvolutionMethod::LEVEL, 25, 60, 255},
    {75, Ui::SpeciesName::GRAVELER, TypeId::ROCK, TypeId::GROUND, {55, 95, 115, 45, 45, 35}, GrowthRate::PARABOLIC, EV(0, 0, 2, 0, 0, 0), 76, EvolutionMethod::TRADE, 0, 137, 120},
    {76, Ui::SpeciesName::GOLEM, TypeId::ROCK, TypeId::GROUND, {80, 120, 130, 55, 65, 45}, GrowthRate::PARABOLIC, EV(0, 0, 3, 0, 0, 0), 0, EvolutionMethod::NONE, 0, 218, 45},
    {133, Ui::SpeciesName::EEVEE, TypeId::NORMAL, TypeId::NONE, {55, 55, 50, 45, 65, 55}, GrowthRate::MEDIUM, EV(0, 0, 0, 0, 1, 0), 134, EvolutionMethod::BRANCH, 0, 65, 45},
    {134, Ui::SpeciesName::VAPOREON, TypeId::WATER, TypeId::NONE, {130, 65, 60, 110, 95, 65}, GrowthRate::MEDIUM, EV(2, 0, 0, 0, 0, 0), 0, EvolutionMethod::NONE, 0, 184, 45},
    {135, Ui::SpeciesName::JOLTEON, TypeId::ELECTRIC, TypeId::NONE, {65, 65, 60, 110, 95, 130}, GrowthRate::MEDIUM, EV(0, 0, 0, 0, 0, 2), 0, EvolutionMethod::NONE, 0, 184, 45},
    {136, Ui::SpeciesName::FLAREON, TypeId::FIRE, TypeId::NONE, {65, 130, 60, 95, 110, 65}, GrowthRate::MEDIUM, EV(0, 2, 0, 0, 0, 0), 0, EvolutionMethod::NONE, 0, 184, 45},
    {196, Ui::SpeciesName::ESPEON, TypeId::PSYCHIC, TypeId::NONE, {65, 65, 60, 130, 95, 110}, GrowthRate::MEDIUM, EV(0, 0, 0, 2, 0, 0), 0, EvolutionMethod::NONE, 0, 184, 45},
    {197, Ui::SpeciesName::UMBREON, TypeId::DARK, TypeId::NONE, {95, 65, 110, 60, 130, 65}, GrowthRate::MEDIUM, EV(0, 0, 0, 0, 2, 0), 0, EvolutionMethod::NONE, 0, 184, 45},
    {380, Ui::SpeciesName::LATIAS, TypeId::DRAGON, TypeId::PSYCHIC, {80, 80, 90, 110, 130, 110}, GrowthRate::SLOW, EV(0, 0, 0, 0, 3, 0), 0, EvolutionMethod::NONE, 0, 270, 3},
    {381, Ui::SpeciesName::LATIOS, TypeId::DRAGON, TypeId::PSYCHIC, {80, 90, 80, 130, 110, 110}, GrowthRate::SLOW, EV(0, 0, 0, 3, 0, 0), 0, EvolutionMethod::NONE, 0, 270, 3},
    {123, Ui::SpeciesName::SCYTHER, TypeId::BUG, TypeId::FLYING, {70, 110, 80, 55, 80, 105}, GrowthRate::MEDIUM, EV(0, 1, 0, 0, 0, 0), 212, EvolutionMethod::TRADE, 0, 100, 45},
    {212, Ui::SpeciesName::SCIZOR, TypeId::BUG, TypeId::STEEL, {70, 130, 100, 55, 80, 65}, GrowthRate::MEDIUM, EV(0, 2, 0, 0, 0, 0), 0, EvolutionMethod::NONE, 0, 175, 25},
    {92, Ui::SpeciesName::GASTLY, TypeId::GHOST, TypeId::POISON, {30, 35, 30, 100, 35, 80}, GrowthRate::PARABOLIC, EV(0, 0, 0, 1, 0, 0), 93, EvolutionMethod::LEVEL, 25, 62, 190},
    {93, Ui::SpeciesName::HAUNTER, TypeId::GHOST, TypeId::POISON, {45, 50, 45, 115, 55, 95}, GrowthRate::PARABOLIC, EV(0, 0, 0, 2, 0, 0), 94, EvolutionMethod::TRADE, 0, 142, 90},
    {94, Ui::SpeciesName::GENGAR, TypeId::GHOST, TypeId::POISON, {60, 65, 60, 130, 75, 110}, GrowthRate::PARABOLIC, EV(0, 0, 0, 3, 0, 0), 0, EvolutionMethod::NONE, 0, 225, 45},
    {129, Ui::SpeciesName::MAGIKARP, TypeId::WATER, TypeId::NONE, {20, 10, 55, 15, 20, 80}, GrowthRate::SLOW, EV(0, 0, 0, 0, 0, 1), 130, EvolutionMethod::LEVEL, 20, 40, 255},
    {130, Ui::SpeciesName::GYARADOS, TypeId::WATER, TypeId::FLYING, {95, 125, 79, 60, 100, 81}, GrowthRate::SLOW, EV(0, 2, 0, 0, 0, 0), 0, EvolutionMethod::NONE, 0, 189, 45},
    {143, Ui::SpeciesName::SNORLAX, TypeId::NORMAL, TypeId::NONE, {160, 110, 65, 65, 110, 30}, GrowthRate::SLOW, EV(2, 0, 0, 0, 0, 0), 0, EvolutionMethod::NONE, 0, 189, 25},
    {147, Ui::SpeciesName::DRATINI, TypeId::DRAGON, TypeId::NONE, {41, 64, 45, 50, 50, 50}, GrowthRate::SLOW, EV(0, 1, 0, 0, 0, 0), 148, EvolutionMethod::LEVEL, 30, 60, 45},
    {148, Ui::SpeciesName::DRAGONAIR, TypeId::DRAGON, TypeId::NONE, {61, 84, 65, 70, 70, 70}, GrowthRate::SLOW, EV(0, 2, 0, 0, 0, 0), 149, EvolutionMethod::LEVEL, 50, 147, 45},
    {149, Ui::SpeciesName::DRAGONITE, TypeId::DRAGON, TypeId::FLYING, {91, 134, 95, 100, 100, 80}, GrowthRate::SLOW, EV(0, 3, 0, 0, 0, 0), 0, EvolutionMethod::NONE, 0, 270, 45},
};

#include "game/OfficialMoveData.inc"

const char* const NATURE_NAMES[Game::NATURE_COUNT] = {
    Ui::NatureName::HARDY,
    Ui::NatureName::LONELY,
    Ui::NatureName::BRAVE,
    Ui::NatureName::ADAMANT,
    Ui::NatureName::NAUGHTY,
    Ui::NatureName::BOLD,
    Ui::NatureName::DOCILE,
    Ui::NatureName::RELAXED,
    Ui::NatureName::IMPISH,
    Ui::NatureName::LAX,
    Ui::NatureName::TIMID,
    Ui::NatureName::HASTY,
    Ui::NatureName::SERIOUS,
    Ui::NatureName::JOLLY,
    Ui::NatureName::NAIVE,
    Ui::NatureName::MODEST,
    Ui::NatureName::MILD,
    Ui::NatureName::QUIET,
    Ui::NatureName::BASHFUL,
    Ui::NatureName::RASH,
    Ui::NatureName::CALM,
    Ui::NatureName::GENTLE,
    Ui::NatureName::SASSY,
    Ui::NatureName::CAREFUL,
    Ui::NatureName::QUIRKY,
};

const uint8_t NATURE_BOOST[Game::NATURE_COUNT] = {
    1, 1, 1, 1, 1,
    2, 2, 2, 2, 2,
    5, 5, 5, 5, 5,
    3, 3, 3, 3, 3,
    4, 4, 4, 4, 4,
};

const uint8_t NATURE_LOWER[Game::NATURE_COUNT] = {
    1, 2, 5, 3, 4,
    1, 2, 5, 3, 4,
    1, 2, 5, 3, 4,
    1, 2, 5, 3, 4,
    1, 2, 5, 3, 4,
};
}

const Species& starterSpecies() {
    return SPECIES[0];
}

const Species* speciesTable() {
    return SPECIES;
}

uint8_t speciesCount() {
    return sizeof(SPECIES) / sizeof(SPECIES[0]);
}

const Species* findSpecies(uint16_t speciesId) {
    for (const Species& species : SPECIES) {
        if (species.id == speciesId) return &species;
    }
    return nullptr;
}

const Species* levelUpEvolutionTarget(const Species& species,
                                      const Game::MonsterRuntime& monster) {
    if (species.evolveTo == 0 || species.evolveTo == species.id) return nullptr;

    bool ready = false;
    switch (species.evolveMethod) {
    case EvolutionMethod::LEVEL:
        ready = species.evolveLevel > 0 && monster.level >= species.evolveLevel;
        break;
    case EvolutionMethod::FRIENDSHIP:
        ready = monster.affection >= FRIENDSHIP_EVOLUTION_THRESHOLD;
        break;
    default:
        break;
    }
    return ready ? findSpecies(species.evolveTo) : nullptr;
}

const MoveInfo* findMove(Game::MoveId moveId) {
    size_t low = 0;
    size_t high = OFFICIAL_MOVE_COUNT;
    while (low < high) {
        const size_t mid = low + (high - low) / 2;
        if (OFFICIAL_MOVES[mid].id < moveId) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    return low < OFFICIAL_MOVE_COUNT && OFFICIAL_MOVES[low].id == moveId
        ? &OFFICIAL_MOVES[low]
        : nullptr;
}

const MoveEffectSpec* moveEffectFor(const MoveInfo& move, uint8_t index) {
    if (index >= move.effectCount) return nullptr;
    const size_t absoluteIndex = static_cast<size_t>(move.effectOffset) + index;
    return absoluteIndex < OFFICIAL_MOVE_EFFECT_COUNT
        ? &OFFICIAL_MOVE_EFFECTS[absoluteIndex]
        : nullptr;
}

const SpeciesLearnset* findLearnset(uint16_t speciesId) {
    size_t low = 0;
    size_t high = OFFICIAL_LEARNSET_COUNT;
    while (low < high) {
        const size_t mid = low + (high - low) / 2;
        if (OFFICIAL_LEARNSETS[mid].speciesId < speciesId) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    return low < OFFICIAL_LEARNSET_COUNT && OFFICIAL_LEARNSETS[low].speciesId == speciesId
        ? &OFFICIAL_LEARNSETS[low]
        : nullptr;
}

uint16_t learnsetEntryCountForSpecies(const Species& species) {
    const SpeciesLearnset* learnset = findLearnset(species.id);
    return learnset ? learnset->count : 0;
}

const LearnsetEntry* learnsetEntryForSpecies(const Species& species, uint16_t index) {
    const SpeciesLearnset* learnset = findLearnset(species.id);
    if (!learnset || index >= learnset->count) return nullptr;
    const size_t absoluteIndex = static_cast<size_t>(learnset->offset) + index;
    return absoluteIndex < OFFICIAL_LEARNSET_ENTRY_COUNT
        ? &OFFICIAL_LEARNSET_ENTRIES[absoluteIndex]
        : nullptr;
}

bool isMoveBattleSupported(Game::MoveId moveId) {
    const MoveInfo* move = findMove(moveId);
    return move && move->battleSupported;
}

Game::MoveId basicMoveIdForSpecies(const Species& species) {
    const SpeciesLearnset* learnset = findLearnset(species.id);
    if (learnset && findMove(learnset->basicMoveId)) return learnset->basicMoveId;
    return findMove(33) ? 33 : 0;
}

bool isBasicFirstMoveForSpecies(const Species& species, Game::MoveId moveId) {
    return moveId != 0 && moveId == basicMoveIdForSpecies(species);
}

uint8_t moveLearnLevelForSpecies(const Species& species, Game::MoveId moveId) {
    if (moveId == 0) return 0;
    const uint16_t count = learnsetEntryCountForSpecies(species);
    for (uint16_t index = 0; index < count; ++index) {
        const LearnsetEntry* entry = learnsetEntryForSpecies(species, index);
        if (entry && entry->moveId == moveId) return entry->level;
    }
    return 0;
}

bool canLearnAsSpecialMove(const Species& species, Game::MoveId moveId) {
    return moveId != 0 &&
           !isBasicFirstMoveForSpecies(species, moveId) &&
           moveLearnLevelForSpecies(species, moveId) > 0 &&
           isMoveBattleSupported(moveId);
}

namespace {
bool canRetainSpecialMoveFromLine(const Species& species, Game::MoveId moveId,
                                  uint8_t level, uint8_t depth) {
    uint8_t learnLevel = moveLearnLevelForSpecies(species, moveId);
    if (learnLevel > 0 && learnLevel <= level && canLearnAsSpecialMove(species, moveId)) {
        return true;
    }
    if (depth >= speciesCount()) return false;

    for (uint8_t index = 0; index < speciesCount(); ++index) {
        const Species& candidate = speciesTable()[index];
        if (candidate.evolveTo == species.id && candidate.id != species.id &&
            canRetainSpecialMoveFromLine(candidate, moveId, level, depth + 1)) {
            return true;
        }
    }
    return false;
}
}

bool canRetainSpecialMove(const Species& species, Game::MoveId moveId, uint8_t level) {
    return moveId != 0 && isMoveBattleSupported(moveId) &&
           canRetainSpecialMoveFromLine(species, moveId, level, 0);
}

void resetMovesForLevel(Game::MonsterRuntime& monster, const Species& species) {
    monster.move1Id = basicMoveIdForSpecies(species);
    monster.move2Id = 0;
    monster.move3Id = 0;
    for (uint8_t slot = 0; slot < Game::MOVE_SLOT_COUNT; ++slot) {
        monster.moveProficiency[slot] = 0;
    }
    monster.moveProficiency[0] = Game::MOVE_PROFICIENCY_MAX;

    const uint16_t count = learnsetEntryCountForSpecies(species);
    for (uint16_t index = 0; index < count; ++index) {
        const LearnsetEntry* entry = learnsetEntryForSpecies(species, index);
        if (!entry || entry->level > monster.level) break;
        if (!canLearnAsSpecialMove(species, entry->moveId) ||
            entry->moveId == monster.move2Id ||
            entry->moveId == monster.move3Id) {
            continue;
        }
        if (monster.move2Id == 0) {
            monster.move2Id = entry->moveId;
        } else if (monster.move3Id == 0) {
            monster.move3Id = entry->moveId;
        } else {
            monster.move3Id = entry->moveId;
        }
    }
}

Game::MoveId specialMoveIdForMonster(const Game::MonsterRuntime& monster, uint8_t specialSlot) {
    Game::MoveId moveId = 0;
    if (specialSlot == 0) moveId = monster.move2Id;
    else if (specialSlot == 1) moveId = monster.move3Id;
    return isMoveBattleSupported(moveId) ? moveId : 0;
}

uint8_t specialMoveCount(const Game::MonsterRuntime& monster) {
    uint8_t count = 0;
    if (specialMoveIdForMonster(monster, 0) != 0) ++count;
    if (specialMoveIdForMonster(monster, 1) != 0) ++count;
    return count;
}

Game::MoveId moveIdForMonster(
    const Species& species,
    const Game::MonsterRuntime& monster,
    bool secondSlot
) {
    if (secondSlot) return specialMoveIdForMonster(monster, 0);
    return isBasicFirstMoveForSpecies(species, monster.move1Id)
        ? monster.move1Id
        : basicMoveIdForSpecies(species);
}

Game::MoveId moveIdForMonster(
    const Species& species,
    const Game::MonsterRuntime& monster,
    uint8_t specialSlot
) {
    if (specialSlot < SPECIAL_MOVE_SLOT_COUNT) {
        return specialMoveIdForMonster(monster, specialSlot);
    }
    return moveIdForMonster(species, monster, false);
}

bool hasSpecialMove(const Game::MonsterRuntime& monster) {
    return specialMoveCount(monster) > 0;
}

bool hasSecondMove(const Game::MonsterRuntime& monster) {
    return hasSpecialMove(monster);
}

uint16_t movePower(Game::MoveId moveId, uint16_t fallback) {
    const MoveInfo* move = findMove(moveId);
    return move ? move->power : fallback;
}
uint16_t maxHpFor(const Species& species, uint8_t level) {
    return (uint16_t)((species.stats.hp * 2 * level) / 100) + level + 10;
}

namespace {
uint8_t baseStatAt(const BaseStats& stats, uint8_t index) {
    switch (index) {
    case 0: return stats.hp;
    case 1: return stats.atk;
    case 2: return stats.def;
    case 3: return stats.spa;
    case 4: return stats.spd;
    case 5: return stats.spe;
    default: return 1;
    }
}
}

uint16_t maxHpFor(const Species& species, const Game::MonsterRuntime& monster) {
    uint16_t base = species.stats.hp;
    uint16_t iv = Game::ivAt(monster.ivPacked, 0);
    uint16_t ev = Game::evAt(monster.ev, 0);
    return (uint16_t)(((base * 2 + iv + ev / 4) * monster.level) / 100) + monster.level + 10;
}

uint16_t statFor(const Species& species, const Game::MonsterRuntime& monster, uint8_t statIndex) {
    if (statIndex == 0) return maxHpFor(species, monster);
    uint16_t base = baseStatAt(species.stats, statIndex);
    uint16_t iv = Game::ivAt(monster.ivPacked, statIndex);
    uint16_t ev = Game::evAt(monster.ev, statIndex);
    uint16_t stat = (uint16_t)(((base * 2 + iv + ev / 4) * monster.level) / 100) + 5;
    if (statIndex == natureBoostStat(monster.nature) && statIndex != natureLowerStat(monster.nature)) {
        stat = (stat * 110) / 100;
    } else if (statIndex == natureLowerStat(monster.nature) && statIndex != natureBoostStat(monster.nature)) {
        stat = (stat * 90) / 100;
    }
    return stat;
}

uint8_t evYieldAt(const Species& species, uint8_t statIndex) {
    if (statIndex >= Game::STAT_COUNT) return 0;
    return (species.evYieldPacked >> (statIndex * 2)) & 0x3;
}

uint32_t minimumExpForLevel(GrowthRate growthRate, uint8_t level) {
    if (level <= 1) return 0;
    if (level > Game::LEVEL_MAX) level = Game::LEVEL_MAX;

    const uint64_t n = level;
    const uint64_t n2 = n * n;
    const uint64_t n3 = n2 * n;

    switch (growthRate) {
    case GrowthRate::ERRATIC:
        if (n <= 50) return static_cast<uint32_t>((n3 * (100 - n)) / 50);
        if (n <= 68) return static_cast<uint32_t>((n3 * (150 - n)) / 100);
        if (n <= 98) return static_cast<uint32_t>((n3 * ((1911 - 10 * n) / 3)) / 500);
        return static_cast<uint32_t>((n3 * (160 - n)) / 100);
    case GrowthRate::FLUCTUATING:
        if (n <= 15) return static_cast<uint32_t>((n3 * (24 + ((n + 1) / 3))) / 50);
        if (n <= 35) return static_cast<uint32_t>((n3 * (14 + n)) / 50);
        return static_cast<uint32_t>((n3 * (32 + (n / 2))) / 50);
    case GrowthRate::PARABOLIC: {
        const int64_t exp = static_cast<int64_t>((6 * n3) / 5) -
                            static_cast<int64_t>(15 * n2) +
                            static_cast<int64_t>(100 * n) - 140;
        return exp > 0 ? static_cast<uint32_t>(exp) : 0;
    }
    case GrowthRate::FAST:
        return static_cast<uint32_t>((4 * n3) / 5);
    case GrowthRate::SLOW:
        return static_cast<uint32_t>((5 * n3) / 4);
    case GrowthRate::MEDIUM:
    default:
        return static_cast<uint32_t>(n3);
    }
}

uint8_t levelForExp(GrowthRate growthRate, uint32_t exp) {
    for (uint16_t level = 2; level <= Game::LEVEL_MAX; ++level) {
        if (exp < minimumExpForLevel(growthRate, static_cast<uint8_t>(level))) {
            return static_cast<uint8_t>(level - 1);
        }
    }
    return Game::LEVEL_MAX;
}

uint32_t expToNextLevel(GrowthRate growthRate, uint8_t level, uint32_t exp) {
    if (level >= Game::LEVEL_MAX) return 0;
    uint32_t next = minimumExpForLevel(growthRate, level + 1);
    return exp < next ? next - exp : 0;
}

const char* natureName(uint8_t nature) {
    if (nature >= Game::NATURE_COUNT) nature = 0;
    return NATURE_NAMES[nature];
}

uint8_t natureBoostStat(uint8_t nature) {
    if (nature >= Game::NATURE_COUNT) nature = 0;
    return NATURE_BOOST[nature];
}

uint8_t natureLowerStat(uint8_t nature) {
    if (nature >= Game::NATURE_COUNT) nature = 0;
    return NATURE_LOWER[nature];
}

int8_t natureLikedFoodIndex(uint8_t nature) {
    if (nature >= Game::NATURE_COUNT) nature = 0;
    if (NATURE_BOOST[nature] == NATURE_LOWER[nature]) return -1; // 无修正性格
    return FoodTuning::STAT_TO_FOOD_INDEX[NATURE_BOOST[nature]];
}

int8_t natureDislikedFoodIndex(uint8_t nature) {
    if (nature >= Game::NATURE_COUNT) nature = 0;
    if (NATURE_BOOST[nature] == NATURE_LOWER[nature]) return -1;
    return FoodTuning::STAT_TO_FOOD_INDEX[NATURE_LOWER[nature]];
}

const char* typeName(TypeId type) {
    switch (type) {
    case TypeId::NORMAL: return Ui::Type::NORMAL;
    case TypeId::FIRE: return Ui::Type::FIRE;
    case TypeId::WATER: return Ui::Type::WATER;
    case TypeId::GRASS: return Ui::Type::GRASS;
    case TypeId::ELECTRIC: return Ui::Type::ELECTRIC;
    case TypeId::ICE: return Ui::Type::ICE;
    case TypeId::FIGHTING: return Ui::Type::FIGHTING;
    case TypeId::POISON: return Ui::Type::POISON;
    case TypeId::GROUND: return Ui::Type::GROUND;
    case TypeId::FLYING: return Ui::Type::FLYING;
    case TypeId::PSYCHIC: return Ui::Type::PSYCHIC;
    case TypeId::BUG: return Ui::Type::BUG;
    case TypeId::ROCK: return Ui::Type::ROCK;
    case TypeId::GHOST: return Ui::Type::GHOST;
    case TypeId::DRAGON: return Ui::Type::DRAGON;
    case TypeId::DARK: return Ui::Type::DARK;
    case TypeId::STEEL: return Ui::Type::STEEL;
    case TypeId::FAIRY: return Ui::Type::FAIRY;
    default: return Ui::Type::NONE;
    }
}

const char* evolutionMethodName(EvolutionMethod method) {
    switch (method) {
    case EvolutionMethod::LEVEL: return Ui::Status::EVOLVE_LEVEL;
    case EvolutionMethod::FRIENDSHIP: return Ui::Status::EVOLVE_FRIENDSHIP;
    case EvolutionMethod::STONE: return Ui::Status::EVOLVE_STONE;
    case EvolutionMethod::TRADE: return Ui::Status::EVOLVE_TRADE;
    case EvolutionMethod::BRANCH: return Ui::Status::EVOLVE_BRANCH;
    default: return Ui::Status::NO_EVOLVE;
    }
}
