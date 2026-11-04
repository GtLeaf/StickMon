#pragma once

#include <cstdint>

#include "AmoledGeometry.h"
#include "game/GameState.h"
#include "ui/models/ScreenModels.h"

class Canvas565;

namespace AmoledV1 {

inline constexpr int CONTACT_ROW_HEIGHT = 90;
inline constexpr int CLAW_TAB_CONNECT_LEFT = 204;
inline constexpr int CLAW_TAB_LOG_LEFT = 286;
inline constexpr int CLAW_TAB_WIDTH = 78;
inline constexpr int CLAW_LOG_TOP = 156;
inline constexpr int CLAW_LOG_HEIGHT = 280;
inline constexpr int CLAW_LOG_ROW_HEIGHT = 32;
inline constexpr int CLAW_LOG_VIEWPORT = 272;

void renderComputerScreen(Canvas565& canvas, const ComputerViewModel& model,
                          uint16_t rowBegin = 0, uint16_t rowEnd = AmoledUi::HEIGHT);
bool computerBackAt(int x, int y,
                    ComputerViewModel::Page page = ComputerViewModel::Page::MENU);
int computerItemAt(int x, int y, ComputerViewModel::Page page,
                   float storageScroll = 0.0f,
                   uint8_t storageCount = Game::STORAGE_CAP,
                   bool clawEnabled = false);
int computerMaxStorageScroll(uint8_t storageCount);
int computerContactActionItemAt(int x, int y, uint8_t actionCount,
                                uint8_t slot, float scroll);
bool computerContactMenuAt(int x, int y, uint8_t actionCount,
                           uint8_t slot, float scroll);
int computerContactConfirmChoiceAt(int x, int y);
int clawTabAt(int x, int y);

}  // namespace AmoledV1
