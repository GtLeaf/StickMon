#include "ui/compact_touch/DebugUi.h"
#if STICKMON_ENABLE_DEBUG_FEATURES
#include <cstdio>
#include <cmath>
#include "ui/round_touch/RoundPage.h"

#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "presentation/MenuGrid.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TouchUi.h"
namespace Ui {
namespace {
constexpr int BACK_TARGET = 230;
constexpr int POPUP_FIRST = 100;
constexpr int WIDTH = 360;
constexpr int HEIGHT = 360;
MenuGrid::Layout pageLayout() {
    return {{0, 56, WIDTH, 264}, 1, WIDTH, 48, 0, 8, 0, 4};
}
MenuGrid::Rect rowBounds(int index, float scroll) {
    auto row = pageLayout().cell(index, scroll);
    const int top = std::clamp(row.y, 56, 320);
    const int bottom = std::clamp(row.y + row.height, 56, 320);
    const int dy = std::max(std::abs(top - 180), std::abs(bottom - 180));
    const int inset = 184 - int(std::sqrt(float(176 * 176 - dy * dy)));
    row.x = inset;
    row.width = WIDTH - 2 * inset;
    return row;
}
MenuGrid::Rect digitRect(int index, bool time) {
    return {(time ? 88 : 112) + index * 48, 154, 40, 40};
}
MenuGrid::Rect popupButton(bool cancel) { return {cancel ? 192 : 80, 214, 96, 38}; }
}  // namespace

DebugLayout DebugUi::layout() const { return {WIDTH, HEIGHT, HEIGHT}; }
void DebugUi::reset() { cancelTouch(); calibrationNotice = nullptr; DebugUiBase::reset(); }

const char* DebugUi::title() const {
    switch (debugCategory) {
    case DebugCategory::MONSTER: return Ui::Debug::CATEGORY_MONSTER;
    case DebugCategory::RESOURCE: return Ui::Debug::CATEGORY_RESOURCE;
    case DebugCategory::ENV: return Ui::Debug::CATEGORY_ENV;
    case DebugCategory::MOTION: return Ui::Debug::CATEGORY_MOTION;
    case DebugCategory::BATTLE: return Ui::Debug::CATEGORY_BATTLE;
    case DebugCategory::CONTACT_EVENT: return Ui::Debug::CATEGORY_CONTACT_EVENT;
    case DebugCategory::TOUCH_TEST: return Ui::Debug::TOUCH_TEST;
    default: return Ui::DEBUG;
    }
}

void DebugUi::render() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, WIDTH, HEIGHT, PixelRenderer::rgb(7, 12, 17));
    if (debugCategory == DebugCategory::TOUCH_TEST) {
        renderTouchTest();
        return;
    }
    const auto list = pageLayout();
    const auto body = list.viewport;
    if (debugCursor >= visibleItemCount()) debugCursor = 0;
    debugScroll = std::clamp(debugScroll, 0.0f, list.maxScroll(visibleItemCount()));
    c.setClipRect(body.x, body.y, body.width, body.height);
    for (uint8_t i = 0; i < visibleItemCount(); ++i) {
        const auto row = rowBounds(i, debugScroll);
        if (row.y + row.height <= body.y || row.y >= body.y + body.height) continue;
        MenuGrid::card(c, row, listTouch.pressed() == i, debugCursor == i);
        PixelRenderer::text(row.x + 12, row.y + 14, debugItemLabel(i), 0xffff);
        char buffer[24] = {};
        if (const char* value = debugItemValue(i, buffer, sizeof(buffer)))
            MenuGrid::label(c, row.x + row.width - 66, row.y + 14, value, 124, 0x7bef);
    }
    c.clearClipRect();
    RoundPage::nativeHeader(title(), listTouch.pressed() == BACK_TARGET);
    char position[24];
    std::snprintf(position, sizeof(position), "%d / %d", debugCursor + 1, visibleItemCount());
    MenuGrid::label(c, 180, 330, position, 100, 0x7bef);
    if (debugSwitchOpen || debugTimeOpen) renderPopup();
}

