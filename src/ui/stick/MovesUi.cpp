#include "ui/stick/MovesUi.h"
#include <algorithm>
#include <cstdio>
#include "core/MathUtil.h"
#include "core/UiMotion.h"
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "presentation/MenuGrid.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TouchUi.h"

namespace Ui {
using namespace MovesDrawing;
using Flow = Features::Moves::Flow;

void MovesUi::resetDescription() {
    descScrollKey = -1;
    descScroll = 0.0f;
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

        } else host.pageBack();
        return true;
    }
    const auto& state = host.pageState();
    if (!valid(state)) { host.pageBack(); return true; }
    clampCursor(state);
    const uint8_t count = rowCount(state);
    if (event.btn == 1 && event.action == BtnAction::PRESSED) {
        if (moveForgetConfirmOpen) moveForgetConfirmYes = !moveForgetConfirmYes;
        else {
            moveCursor = (moveCursor + 1) % count;

            resetDescription();
        }
        return true;
    }
    if (event.btn != 0 || event.action != BtnAction::PRESSED) return false;
    if (movePageMode == MovePageMode::MANAGE && moveForgetConfirmOpen) {
        if (moveForgetConfirmYes) forgetMove(moveForgetSlot);
        moveForgetConfirmOpen = moveForgetConfirmYes = false;
        resetDescription();
        clampCursor(host.pageState());
        return true;
    }
    const auto action = select(state);
    if (action.kind == Features::Moves::Selection::FORGET) {
        moveForgetSlot = action.slot;
        moveForgetConfirmOpen = true;
        moveForgetConfirmYes = false;
    } else if (action.kind == Features::Moves::Selection::RECALL_CHOICE) {
        resetPresentation();

    } else execute(action);
    return true;
}

bool MovesUi::updateDescriptionScroll(int scrollKey, int maxScroll) {
    float before = descScroll;
    int previousKey = descScrollKey;
    if (scrollKey != descScrollKey) {
        descScrollKey = scrollKey;
        descScroll = 0.0f;
        descScrollLastMs = Hal::ins().millis();
    }
    if (maxScroll <= 0) {
        descScroll = 0.0f;
        descScrollLastMs = Hal::ins().millis();
        return previousKey != descScrollKey || before != descScroll;
    }
    uint32_t now = Hal::ins().millis();
    uint32_t dt = now - descScrollLastMs;
    if (dt > 120) dt = 120;
    descScrollLastMs = now;
    descScroll += 28.0f * (float(dt) / 1000.0f);
    if (descScroll > float(maxScroll + 18)) descScroll = 0.0f;
    return previousKey != descScrollKey || before != descScroll;
}

void MovesUi::renderDescription(const char* const* lines, int lineCount,
        int x, int y, int w, uint16_t color, int scrollKey) {
    if (lineCount <= 0 || w <= 0) return;
    (void)scrollKey;
    auto& c = PixelRenderer::canvas();
    c.setClipRect(x, y - 1, w, lineCount * 16 + 2);
    for (int i = 0; i < lineCount; ++i)
        PixelRenderer::text(x - int(descScroll), y + i * 16, lines[i], color, 1);
    c.clearClipRect();
}


void MovesUi::cancelTouch() { Scene::cancelTouch(); }

bool MovesUi::onTouch(const Platform::IInputDevice::TouchEvent& event) { return Scene::onTouch(event); }

int MovesUi::touchTarget(int16_t x, int16_t y) {
    if (moveForgetConfirmOpen && y >= 75 && y < 104 && x >= 65 && x < 205) return 200 + (x >= 139);
    return -1;
}

bool MovesUi::onTap(int target) {
    if (!moveForgetConfirmOpen || (target != 200 && target != 201)) return false;
    moveForgetConfirmYes = target == 200;
    host.pageNavigationTutorial();
    return onButton({0, BtnAction::PRESSED});
}

