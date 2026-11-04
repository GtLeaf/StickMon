#pragma once

#include <cstdint>

namespace Amoled {

// The PMU latches a short press, so sleeping UI loops cannot miss the click.
// Drain boot-time events before arming: the power-on click must not power off.
class PowerButton {
public:
    template<class Pmu>
    bool poll(Pmu& pmu, uint32_t nowMs) {
        if (sampled_ && nowMs - lastPollMs_ < 50) return false;
        sampled_ = true;
        lastPollMs_ = nowMs;
        if (!ready_) {
            if (pmu.configurePowerButton()) {
                ready_ = true;
                beganMs_ = nowMs;
            }
            return false;
        }
        const bool pressed = pmu.takePowerButtonPress();
        return nowMs - beganMs_ >= 1000 && pressed;
    }

private:
    uint32_t lastPollMs_ = 0, beganMs_ = 0;
    bool sampled_ = false, ready_ = false;
};

} // namespace Amoled
