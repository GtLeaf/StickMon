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
    EXPLORE_TILE_0072,
    EXPLORE_TILE_0144,
    EXPLORE_TILE_0168,
    EXPLORE_TILE_0385,
    EXPLORE_TILE_0386,
    EXPLORE_TILE_0387,
    EXPLORE_TILE_0388,
    EXPLORE_TILE_0389,
    EXPLORE_TILE_0390,
    EXPLORE_TILE_0415,
    EXPLORE_TILE_0537,
    EXPLORE_TILE_0538,
    EXPLORE_TILE_0539,
    EXPLORE_TILE_0540,
    EXPLORE_TILE_0542,
    EXPLORE_TILE_0545,
    EXPLORE_TILE_0546,
    EXPLORE_TILE_0547,
    EXPLORE_TILE_0553,
    EXPLORE_TILE_0554,
    EXPLORE_TILE_0555,
    EXPLORE_TILE_0556,
    EXPLORE_TILE_0558,
    EXPLORE_TILE_0800,
    EXPLORE_TILE_0801,
    EXPLORE_TILE_0802,
    EXPLORE_TILE_0804,
    EXPLORE_TILE_0805,
    EXPLORE_TILE_0808,
    EXPLORE_TILE_0809,
    EXPLORE_TILE_0810,
    EXPLORE_TILE_0811,
    EXPLORE_TILE_0818,
    EXPLORE_TILE_0819,
    EXPLORE_TILE_1662,
    EXPLORE_TILE_1665,
    EXPLORE_TILE_1681,
    EXPLORE_TILE_1682,
    EGG,
    COUNT,
};

bool begin();
bool available();
uint32_t compressedBytes();
bool draw(Kind kind, int x, int y, float scale = 1.0f);
bool drawCentered(Kind kind, int centerX, int centerY, float scale = 1.0f);
bool drawBattleBackground(Kind kind);
bool drawBackgroundViewport(Kind kind, int cameraX, int cameraY);
bool drawExploreTile(uint16_t tileId, int x, int y);

Kind itemKind(Game::ItemId item);
Kind ballFrameKind(Game::ItemId item, uint8_t frame);
Kind ballOpenKind(Game::ItemId item);

}  // namespace GameAssets
