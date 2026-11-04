// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "platform/api/PlatformServices.h"

// Opt-in physical damage. Every frame starts full; only a renderer that has
// restored all changed pixels may restrict it. Overlays/transitions opt out.
namespace FrameDamage {
inline uint32_t serial = 0;
inline bool allowed = false, partial = false;
inline int x = 0, y = 0, width = 0, height = 0;
inline void begin(bool allowPartial) {
    ++serial;
    allowed = allowPartial;
    partial = false;
}
inline void restrictTo(int left, int top, int w, int h) {
    if (!allowed) return;
    partial = true;
    x = left; y = top; width = w; height = h;
}
inline void present() {
    if (partial) Platform::display().presentRegion(x, y, width, height);
    else Platform::display().present();
    allowed = false;
}
}
