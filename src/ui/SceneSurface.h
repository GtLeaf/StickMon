// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ui/UiProfile.h"

enum class SceneID : uint8_t;
namespace Ui { enum class ExplorePhase : uint8_t; }

namespace Ui::SceneSurface {
class InputScope {
public:
    InputScope(const InputScope&) = delete;
    InputScope& operator=(const InputScope&) = delete;
#if STICKMON_UI_ROUND_TOUCH
    explicit InputScope(const Platform::FrameBuffer565& output);
    ~InputScope();
private:
    Platform::FrameBuffer565 output_;
#else
    explicit InputScope(const Platform::FrameBuffer565&) {}
#endif
};
#if STICKMON_UI_ROUND_TOUCH
void select(SceneID scene);
// Detail pages use the native canvas in both home and exploration menus.
void mainMenu(bool native, bool nativeDetail = false);
void exploreSelection(bool native);
void explorePhase(ExplorePhase phase);
// Optional native header above a centered page. Cleared on page navigation.
void pageHeader(const char* title, bool pressed);
// Optional fixed, read-only footer below the centered page. Text is copied.
void pageFooter(const char* text);
bool begin(const Platform::FrameBuffer565& output);
void finish();
bool mapTouch(Platform::IInputDevice::TouchEvent& event);
int width();
int height();
#else
inline void select(SceneID) {}
inline void mainMenu(bool, bool = false) {}
inline void exploreSelection(bool) {}
inline void explorePhase(ExplorePhase) {}
inline bool begin(const Platform::FrameBuffer565&) { return true; }
inline void finish() {}
inline bool mapTouch(Platform::IInputDevice::TouchEvent&) { return true; }
constexpr int width() { return device.width; }
constexpr int height() { return device.height; }
#endif
}  // namespace Ui::SceneSurface
