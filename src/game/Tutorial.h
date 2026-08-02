#pragma once

#include <cstdint>

namespace Game {

enum class TutorialStep : uint8_t {
    ROOM_FEED = 1U << 0,
    ROOM_PET = 1U << 1,
    OPEN_MENU = 1U << 2,
    MENU_NAV = 1U << 3,
    MENU_BACK = 1U << 4,
    EXPLORE_WALK = 1U << 5,
    EXPLORE_MENU = 1U << 6,
    BATTLE_ACTION = 1U << 7,
};

constexpr uint8_t tutorialMask(TutorialStep step) {
    return static_cast<uint8_t>(step);
}

constexpr uint8_t TUTORIAL_ALL_FLAGS = 0xFF;

} // namespace Game
