// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <array>
#include "core/GameEngine.h"

namespace Sdgoods {
class RoomSession {
public:
    bool begin();
    void poll();
    bool save();
    bool restart();
    bool ready() const { return ready_; }
    bool lastSaveOk() const { return lastSaveOk_; }
private:
    using FoodState = std::array<uint16_t, Game::ROOM_FOOD_COUNT + 5>;
    FoodState foodState() const;
    FoodState savedFood_{};
    uint32_t lastAttemptMs_ = 0;
    bool ready_ = false, lastSaveOk_ = true;
};
}  // namespace Sdgoods
