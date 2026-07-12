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

constexpr uint16_t SPECIES_MAGIKARP = 129;
constexpr uint8_t MOVE_SPLASH = 33;
constexpr uint8_t MOVE_TACKLE = 34;
constexpr uint8_t MOVE_SCRATCH = 42;
constexpr uint8_t MOVE_POUND = 43;
constexpr uint8_t MOVE_PECK = 44;
constexpr uint8_t MAGIKARP_TACKLE_LEVEL = 15;
constexpr uint8_t DEFAULT_SECOND_MOVE_LEVEL = 7;

const Species SPECIES[] = {
    {1, Ui::SpeciesName::BULBASAUR, TypeId::GRASS, TypeId::POISON, {45, 49, 49, 65, 65, 45}, GrowthRate::PARABOLIC, EV(0, 0, 0, 1, 0, 0), 2, EvolutionMethod::LEVEL, 16, C(72, 177, 128), C(89, 156, 92), 34, 2},
    {2, Ui::SpeciesName::IVYSAUR, TypeId::GRASS, TypeId::POISON, {60, 62, 63, 80, 80, 60}, GrowthRate::PARABOLIC, EV(0, 0, 0, 1, 1, 0), 3, EvolutionMethod::LEVEL, 32, C(74, 153, 126), C(203, 104, 174), 34, 2},
    {3, Ui::SpeciesName::VENUSAUR, TypeId::GRASS, TypeId::POISON, {80, 82, 83, 100, 100, 80}, GrowthRate::PARABOLIC, EV(0, 0, 0, 2, 1, 0), 0, EvolutionMethod::NONE, 0, C(68, 140, 104), C(226, 112, 172), 34, 2},
    {4, Ui::SpeciesName::CHARMANDER, TypeId::FIRE, TypeId::NONE, {39, 52, 43, 60, 50, 65}, GrowthRate::PARABOLIC, EV(0, 0, 0, 0, 0, 1), 5, EvolutionMethod::LEVEL, 16, C(244, 118, 47), C(255, 204, 83), 42, 5},
    {5, Ui::SpeciesName::CHARMELEON, TypeId::FIRE, TypeId::NONE, {58, 64, 58, 80, 65, 80}, GrowthRate::PARABOLIC, EV(0, 0, 0, 1, 0, 1), 6, EvolutionMethod::LEVEL, 36, C(218, 72, 44), C(255, 170, 60), 42, 5},
    {6, Ui::SpeciesName::CHARIZARD, TypeId::FIRE, TypeId::FLYING, {78, 84, 78, 109, 85, 100}, GrowthRate::PARABOLIC, EV(0, 0, 0, 3, 0, 0), 0, EvolutionMethod::NONE, 0, C(198, 62, 39), C(69, 142, 219), 42, 6},
    {7, Ui::SpeciesName::SQUIRTLE, TypeId::WATER, TypeId::NONE, {44, 48, 65, 50, 64, 43}, GrowthRate::PARABOLIC, EV(0, 0, 1, 0, 0, 0), 8, EvolutionMethod::LEVEL, 16, C(78, 163, 219), C(235, 210, 150), 34, 8},
    {8, Ui::SpeciesName::WARTORTLE, TypeId::WATER, TypeId::NONE, {59, 63, 80, 65, 80, 58}, GrowthRate::PARABOLIC, EV(0, 0, 1, 0, 1, 0), 9, EvolutionMethod::LEVEL, 36, C(64, 137, 209), C(214, 229, 242), 34, 8},
    {9, Ui::SpeciesName::BLASTOISE, TypeId::WATER, TypeId::NONE, {79, 83, 100, 85, 105, 78}, GrowthRate::PARABOLIC, EV(0, 0, 0, 0, 3, 0), 0, EvolutionMethod::NONE, 0, C(63, 119, 188), C(154, 132, 109), 34, 18},
    {16, Ui::SpeciesName::PIDGEY, TypeId::NORMAL, TypeId::FLYING, {40, 45, 40, 35, 35, 56}, GrowthRate::PARABOLIC, EV(0, 0, 0, 0, 0, 1), 17, EvolutionMethod::LEVEL, 18, C(181, 132, 69), C(238, 216, 158), 44, 6},
    {17, Ui::SpeciesName::PIDGEOTTO, TypeId::NORMAL, TypeId::FLYING, {63, 60, 55, 50, 50, 71}, GrowthRate::PARABOLIC, EV(0, 0, 0, 0, 0, 2), 18, EvolutionMethod::LEVEL, 36, C(184, 124, 59), C(238, 211, 130), 44, 6},
    {18, Ui::SpeciesName::PIDGEOT, TypeId::NORMAL, TypeId::FLYING, {83, 80, 75, 70, 70, 101}, GrowthRate::PARABOLIC, EV(0, 0, 0, 0, 0, 3), 0, EvolutionMethod::NONE, 0, C(179, 115, 51), C(245, 210, 91), 44, 6},
    {151, Ui::SpeciesName::MEW, TypeId::PSYCHIC, TypeId::NONE, {100, 100, 100, 100, 100, 100}, GrowthRate::PARABOLIC, EV(3, 0, 0, 0, 0, 0), 0, EvolutionMethod::NONE, 0, C(247, 174, 202), C(255, 224, 238), 43, 22},
    {161, Ui::SpeciesName::SENTRET, TypeId::NORMAL, TypeId::NONE, {35, 46, 34, 35, 45, 20}, GrowthRate::MEDIUM, EV(0, 1, 0, 0, 0, 0), 162, EvolutionMethod::LEVEL, 15, C(190, 122, 60), C(245, 222, 181), 34, 14},
    {162, Ui::SpeciesName::FURRET, TypeId::NORMAL, TypeId::NONE, {85, 76, 64, 45, 55, 90}, GrowthRate::MEDIUM, EV(0, 0, 0, 0, 0, 2), 0, EvolutionMethod::NONE, 0, C(187, 128, 71), C(245, 222, 181), 34, 14},
    {261, Ui::SpeciesName::POOCHYENA, TypeId::DARK, TypeId::NONE, {35, 55, 35, 30, 30, 35}, GrowthRate::MEDIUM, EV(0, 1, 0, 0, 0, 0), 262, EvolutionMethod::LEVEL, 18, C(85, 95, 105), C(190, 205, 215), 34, 24},
    {262, Ui::SpeciesName::MIGHTYENA, TypeId::DARK, TypeId::NONE, {70, 90, 70, 60, 60, 70}, GrowthRate::MEDIUM, EV(0, 2, 0, 0, 0, 0), 0, EvolutionMethod::NONE, 0, C(60, 72, 88), C(184, 195, 207), 34, 24},
    {278, Ui::SpeciesName::WINGULL, TypeId::WATER, TypeId::FLYING, {40, 30, 30, 55, 30, 85}, GrowthRate::MEDIUM, EV(0, 0, 0, 0, 0, 1), 279, EvolutionMethod::LEVEL, 25, C(230, 238, 240), C(68, 157, 206), 44, 8},
    {279, Ui::SpeciesName::PELIPPER, TypeId::WATER, TypeId::FLYING, {60, 50, 100, 95, 70, 65}, GrowthRate::MEDIUM, EV(0, 0, 2, 0, 0, 0), 0, EvolutionMethod::NONE, 0, C(54, 145, 195), C(238, 183, 86), 44, 8},
    {172, Ui::SpeciesName::PICHU, TypeId::ELECTRIC, TypeId::NONE, {20, 40, 15, 35, 35, 60}, GrowthRate::MEDIUM, EV(0, 0, 0, 0, 0, 1), 25, EvolutionMethod::FRIENDSHIP, 0, C(255, 219, 56), C(92, 62, 42), 34, 11},
    {25, Ui::SpeciesName::PIKACHU, TypeId::ELECTRIC, TypeId::NONE, {35, 55, 40, 50, 50, 90}, GrowthRate::MEDIUM, EV(0, 0, 0, 0, 0, 2), 26, EvolutionMethod::STONE, 0, C(255, 211, 43), C(120, 76, 38), 34, 11},
    {26, Ui::SpeciesName::RAICHU, TypeId::ELECTRIC, TypeId::NONE, {60, 90, 55, 90, 80, 110}, GrowthRate::MEDIUM, EV(0, 0, 0, 0, 0, 3), 0, EvolutionMethod::NONE, 0, C(235, 139, 48), C(255, 225, 86), 34, 12},
    {133, Ui::SpeciesName::EEVEE, TypeId::NORMAL, TypeId::NONE, {55, 55, 50, 45, 65, 55}, GrowthRate::MEDIUM, EV(0, 0, 0, 0, 1, 0), 134, EvolutionMethod::BRANCH, 0, C(156, 102, 55), C(239, 211, 155), 34, 14},
    {134, Ui::SpeciesName::VAPOREON, TypeId::WATER, TypeId::NONE, {130, 65, 60, 110, 95, 65}, GrowthRate::MEDIUM, EV(2, 0, 0, 0, 0, 0), 0, EvolutionMethod::NONE, 0, C(61, 142, 216), C(193, 230, 255), 34, 18},
    {135, Ui::SpeciesName::JOLTEON, TypeId::ELECTRIC, TypeId::NONE, {65, 65, 60, 110, 95, 130}, GrowthRate::MEDIUM, EV(0, 0, 0, 0, 0, 2), 0, EvolutionMethod::NONE, 0, C(249, 218, 52), C(250, 247, 181), 34, 20},
    {136, Ui::SpeciesName::FLAREON, TypeId::FIRE, TypeId::NONE, {65, 130, 60, 95, 110, 65}, GrowthRate::MEDIUM, EV(0, 2, 0, 0, 0, 0), 0, EvolutionMethod::NONE, 0, C(221, 91, 45), C(255, 210, 106), 34, 19},
    {196, Ui::SpeciesName::ESPEON, TypeId::PSYCHIC, TypeId::NONE, {65, 65, 60, 130, 95, 110}, GrowthRate::MEDIUM, EV(0, 0, 0, 2, 0, 0), 0, EvolutionMethod::NONE, 0, C(190, 116, 205), C(248, 194, 238), 34, 22},
    {197, Ui::SpeciesName::UMBREON, TypeId::DARK, TypeId::NONE, {95, 65, 110, 60, 130, 65}, GrowthRate::MEDIUM, EV(0, 0, 0, 0, 2, 0), 0, EvolutionMethod::NONE, 0, C(42, 45, 58), C(238, 204, 57), 34, 24},
    {380, Ui::SpeciesName::LATIAS, TypeId::DRAGON, TypeId::PSYCHIC, {80, 80, 90, 110, 130, 110}, GrowthRate::SLOW, EV(0, 0, 0, 0, 3, 0), 0, EvolutionMethod::NONE, 0, C(214, 59, 84), C(245, 245, 245), 34, 22},
    {381, Ui::SpeciesName::LATIOS, TypeId::DRAGON, TypeId::PSYCHIC, {80, 90, 80, 130, 110, 110}, GrowthRate::SLOW, EV(0, 0, 0, 3, 0, 0), 0, EvolutionMethod::NONE, 0, C(54, 117, 215), C(245, 245, 245), 34, 22},
    {123, Ui::SpeciesName::SCYTHER, TypeId::BUG, TypeId::FLYING, {70, 110, 80, 55, 80, 105}, GrowthRate::MEDIUM, EV(0, 1, 0, 0, 0, 0), 212, EvolutionMethod::TRADE, 0, C(101, 179, 78), C(226, 238, 148), 42, 30},
    {212, Ui::SpeciesName::SCIZOR, TypeId::BUG, TypeId::STEEL, {70, 130, 100, 55, 80, 65}, GrowthRate::MEDIUM, EV(0, 2, 0, 0, 0, 0), 0, EvolutionMethod::NONE, 0, C(194, 48, 55), C(160, 175, 186), 42, 32},
    {92, Ui::SpeciesName::GASTLY, TypeId::GHOST, TypeId::POISON, {30, 35, 30, 100, 35, 80}, GrowthRate::PARABOLIC, EV(0, 0, 0, 1, 0, 0), 93, EvolutionMethod::LEVEL, 25, C(117, 70, 167), C(65, 39, 92), 34, 16},
    {93, Ui::SpeciesName::HAUNTER, TypeId::GHOST, TypeId::POISON, {45, 50, 45, 115, 55, 95}, GrowthRate::PARABOLIC, EV(0, 0, 0, 2, 0, 0), 94, EvolutionMethod::TRADE, 0, C(82, 47, 137), C(173, 103, 220), 34, 16},
    {94, Ui::SpeciesName::GENGAR, TypeId::GHOST, TypeId::POISON, {60, 65, 60, 130, 75, 110}, GrowthRate::PARABOLIC, EV(0, 0, 0, 3, 0, 0), 0, EvolutionMethod::NONE, 0, C(55, 38, 89), C(139, 81, 184), 34, 17},
    {129, Ui::SpeciesName::MAGIKARP, TypeId::WATER, TypeId::NONE, {20, 10, 55, 15, 20, 80}, GrowthRate::SLOW, EV(0, 0, 0, 0, 0, 1), 130, EvolutionMethod::LEVEL, 20, C(229, 80, 48), C(247, 190, 66), 33, 34},
    {130, Ui::SpeciesName::GYARADOS, TypeId::WATER, TypeId::FLYING, {95, 125, 79, 60, 100, 81}, GrowthRate::SLOW, EV(0, 2, 0, 0, 0, 0), 0, EvolutionMethod::NONE, 0, C(55, 124, 199), C(230, 223, 182), 34, 36},
    {143, Ui::SpeciesName::SNORLAX, TypeId::NORMAL, TypeId::NONE, {160, 110, 65, 65, 110, 30}, GrowthRate::SLOW, EV(2, 0, 0, 0, 0, 0), 0, EvolutionMethod::NONE, 0, C(49, 84, 105), C(231, 218, 184), 34, 38},
    {147, Ui::SpeciesName::DRATINI, TypeId::DRAGON, TypeId::NONE, {41, 64, 45, 50, 50, 50}, GrowthRate::SLOW, EV(0, 1, 0, 0, 0, 0), 148, EvolutionMethod::LEVEL, 30, C(92, 162, 219), C(238, 238, 250), 34, 40},
    {148, Ui::SpeciesName::DRAGONAIR, TypeId::DRAGON, TypeId::NONE, {61, 84, 65, 70, 70, 70}, GrowthRate::SLOW, EV(0, 2, 0, 0, 0, 0), 149, EvolutionMethod::LEVEL, 50, C(73, 140, 209), C(247, 247, 255), 34, 40},
    {149, Ui::SpeciesName::DRAGONITE, TypeId::DRAGON, TypeId::FLYING, {91, 134, 95, 100, 100, 80}, GrowthRate::SLOW, EV(0, 3, 0, 0, 0, 0), 0, EvolutionMethod::NONE, 0, C(229, 156, 62), C(87, 158, 213), 34, 41},
};

