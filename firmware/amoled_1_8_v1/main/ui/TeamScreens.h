#pragma once

#include <cstdint>

#include "AmoledGeometry.h"
#include "ui/UiMetrics.h"
#include "ui/models/ScreenModels.h"

class Canvas565;

namespace AmoledV1 {

inline constexpr int TEAM_MOVES_HEADER_HEIGHT = UiMetrics::PAGE_HEADER_HEIGHT;

void renderTeamScreen(Canvas565& canvas, const TeamViewModel& model,
                      uint16_t rowBegin = 0, uint16_t rowEnd = AmoledUi::HEIGHT);
bool teamBackAt(int x, int y);
int teamMemberAt(int x, int y, uint8_t teamCount);
bool teamMovesButtonAt(int x, int y, uint8_t teamSlot);
int teamConfirmChoiceAt(int x, int y);
int teamActionPopupItemAt(int x, int y, uint8_t teamSlot);
void renderTeamStatusScreen(Canvas565& canvas,
                            const TeamStatusViewModel& model,
                            uint16_t rowBegin = 0,
                            uint16_t rowEnd = AmoledUi::HEIGHT);
bool teamStatusBackAt(int x, int y);
void renderTeamMovesScreen(Canvas565& canvas,
                           const TeamMovesViewModel& model,
                           uint16_t rowBegin = 0, uint16_t rowEnd = AmoledUi::HEIGHT);
bool teamMovesBackAt(int x, int y);
int teamMovesItemAt(int x, int y, TeamMovesViewModel::Mode mode,
                    uint8_t recallCount);
int teamMovesItemAt(int x, int y, TeamMovesViewModel::Mode mode,
                    uint8_t recallCount, int16_t scrollOffsetY,
                    uint8_t selectedItem, float detailProgress);

}  // namespace AmoledV1
