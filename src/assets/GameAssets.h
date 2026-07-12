#pragma once

#include <cstdint>
#include "game/GameState.h"

namespace GameAssets {

enum class Kind : uint16_t {
    ITEM_POKE_BALL,
    ITEM_GREAT_BALL,
    ITEM_HEAVY_BALL,
    ITEM_TIMER_BALL,
    ITEM_NORMAL_FOOD,
    ITEM_POTION,
    ITEM_SUPER_POTION,
    ITEM_ANTIDOTE,
    ITEM_CANDY,
    BALL_POKE_BALL_0,
    BALL_POKE_BALL_1,
    BALL_POKE_BALL_2,
    BALL_POKE_BALL_3,
    BALL_POKE_BALL_4,
    BALL_POKE_BALL_5,
    BALL_POKE_BALL_6,
    BALL_POKE_BALL_7,
    BALL_POKE_BALL_OPEN,
    BALL_GREAT_BALL_0,
    BALL_GREAT_BALL_1,
    BALL_GREAT_BALL_2,
    BALL_GREAT_BALL_3,
    BALL_GREAT_BALL_4,
    BALL_GREAT_BALL_5,
    BALL_GREAT_BALL_6,
    BALL_GREAT_BALL_7,
    BALL_GREAT_BALL_OPEN,
    BALL_HEAVY_BALL_0,
    BALL_HEAVY_BALL_1,
    BALL_HEAVY_BALL_2,
    BALL_HEAVY_BALL_3,
    BALL_HEAVY_BALL_4,
    BALL_HEAVY_BALL_5,
    BALL_HEAVY_BALL_6,
    BALL_HEAVY_BALL_7,
    BALL_HEAVY_BALL_OPEN,
    BALL_TIMER_BALL_0,
    BALL_TIMER_BALL_1,
    BALL_TIMER_BALL_2,
    BALL_TIMER_BALL_3,
    BALL_TIMER_BALL_4,
    BALL_TIMER_BALL_5,
    BALL_TIMER_BALL_6,
    BALL_TIMER_BALL_7,
    BALL_TIMER_BALL_OPEN,
    BALL_BURST_STAR,
    BATTLE_BG_GRASS,
    BATTLE_BG_RIVERSIDE,
    BATTLE_BG_DEEP_FOREST,
    EGG,
    COUNT,
};

bool begin();
bool available();
uint32_t compressedBytes();
bool draw(Kind kind, int x, int y, float scale = 1.0f);
bool drawCentered(Kind kind, int centerX, int centerY, float scale = 1.0f);
bool drawBattleBackground(Kind kind);

Kind itemKind(Game::ItemId item);
Kind ballFrameKind(Game::ItemId item, uint8_t frame);
Kind ballOpenKind(Game::ItemId item);

}  // namespace GameAssets
