// SPDX-License-Identifier: Apache-2.0
#include "ui/compact_touch/MovesUi.h"
#include "ui/round_touch/RoundPage.h"

#include <algorithm>
#include <cstdio>
#include "core/MathUtil.h"
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "presentation/MenuGrid.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TouchUi.h"
#include "ui/common/StatusUiContent.h"

namespace {
constexpr int WIDTH = 360;
constexpr int HEIGHT = 360;
MenuGrid::Rect contentBody() { return {40, 64, 280, 240}; }

MenuGrid::Layout touchMoveLayout() {
    auto body = contentBody();
    body.y += 6;
    body.height -= 12;
    body.width = 90;
    return MenuGrid::fit(body, 90, 44, 0, 6, 0);
}
MenuGrid::Rect descriptionArea() {
    const auto body = contentBody();
    return {body.x + 99, body.y + 76, body.width - 111, body.height - 88};
}
MenuGrid::Rect touchForgetPopup() {
    const auto body = contentBody();
    return {body.x + (body.width - 190) / 2, body.y + 32, 190, 72};
}
MenuGrid::Rect touchForgetChoice(bool yes) {
    const auto popup = touchForgetPopup();
    return {popup.x + (yes ? 44 : 114), popup.y + 32, 64, 40};
}
}

namespace Ui {
using namespace MovesDrawing;
using Flow = Features::Moves::Flow;

void MovesUi::resetDescription() {
    descScrollKey = -1;
    descScroll = 0.0f;
    descriptionTouch.cancel();
    descriptionGesture = false;
}

void MovesUi::resetPresentation() {
    moveListScroll = 0.0f;
    resetDescription();
    cancelTouch();
}

void MovesUi::reset() {
    resetFlow();
    moveForgetSlot = 0;
    moveForgetConfirmOpen = moveForgetConfirmYes = false;
    resetPresentation();
}

void MovesUi::openManage(uint8_t slot) {
    reset();
    manage(slot);
}

bool MovesUi::openRecall(uint8_t slot) {
    reset();
    const uint8_t count = host.collectPageRecallable(slot, recallMoveIds, MAX_RECALLABLE_MOVE_COUNT);
    return recall(slot, count);
}

bool MovesUi::onButton(const ButtonEvent& event) {
    cancelTouch();
    if (!host.pageExploreContext() && event.btn == 1 && event.action == BtnAction::LONG_PRESS)
        host.pageBackTutorial();
    if (event.btn == 0 && event.action == BtnAction::LONG_PRESS) {
        host.pageHome();
        return true;
    }
    if (event.btn == 1 && event.action == BtnAction::LONG_PRESS) {
        if (moveForgetConfirmOpen) moveForgetConfirmOpen = false;
        else if (movePageMode == MovePageMode::RECALL_REPLACE) {
            returnToRecall();
            resetPresentation();
            moveListScroll = touchMoveLayout().reveal(moveCursor, 0, touchMoveRowCount());
        } else host.pageBack();
        return true;
    }
    const auto& state = host.pageState();
    if (!valid(state)) { host.pageBack(); return true; }
    const uint8_t count = touchMoveRowCount();
    if (moveCursor >= count) moveCursor = count ? count - 1 : 0;
    if (event.btn == 1 && event.action == BtnAction::PRESSED) {
        if (moveForgetConfirmOpen) moveForgetConfirmYes = !moveForgetConfirmYes;
        else {
            if (count) moveCursor = (moveCursor + 1) % count;
            moveListScroll = touchMoveLayout().reveal(moveCursor, moveListScroll, count);
            resetDescription();
        }
        return true;
    }
    if (event.btn != 0 || event.action != BtnAction::PRESSED) return false;
    if (movePageMode == MovePageMode::MANAGE && moveForgetConfirmOpen) {
        if (moveForgetConfirmYes) forgetMove(moveForgetSlot);
        moveForgetConfirmOpen = moveForgetConfirmYes = false;
        resetDescription();
        if (moveCursor >= touchMoveRowCount()) moveCursor = 0;
        return true;
    }
    if (!count) return true;
    const auto action = select(state);
    if (action.kind == Features::Moves::Selection::FORGET) {
        moveForgetSlot = action.slot;
        moveForgetConfirmOpen = true;
        moveForgetConfirmYes = false;
    } else if (action.kind == Features::Moves::Selection::RECALL_CHOICE) {
        resetPresentation();
        moveListScroll = touchMoveLayout().reveal(moveCursor, 0, touchMoveRowCount());
    } else execute(action);
    return true;
}

float MovesUi::descriptionMaximum() const {
    const auto* move = touchMoveInfo(moveCursor);
    if (!move) return 0;
    const auto area = descriptionArea();
    return std::max(0, StatusContent::wrappedTextLineCount(move->description, area.width - 6) * 16 - area.height);
}

bool MovesUi::syncDescription() {
    const int key = descriptionKey(host.pageState(), moveCursor);
    const float before = descScroll;
    const bool changed = key != descScrollKey;
    if (changed) { resetDescription(); descScrollKey = key; }
    descScroll = std::clamp(descScroll, 0.0f, descriptionMaximum());
    return changed || before != descScroll;
}

void MovesUi::renderDescription(const char* text, uint16_t color) {
    const auto area = descriptionArea();
    auto& c = PixelRenderer::canvas();
    c.setClipRect(area.x, area.y, area.width, area.height);
    StatusContent::wrapText(text, area.width - 6, area.x, area.y - int(descScroll), color, true);
    const float maximum = descriptionMaximum();
    if (maximum > 0) {
        const int thumb = std::max(14, int(area.height * area.height / (area.height + maximum)));
        const int top = area.y + int((area.height - thumb) * descScroll / maximum);
        c.drawFastVLine(area.x + area.width - 3, area.y, area.height, PixelRenderer::rgb(55, 63, 76));
        c.drawFastVLine(area.x + area.width - 3, top, thumb, PixelRenderer::rgb(255, 216, 72));
    }
    c.clearClipRect();
}

void MovesUi::cancelTouch() {
    Scene::cancelTouch();
    gridTouch.cancel();
    descriptionTouch.cancel();
    descriptionGesture = false;
}

bool MovesUi::onTouch(const Platform::IInputDevice::TouchEvent& event) {
    using Type = Platform::IInputDevice::TouchType;
    syncDescription();
    if (event.type == Type::DOWN) {
        cancelTouch();
        descriptionGesture = !moveForgetConfirmOpen && descriptionArea().contains(event.x, event.y);
    }
    if (descriptionGesture) {
        const auto result = descriptionTouch.update(event, -1, descScroll, descriptionMaximum(), true);
        if (event.type == Type::UP || event.type == Type::CANCEL) descriptionGesture = false;
        return result.changed;
    }
    const auto layout = touchMoveLayout();
    const auto result = gridTouch.update(event, touchTarget(event.x, event.y), moveListScroll,
        layout.maxScroll(touchMoveRowCount()),
        layout.viewport.contains(event.x, event.y) && !moveForgetConfirmOpen);
    const bool activated = result.tap >= 0 && onTap(result.tap);
    return result.changed || activated;
}

int MovesUi::touchTarget(int16_t x, int16_t y) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return -1;
    if (Ui::RoundPage::NATIVE_BACK.contains(x, y)) return 230;
    if (moveForgetConfirmOpen) {
        if (touchForgetChoice(true).contains(x, y)) return 200;
        if (touchForgetChoice(false).contains(x, y)) return 201;
        return -1;
    }
    return touchMoveLayout().itemAt(x, y, moveListScroll, touchMoveRowCount());
}

