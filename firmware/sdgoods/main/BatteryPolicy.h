// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <algorithm>

namespace Sdgoods {
// Match sdgoods_cc_bat_read: 3.00V=0%, 4.28V=100%, rounded.
// Voltage estimate only; the SDK exposes no hardware charging status.
// ChargingTrend provides a separate, provisional UI estimate.
inline int batteryPercent(int millivolts) {
    if (millivolts < 2500 || millivolts > 4500) return -1;
    return std::clamp(((millivolts - 3000) * 100 + 640) / 1280, 0, 100);
}
}  // namespace Sdgoods
