#pragma once

#include <cstdint>

#include "game/GameState.h"

// ---------------------------------------------------------------------------
// FoodTuning：食物相关的全部可调数值集中于此，改数值只动这一个文件。
// 覆盖三块：房间喂食档案、性格→口味映射、战斗投掷结算。
// ---------------------------------------------------------------------------
namespace FoodTuning {

// ---- 房间喂食档案（按 RoomState::food 索引） ------------------------------
struct FoodProfile {
    uint8_t satietyGain;
    uint8_t moodGain;
    uint8_t careExp;
};

static constexpr FoodProfile PROFILES[Game::ROOM_FOOD_COUNT] = {
    {25, 3, 1}, // 0 饼干（3 口/份，见 roomFoodBitesPerServing）
    {22, 5, 6}, // 1 能量方块
    {20, 8, 6}, // 2 桃桃果
    {24, 4, 6}, // 3 樱子果
    {22, 6, 6}, // 4 利木果
    {26, 3, 7}, // 5 莓莓果
    {23, 5, 6}, // 6 零余果
};

// 口味偏好对心情的修正（百分比，100 = 不变）。仅作用于 2~6 号口味树果。
static constexpr uint8_t LIKED_MOOD_PERCENT = 150;
static constexpr uint8_t DISLIKED_MOOD_PERCENT = 75;

// ---- 性格 → 口味映射 -------------------------------------------------------
// 按性格修正项索引（1物攻 2物防 3特攻 4特防 5速度）给出喜欢的食物索引，
// 与 NATURE_BOOST / NATURE_LOWER 搭配使用；无修正性格（boost == lower）无偏好。
static constexpr int8_t STAT_TO_FOOD_INDEX[Game::STAT_COUNT] = {
    -1,                                // 0 HP（不参与性格修正）
    Game::ROOM_SPICY_FOOD_INDEX,       // 1 物攻 → 辣
    Game::ROOM_DRY_FOOD_INDEX,         // 2 物防 → 涩
    Game::ROOM_BITTER_FOOD_INDEX,      // 3 特攻 → 苦
    Game::ROOM_SOUR_FOOD_INDEX,        // 4 特防 → 酸
    Game::ROOM_SWEET_FOOD_INDEX,       // 5 速度 → 甜
};

// ---- 战斗投掷结算 -----------------------------------------------------------
enum class ThrowClass : uint8_t {
    NORMAL = 0, // 饼干，或不对口味的口味树果
    TASTY,      // 能量方块
    LIKED,      // 野生精灵喜欢口味
    DISLIKED,   // 野生精灵讨厌口味
    COUNT,
};

// 接受概率（%）：[0]=普通野生，[1]=Boss。
static constexpr uint8_t THROW_ACCEPT_PERCENT[2][(uint8_t)ThrowClass::COUNT] = {
    {50, 55, 60, 40}, // 普通野生
    {30, 35, 40, 20}, // Boss
};

// 接受后的羁绊增量（上限见 FriendshipSystem::FOOD_BOND_MAX）。
static constexpr uint8_t THROW_BOND_GAIN[(uint8_t)ThrowClass::COUNT] = {
    30, 35, 40, 20,
};

} // namespace FoodTuning
