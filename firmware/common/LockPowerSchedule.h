#pragma once

#include <algorithm>
#include <cstdint>

namespace Amoled {

// Input wakes the task independently of these simulation/presentation clocks.
class LockPowerSchedule {
public:
    static constexpr uint32_t LOGIC_MS = 100;
    static constexpr uint32_t ACTIVE_FRAME_MS = 200;
    static constexpr uint32_t SLEEP_FRAME_MS = 67;
    static constexpr uint32_t SLEEP_CONTENT_MS = 500;

    bool logicDue(uint32_t nowMs, bool lockedRoom) const {
        return !lockedRoom || nowMs - lastLogicMs_ >= LOGIC_MS;
    }
    void updated(uint32_t nowMs) { lastLogicMs_ = nowMs; }
    uint32_t logicWait(uint32_t nowMs) const {
        return LOGIC_MS - std::min(LOGIC_MS, nowMs - lastLogicMs_);
    }
    bool frameDue(uint32_t nowMs, bool sleeping) const {
        return !presented_ || nowMs - lastFrameMs_ >=
            (sleeping ? SLEEP_FRAME_MS : ACTIVE_FRAME_MS);
    }
    bool contentDue(uint32_t nowMs, bool sleeping) const {
        return !contentPresented_ || nowMs - lastContentMs_ >=
            (sleeping ? SLEEP_CONTENT_MS : ACTIVE_FRAME_MS);
    }
    void contentPresented(uint32_t nowMs) {
        lastContentMs_ = nowMs;
        contentPresented_ = true;
    }
    uint32_t wait(uint32_t nowMs, bool breathing) const {
        const uint32_t frameMs = breathing ? SLEEP_FRAME_MS : ACTIVE_FRAME_MS;
        return std::min(logicWait(nowMs),
            frameMs - std::min(frameMs, nowMs - lastFrameMs_));
    }
    void presented(uint32_t nowMs) { lastFrameMs_ = nowMs; presented_ = true; }
    static bool intersects(int left, int top, int right, int bottom,
                           int x, int y, int radius) {
        return left < x + radius && right > x - radius &&
               top < y + radius && bottom > y - radius;
    }

private:
    uint32_t lastLogicMs_ = 0;
    uint32_t lastFrameMs_ = 0;
    bool presented_ = false;
    uint32_t lastContentMs_ = 0;
    bool contentPresented_ = false;
};

} // namespace Amoled
