#pragma once

#include <cstdint>

namespace Ui {
struct ApplicationLayout { int width, height, navigationHeight; };
struct ApplicationResourceAlertFrame {
    bool fontMissing = false;
    bool roomMissing = false;
    uint8_t missingSpecies = 0;
    uint16_t firstMissingSpecies = 0;
    bool visible() const { return fontMissing || roomMissing || missingSpecies > 0; }
};
struct ApplicationFadeFrame {
    bool active = false;
    bool hold = false;
    bool out = false;
    uint16_t durationMs = 300;
    uint16_t progressMs = 0;
};
}  // namespace Ui