void DebugUi::renderTouchTest() {
    if (calibrationOpen) { renderCalibration(); return; }
    auto& c = PixelRenderer::canvas();
    RoundPage::nativeHeader(Ui::Debug::TOUCH_TEST, touchHeaderDown);
    if (calibrationNotice) MenuGrid::label(c, 180, 74, calibrationNotice, 244, 0x67df);
    char buffer[64] = {};
    if (debugTouchTest.count < TouchTest::COUNT) {
        const auto next = TouchTest::TARGETS[debugTouchTest.count];
        std::snprintf(buffer, sizeof(buffer), "%d/9  %d,%d", debugTouchTest.count, next.x, next.y);
    } else {
        std::snprintf(buffer, sizeof(buffer), "9/9  COMPLETE");
    }
    MenuGrid::label(c, 180, 96, buffer, 232, 0xffe0);
    for (int i = 0; i < TouchTest::COUNT; ++i) {
        const auto target = TouchTest::TARGETS[i];
        const uint16_t color = i == debugTouchTest.count ? 0xffe0
            : i < debugTouchTest.count ? PixelRenderer::rgb(115, 226, 183)
                                       : PixelRenderer::rgb(85, 103, 113);
        c.drawCircle(target.x, target.y, 10, color);
        c.drawFastHLine(target.x - 14, target.y, 29, color);
        c.drawFastVLine(target.x, target.y - 14, 29, color);
        const int labelX = target.x + (target.x < 180 ? 17 : -23);
        const int labelY = target.y + (target.y < 180 ? 15 : -29);
        char number[] = {char('1' + i), '\0'};
        PixelRenderer::text(labelX, labelY, number, color);
    }
    if (debugTouchTest.lastValid) {
        if (debugTouchTest.lastWasUp) {
            std::snprintf(buffer, sizeof(buffer), "D %d,%d  U %d,%d",
                          debugTouchTest.pending.down.x, debugTouchTest.pending.down.y,
                          debugTouchTest.pending.up.x, debugTouchTest.pending.up.y);
        } else {
            std::snprintf(buffer, sizeof(buffer), "LIVE %d,%d",
                          debugTouchTest.last.x, debugTouchTest.last.y);
        }
        MenuGrid::label(c, 180, 208, buffer, 280, debugTouchTest.pressed ? 0xf471 : 0x7bef);
    }
    if (debugTouchTest.count) {
        float dx, dy, error;
        debugTouchTest.statistics(dx, dy, error);
        std::snprintf(buffer, sizeof(buffer), "dX%+.1f dY%+.1f MAX%.1f", dx, dy, error);
        MenuGrid::label(c, 180, 230, buffer, 224, 0x7bef);
    }
    const auto controlPressed = [&](int control) {
        return debugTouchTest.pressed && TouchTest::controlAt(debugTouchTest.pending.down) == control &&
            TouchTest::controlAt(debugTouchTest.last) == control;
    };
    TouchUi::action(TouchTest::CLEAR_X, TouchTest::CLEAR_Y,
                    TouchTest::CLEAR_W, "重测", controlPressed(0));
    TouchUi::action(TouchTest::BACK_X, TouchTest::BACK_Y,
                    TouchTest::BACK_W, Ui::BACK, controlPressed(1));
    if (Platform::input().touchCalibrationSupported())
        TouchUi::action(TouchTest::CALIBRATE_X, TouchTest::CLEAR_Y,
                        TouchTest::CALIBRATE_W, Ui::Debug::TOUCH_CALIBRATE, calibrationButtonDown);
    for (int i = 0; i < debugTouchTest.count; ++i) {
        const auto& point = debugTouchTest.samples[i];
        c.fillCircle(point.down.x, point.down.y, 3, 0xf471);
        c.drawCircle(point.up.x, point.up.y, 5, 0x67df);
    }
    if (debugTouchTest.pressed)
        c.drawCircle(debugTouchTest.last.x, debugTouchTest.last.y, 5, 0xf471);
}