const MoveInfo MOVES[] = {
    {42, Ui::MoveName::SCRATCH, Ui::MoveDesc::SCRATCH, 35, TypeId::NORMAL, false},
    {43, Ui::MoveName::POUND, Ui::MoveDesc::POUND, 35, TypeId::NORMAL, false},
    {44, Ui::MoveName::PECK, Ui::MoveDesc::PECK, 35, TypeId::FLYING, false},
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
    {18, Ui::MoveName::HYDRO_SURGE, Ui::MoveDesc::HYDRO_SURGE, 75, TypeId::WATER, true},
    {19, Ui::MoveName::FLARE_BLAST, Ui::MoveDesc::FLARE_BLAST, 75, TypeId::FIRE, true},
    {20, Ui::MoveName::THUNDER_SPIKE, Ui::MoveDesc::THUNDER_SPIKE, 75, TypeId::ELECTRIC, true},
    {21, Ui::MoveName::MIND_TAP, Ui::MoveDesc::MIND_TAP, 35, TypeId::PSYCHIC, true},
    {22, Ui::MoveName::PSY_BEAM, Ui::MoveDesc::PSY_BEAM, 75, TypeId::PSYCHIC, true},
    {23, Ui::MoveName::MOON_NIP, Ui::MoveDesc::MOON_NIP, 35, TypeId::DARK, false},
    {24, Ui::MoveName::DARK_PULSE, Ui::MoveDesc::DARK_PULSE, 75, TypeId::DARK, true},
    {25, Ui::MoveName::ICE_SHARD, Ui::MoveDesc::ICE_SHARD, 35, TypeId::ICE, true},
    {26, Ui::MoveName::FROST_BEAM, Ui::MoveDesc::FROST_BEAM, 75, TypeId::ICE, true},
    {27, Ui::MoveName::FAIRY_TOUCH, Ui::MoveDesc::FAIRY_TOUCH, 35, TypeId::FAIRY, true},
    {28, Ui::MoveName::MOON_BLAST, Ui::MoveDesc::MOON_BLAST, 75, TypeId::FAIRY, true},
    {29, Ui::MoveName::BUG_SLASH, Ui::MoveDesc::BUG_SLASH, 35, TypeId::BUG, false},
    {30, Ui::MoveName::SCYTHE_DANCE, Ui::MoveDesc::SCYTHE_DANCE, 75, TypeId::BUG, false},
    {31, Ui::MoveName::STEEL_CLAMP, Ui::MoveDesc::STEEL_CLAMP, 35, TypeId::STEEL, false},
    {32, Ui::MoveName::IRON_CRUSH, Ui::MoveDesc::IRON_CRUSH, 75, TypeId::STEEL, false},
    {33, Ui::MoveName::SPLASH, Ui::MoveDesc::SPLASH, 0, TypeId::NORMAL, false},
    {34, Ui::MoveName::TACKLE, Ui::MoveDesc::TACKLE, 35, TypeId::NORMAL, false},
    {35, Ui::MoveName::WATER_FANG, Ui::MoveDesc::WATER_FANG, 35, TypeId::WATER, false},
    {36, Ui::MoveName::DRAGON_RAGE, Ui::MoveDesc::DRAGON_RAGE, 75, TypeId::DRAGON, true},
    {37, Ui::MoveName::HEAVY_BODY, Ui::MoveDesc::HEAVY_BODY, 35, TypeId::NORMAL, false},
    {38, Ui::MoveName::BODY_PRESS, Ui::MoveDesc::BODY_PRESS, 75, TypeId::NORMAL, false},
    {39, Ui::MoveName::DRAGON_TAIL, Ui::MoveDesc::DRAGON_TAIL, 35, TypeId::DRAGON, false},
    {40, Ui::MoveName::DRAGON_DANCE, Ui::MoveDesc::DRAGON_DANCE, 75, TypeId::DRAGON, true},
    {41, Ui::MoveName::SKY_DRAGON, Ui::MoveDesc::SKY_DRAGON, 75, TypeId::FLYING, false},
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

bool isBasicFirstMove(uint8_t moveId) {
    switch (moveId) {
    case MOVE_SPLASH:
    case MOVE_TACKLE:
    case MOVE_SCRATCH:
    case MOVE_POUND:
    case MOVE_PECK:
        return true;
    default:
        return false;
    }
}

uint8_t basicMoveIdForSpecies(const Species& species) {
    return isBasicFirstMove(species.basicMoveId) ? species.basicMoveId : MOVE_TACKLE;
}

uint8_t secondMoveIdForSpecies(const Species& species) {
    return findMove(species.specialMoveId) ? species.specialMoveId : 0;
}

uint8_t secondMoveLearnLevelForSpecies(const Species& species) {
    if (secondMoveIdForSpecies(species) == 0) return 0;
    if (species.id == SPECIES_MAGIKARP) return MAGIKARP_TACKLE_LEVEL;
    return DEFAULT_SECOND_MOVE_LEVEL;
}

uint8_t specialMoveIdForMonster(const Game::MonsterRuntime& monster, uint8_t specialSlot) {
    uint8_t moveId = 0;
    if (specialSlot == 0) moveId = monster.move2Id;
    else if (specialSlot == 1) moveId = monster.move3Id;
    return findMove(moveId) ? moveId : 0;
}

uint8_t specialMoveCount(const Game::MonsterRuntime& monster) {
    uint8_t count = 0;
    if (specialMoveIdForMonster(monster, 0) != 0) ++count;
    if (specialMoveIdForMonster(monster, 1) != 0) ++count;
    return count;
}

uint8_t moveIdForMonster(const Species& species, const Game::MonsterRuntime& monster, bool secondSlot) {
    if (secondSlot) {
        return specialMoveIdForMonster(monster, 0);
    }
    return isBasicFirstMove(monster.move1Id) ? monster.move1Id : basicMoveIdForSpecies(species);
}

uint8_t moveIdForMonster(const Species& species, const Game::MonsterRuntime& monster, uint8_t specialSlot) {
    if (specialSlot < 2) return specialMoveIdForMonster(monster, specialSlot);
    return moveIdForMonster(species, monster, false);
}

bool hasSpecialMove(const Game::MonsterRuntime& monster) {
    return specialMoveCount(monster) > 0;
}

bool hasSecondMove(const Game::MonsterRuntime& monster) {
    return hasSpecialMove(monster);
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

uint32_t minimumExpForLevel(GrowthRate growthRate, uint8_t level) {
    if (level <= 1) return 0;
    if (level > Game::LEVEL_MAX) level = Game::LEVEL_MAX;

    const uint32_t n = level;
    const uint32_t n2 = n * n;
    const uint32_t n3 = n2 * n;

    switch (growthRate) {
    case GrowthRate::ERRATIC:
        if (n <= 50) return (n3 * (100 - n)) / 50;
        if (n <= 68) return (n3 * (150 - n)) / 100;
        if (n <= 98) return (n3 * ((1911 - 10 * n) / 3)) / 500;
        return (n3 * (160 - n)) / 100;
    case GrowthRate::FLUCTUATING:
        if (n <= 15) return (n3 * (24 + ((n + 1) / 3))) / 50;
        if (n <= 35) return (n3 * (14 + n)) / 50;
        return (n3 * (32 + (n / 2))) / 50;
    case GrowthRate::PARABOLIC: {
        const int32_t exp = (int32_t)((6 * n3) / 5) - (int32_t)(15 * n2) + (int32_t)(100 * n) - 140;
        return exp > 0 ? (uint32_t)exp : 0;
    }
    case GrowthRate::FAST:
        return (4 * n3) / 5;
    case GrowthRate::SLOW:
        return (5 * n3) / 4;
    case GrowthRate::MEDIUM:
    default:
        return n3;
    }
}

uint8_t levelForExp(GrowthRate growthRate, uint32_t exp) {
    for (uint8_t level = 2; level <= Game::LEVEL_MAX; ++level) {
        if (exp < minimumExpForLevel(growthRate, level)) return level - 1;
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
