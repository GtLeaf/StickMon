#pragma once

#include <algorithm>
#include <cstdint>

namespace Amoled {

// PMU key events are latched. Poll them without waking at animation cadence.
class DisplayStandbySchedule {
public:
    static constexpr uint32_t BUTTON_MS = 250;
    static constexpr uint32_t LOGIC_MS = 1000;
    void begin(uint32_t nowMs) { lastButtonMs_ = lastLogicMs_ = nowMs; }
    bool buttonDue(uint32_t nowMs) const { return nowMs - lastButtonMs_ >= BUTTON_MS; }
    bool logicDue(uint32_t nowMs) const { return nowMs - lastLogicMs_ >= LOGIC_MS; }
    void polled(uint32_t nowMs) { lastButtonMs_ = nowMs; }
    void updated(uint32_t nowMs) { lastLogicMs_ = nowMs; }
    uint32_t wait(uint32_t nowMs) const {
        return std::min(BUTTON_MS - std::min(BUTTON_MS, nowMs - lastButtonMs_),
                        LOGIC_MS - std::min(LOGIC_MS, nowMs - lastLogicMs_));
    }
private:
    uint32_t lastButtonMs_ = 0, lastLogicMs_ = 0;
};

} // namespace Amoled