SceneUpdateResult MovesUi::update(uint32_t nowMs, float dtSeconds) {
    (void)nowMs; (void)dtSeconds;
    RenderDemand demand;
    const auto& state = host.pageState();
    if (!valid(state)) return demand.result();
    static constexpr int ROW_H = 24;
    static constexpr int VISIBLE_H = TouchUi::CONTENT_H - 12;
    const int maxScroll = std::max(0, int(rowCount(state)) * ROW_H - VISIBLE_H);
    const int target = std::clamp(moveCursor * ROW_H - VISIBLE_H / 2 + ROW_H / 2, 0, maxScroll);
    const auto step = UiMotion::lerp(moveListScroll, float(target), 0.25f, 0.5f);
    demand.changed(step.changed);
    demand.animate(step.active);
    const auto* move = info(state, moveCursor);
    if (move) {
        const int length = textPixelWidth(move->description);
        const int width = Hal::DISPLAY_W - 101;
        const int maximum = length > width ? length - width + 4 : 0;
        demand.changed(updateDescriptionScroll(descriptionKey(state, moveCursor), maximum));
        demand.animate(maximum > 0);
    }
    return demand.result();
}

void MovesUi::render() {
    auto& c = PixelRenderer::canvas();
    const auto& state = host.pageState();
    if (moveMonsterIndex >= state.teamCount) {
        host.pageBack();
        return;
    }

    const uint8_t rowCount = this->rowCount(state);
    if (moveCursor >= rowCount) moveCursor = rowCount - 1;

    const auto body = MenuGrid::Rect{0, 0, Hal::DISPLAY_W, TouchUi::CONTENT_H};
    auto sx = [body](int x) { return body.x + x; };
    auto sy = [body](int y) { return body.y + y; };
    static constexpr int LEFT_W = 90;
    static constexpr int RIGHT_X = LEFT_W + 7;
    static constexpr int LIST_Y = 6;
    static constexpr int ROW_H = 24;
    static constexpr int TEXT_X = 7;

    c.fillRect(body.x, body.y, body.width, body.height,
               PixelRenderer::rgb(10, 14, 20));
    c.drawFastVLine(sx(LEFT_W), sy(6), body.height - 12,
                    PixelRenderer::rgb(123, 125, 123));

    c.setClipRect(body.x, sy(LIST_Y), LEFT_W, body.height - LIST_Y * 2);
    for (uint8_t index = 0; index < rowCount; ++index) {
        int y = sy(LIST_Y) + index * ROW_H - static_cast<int>(moveListScroll);
        if (y + ROW_H < sy(LIST_Y) || y >= body.y + body.height - LIST_Y) continue;
        bool selected = index == moveCursor;
        uint16_t color = selected
            ? PixelRenderer::rgb(255, 216, 72)
            : PixelRenderer::rgb(241, 242, 232);
        if (selected) {
            c.fillRect(sx(2), y + 2, 3, 18, PixelRenderer::rgb(255, 216, 72));
        }

        const MoveInfo* rowMove = info(state, index);
        const bool isBack = index + 1 == rowCount;

        if (rowMove) {
            PixelRenderer::text(sx(TEXT_X), y + 3, rowMove->name, color, 1);
        } else if (isBack) {
            PixelRenderer::text(sx(TEXT_X), y + 3, Ui::BACK, color, 1);
        } else {
            PixelRenderer::text(sx(TEXT_X), y + 3,
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
    const int selectedDescriptionKey = descriptionKey(state, moveCursor);
    const char* emptyHint = nullptr;
    if (!move) {
        if (movePageMode == MovePageMode::RECALL_REPLACE && moveCursor < SPECIAL_MOVE_SLOT_COUNT)
            emptyHint = Ui::Team::MOVE_UNLEARNED;
        else if (movePageMode == MovePageMode::MANAGE) emptyHint = Ui::Team::MOVE_BACK_HINT;
        else if (movePageMode == MovePageMode::RECALL_SELECT) emptyHint = Ui::Team::MOVE_RECALL_BACK_HINT;
        else emptyHint = Ui::Team::MOVE_REPLACE_BACK_HINT;
    }

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

        const char* description[] = {move->description};
        renderDescription(
            description, 1, sx(RIGHT_X + 2), sy(76),
            body.width - RIGHT_X - 4,
            PixelRenderer::rgb(255, 218, 178), selectedDescriptionKey);
    } else if (emptyHint) {
        int detailWidth = body.width - RIGHT_X;
        int hintX = sx(RIGHT_X) + MathUtil::max(
            2, (detailWidth - textPixelWidth(emptyHint)) / 2);
        PixelRenderer::text(hintX, sy(58), emptyHint,
                            PixelRenderer::rgb(156, 164, 176), 1);
    }

    c.clearClipRect();
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
    static constexpr int POP_X = 25;
    static constexpr int POP_Y = 32;
    static constexpr int POP_W = 190;
    static constexpr int POP_H = 72;
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
