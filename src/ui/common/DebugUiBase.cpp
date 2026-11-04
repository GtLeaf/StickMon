#include "ui/common/DebugUiBase.h"
#if STICKMON_ENABLE_DEBUG_FEATURES
#include <cstdio>
#include "core/UiStrings.h"
#include "core/MathUtil.h"
#include "core/UiMotion.h"
#include "presentation/PixelRenderer.h"
namespace {
int textPixelWidth(const char* value) {
    int width = 0;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(value);
    while (*p) {
        if (*p < 0x80) {
            width += *p == ' ' ? 5 : 8;
            ++p;
        } else if ((*p & 0xE0) == 0xC0) {
            width += 16;
            p += 2;
        } else if ((*p & 0xF0) == 0xE0) {
            width += 16;
            p += 3;
        } else {
            width += 8;
            ++p;
        }
    }
    return width;
}
}  // namespace
namespace Ui {
void DebugUiBase::reset() {
    debugCategory = DebugCategory::ROOT;
    debugCursor = 0;
    closePopups();
    debugSwitchFocus = debugTimeFocus = 0;
    debugScroll = 0.0f;
    debugTouchTest = {};
}
void DebugUiBase::closePopups() { debugSwitchOpen = debugTimeOpen = false; }
void DebugUiBase::returnFromBattle() {
    debugCategory = DebugCategory::BATTLE;
    debugCursor = DEBUG_BATTLE_RANDOM_INDEX;
}
SceneUpdateResult DebugUiBase::update(uint32_t, float) {
    RenderDemand demand;
    constexpr int ROW_Y = 6;
    constexpr int ROW_H = 24;
    const int maximum = MathUtil::max(0, ROW_Y * 2 + int(debugItemCount()) * ROW_H - layout().contentHeight);
    const int target = ROW_Y + debugCursor * ROW_H + ROW_H / 2 - layout().contentHeight / 2;
    const auto step = UiMotion::lerp(debugScroll, float(MathUtil::clamp(target, 0, maximum)), 0.25f, 0.5f);
    demand.changed(step.changed);
    demand.animate(step.active);
    return demand.result();
}
bool DebugUiBase::onButton(const ButtonEvent& event) {
    if (!host.pageExploreContext() && event.btn == 1 && event.action == BtnAction::LONG_PRESS)
        host.pageBackTutorial();
    if (event.btn == 0 && event.action == BtnAction::LONG_PRESS) {
        host.pageHome();
        return true;
    }
    if (event.btn == 1 && event.action == BtnAction::LONG_PRESS) {
        if (debugSwitchOpen) {
            debugSwitchOpen = false;
            return true;
        }
        if (debugTimeOpen) {
            debugTimeOpen = false;
            return true;
        }
        if (debugCategory != DebugCategory::ROOT) {
            debugCategory = DebugCategory::ROOT;
            debugCursor = 0;
            debugScroll = 0.0f;
            return true;
        }
        host.pageBack();
        return true;
    }
    if (debugSwitchOpen && event.btn == 1 && event.action == BtnAction::PRESSED) {
        debugSwitchFocus = (debugSwitchFocus + 1) % DEBUG_SWITCH_FOCUS_COUNT;
        return true;
    }
    if (debugSwitchOpen && event.btn == 0 && event.action == BtnAction::PRESSED) {
        if (debugSwitchFocus < 3) {
            debugSwitchDigits[debugSwitchFocus] = (debugSwitchDigits[debugSwitchFocus] + 1) % 10;
        } else if (debugSwitchFocus == 3) {
            if (host.activateDebug(DebugAction::SWITCH_SPECIES, debugSwitchTargetId())) {
                debugSwitchOpen = false;
            }
        } else {
            debugSwitchOpen = false;
        }
        return true;
    }
    if (debugTimeOpen && event.btn == 1 && event.action == BtnAction::PRESSED) {
        debugTimeFocus = (debugTimeFocus + 1) % DEBUG_TIME_FOCUS_COUNT;
        return true;
    }
    if (debugTimeOpen && event.btn == 0 && event.action == BtnAction::PRESSED) {
        if (debugTimeFocus < 4) {
            incrementDebugTimeDigit();
        } else if (debugTimeFocus == 4) {
            host.activateDebug(DebugAction::SET_TIME, debugTimeTargetMinutes());
            debugTimeOpen = false;
        } else {
            debugTimeOpen = false;
        }
        return true;
    }
    if (event.btn == 1 && event.action == BtnAction::PRESSED) {
        debugCursor = (debugCursor + 1) % debugItemCount();
        if (debugCursor == 0) debugScroll = 0.0f;
        return true;
    }
    if (event.btn == 0 && event.action == BtnAction::PRESSED) {
        handleDebugAction();
        return true;
    }
    return false;
}
void DebugUiBase::renderDebugPage() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, layout().width, layout().contentHeight, 0x0000);

    static constexpr int ROW_X = 8;
    static constexpr int TEXT_X = 20;
    static constexpr int ROW_Y = 6;
    static constexpr int ROW_H = 24;
    static constexpr int TEXT_Y_OFFSET = 4;
    static constexpr int SEP_Y_OFFSET = ROW_H - 1;
    const int SEP_W = layout().width - TEXT_X - 10;
    uint8_t itemCount = debugItemCount();
    if (debugCursor >= itemCount) {
        debugCursor = 0;
        debugScroll = 0.0f;
    }

    for (uint8_t i = 0; i < itemCount; ++i) {
        int rowY = ROW_Y + i * ROW_H - (int)debugScroll;
        if (rowY + ROW_H <= 0 || rowY >= layout().contentHeight) continue;
        int textY = rowY + TEXT_Y_OFFSET;
        bool selected = i == debugCursor;
        uint16_t color = selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);
        if (selected) {
            c.fillRect(ROW_X, textY, 4, 16, PixelRenderer::rgb(255, 216, 72));
        }
        PixelRenderer::text(TEXT_X, textY, debugItemLabel(i), color, 1);
        char valueBuffer[24] = {};
        const char* value = debugItemValue(i, valueBuffer, sizeof(valueBuffer));
        if (value) {
            PixelRenderer::text(layout().width - textPixelWidth(value) - 12,
                                textY, value,
                                selected ? PixelRenderer::rgb(255, 218, 178)
                                         : 0x7BEF,
                                1);
        }
        if (i + 1 < itemCount) {
            c.drawFastHLine(TEXT_X, rowY + SEP_Y_OFFSET, SEP_W, PixelRenderer::rgb(55, 63, 76));
        }
    }

    if (debugSwitchOpen) renderDebugSwitchPopup();
    if (debugTimeOpen) renderDebugTimePopup();
}

