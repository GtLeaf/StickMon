#pragma once

#include "ui/UiProfile.h"
#include "ui/common/SceneInputPolicy.h"

namespace Ui {
// Keep the explicit Scene touch fallback even without a visible navigation bar.
using SceneInput = SceneInputPolicy<device.width, device.height, 32>;
}  // namespace Ui
