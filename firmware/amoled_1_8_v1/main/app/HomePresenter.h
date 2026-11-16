#pragma once

#include <cstdint>

#include "SleepBreathClock.h"

namespace AmoledV1 {

// Home presentation only. The caller supplies room geometry and owns damage.
class HomePresenter {
public:
    struct RoomLayout {
        bool available = false;
        int width = 0;
        int height = 0;
        int roomY = 0;
    };

    void updateCamera(const RoomLayout& room, float actorX, float actorY);
    float cameraX() const { return cameraX_; }
    float cameraY() const { return cameraY_; }
    int worldToScreenX(const RoomLayout& room, float worldX) const;
    int worldToScreenY(const RoomLayout& room, float worldY) const;

    void startSleep(uint16_t speciesId, uint32_t nowMs, uint16_t frameMs);
    uint8_t sleepFrame(uint32_t nowMs) const { return sleepBreath_.frame(nowMs); }
    float sleepExpansion(uint32_t nowMs) const {
        return sleepBreath_.expansion(nowMs);
    }

private:
    float cameraX_ = 0.0f;
    float cameraY_ = 0.0f;
    SleepBreathClock sleepBreath_;
};

}  // namespace AmoledV1