const char* DebugUiBase::debugItemValue(uint8_t index, char* buffer, size_t size) const {
    const char* value = nullptr;
    const auto& state = host.pageState();
    const auto frame = host.debugFrame();
    if (debugCategory == DebugCategory::MONSTER) {
        if (index == DEBUG_MONSTER_LEVEL_UP_INDEX && state.teamCount > 0) {
            snprintf(buffer, size, Ui::Common::LEVEL_FMT,
                     state.team[0].level);
            value = buffer;
        } else if (index == DEBUG_MONSTER_SWITCH_INDEX) {
            uint16_t speciesId =
                state.teamCount > 0 ? state.team[0].speciesId : 0;
            snprintf(buffer, size,
                     Ui::Debug::CURRENT_ID_FMT, speciesId);
            value = buffer;
        }
    } else if (debugCategory == DebugCategory::ENV) {
        if (index == 0) {
            uint16_t minutes = frame.minutes;
            snprintf(buffer, size,
                     Ui::Debug::CURRENT_TIME_FMT,
                     minutes / 60, minutes % 60);
            value = buffer;
        } else if (index == 1) {
            value = frame.lightLabel;
        }
    } else if (debugCategory == DebugCategory::MOTION) {
        if (index == 0) {
            value = frame.tilt
                ? Ui::Settings::ON : Ui::Settings::OFF;
        } else if (index == 1) {
            value = frame.walkBounds
                ? Ui::Settings::ON : Ui::Settings::OFF;
        }
    } else if (debugCategory == DebugCategory::BATTLE &&
               index == DEBUG_BATTLE_DRAW_BOUNDS_INDEX) {
        value = frame.battleBounds
            ? Ui::Settings::ON : Ui::Settings::OFF;
    }
    return value;
}

