#pragma once

#include "game/GameState.h"
#include "game/Species.h"
#include "presentation/MenuGrid.h"

namespace Ui { namespace StatusContent {
const Species& speciesFor(const Game::MonsterRuntime& mon);
const char* originName(Game::Origin origin);
int textPixelWidth(const char* value);
int wrapText(const char* value, int maxWidth, int x, int y, uint16_t color, bool draw);
int wrappedTextLineCount(const char* value, int maxWidth);
int statusPageContentHeight(uint8_t page);
const char* genderName(const Species& species, const Game::MonsterRuntime& mon);
const char* metAreaName(uint8_t metArea);
void drawStatusMonsterIcon(const Species& species, int x, int y);
void paintDetails(uint8_t page, const Species& species, const Game::MonsterRuntime& mon,
                  const MenuGrid::Rect& body, int scrollY);
} }  // namespace Ui::StatusContent
