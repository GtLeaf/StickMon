#pragma once

#include "ui/PageProfile.h"
#include "ui/common/SceneInputPolicy.h"

namespace Ui {
// Keep the explicit Scene touch fallback even without a visible navigation bar.
using SceneInput = SceneInputPolicy<Page::WIDTH, Page::HEIGHT, 32>;
}  // namespace Ui