uint8_t DebugUiBase::debugItemCount() const {
    static_assert(DEBUG_ROOT_BASE_ITEM_COUNT ==
                      sizeof(Ui::Debug::ROOT_ITEMS) /
                          sizeof(Ui::Debug::ROOT_ITEMS[0]),
                  "debug root labels must match the root item count");
    static_assert(DEBUG_MONSTER_ITEM_COUNT ==
                      sizeof(Ui::Debug::MONSTER_ITEMS) /
                          sizeof(Ui::Debug::MONSTER_ITEMS[0]),
                  "debug monster labels must match the item count");
    static_assert(DEBUG_BATTLE_ITEM_COUNT ==
                      sizeof(Ui::Debug::BATTLE_ITEMS) /
                          sizeof(Ui::Debug::BATTLE_ITEMS[0]),
                  "debug battle labels must match the item count");
    static_assert(DEBUG_CONTACT_EVENT_ITEM_COUNT ==
                      sizeof(Ui::Debug::CONTACT_EVENT_ITEMS) /
                          sizeof(Ui::Debug::CONTACT_EVENT_ITEMS[0]),
                  "debug contact event labels must match the item count");
    static_assert(DEBUG_RESOURCE_ITEM_COUNT ==
                      sizeof(Ui::Debug::RESOURCE_ITEMS) /
                          sizeof(Ui::Debug::RESOURCE_ITEMS[0]) &&
                  DEBUG_ENV_ITEM_COUNT ==
                      sizeof(Ui::Debug::ENV_ITEMS) /
                          sizeof(Ui::Debug::ENV_ITEMS[0]) &&
                  DEBUG_MOTION_ITEM_COUNT ==
                      sizeof(Ui::Debug::MOTION_ITEMS) /
                          sizeof(Ui::Debug::MOTION_ITEMS[0]),
                  "debug category labels must match their item counts");
    switch (debugCategory) {
    case DebugCategory::MONSTER: return DEBUG_MONSTER_ITEM_COUNT;
    case DebugCategory::RESOURCE: return DEBUG_RESOURCE_ITEM_COUNT;
    case DebugCategory::ENV: return DEBUG_ENV_ITEM_COUNT;
    case DebugCategory::MOTION: return DEBUG_MOTION_ITEM_COUNT;
    case DebugCategory::BATTLE: return DEBUG_BATTLE_ITEM_COUNT;
    case DebugCategory::CONTACT_EVENT: return DEBUG_CONTACT_EVENT_ITEM_COUNT;
    case DebugCategory::TOUCH_TEST: return 1;
    case DebugCategory::ROOT:
    default:
        return DEBUG_ROOT_ITEM_COUNT;
    }
}

const char* DebugUiBase::debugItemLabel(uint8_t index) const {
    if (index >= debugItemCount()) return Ui::BACK;
    switch (debugCategory) {
    case DebugCategory::MONSTER: return Ui::Debug::MONSTER_ITEMS[index];
    case DebugCategory::RESOURCE: return Ui::Debug::RESOURCE_ITEMS[index];
    case DebugCategory::ENV: return Ui::Debug::ENV_ITEMS[index];
    case DebugCategory::MOTION: return Ui::Debug::MOTION_ITEMS[index];
    case DebugCategory::BATTLE: return Ui::Debug::BATTLE_ITEMS[index];
    case DebugCategory::CONTACT_EVENT:
        return Ui::Debug::CONTACT_EVENT_ITEMS[index];
    case DebugCategory::TOUCH_TEST: return Ui::BACK;
    case DebugCategory::ROOT:
    default:
#if STICKMON_UI_COMPACT_TOUCH
        if (index == DEBUG_TOUCH_ROOT_INDEX) return Ui::Debug::TOUCH_TEST;
        if (index == DEBUG_ROOT_ITEM_COUNT - 1) return Ui::BACK;
#endif
        return Ui::Debug::ROOT_ITEMS[index];
    }
}

