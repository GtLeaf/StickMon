#pragma once
#include <cstddef>
#include "core/BuildConfig.h"
#include "ui/UiProfile.h"
#if STICKMON_ENABLE_DEBUG_FEATURES
#include "core/Scene.h"
#include "ui/common/DebugUiHost.h"
#include "ui/common/TouchTest.h"

namespace Ui {
class DebugUiBase : public Scene {
public:
    explicit DebugUiBase(DebugHost& host) : host(host) {}
    enum class DebugCategory : uint8_t {
        ROOT,
        MONSTER,
        RESOURCE,
        ENV,
        MOTION,
        BATTLE,
        CONTACT_EVENT,
        TOUCH_TEST,
    };
    static constexpr uint8_t DEBUG_ROOT_BASE_ITEM_COUNT = 8;
    static constexpr uint8_t DEBUG_ROOT_ITEM_COUNT =
#if STICKMON_UI_COMPACT_TOUCH
        9;
#else
        8;
#endif
    static constexpr uint8_t DEBUG_BATTLE_ROOT_INDEX = 4;
    static constexpr uint8_t DEBUG_CONTACT_EVENT_ROOT_INDEX = 5;
    static constexpr uint8_t DEBUG_MONSTER_ITEM_COUNT = 4;
    static constexpr uint8_t DEBUG_MONSTER_RECOVER_INDEX = 0;
    static constexpr uint8_t DEBUG_MONSTER_LEVEL_UP_INDEX = 1;
    static constexpr uint8_t DEBUG_MONSTER_SWITCH_INDEX = 2;
    static constexpr uint8_t DEBUG_RESOURCE_ITEM_COUNT = 2;
    static constexpr uint8_t DEBUG_ENV_ITEM_COUNT = 3;
    static constexpr uint8_t DEBUG_MOTION_ITEM_COUNT = 5;
    static constexpr uint8_t DEBUG_MOTION_PAIR_INTERACTION_INDEX = 3;
    static constexpr uint8_t DEBUG_BATTLE_ITEM_COUNT = 3;
    static constexpr uint8_t DEBUG_BATTLE_RANDOM_INDEX = 0;
    static constexpr uint8_t DEBUG_BATTLE_DRAW_BOUNDS_INDEX = 1;
    static constexpr uint8_t DEBUG_CONTACT_EVENT_ITEM_COUNT = 4;
    static constexpr uint8_t DEBUG_TOUCH_ROOT_INDEX = 7;
    static constexpr uint8_t DEBUG_SWITCH_FOCUS_COUNT = 5;
    static constexpr uint8_t DEBUG_TIME_FOCUS_COUNT = 6;
    DebugCategory debugCategory = DebugCategory::ROOT;
    uint8_t debugCursor = 0;
    bool debugSwitchOpen = false;
    uint8_t debugSwitchFocus = 0;
    uint8_t debugSwitchDigits[3] = {0, 0, 1};
    bool debugTimeOpen = false;
    uint8_t debugTimeFocus = 0;
    uint8_t debugTimeDigits[4] = {0, 0, 0, 0};
    float debugScroll = 0.0f;
    TouchTest::State debugTouchTest;
    void reset();
    void closePopups();
    void returnFromBattle();
    SceneUpdateResult update(uint32_t nowMs, float dtSeconds) override;
    bool onButton(const ButtonEvent& event) override;
    bool touchNavigation() const override { return true; }
    virtual DebugLayout layout() const = 0;
protected:
    DebugHost& host;
    void renderDebugPage();
    void renderDebugSwitchPopup();
    void renderDebugTimePopup();
    uint8_t debugItemCount() const;
    const char* debugItemLabel(uint8_t index) const;
    const char* debugItemValue(uint8_t index, char* buffer, size_t size) const;
    void handleDebugAction();
    void openDebugSwitchPopup();
    void openDebugTimePopup();
    uint16_t debugSwitchTargetId() const;
    uint16_t debugTimeTargetMinutes() const;
    void incrementDebugTimeDigit();
};
}  // namespace Ui
#endif