void DebugUi::handleTouchTest(const Platform::IInputDevice::TouchEvent& event) {
    if (calibrationOpen) { handleCalibration(event); return; }
    using Type = Platform::IInputDevice::TouchType;
    const bool calibrationButton = Platform::input().touchCalibrationSupported() &&
        TouchTest::inRect({event.x, event.y}, TouchTest::CALIBRATE_X,
                         TouchTest::CLEAR_Y, TouchTest::CALIBRATE_W, TouchTest::CLEAR_H);
    if (event.type == Type::DOWN && calibrationButton) {
        calibrationButtonDown = true;
        return;
    }
    if (calibrationButtonDown) {
        if (event.type == Type::CANCEL || event.type == Type::UP) {
            const bool activate = event.type == Type::UP && calibrationButton;
            calibrationButtonDown = false;
            if (activate) openCalibration();
        } else if (event.type == Type::MOVE && !calibrationButton) calibrationButtonDown = false;
        return;
    }
    const bool header = RoundPage::NATIVE_BACK.contains(event.x, event.y);
    if (event.type == Type::DOWN && header) {
        touchHeaderDown = true;
        return;
    }
    if (touchHeaderDown) {
        if (event.type == Type::MOVE && !header) touchHeaderDown = false;
        if (event.type == Type::CANCEL || event.type == Type::UP) {
            const bool activate = event.type == Type::UP && header;
            touchHeaderDown = false;
            if (activate) {
                debugCategory = DebugCategory::ROOT;
                debugCursor = 0;
                debugScroll = 0.0f;
            }
        }
        return;
    }
    if (event.type == Type::DOWN) {
        debugTouchTest.down({event.x, event.y});
    } else if (event.type == Type::MOVE) {
        debugTouchTest.move({event.x, event.y});
    } else if (event.type == Type::CANCEL) {
        debugTouchTest.cancel();
    } else {
        const auto result = debugTouchTest.up({event.x, event.y});
        if (result == TouchTest::Result::BACK) {
            debugCategory = DebugCategory::ROOT;
            debugCursor = 0;
            debugScroll = 0.0f;
        } else if (result == TouchTest::Result::SAMPLE) {
            const int index = debugTouchTest.count - 1;
            const auto& sample = debugTouchTest.samples[index];
            Platform::logf("[TouchTest] target=%d down=%d,%d up=%d,%d\n",
                index + 1, sample.down.x, sample.down.y,
                sample.up.x, sample.up.y);
        } else if (result == TouchTest::Result::CLEAR) {
            Platform::logLine("[TouchTest] cleared");
        }
    }
}

void DebugUi::renderPopup() {
    auto& c = PixelRenderer::canvas();
    const bool time = debugTimeOpen;
    const int digits = time ? 4 : 3;
    const int focus = time ? debugTimeFocus : debugSwitchFocus;
    c.fillRoundRect(64, 94, 240, 164, 8, PixelRenderer::rgb(18, 22, 30));
    c.drawRoundRect(64, 94, 240, 164, 8, 0x7bef);
    PixelRenderer::text(80, 106, time ? Ui::Debug::SET_TIME : Ui::Debug::SWITCH_MON, 0x47ff);
    PixelRenderer::text(80, 132, time ? Ui::Debug::TARGET_TIME : Ui::Debug::INPUT_ID, 0xffff);
    for (int i = 0; i < digits; ++i) {
        const auto rect = digitRect(i, time);
        MenuGrid::card(c, rect, listTouch.pressed() == POPUP_FIRST + i, focus == i);
        char value[] = {char('0' + (time ? debugTimeDigits[i] : debugSwitchDigits[i])), '\0'};
        PixelRenderer::text(rect.x + 16, rect.y + 13, value, 0xffff);
    }
    if (time) PixelRenderer::text(180, 166, ":", 0xffff);
    for (int i = 0; i < 2; ++i) {
        const auto rect = popupButton(i == 1);
        MenuGrid::card(c, rect, listTouch.pressed() == POPUP_FIRST + digits + i, focus == digits + i);
        MenuGrid::label(c, rect.x + rect.width / 2, rect.y + 11,
            i == 0 ? Ui::Debug::YES : Ui::Debug::CANCEL, rect.width - 12, 0xffff);
    }
}

bool DebugUi::onButton(const ButtonEvent& event) {
    const bool wasCalibrating = calibrationOpen;
    cancelTouch();
    if (wasCalibrating) return true;
    if (debugCategory == DebugCategory::TOUCH_TEST)
        return DebugUiBase::onButton(event);
    if (!debugSwitchOpen && !debugTimeOpen) {
        if (debugCursor >= visibleItemCount()) debugCursor = 0;
        if (event.btn == 1 && event.action == BtnAction::PRESSED) {
            debugCursor = (debugCursor + 1) % visibleItemCount();
            debugScroll = pageLayout().reveal(debugCursor, debugScroll, visibleItemCount());
            if (debugCursor == 0) debugScroll = 0;
            return true;
        }
    }
    // The shared action can synchronously destroy this page.
    return DebugUiBase::onButton(event);
}

void DebugUi::cancelTouch() {
    closeCalibration();
    Scene::cancelTouch();
    listTouch.cancel();
    debugTouchTest.cancel();
    touchHeaderDown = false;
    calibrationButtonDown = false;
}

