#include "ui/compact_touch/DebugUi.h"
#if STICKMON_ENABLE_DEBUG_FEATURES
#include <cstdio>
#include "ui/PageProfile.h"

#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "presentation/MenuGrid.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TouchUi.h"
namespace Ui {
namespace {
constexpr int BACK_TARGET = 230;
constexpr int POPUP_FIRST = 100;
MenuGrid::Layout pageLayout() {
    return MenuGrid::fit(TouchUi::pageBody(), Ui::Page::WIDTH - 16, 44, 0, 6, 8, 4);
}
MenuGrid::Rect digitRect(int index, bool time) {
    return {(time ? 52 : 76) + index * 48, 120, 40, 40};
}
MenuGrid::Rect popupButton(bool cancel) { return {cancel ? 156 : 44, 174, 96, 38}; }
}  // namespace

DebugLayout DebugUi::layout() const { return {Ui::Page::WIDTH, Ui::Page::HEIGHT, Ui::Page::HEIGHT}; }
void DebugUi::reset() { cancelTouch(); DebugUiBase::reset(); }

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
    c.fillRect(0, 0, Ui::Page::WIDTH, Ui::Page::HEIGHT, 0);
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
        const auto row = list.cell(i, debugScroll);
        if (row.y + row.height <= body.y || row.y >= body.y + body.height) continue;
        MenuGrid::card(c, row, listTouch.pressed() == i, debugCursor == i);
        PixelRenderer::text(row.x + 12, row.y + 14, debugItemLabel(i), 0xffff);
        char buffer[24] = {};
        if (const char* value = debugItemValue(i, buffer, sizeof(buffer)))
            MenuGrid::label(c, row.x + row.width - 66, row.y + 14, value, 124, 0x7bef);
    }
    c.clearClipRect();
    TouchUi::safePageHeader(title(), listTouch.pressed() == BACK_TARGET);
    if (debugSwitchOpen || debugTimeOpen) renderPopup();
}

void DebugUi::renderTouchTest() {
    auto& c = PixelRenderer::canvas();
    TouchUi::safePageHeader(Ui::Debug::TOUCH_TEST, touchHeaderDown);
    char buffer[48] = {};
    if (debugTouchTest.count < TouchTest::COUNT) {
        std::snprintf(buffer, sizeof(buffer), "%d/9  %d,%d", debugTouchTest.count,
                      TouchTest::TARGETS[debugTouchTest.count].x,
                      TouchTest::TARGETS[debugTouchTest.count].y);
    } else {
        std::snprintf(buffer, sizeof(buffer), "9/9  COMPLETE");
    }
    PixelRenderer::text(8, 47, buffer, 0xffe0);
    for (int i = 0; i < TouchTest::COUNT; ++i) {
        const auto target = TouchTest::TARGETS[i];
        const uint16_t color = i == debugTouchTest.count ? 0xffe0
            : i < debugTouchTest.count ? PixelRenderer::rgb(115, 226, 183)
                                       : PixelRenderer::rgb(65, 78, 85);
        c.drawCircle(target.x, target.y, 10, color);
        c.drawFastHLine(target.x - 15, target.y, 31, color);
        c.drawFastVLine(target.x, target.y - 15, 31, color);
    }
    if (debugTouchTest.lastValid) {
        if (debugTouchTest.lastWasUp) {
            std::snprintf(buffer, sizeof(buffer), "D%d,%d U%d,%d",
                          debugTouchTest.pending.down.x,
                          debugTouchTest.pending.down.y,
                          debugTouchTest.pending.up.x,
                          debugTouchTest.pending.up.y);
        } else {
            std::snprintf(buffer, sizeof(buffer), "LIVE %d,%d",
                          debugTouchTest.last.x, debugTouchTest.last.y);
        }
        PixelRenderer::text(8, 180, buffer,
                            debugTouchTest.pressed ? 0xf471 : 0x7bef);
    }
    TouchUi::action(TouchTest::CLEAR_X, TouchTest::CLEAR_Y,
                    TouchTest::CLEAR_W, "重测", false);
    TouchUi::action(TouchTest::BACK_X, TouchTest::BACK_Y,
                    TouchTest::BACK_W, Ui::BACK, false);
    for (int i = 0; i < debugTouchTest.count; ++i) {
        const auto& point = debugTouchTest.samples[i];
        c.fillCircle(point.down.x, point.down.y, 3, 0xf471);
        c.drawCircle(point.up.x, point.up.y, 5, 0x67df);
    }
    if (debugTouchTest.pressed)
        c.drawCircle(debugTouchTest.last.x, debugTouchTest.last.y, 5, 0xf471);
}

void DebugUi::handleTouchTest(const Platform::IInputDevice::TouchEvent& event) {
    using Type = Platform::IInputDevice::TouchType;
    const bool header = TouchUi::safeBack().contains(event.x, event.y);
    if (event.type == Type::DOWN && header) {
        touchHeaderDown = true;
        return;
    }
    if (touchHeaderDown) {
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
    c.fillRoundRect(28, 60, 240, 164, 8, PixelRenderer::rgb(18, 22, 30));
    c.drawRoundRect(28, 60, 240, 164, 8, 0x7bef);
    PixelRenderer::text(44, 72, time ? Ui::Debug::SET_TIME : Ui::Debug::SWITCH_MON, 0x47ff);
    PixelRenderer::text(44, 98, time ? Ui::Debug::TARGET_TIME : Ui::Debug::INPUT_ID, 0xffff);
    for (int i = 0; i < digits; ++i) {
        const auto rect = digitRect(i, time);
        MenuGrid::card(c, rect, listTouch.pressed() == POPUP_FIRST + i, focus == i);
        char value[] = {char('0' + (time ? debugTimeDigits[i] : debugSwitchDigits[i])), '\0'};
        PixelRenderer::text(rect.x + 16, rect.y + 13, value, 0xffff);
    }
    if (time) PixelRenderer::text(144, 132, ":", 0xffff);
    for (int i = 0; i < 2; ++i) {
        const auto rect = popupButton(i == 1);
        MenuGrid::card(c, rect, listTouch.pressed() == POPUP_FIRST + digits + i, focus == digits + i);
        MenuGrid::label(c, rect.x + rect.width / 2, rect.y + 11,
            i == 0 ? Ui::Debug::YES : Ui::Debug::CANCEL, rect.width - 12, 0xffff);
    }
}

bool DebugUi::onButton(const ButtonEvent& event) {
    cancelTouch();
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
    Scene::cancelTouch();
    listTouch.cancel();
    debugTouchTest.cancel();
    touchHeaderDown = false;
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
    if (x < 0 || x >= Ui::Page::WIDTH || y < 0 || y >= Ui::Page::HEIGHT) return -1;
    if (TouchUi::safeBack().contains(x, y)) return BACK_TARGET;
    if (debugSwitchOpen || debugTimeOpen) {
        const bool time = debugTimeOpen;
        const int digits = time ? 4 : 3;
        for (int i = 0; i < digits; ++i)
            if (digitRect(i, time).contains(x, y)) return POPUP_FIRST + i;
        for (int i = 0; i < 2; ++i)
            if (popupButton(i == 1).contains(x, y)) return POPUP_FIRST + digits + i;
        return -1;
    }
    return pageLayout().itemAt(x, y, debugScroll, visibleItemCount());
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
}  // namespace Ui
#endif