bool MovesUi::onTap(int target) {
    if (target == 230) return onButton({1, BtnAction::LONG_PRESS});
    if (!moveForgetConfirmOpen) {
        if (target < 0 || target >= touchMoveRowCount()) return false;
        const bool activate = moveCursor == target;
        moveCursor = target;
        resetDescription();
        moveListScroll = touchMoveLayout().reveal(moveCursor, moveListScroll, touchMoveRowCount());
        return activate ? onButton({0, BtnAction::PRESSED}) : true;
    }
    if (target != 200 && target != 201) return false;
    moveForgetConfirmYes = target == 200;
    host.pageNavigationTutorial();
    return onButton({0, BtnAction::PRESSED});
}

SceneUpdateResult MovesUi::update(uint32_t, float) {
    RenderDemand demand;
    const float before = moveListScroll;
    moveListScroll = std::clamp(moveListScroll, 0.0f, touchMoveLayout().maxScroll(touchMoveRowCount()));
    demand.changed(before != moveListScroll);
    demand.changed(syncDescription());
    return demand.result();
}

void MovesUi::render() {
    auto& c = PixelRenderer::canvas();
    const auto& state = host.pageState();
    if (moveMonsterIndex >= state.teamCount) {
        host.pageBack();
        return;
    }

    const uint8_t rowCount = touchMoveRowCount();
    if (moveCursor >= rowCount) moveCursor = rowCount ? rowCount - 1 : 0;
    syncDescription();

    const auto body = contentBody();
    auto sx = [body](int x) { return body.x + x; };
    auto sy = [body](int y) { return body.y + y; };
    c.fillRect(0, 0, WIDTH, HEIGHT, 0);
    moveListScroll = std::clamp(moveListScroll, 0.0f, touchMoveLayout().maxScroll(rowCount));
    static constexpr int LEFT_W = 90;
    static constexpr int RIGHT_X = LEFT_W + 7;
    static constexpr int LIST_Y = 6;
    const auto list = touchMoveLayout();
    static constexpr int ROW_H = 44;
    static constexpr int TEXT_X = 7;

    c.fillRect(0, 52, WIDTH, HEIGHT - 52,
               PixelRenderer::rgb(10, 14, 20));
    c.drawFastVLine(sx(LEFT_W), sy(6), body.height - 12,
                    PixelRenderer::rgb(123, 125, 123));

    c.setClipRect(body.x, sy(LIST_Y), LEFT_W, body.height - LIST_Y * 2);
    for (uint8_t index = 0; index < rowCount; ++index) {
        int y = list.cell(index, moveListScroll).y;
        if (y + ROW_H < sy(LIST_Y) || y >= body.y + body.height - LIST_Y) continue;
        bool selected = index == moveCursor;
        uint16_t color = selected
            ? PixelRenderer::rgb(255, 216, 72)
            : PixelRenderer::rgb(241, 242, 232);
        if (selected) {
            c.fillRect(sx(2), y + 13, 3, 18, PixelRenderer::rgb(255, 216, 72));
        }

        const MoveInfo* rowMove = info(state, index);

        if (rowMove) {
            PixelRenderer::text(sx(TEXT_X), y + 14, rowMove->name, color, 1);
        } else {
            PixelRenderer::text(sx(TEXT_X), y + 14,
                                Ui::Team::MOVE_UNLEARNED, color, 1);
        }
        if (index + 1 < rowCount) {
            c.drawFastHLine(sx(5), y + ROW_H - 1, LEFT_W - 10,
                            PixelRenderer::rgb(55, 63, 76));
        }
    }
    c.clearClipRect();

    uint8_t proficiency = 0;
    const MoveInfo* move = info(state, moveCursor, &proficiency);
    const char* emptyHint = move ? nullptr : Ui::Team::MOVE_UNLEARNED;

    c.setClipRect(sx(RIGHT_X), body.y, body.width - RIGHT_X, body.height);
    if (move) {
        int moveNameX = drawTypeBracket(sx(RIGHT_X + 2), sy(7), move->type);
        PixelRenderer::text(moveNameX + 3, sy(7), move->name,
                            PixelRenderer::rgb(241, 242, 232), 1);

        char line[48];
        char power[8];
        char accuracy[8];
        if (move->power > 0) {
            snprintf(power, sizeof(power), "%u", move->power);
        } else {
            snprintf(power, sizeof(power), "--");
        }
        if (move->accuracy > 0) {
            snprintf(accuracy, sizeof(accuracy), "%u", move->accuracy);
        } else {
            snprintf(accuracy, sizeof(accuracy), "--");
        }
        snprintf(line, sizeof(line), Ui::Team::MOVE_POWER_ACCURACY_FMT,
                 power, accuracy);
        PixelRenderer::text(sx(RIGHT_X + 2), sy(31), line,
                            PixelRenderer::rgb(255, 216, 72), 1);
        PixelRenderer::text(sx(RIGHT_X + 2), sy(52), Ui::Team::MOVE_PROFICIENCY,
                            PixelRenderer::rgb(135, 214, 238), 1);
        drawProficiencyBar(sx(RIGHT_X + 48), sy(56), 86, 9, proficiency);

        renderDescription(move->description, PixelRenderer::rgb(255, 218, 178));
    } else if (emptyHint) {
        int detailWidth = body.width - RIGHT_X;
        int hintX = sx(RIGHT_X) + MathUtil::max(
            2, (detailWidth - textPixelWidth(emptyHint)) / 2);
        PixelRenderer::text(hintX, sy(58), emptyHint,
                            PixelRenderer::rgb(156, 164, 176), 1);
    }

    c.clearClipRect();
    RoundPage::nativeHeader(Ui::Team::ACTION_MOVES,
        gridTouch.pressed() == 230);
    if (movePageMode == MovePageMode::MANAGE && moveForgetConfirmOpen) {
        renderMoveForgetConfirmPopup();
    }
}

