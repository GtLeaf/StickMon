#pragma once

#include <cstdint>
#include "game/GameState.h"

enum class TypeId : uint8_t {
    NORMAL,
    FIRE,
    WATER,
    GRASS,
    ELECTRIC,
    ICE,
    FIGHTING,
    POISON,
    GROUND,
    FLYING,
    PSYCHIC,
    BUG,
    ROCK,
    GHOST,
    DRAGON,
    DARK,
    STEEL,
    FAIRY,
    NONE = 0xFF,
};

struct BaseStats {
    uint8_t hp;
    uint8_t atk;
    uint8_t def;
    uint8_t spa;
    uint8_t spd;
    uint8_t spe;
};

enum class GrowthRate : uint8_t {
    MEDIUM,
    ERRATIC,
    FLUCTUATING,
    PARABOLIC,
    FAST,
    SLOW,
};

enum class EvolutionMethod : uint8_t {
    NONE,
    LEVEL,
    FRIENDSHIP,
    STONE,
    TRADE,
    BRANCH,
};

struct Species {
    uint16_t id;
    const char* name;
    TypeId type1;
    TypeId type2;
    BaseStats stats;
    GrowthRate growthRate;
    uint16_t evYieldPacked;
    uint16_t evolveTo;
    EvolutionMethod evolveMethod;
    uint8_t evolveLevel;
    uint16_t colorA;
    uint16_t colorB;
    uint8_t basicMoveId;
    uint8_t specialMoveId;
};

struct MoveInfo {
    uint8_t id;
    const char* name;
    const char* description;
    uint8_t power;
    TypeId type;
    bool special;
};

const Species& starterSpecies();
const Species* speciesTable();
uint8_t speciesCount();
const Species* findSpecies(uint16_t speciesId);
const MoveInfo* findMove(uint8_t moveId);
bool isBasicFirstMove(uint8_t moveId);
uint8_t basicMoveIdForSpecies(const Species& species);
uint8_t secondMoveIdForSpecies(const Species& species);
uint8_t secondMoveLearnLevelForSpecies(const Species& species);
uint8_t moveIdForMonster(const Species& species, const Game::MonsterRuntime& monster, bool secondSlot);
bool hasSecondMove(const Game::MonsterRuntime& monster);
uint8_t movePower(uint8_t moveId, uint8_t fallback);
uint16_t maxHpFor(const Species& species, uint8_t level);
uint16_t maxHpFor(const Species& species, const Game::MonsterRuntime& monster);
uint16_t statFor(const Species& species, const Game::MonsterRuntime& monster, uint8_t statIndex);
uint8_t evYieldAt(const Species& species, uint8_t statIndex);
uint32_t minimumExpForLevel(GrowthRate growthRate, uint8_t level);
uint8_t levelForExp(GrowthRate growthRate, uint32_t exp);
uint32_t expToNextLevel(GrowthRate growthRate, uint8_t level, uint32_t exp);
const char* natureName(uint8_t nature);
uint8_t natureBoostStat(uint8_t nature);
uint8_t natureLowerStat(uint8_t nature);
const char* typeName(TypeId type);
const char* evolutionMethodName(EvolutionMethod method);
