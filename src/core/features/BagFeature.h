#pragma once

#include <cstdint>
#include "game/GameState.h"

namespace Features { namespace Bag {

constexpr uint8_t ITEM_COUNT = 26;
constexpr uint8_t SOURCE_FOOD_BASE = 4;
constexpr uint8_t SOURCE_HEAL_BASE = 11;
constexpr uint8_t SOURCE_EXTRA_BASE = 15;
constexpr uint8_t SOURCE_EXTRA_COUNT = 10;
constexpr uint8_t SOURCE_HEART_SCALE = SOURCE_EXTRA_BASE + 9;
constexpr uint8_t SOURCE_BACK = 25;

struct Row { uint8_t source; uint8_t count; };
struct Context {
    bool battle = false;
    uint8_t targetTeamSlot = 0;
    const char* targetName = nullptr;
};
enum class BattleResult : uint8_t {
        NONE,
        POTION,
        SUPER_POTION,
        ANTIDOTE,
        FOOD_THROWN,
        PARALYZE_HEAL,
        AWAKENING,
        BURN_HEAL,
        ICE_HEAL,
        MAX_POTION,
        FULL_RESTORE,
        FULL_HEAL,
        REVIVE,
};
enum class Selection : uint8_t { NONE, CONFIRM, RECALL, BACK };
enum class Notice : uint8_t { NONE,
    FAINTED_CANNOT_HEAL,
    USED_POTION,
    HP_FULL,
    USED_SUPER_POTION,
    USED_ANTIDOTE,
    STATUS_NORMAL,
    USED_PARALYZE_HEAL,
    USED_AWAKENING,
    USED_BURN_HEAL,
    USED_ICE_HEAL,
    USED_MAX_POTION,
    USED_FULL_RESTORE,
    FULL_RESTORE_NOT_NEEDED,
    USED_FULL_HEAL,
    STONE_EVOLUTION_BEGIN,
    STONE_NO_EFFECT,
    USED_REVIVE,
    REVIVE_NO_FAINTED,
    USED_MAX_REPEL,
    MAX_REPEL_ACTIVE,
    USED_HONEY,
    HONEY_ACTIVE,
    FOOD_NO_STOCK,
    THREW_FOOD
};
struct Outcome {
    bool used = false;
    BattleResult battleResult = BattleResult::NONE;
    Notice notice = Notice::NONE;
    uint8_t foodIndex = 0;
};

Game::ItemId itemId(uint8_t source);
uint8_t itemCount(const Game::GameState& state, uint8_t source);
bool visible(uint8_t source, bool battle);
uint8_t targetSlot(uint8_t source, const Context& context, const Game::GameState& state);
uint8_t collect(const Game::GameState& state, bool battle, Row* rows, uint8_t capacity);
uint8_t visibleCount(const Game::GameState& state, bool battle);
uint8_t sourceAt(const Game::GameState& state, bool battle, uint8_t index);
Selection select(uint8_t source, bool battle);
using ApplyItem = bool (*)(Game::ItemId item, uint8_t targetSlot);
Outcome use(const Game::GameState& state, uint8_t source, const Context& context, ApplyItem apply);

} }  // namespace Features::Bag