void MovesUi::renderMoveForgetConfirmPopup() {
    const auto& state = host.pageState();
    if (moveMonsterIndex >= state.teamCount ||
        moveForgetSlot == 0 ||
        moveForgetSlot >= Game::MOVE_SLOT_COUNT) {
        moveForgetConfirmOpen = false;
        return;
    }
    const Game::MonsterRuntime& mon = state.team[moveMonsterIndex];
    const Species& species = speciesFor(mon);
    const MoveInfo* move = moveInfoForSlot(species, mon, moveForgetSlot);
    if (!move) {
        moveForgetConfirmOpen = false;
        return;
    }

    auto& c = PixelRenderer::canvas();
    const auto popup = touchForgetPopup();
    const int POP_X = popup.x;
    const int POP_Y = popup.y;
    const int POP_W = popup.width;
    const int POP_H = popup.height;
    c.fillRect(POP_X, POP_Y, POP_W, POP_H, PixelRenderer::rgb(24, 28, 36));
    c.drawRect(POP_X, POP_Y, POP_W, POP_H,
               PixelRenderer::rgb(241, 242, 232));

    char line[64];
    snprintf(line, sizeof(line), Ui::Team::MOVE_FORGET_CONFIRM_FMT, move->name);
    int lineX = POP_X + (POP_W - textPixelWidth(line)) / 2;
    if (lineX < POP_X + 5) lineX = POP_X + 5;
    PixelRenderer::text(lineX, POP_Y + 17, line,
                        PixelRenderer::rgb(241, 242, 232), 1);

    uint16_t yesColor = moveForgetConfirmYes
        ? PixelRenderer::rgb(255, 216, 72)
        : PixelRenderer::rgb(156, 164, 176);
    uint16_t noColor = !moveForgetConfirmYes
        ? PixelRenderer::rgb(255, 216, 72)
        : PixelRenderer::rgb(156, 164, 176);
    PixelRenderer::text(POP_X + 60, POP_Y + 49, Ui::Team::YES, yesColor, 1);
    PixelRenderer::text(POP_X + 130, POP_Y + 49, Ui::Team::NO, noColor, 1);
}


}  // namespace Ui