bool DebugUi::onTouch(const Platform::IInputDevice::TouchEvent& event) {
    if (debugCategory == DebugCategory::TOUCH_TEST) {
        handleTouchTest(event);
        return true;
    }
    const auto list = pageLayout();
    const auto result = listTouch.update(event, touchTarget(event.x, event.y), debugScroll,
        list.maxScroll(visibleItemCount()),
        !debugSwitchOpen && !debugTimeOpen && list.viewport.contains(event.x, event.y));
    const bool activated = result.tap >= 0 && onTap(result.tap);
    return result.changed || activated;
}

int DebugUi::touchTarget(int16_t x, int16_t y) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return -1;
    if (RoundPage::NATIVE_BACK.contains(x, y)) return BACK_TARGET;
    if (debugSwitchOpen || debugTimeOpen) {
        const bool time = debugTimeOpen;
        const int digits = time ? 4 : 3;
        for (int i = 0; i < digits; ++i)
            if (digitRect(i, time).contains(x, y)) return POPUP_FIRST + i;
        for (int i = 0; i < 2; ++i)
            if (popupButton(i == 1).contains(x, y)) return POPUP_FIRST + digits + i;
        return -1;
    }
    const int item = pageLayout().itemAt(x, y, debugScroll, visibleItemCount());
    return item >= 0 && rowBounds(item, debugScroll).contains(x, y) ? item : -1;
}

