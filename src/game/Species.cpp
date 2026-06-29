#include "game/Species.h"
#include "core/UiStrings.h"
#include "hardware/PixelRenderer.h"

namespace {
constexpr uint16_t C(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

constexpr uint16_t EV(uint8_t hp, uint8_t atk, uint8_t def, uint8_t spa, uint8_t spd, uint8_t spe) {
    return (hp & 0x3) | ((atk & 0x3) << 2) | ((def & 0x3) << 4) |
           ((spa & 0x3) << 6) | ((spd & 0x3) << 8) | ((spe & 0x3) << 10);
}

const Species SPECIES[] = {
    {906, Ui::SpeciesName::SPRIGATITO, TypeId::GRASS, TypeId::NONE, {40, 61, 54, 45, 45, 65}, EV(0, 0, 0, 0, 0, 1), 907, EvolutionMethod::LEVEL, 16, C(65, 185, 88), C(190, 236, 126), 1, 2},
    {907, Ui::SpeciesName::FLORAGATO, TypeId::GRASS, TypeId::NONE, {61, 80, 63, 60, 63, 83}, EV(0, 0, 0, 0, 0, 2), 908, EvolutionMethod::LEVEL, 36, C(34, 150, 74), C(106, 221, 114), 1, 2},
    {908, Ui::SpeciesName::MEOWSCARADA, TypeId::GRASS, TypeId::DARK, {76, 110, 70, 81, 70, 123}, EV(0, 0, 0, 0, 0, 3), 0, EvolutionMethod::NONE, 0, C(28, 122, 70), C(126, 57, 178), 1, 3},
    {4, Ui::SpeciesName::CHARMANDER, TypeId::FIRE, TypeId::NONE, {39, 52, 43, 60, 50, 65}, EV(0, 0, 0, 0, 0, 1), 5, EvolutionMethod::LEVEL, 16, C(244, 118, 47), C(255, 204, 83), 4, 5},
    {5, Ui::SpeciesName::CHARMELEON, TypeId::FIRE, TypeId::NONE, {58, 64, 58, 80, 65, 80}, EV(0, 0, 0, 1, 0, 1), 6, EvolutionMethod::LEVEL, 36, C(218, 72, 44), C(255, 170, 60), 4, 5},
    {6, Ui::SpeciesName::CHARIZARD, TypeId::FIRE, TypeId::FLYING, {78, 84, 78, 109, 85, 100}, EV(0, 0, 0, 3, 0, 0), 0, EvolutionMethod::NONE, 0, C(198, 62, 39), C(69, 142, 219), 4, 6},
    {656, Ui::SpeciesName::FROAKIE, TypeId::WATER, TypeId::NONE, {41, 56, 40, 62, 44, 71}, EV(0, 0, 0, 0, 0, 1), 657, EvolutionMethod::LEVEL, 16, C(70, 173, 224), C(202, 239, 255), 7, 8},
    {657, Ui::SpeciesName::FROGADIER, TypeId::WATER, TypeId::NONE, {54, 63, 52, 83, 56, 97}, EV(0, 0, 0, 0, 0, 2), 658, EvolutionMethod::LEVEL, 36, C(34, 111, 198), C(157, 216, 255), 7, 8},
    {658, Ui::SpeciesName::GRENINJA, TypeId::WATER, TypeId::DARK, {72, 95, 67, 103, 71, 122}, EV(0, 0, 0, 0, 0, 3), 0, EvolutionMethod::NONE, 0, C(28, 77, 153), C(126, 57, 178), 7, 9},
    {172, Ui::SpeciesName::PICHU, TypeId::ELECTRIC, TypeId::NONE, {20, 40, 15, 35, 35, 60}, EV(0, 0, 0, 0, 0, 1), 25, EvolutionMethod::FRIENDSHIP, 0, C(255, 219, 56), C(92, 62, 42), 10, 11},
    {25, Ui::SpeciesName::PIKACHU, TypeId::ELECTRIC, TypeId::NONE, {35, 55, 40, 50, 50, 90}, EV(0, 0, 0, 0, 0, 2), 26, EvolutionMethod::STONE, 0, C(255, 211, 43), C(120, 76, 38), 10, 11},
    {26, Ui::SpeciesName::RAICHU, TypeId::ELECTRIC, TypeId::NONE, {60, 90, 55, 90, 80, 110}, EV(0, 0, 0, 0, 0, 3), 0, EvolutionMethod::NONE, 0, C(235, 139, 48), C(255, 225, 86), 10, 12},
    {133, Ui::SpeciesName::EEVEE, TypeId::NORMAL, TypeId::NONE, {55, 55, 50, 45, 65, 55}, EV(0, 0, 0, 0, 1, 0), 0, EvolutionMethod::BRANCH, 0, C(156, 102, 55), C(239, 211, 155), 13, 14},
    {92, Ui::SpeciesName::GASTLY, TypeId::GHOST, TypeId::POISON, {30, 35, 30, 100, 35, 80}, EV(0, 0, 0, 1, 0, 0), 93, EvolutionMethod::LEVEL, 25, C(117, 70, 167), C(65, 39, 92), 15, 16},
    {93, Ui::SpeciesName::HAUNTER, TypeId::GHOST, TypeId::POISON, {45, 50, 45, 115, 55, 95}, EV(0, 0, 0, 2, 0, 0), 94, EvolutionMethod::TRADE, 0, C(82, 47, 137), C(173, 103, 220), 15, 16},
    {94, Ui::SpeciesName::GENGAR, TypeId::GHOST, TypeId::POISON, {60, 65, 60, 130, 75, 110}, EV(0, 0, 0, 3, 0, 0), 0, EvolutionMethod::NONE, 0, C(55, 38, 89), C(139, 81, 184), 15, 17},
};

const MoveInfo MOVES[] = {
    {1, Ui::MoveName::LEAF_SCRATCH, Ui::MoveDesc::LEAF_SCRATCH, 35, TypeId::GRASS, false},
    {2, Ui::MoveName::PETAL_BURST, Ui::MoveDesc::PETAL_BURST, 60, TypeId::GRASS, true},
    {3, Ui::MoveName::NIGHT_BLOOM, Ui::MoveDesc::NIGHT_BLOOM, 75, TypeId::DARK, true},
    {4, Ui::MoveName::EMBER_CLAW, Ui::MoveDesc::EMBER_CLAW, 35, TypeId::FIRE, false},
    {5, Ui::MoveName::FLAME_TAIL, Ui::MoveDesc::FLAME_TAIL, 60, TypeId::FIRE, true},
    {6, Ui::MoveName::AIR_BURN, Ui::MoveDesc::AIR_BURN, 75, TypeId::FLYING, true},
    {7, Ui::MoveName::BUBBLE_JAB, Ui::MoveDesc::BUBBLE_JAB, 35, TypeId::WATER, false},
    {8, Ui::MoveName::WATER_PULSE, Ui::MoveDesc::WATER_PULSE, 60, TypeId::WATER, true},
    {9, Ui::MoveName::SHADOW_WATER, Ui::MoveDesc::SHADOW_WATER, 75, TypeId::DARK, true},
    {10, Ui::MoveName::STATIC_NIP, Ui::MoveDesc::STATIC_NIP, 35, TypeId::ELECTRIC, false},
    {11, Ui::MoveName::THUNDER_DASH, Ui::MoveDesc::THUNDER_DASH, 60, TypeId::ELECTRIC, true},
    {12, Ui::MoveName::VOLT_TAIL, Ui::MoveDesc::VOLT_TAIL, 75, TypeId::ELECTRIC, true},
    {13, Ui::MoveName::QUICK_BITE, Ui::MoveDesc::QUICK_BITE, 35, TypeId::NORMAL, false},
    {14, Ui::MoveName::ADAPT_BURST, Ui::MoveDesc::ADAPT_BURST, 60, TypeId::NORMAL, true},
    {15, Ui::MoveName::LICK, Ui::MoveDesc::LICK, 35, TypeId::GHOST, false},
    {16, Ui::MoveName::SHADOW_BALL, Ui::MoveDesc::SHADOW_BALL, 60, TypeId::GHOST, true},
    {17, Ui::MoveName::DREAM_EATER, Ui::MoveDesc::DREAM_EATER, 75, TypeId::PSYCHIC, true},
};

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

const MoveInfo* findMove(uint8_t moveId) {
    for (const MoveInfo& move : MOVES) {
        if (move.id == moveId) return &move;
    }
    return nullptr;
}

uint8_t movePower(uint8_t moveId, uint8_t fallback) {
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
