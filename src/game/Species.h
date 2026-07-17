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

static constexpr uint8_t SPECIAL_MOVE_SLOT_COUNT = 2;
static constexpr uint8_t FRIENDSHIP_EVOLUTION_THRESHOLD = 220;

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
    uint16_t baseExp;
};

enum class DamageClass : uint8_t {
    PHYSICAL,
    SPECIAL,
    STATUS,
};

enum class BattleStat : uint8_t {
    ATTACK = 0,
    DEFENSE,
    SP_ATTACK,
    SP_DEFENSE,
    SPEED,
    ACCURACY,
    EVASION,
    COUNT,
};

enum class MoveEffectKind : uint8_t {
    MAJOR_STATUS = 0,
    CONFUSION,
    FLINCH,
    BIND,
    STAT_STAGE,
    DRAIN,
    RECOIL,
    HEAL,
    REST,
    CURE_STATUS,
    CLEAR_BIND,
    YAWN,
};

enum class MoveEffectTarget : uint8_t {
    DEFENDER = 0,
    ATTACKER,
};

struct MoveEffectSpec {
    MoveEffectKind kind;
    MoveEffectTarget target;
    uint8_t chance;
    int8_t value;
    uint8_t aux;
    uint8_t minTurns;
    uint8_t maxTurns;
};

static_assert(sizeof(MoveEffectSpec) == 7, "move effects must remain flash compact");

enum MoveFlags : uint8_t {
    MOVE_FLAG_NONE = 0,
    MOVE_FLAG_USABLE_ASLEEP = 1 << 0,
};

struct MoveInfo {
    Game::MoveId id;
    const char* name;
    const char* description;
    uint16_t power;
    TypeId type;
    DamageClass damageClass;
    uint8_t accuracy;
    uint8_t pp;
    int8_t priority;
    bool battleSupported;
    uint16_t effectOffset;
    uint8_t effectCount;
    uint8_t minHits;
    uint8_t maxHits;
    uint8_t criticalStage;
    uint8_t flags;
};

struct LearnsetEntry {
    uint8_t level;
    Game::MoveId moveId;
};

struct SpeciesLearnset {
    uint16_t speciesId;
    uint16_t offset;
    uint8_t count;
    Game::MoveId basicMoveId;
};

const Species& starterSpecies();
const Species* speciesTable();
uint8_t speciesCount();
const Species* findSpecies(uint16_t speciesId);
const Species* levelUpEvolutionTarget(const Species& species,
                                      const Game::MonsterRuntime& monster);
const MoveInfo* findMove(Game::MoveId moveId);
const MoveEffectSpec* moveEffectFor(const MoveInfo& move, uint8_t index);
const SpeciesLearnset* findLearnset(uint16_t speciesId);
const LearnsetEntry* learnsetEntryForSpecies(const Species& species, uint16_t index);
uint16_t learnsetEntryCountForSpecies(const Species& species);
bool isMoveBattleSupported(Game::MoveId moveId);
bool canLearnAsSpecialMove(const Species& species, Game::MoveId moveId);
bool canRetainSpecialMove(const Species& species, Game::MoveId moveId, uint8_t level);
bool isBasicFirstMoveForSpecies(const Species& species, Game::MoveId moveId);
Game::MoveId basicMoveIdForSpecies(const Species& species);
uint8_t moveLearnLevelForSpecies(const Species& species, Game::MoveId moveId);
void resetMovesForLevel(Game::MonsterRuntime& monster, const Species& species);
Game::MoveId specialMoveIdForMonster(const Game::MonsterRuntime& monster, uint8_t specialSlot);
uint8_t specialMoveCount(const Game::MonsterRuntime& monster);
Game::MoveId moveIdForMonster(const Species& species, const Game::MonsterRuntime& monster, bool secondSlot);
Game::MoveId moveIdForMonster(const Species& species, const Game::MonsterRuntime& monster, uint8_t specialSlot);
bool hasSpecialMove(const Game::MonsterRuntime& monster);
bool hasSecondMove(const Game::MonsterRuntime& monster);
uint16_t movePower(Game::MoveId moveId, uint16_t fallback);
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
