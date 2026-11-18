#pragma once
#include "core/VisitSessionService.h"
#include "presentation/Canvas565.h"
#include "presentation/MenuGrid.h"
namespace Ui::VisitMenu {
using Model = Communication::VisitSessionService::ViewModel;
using State = Communication::VisitSessionService::State;
constexpr int CANCEL = 4;
struct Error {
    const char *title;
    const char *detail;
    const char *action;
    bool adjustTeam;
};
Error error(const char *code);
const char *title(const Model &model);
bool waiting(State state);
bool sameRooms(const Model &before, const Model &after);
bool sameTarget(const Model &before, const Model &after, int item);
MenuGrid::Rect homeButton(bool round, int index);
MenuGrid::Rect roomRow(bool round, int index);
MenuGrid::Rect decision(bool round, int index);
MenuGrid::Rect roomViewport();
int roomScrollMaximum(uint8_t roomCount);
MenuGrid::Rect cancelButton(bool round);
MenuGrid::Rect resultButton(bool round);
MenuGrid::Rect backButton(bool round);
void shortId(const uint8_t mac[6], char out[5]);
int itemAt(int x, int y, const Model &model, bool round, int scrollOffset = 0);
void render(Canvas565 &canvas, const Model &model, bool round, uint32_t nowMs, int pressed = -1,
            int scrollOffset = 0);
} // namespace Ui::VisitMenu
