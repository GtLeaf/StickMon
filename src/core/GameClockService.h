#pragma once

#include <cstdint>

class GameClockService {
public:
    void start(uint32_t nowMs, uint32_t gameMinutes);
    uint32_t minutesAt(uint32_t nowMs, float speed) const;
    uint64_t millisecondsAt(uint32_t nowMs,float speed) const;
    void setMilliseconds(uint32_t nowMs,uint64_t gameMs);
    bool sync(uint32_t nowMs, float speed, uint32_t& gameMinutes);
    void advance(uint32_t minutes) { anchorMinutes_ += minutes; }
    void set(uint32_t nowMs, uint32_t gameMinutes);

private:
    uint32_t anchorMs_ = 0;
    uint32_t anchorMinutes_ = 0;
    uint32_t fractionMs_ = 0;
};
