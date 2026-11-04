// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ui/SceneSurface.h"
#include "ui/common/SceneInputPolicy.h"

namespace Ui {
struct SceneInput {
    using PageInput = SceneInputPolicy<240, 240, 32>;
    using RoomInput = SceneInputPolicy<360, 360, 32>;
    static constexpr int CONTENT = PageInput::CONTENT;
    static bool contains(int x, int y) {
        return x >= 0 && y >= 0 && x < SceneSurface::width() && y < SceneSurface::height();
    }
    static int target(int x, int y, bool navigation) {
        return SceneSurface::width() == 240 ? PageInput::target(x, y, navigation)
            : RoomInput::target(x, y, navigation);
    }
    static bool button(int target, ButtonEvent& event) { return PageInput::button(target, event); }
};
}  // namespace Ui