bool DebugUi::onTap(int target) {
    if (target == BACK_TARGET) return onButton({1, BtnAction::LONG_PRESS});
    if (debugSwitchOpen || debugTimeOpen) {
        const int focus = target - POPUP_FIRST;
        const int count = debugTimeOpen ? DEBUG_TIME_FOCUS_COUNT : DEBUG_SWITCH_FOCUS_COUNT;
        if (focus < 0 || focus >= count) return false;
        if (debugTimeOpen) debugTimeFocus = focus;
        else debugSwitchFocus = focus;
    } else {
        if (target < 0 || target >= visibleItemCount()) return false;
        debugCursor = target;
    }
    return onButton({0, BtnAction::PRESSED});
}
namespace {
int controlAt(int x, int y) {
    if (RoundPage::NATIVE_BACK.contains(x, y)) return 0;
    if (TouchTest::inRect({x,y}, 98, 250, 76, 34)) return 1;
    if (TouchTest::inRect({x,y}, 186, 250, 76, 34)) return 2;
    return -1;
}
}  // namespace
void DebugUi::openCalibration() {
    debugTouchTest.cancel();
    calibration = {};
    calibrationOpen = true;
    calibrationControl = -1;
    calibrationNotice = nullptr;
    // Keep the intro on the active mapping so its controls don't jump.
    Platform::logLine("[TouchCalibration] opened");
}
void DebugUi::closeCalibration() {
    if (!calibrationOpen) return;
    calibrationOpen = false;
    calibrationControl = -1;
    calibrationNotice = nullptr;
    calibration.cancelContact();
    Platform::input().bypassTouchCalibration(false);
    Platform::logLine("[TouchCalibration] closed");
}
void DebugUi::renderCalibration() {
    auto& c = PixelRenderer::canvas();
    RoundPage::nativeHeader(Ui::Debug::TOUCH_CALIBRATION_TITLE, calibrationControl == 0);
    char buffer[80]{};
    using Stage = TouchCalibrationSession::Stage;
    if (calibration.stage == Stage::INTRO) {
        MenuGrid::label(c, 180, 108, Ui::Debug::TOUCH_CALIBRATION_ORDER, 296, 0xffff);
        MenuGrid::label(c, 180, 138, Ui::Debug::TOUCH_CALIBRATION_HOLD, 296, 0x7bef);
        MenuGrid::label(c, 180, 174, Ui::Debug::TOUCH_CALIBRATION_FIT, 296, 0x7bef);
        MenuGrid::label(c, 180, 206, Ui::Debug::TOUCH_CALIBRATION_SAVE_HINT, 296, 0x7bef);
    } else if (calibration.stage == Stage::COLLECT) {
        const auto target = TouchTest::TARGETS[calibration.count];
        std::snprintf(buffer, sizeof(buffer), "%d/9  %s", calibration.count+1,
                      calibration.count == 8 ? "CENTER CHECK" : "HOLD & RELEASE");
        MenuGrid::label(c, 180, 96, buffer, 268, 0xffe0);
        c.drawCircle(target.x, target.y, 10, 0xffe0);
        c.drawFastHLine(target.x-14, target.y, 29, 0xffe0);
        c.drawFastVLine(target.x, target.y-14, 29, 0xffe0);
        MenuGrid::label(c, 180, 208, Ui::Debug::TOUCH_CALIBRATION_AIM, 296, 0x7bef);
    } else {
        MenuGrid::label(c, 180, 112, Ui::Debug::TOUCH_CALIBRATION_READY, 280, 0x67df);
        std::snprintf(buffer, sizeof(buffer), "RMS %.1f  MAX %.1f px", calibration.rms, calibration.maximum);
        MenuGrid::label(c, 180, 150, buffer, 280, 0x7bef);
        std::snprintf(buffer, sizeof(buffer), "CENTER %.1f px", calibration.centerError);
        MenuGrid::label(c, 180, 182, buffer, 280, 0x7bef);
    }
    if (calibrationNotice) MenuGrid::label(c, 180, 230, calibrationNotice, 296, 0xf471);
    const char* left = calibration.stage == Stage::INTRO ? Ui::Debug::TOUCH_CALIBRATION_START :
        calibration.stage == Stage::READY ? Ui::Debug::TOUCH_CALIBRATION_SAVE : Ui::Debug::TOUCH_CALIBRATION_RETRY;
    TouchUi::action(98, 250, 76, left, calibrationControl == 1);
    if (calibration.stage != Stage::COLLECT)
        TouchUi::action(186, 250, 76, calibration.stage == Stage::INTRO ? Ui::Debug::TOUCH_CALIBRATION_RESTORE : Ui::Debug::TOUCH_CALIBRATION_RETRY,
                        calibrationControl == 2);
}
void DebugUi::handleCalibration(const Platform::IInputDevice::TouchEvent& event) {
    using Type = Platform::IInputDevice::TouchType;
    using Stage = TouchCalibrationSession::Stage;
    int control = controlAt(event.x, event.y);
    if (calibration.stage == Stage::COLLECT && control == 2) control = -1;
    if (event.type == Type::CANCEL) {
        calibrationControl = -1;
        calibration.cancelContact();
        return;
    }
    if (event.type == Type::DOWN) {
        calibrationControl = control;
        if (control < 0 && calibration.stage == Stage::COLLECT)
            calibration.down(event.x, event.y, event.timestampMs);
        return;
    }
    if (event.type == Type::MOVE) {
        if (calibrationControl >= 0) {
            if (control != calibrationControl) calibrationControl = -2;
        } else if (calibrationControl == -1) calibration.move(event.x, event.y);
        return;
    }
    if (event.type != Type::UP) return;
    const int pressed = calibrationControl;
    calibrationControl = -1;
    if (pressed >= 0) {
        if (pressed != control) return;
        if (pressed == 0) { closeCalibration(); return; }
        if (calibration.stage == Stage::INTRO && pressed == 2) {
            if (Platform::input().saveTouchCalibration({})) {
                closeCalibration(); debugTouchTest = {};
                calibrationNotice = Ui::Debug::TOUCH_CALIBRATION_RESTORED;
            } else calibrationNotice = Ui::Debug::TOUCH_CALIBRATION_SAVE_FAILED;
        } else if (calibration.stage == Stage::READY && pressed == 1) {
            if (Platform::input().saveTouchCalibration(calibration.transform)) {
                closeCalibration(); debugTouchTest = {};
                calibrationNotice = Ui::Debug::TOUCH_CALIBRATION_SAVED;
            } else calibrationNotice = Ui::Debug::TOUCH_CALIBRATION_SAVE_FAILED;
        } else {
            calibration.start(); calibrationNotice = nullptr;
            Platform::input().bypassTouchCalibration(true);
            Platform::logLine("[TouchCalibration] sampling uncorrected screen coordinates");
        }
        return;
    }
    if (pressed != -1 || calibration.stage != Stage::COLLECT) return;
    const int target = calibration.count;
    const auto result = calibration.up(event.timestampMs);
    using Result = TouchCalibrationSession::Result;
    calibrationNotice = nullptr;
    if (result == Result::HOLD) calibrationNotice = Ui::Debug::TOUCH_CALIBRATION_UNSTABLE;
    if (result == Result::FIT_FAILED) calibrationNotice = Ui::Debug::TOUCH_CALIBRATION_FIT_FAILED;
    if (result == Result::CENTER_FAILED) calibrationNotice = Ui::Debug::TOUCH_CALIBRATION_CENTER_FAILED;
    Platform::logf("[TouchCalibration] target=%d raw=%d,%d result=%d count=%d rms=%.2f max=%.2f center=%.2f\n",
        target+1, calibration.last.x, calibration.last.y, int(result), calibration.count,
        calibration.rms, calibration.maximum, calibration.centerError);
}
}  // namespace Ui
#endif