void DebugUiBase::handleDebugAction() {
    if (debugCategory == DebugCategory::ROOT) {
        switch (debugCursor) {
        case 0: debugCategory = DebugCategory::MONSTER; break;
        case 1: debugCategory = DebugCategory::RESOURCE; break;
        case 2: debugCategory = DebugCategory::ENV; break;
        case 3: debugCategory = DebugCategory::MOTION; break;
        case DEBUG_BATTLE_ROOT_INDEX:
            debugCategory = DebugCategory::BATTLE;
            break;
        case DEBUG_CONTACT_EVENT_ROOT_INDEX:
            debugCategory = DebugCategory::CONTACT_EVENT;
            break;
#if STICKMON_UI_COMPACT_TOUCH
        case DEBUG_TOUCH_ROOT_INDEX:
            debugCategory = DebugCategory::TOUCH_TEST;
            debugTouchTest = {};
            break;
#endif
        default:
            host.pageBack();
            return;
        }
        debugCursor = 0;
        debugScroll = 0.0f;
        return;
    }

    if (debugCursor + 1 >= debugItemCount()) {
        debugCategory = DebugCategory::ROOT;
        debugCursor = 0;
        debugScroll = 0.0f;
        return;
    }

    DebugAction action;
    switch (debugCategory) {
    case DebugCategory::MONSTER:
        if (debugCursor == DEBUG_MONSTER_SWITCH_INDEX) {
            openDebugSwitchPopup();
            return;
        }
        action = debugCursor == DEBUG_MONSTER_RECOVER_INDEX
            ? DebugAction::RECOVER : DebugAction::LEVEL_UP;
        break;
    case DebugCategory::RESOURCE: action = DebugAction::ADD_COINS; break;
    case DebugCategory::ENV:
        if (debugCursor == 0) { openDebugTimePopup(); return; }
        action = DebugAction::CYCLE_LIGHT;
        break;
    case DebugCategory::MOTION:
        if (debugCursor == 0) action = DebugAction::TOGGLE_TILT;
        else if (debugCursor == 1) action = DebugAction::TOGGLE_WALK_BOUNDS;
        else if (debugCursor == DEBUG_MOTION_PAIR_INTERACTION_INDEX)
            action = DebugAction::PAIR_INTERACTION;
        else return;
        break;
    case DebugCategory::BATTLE:
        action = debugCursor == DEBUG_BATTLE_RANDOM_INDEX
            ? DebugAction::RANDOM_BATTLE : DebugAction::TOGGLE_BATTLE_BOUNDS;
        break;
    case DebugCategory::CONTACT_EVENT:
        action = debugCursor == 0 ? DebugAction::CONTACT_PLAY
            : (debugCursor == 1 ? DebugAction::CONTACT_GIFT : DebugAction::CONTACT_EXPLORE);
        break;
    case DebugCategory::TOUCH_TEST:
        return;
    default: return;
    }
    host.activateDebug(action);
}

void DebugUiBase::openDebugSwitchPopup() {
    debugTimeOpen = false;
    const auto& state = host.pageState();
    uint16_t speciesId = state.teamCount > 0 ? state.team[0].speciesId : 1;
    if (speciesId > 999) speciesId = 999;
    debugSwitchDigits[0] = (speciesId / 100) % 10;
    debugSwitchDigits[1] = (speciesId / 10) % 10;
    debugSwitchDigits[2] = speciesId % 10;
    debugSwitchFocus = 0;
    debugSwitchOpen = true;
}

uint16_t DebugUiBase::debugSwitchTargetId() const {
    return (uint16_t)(debugSwitchDigits[0] * 100 + debugSwitchDigits[1] * 10 + debugSwitchDigits[2]);
}

void DebugUiBase::openDebugTimePopup() {
    debugSwitchOpen = false;
    uint16_t minutes = host.debugFrame().minutes;
    uint8_t hour = minutes / 60;
    uint8_t minute = minutes % 60;
    debugTimeDigits[0] = hour / 10;
    debugTimeDigits[1] = hour % 10;
    debugTimeDigits[2] = minute / 10;
    debugTimeDigits[3] = minute % 10;
    debugTimeFocus = 0;
    debugTimeOpen = true;
}

uint16_t DebugUiBase::debugTimeTargetMinutes() const {
    uint8_t hour = (uint8_t)(debugTimeDigits[0] * 10 + debugTimeDigits[1]);
    uint8_t minute = (uint8_t)(debugTimeDigits[2] * 10 + debugTimeDigits[3]);
    if (hour > 23) hour = 23;
    if (minute > 59) minute = 59;
    return (uint16_t)(hour * 60 + minute);
}

