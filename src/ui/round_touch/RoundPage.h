// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "presentation/MenuGrid.h"

namespace Ui { namespace RoundPage {
constexpr MenuGrid::Rect BACK{8, 4, 40, 40};
constexpr MenuGrid::Rect NATIVE_BACK{138, 12, 40, 40};
constexpr MenuGrid::Rect BODY{0, 48, 240, 184};
constexpr MenuGrid::Rect NOTICE{8, 210, 224, 26};
constexpr int BACK_TARGET = 230;
constexpr MenuGrid::Layout listLayout() { return {BODY, 1, 240, 50, 0, 6, 0, 0}; }
bool safePoint(int x, int y);
bool backHit(int x, int y);
void background();
void header(const char* title, bool pressed);
void nativeHeader(const char* title, bool pressed);
void notice(const char* text);
} }  // namespace Ui::RoundPage
