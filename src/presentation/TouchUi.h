#pragma once

#include "hardware/Hal.h"
#include "presentation/MenuGrid.h"

namespace TouchUi {
#if defined(STICKMON_TOUCH_INPUT) && STICKMON_TOUCH_INPUT
constexpr int NAV_H = 32;
#else
constexpr int NAV_H = 0;
#endif
constexpr int CONTENT_H = Hal::DISPLAY_H - NAV_H;
bool enabled();
constexpr int PAGE_HEADER_H = 38;
MenuGrid::Layout listLayout(int rowHeight = 44);
void pageHeader(const char* title, bool pressed = false, bool selected = false);
bool pageBackHit(int x, int y);
// Corner clearance applies to the header, not the body viewport.
constexpr int SAFE_HEADER_TOP = 4;
constexpr int SAFE_HEADER_H = SAFE_HEADER_TOP + 44;
MenuGrid::Rect safeHeaderBounds();
MenuGrid::Rect pageBody();
MenuGrid::Rect safeBack();
void safePageHeader(const char* title, bool pressed = false, bool selected = false);
void slider(int left, int right, int centerY, int value, int minimum, int maximum, bool pressed);
void header(const char* title, const char* trailing = nullptr, bool pressed = false);
bool backHit(int x, int y);
void action(int x, int y, int width, const char* label, bool pressed = false);
void drawNavigation();
void drawHomeMenu();
bool homeMenuHit(int x, int y);
}  // namespace TouchUi