void DebugUiBase::incrementDebugTimeDigit() {
    if (debugTimeFocus == 0) {
        debugTimeDigits[0] = (debugTimeDigits[0] + 1) % 3;
        if (debugTimeDigits[0] == 2 && debugTimeDigits[1] > 3) debugTimeDigits[1] = 3;
        return;
    }
    if (debugTimeFocus == 1) {
        uint8_t maxHourOnes = debugTimeDigits[0] == 2 ? 3 : 9;
        debugTimeDigits[1] = (debugTimeDigits[1] + 1) % (maxHourOnes + 1);
        return;
    }
    if (debugTimeFocus == 2) {
        debugTimeDigits[2] = (debugTimeDigits[2] + 1) % 6;
        return;
    }
    if (debugTimeFocus == 3) {
        debugTimeDigits[3] = (debugTimeDigits[3] + 1) % 10;
    }
}
void DebugUiBase::renderDebugSwitchPopup() {
    auto& c = PixelRenderer::canvas();
    static constexpr int POP_X = 34;
    static constexpr int POP_Y = 26;
    static constexpr int POP_W = 172;
    static constexpr int POP_H = 84;
    c.fillRect(POP_X, POP_Y, POP_W, POP_H, PixelRenderer::rgb(18, 22, 30));
    c.drawRect(POP_X, POP_Y, POP_W, POP_H, PixelRenderer::rgb(241, 242, 232));

    PixelRenderer::text(POP_X + 14, POP_Y + 10, Ui::Debug::SWITCH_MON,
                        PixelRenderer::rgb(67, 213, 224), 1);
    PixelRenderer::text(POP_X + 14, POP_Y + 32, Ui::Debug::INPUT_ID,
                        PixelRenderer::rgb(241, 242, 232), 1);

    char digitBuf[2] = {'0', '\0'};
    for (uint8_t i = 0; i < 3; ++i) {
        int x = POP_X + 68 + i * 18;
        bool selected = debugSwitchFocus == i;
        if (selected) {
            c.drawRect(x - 3, POP_Y + 29, 14, 18, PixelRenderer::rgb(255, 216, 72));
        }
        digitBuf[0] = (char)('0' + debugSwitchDigits[i]);
        PixelRenderer::text(x, POP_Y + 32, digitBuf,
                            selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232),
                            1);
    }

    uint16_t yesColor = debugSwitchFocus == 3 ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);
    uint16_t cancelColor = debugSwitchFocus == 4 ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);
    if (debugSwitchFocus == 3) c.fillRect(POP_X + 38, POP_Y + 60, 4, 16, PixelRenderer::rgb(255, 216, 72));
    if (debugSwitchFocus == 4) c.fillRect(POP_X + 94, POP_Y + 60, 4, 16, PixelRenderer::rgb(255, 216, 72));
    PixelRenderer::text(POP_X + 48, POP_Y + 60, Ui::Debug::YES, yesColor, 1);
    PixelRenderer::text(POP_X + 104, POP_Y + 60, Ui::Debug::CANCEL, cancelColor, 1);
}

void DebugUiBase::renderDebugTimePopup() {
    auto& c = PixelRenderer::canvas();
    static constexpr int POP_X = 30;
    static constexpr int POP_Y = 26;
    static constexpr int POP_W = 180;
    static constexpr int POP_H = 84;
    c.fillRect(POP_X, POP_Y, POP_W, POP_H, PixelRenderer::rgb(18, 22, 30));
    c.drawRect(POP_X, POP_Y, POP_W, POP_H, PixelRenderer::rgb(241, 242, 232));

    PixelRenderer::text(POP_X + 14, POP_Y + 10, Ui::Debug::SET_TIME,
                        PixelRenderer::rgb(67, 213, 224), 1);
    PixelRenderer::text(POP_X + 14, POP_Y + 32, Ui::Debug::TARGET_TIME,
                        PixelRenderer::rgb(241, 242, 232), 1);

    char digitBuf[2] = {'0', '\0'};
    for (uint8_t i = 0; i < 4; ++i) {
        int x = POP_X + 90 + i * 16 + (i >= 2 ? 8 : 0);
        bool selected = debugTimeFocus == i;
        if (selected) {
            c.drawRect(x - 3, POP_Y + 29, 14, 18, PixelRenderer::rgb(255, 216, 72));
        }
        digitBuf[0] = (char)('0' + debugTimeDigits[i]);
        PixelRenderer::text(x, POP_Y + 32, digitBuf,
                            selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232),
                            1);
    }
    PixelRenderer::text(POP_X + 122, POP_Y + 32, ":", PixelRenderer::rgb(241, 242, 232), 1);

    uint16_t yesColor = debugTimeFocus == 4 ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);
    uint16_t cancelColor = debugTimeFocus == 5 ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);
    if (debugTimeFocus == 4) c.fillRect(POP_X + 38, POP_Y + 60, 4, 16, PixelRenderer::rgb(255, 216, 72));
    if (debugTimeFocus == 5) c.fillRect(POP_X + 94, POP_Y + 60, 4, 16, PixelRenderer::rgb(255, 216, 72));
    PixelRenderer::text(POP_X + 48, POP_Y + 60, Ui::Debug::YES, yesColor, 1);
    PixelRenderer::text(POP_X + 104, POP_Y + 60, Ui::Debug::CANCEL, cancelColor, 1);
}
}  // namespace Ui
#endif
