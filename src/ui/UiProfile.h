#pragma once

#include "platform/api/PlatformServices.h"

#ifndef STICKMON_UI_COMPACT_TOUCH
#define STICKMON_UI_COMPACT_TOUCH 0
#endif

#ifndef STICKMON_UI_ROUND_TOUCH
#define STICKMON_UI_ROUND_TOUCH 0
#endif
#if STICKMON_UI_ROUND_TOUCH && !STICKMON_UI_COMPACT_TOUCH
#error "Round UI requires the compact touch shared pages"
#endif

#if STICKMON_UI_COMPACT_TOUCH != 0 && STICKMON_UI_COMPACT_TOUCH != 1
#error "STICKMON_UI_COMPACT_TOUCH must be 0 or 1"
#endif
#if STICKMON_UI_COMPACT_TOUCH && (!defined(STICKMON_TOUCH_INPUT) || !STICKMON_TOUCH_INPUT)
#error "Compact touch UI requires touch input support"
#endif

#ifndef STICKMON_UI_HEADER_INSET_X
#define STICKMON_UI_HEADER_INSET_X 0
#endif
#ifndef STICKMON_UI_HEADER_TOP
#define STICKMON_UI_HEADER_TOP 0
#endif

namespace Ui {

enum class Family { STICK, COMPACT_TOUCH, ROUND_TOUCH };
constexpr Family family = STICKMON_UI_ROUND_TOUCH ? Family::ROUND_TOUCH
    : STICKMON_UI_COMPACT_TOUCH ? Family::COMPACT_TOUCH : Family::STICK;
constexpr bool compactTouch = STICKMON_UI_COMPACT_TOUCH;

struct DeviceProfile {
    int width;
    int height;
    int headerInsetX;
    int headerTop;
    bool touchInput;
    bool buttonInput;
};

constexpr DeviceProfile device {
    Platform::LOGICAL_DISPLAY_W,
    Platform::LOGICAL_DISPLAY_H,
    STICKMON_UI_HEADER_INSET_X,
    STICKMON_UI_HEADER_TOP,
#if defined(STICKMON_TOUCH_INPUT) && STICKMON_TOUCH_INPUT
    true,
#else
    false,
#endif
    true,
};

static_assert(device.headerInsetX >= 0 && device.headerInsetX * 2 < device.width,
              "Header clearance must fit the display width");
static_assert(device.headerTop >= 0 && device.headerTop < device.height,
              "Header top must fit the display height");

}  // namespace Ui
