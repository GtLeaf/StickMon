#pragma once

#include <cstdint>

class GameClockService {
public:
    void start(uint32_t nowMs, uint32_t gameMinutes);
    uint32_t minutesAt(uint32_t nowMs, float speed) const;
    bool sync(uint32_t nowMs, float speed, uint32_t& gameMinutes);
    void set(uint32_t nowMs, uint32_t gameMinutes);

private:
    uint32_t anchorMs_ = 0;
    uint32_t anchorMinutes_ = 0;
};
