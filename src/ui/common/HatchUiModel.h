#pragma once

#include <cstdint>

namespace Ui {
struct HatchFrame {
    uint32_t nowMs = 0;
    uint8_t progress = 0;
    uint16_t remainingSeconds = 0;
    bool night = false;
};
struct HatchLayout { int width, height; };
enum class HatchAction : uint8_t { NONE, MENU, WIPE, POKE };
}  // namespace Ui
