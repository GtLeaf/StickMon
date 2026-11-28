#pragma once

#include <cstdint>

#include "SleepBreathClock.h"
#include "game/PokemonAnimationTiming.h"

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

    // Manual horizontal look-around layered over the follow camera. Offsets
    // are logical room pixels; the displayed camera stays inside the room.
    static constexpr uint32_t PAN_IDLE_RETURN_MS = 8000;
    static constexpr uint32_t PAN_RETURN_MS = 300;
    static constexpr float PAN_ACTOR_MARGIN = 12.0f;
    bool canPan(const RoomLayout& room) const;
    void beginPan();
    // Returns true when the rounded displayed camera changed.
    bool panBy(const RoomLayout& room, float logicalDx);
    void endPan(uint32_t nowMs, float actorX);
    // Drops the offset immediately, e.g. when the room is no longer shown.
    bool clearPan(const RoomLayout& room);
    // Starts and advances the eased return. forceReturn covers story events
    // that need the follow view back. Returns true when the camera changed.
    bool updatePan(const RoomLayout& room, uint32_t nowMs, float actorX,
                   bool forceReturn);
    bool panning() const { return panActive_; }
    bool panReturning() const { return panReturning_; }
    float panOffset() const { return panX_; }
    float cameraY() const { return cameraY_; }
    int worldToScreenX(const RoomLayout& room, float worldX) const;
    int worldToScreenY(const RoomLayout& room, float worldY) const;

    void startSleep(uint16_t speciesId, uint32_t nowMs, uint16_t frameMs);
    uint8_t sleepFrame(uint32_t nowMs) const {
        return PokemonAnimationTiming::hasTiming(sleepSpecies_)
            ? PokemonAnimationTiming::frameAt(sleepSpecies_, PokemonAnimationTiming::Action::Sleep,
                                               animationClocks_[0].elapsedMs())
            : sleepBreath_.frame(nowMs);
    }
    // Shared by ordinary home motion and scripted room/visitor motion. Facing
    // is deliberately absent so turning cannot restart an animation.
    uint8_t animationFrame(uint8_t actor, uint16_t speciesId,
                           PokemonAnimationTiming::Action action, uint32_t nowMs,
                           bool active = true) {
        return animationClocks_[actor == 0 ? 0 : 1].update(speciesId, action, nowMs, active);
    }
    float sleepExpansion(uint32_t nowMs) const {
        return sleepBreath_.expansion(nowMs);
    }

private:
    float maximumCameraX(const RoomLayout& room) const;
    bool applyPan(const RoomLayout& room);

    float cameraX_ = 0.0f;
    float cameraY_ = 0.0f;
    float followX_ = 0.0f;
    float panX_ = 0.0f;
    float panActorX_ = 0.0f;
    float panReturnFrom_ = 0.0f;
    uint32_t panReleasedMs_ = 0;
    uint32_t panReturnStartMs_ = 0;
    bool panActive_ = false;
    bool panReturning_ = false;
    SleepBreathClock sleepBreath_;
    uint16_t sleepSpecies_ = 0;
    PokemonAnimationTiming::Clock animationClocks_[2];
};

}  // namespace AmoledV1
