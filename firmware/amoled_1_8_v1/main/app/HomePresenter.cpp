#include "app/HomePresenter.h"

#include <algorithm>
#include <cmath>

#include "assets/PokemonSprites.h"
#include "ui/UiMetrics.h"

namespace AmoledV1 {
namespace {
constexpr float CAMERA_SAFE_LEFT = 62.0f;
constexpr float CAMERA_SAFE_RIGHT = 122.0f;
constexpr float CAMERA_SAFE_TOP = 58.0f;
constexpr float CAMERA_SAFE_BOTTOM = 126.0f;
}  // namespace

void HomePresenter::updateCamera(const RoomLayout& room, float actorX, float actorY) {
    if (!room.available) {
        cameraX_ = cameraY_ = followX_ = 0.0f;
        panX_ = 0.0f;
        panActive_ = panReturning_ = false;
        return;
    }

    // The safe area follows the actor in the follow camera; a manual pan is
    // only an offset on top, so releasing it returns to the usual framing.
    const float screenX = actorX - followX_;
    if (screenX < CAMERA_SAFE_LEFT) followX_ = actorX - CAMERA_SAFE_LEFT;
    else if (screenX > CAMERA_SAFE_RIGHT) followX_ = actorX - CAMERA_SAFE_RIGHT;

    const float screenY = actorY - cameraY_;
    if (screenY < CAMERA_SAFE_TOP) cameraY_ = actorY - CAMERA_SAFE_TOP;
    else if (screenY > CAMERA_SAFE_BOTTOM) cameraY_ = actorY - CAMERA_SAFE_BOTTOM;

    const float minimumCameraY = static_cast<float>(room.roomY);
    const float maximumCameraY = std::max<float>(minimumCameraY,
        static_cast<float>(room.roomY + room.height -
            UiMetrics::HOME_ROOM_HEIGHT / AmoledUi::RESOURCE_SCALE));
    // Room sampling, sprite placement and damage use the same logical grid.
    followX_ = std::clamp(std::round(followX_), 0.0f, maximumCameraX(room));
    cameraY_ = std::clamp(std::round(cameraY_), minimumCameraY, maximumCameraY);
    applyPan(room);
}

float HomePresenter::maximumCameraX(const RoomLayout& room) const {
    return std::max<float>(0.0f,
        static_cast<float>(room.width - AmoledUi::WIDTH / AmoledUi::RESOURCE_SCALE));
}

bool HomePresenter::applyPan(const RoomLayout& room) {
    const float previous = cameraX_;
    if (!room.available) {
        cameraX_ = 0.0f;
        return previous != cameraX_;
    }
    const float maximum = maximumCameraX(room);
    // Keep the offset within what can be shown so a return starts moving
    // immediately instead of first unwinding an invisible overshoot.
    panX_ = std::clamp(panX_, -followX_, maximum - followX_);
    cameraX_ = std::clamp(std::round(followX_ + panX_), 0.0f, maximum);
    return previous != cameraX_;
}

bool HomePresenter::canPan(const RoomLayout& room) const {
    return room.available && maximumCameraX(room) > 0.0f;
}

void HomePresenter::beginPan() {
    panActive_ = true;
    panReturning_ = false;
}

bool HomePresenter::panBy(const RoomLayout& room, float logicalDx) {
    if (!panActive_ || !canPan(room)) return false;
    panX_ += logicalDx;
    return applyPan(room);
}

void HomePresenter::endPan(uint32_t nowMs, float actorX) {
    if (!panActive_) return;
    panActive_ = false;
    panReleasedMs_ = nowMs;
    panActorX_ = actorX;
}

bool HomePresenter::clearPan(const RoomLayout& room) {
    panActive_ = panReturning_ = false;
    if (panX_ == 0.0f) return false;
    panX_ = 0.0f;
    return applyPan(room);
}

bool HomePresenter::updatePan(const RoomLayout& room, uint32_t nowMs, float actorX,
                              bool forceReturn) {
    if (panActive_ || panX_ == 0.0f) {
        panReturning_ = false;
        return false;
    }
    if (!panReturning_) {
        const float viewWidth =
            static_cast<float>(AmoledUi::WIDTH / AmoledUi::RESOURCE_SCALE);
        const float actorScreenX = actorX - cameraX_;
        // A resting actor may be panned out of view on purpose; only follow
        // again once it actually walks toward or past the visible edge.
        const bool actorLeaving = std::fabs(actorX - panActorX_) > 2.0f &&
            (actorScreenX < PAN_ACTOR_MARGIN ||
             actorScreenX > viewWidth - PAN_ACTOR_MARGIN);
        if (!forceReturn && !actorLeaving &&
            nowMs - panReleasedMs_ < PAN_IDLE_RETURN_MS) {
            return false;
        }
        panReturning_ = true;
        panReturnStartMs_ = nowMs;
        panReturnFrom_ = panX_;
    }
    const uint32_t elapsed = nowMs - panReturnStartMs_;
    if (elapsed >= PAN_RETURN_MS) {
        panX_ = 0.0f;
        panReturning_ = false;
    } else {
        const float t = static_cast<float>(elapsed) / PAN_RETURN_MS;
        const float eased = t * t * (3.0f - 2.0f * t);
        panX_ = panReturnFrom_ * (1.0f - eased);
    }
    return applyPan(room);
}

int HomePresenter::worldToScreenX(const RoomLayout& room, float worldX) const {
    if (!room.available) {
        return static_cast<int>(std::lround(worldX * AmoledUi::RESOURCE_SCALE));
    }
    return static_cast<int>(std::lround((worldX - cameraX_) * AmoledUi::RESOURCE_SCALE));
}

int HomePresenter::worldToScreenY(const RoomLayout& room, float worldY) const {
    if (!room.available) {
        return static_cast<int>(std::lround(worldY * AmoledUi::RESOURCE_SCALE));
    }
    return UiMetrics::HOME_HEADER_HEIGHT +
        static_cast<int>(std::lround((worldY - cameraY_) * AmoledUi::RESOURCE_SCALE));
}

void HomePresenter::startSleep(uint16_t speciesId, uint32_t nowMs, uint16_t frameMs) {
    PokemonSprites::PetAnimationProfile profile{};
    PokemonSprites::petAnimationProfile(speciesId, profile);
    uint8_t expanded = 0;
    PokemonSprites::FrameBounds largest{};
    for (uint8_t i = 0; i < profile.sleepingFrames; ++i) {
        const auto* frame = PokemonSprites::findSpeciesSprite(
            speciesId, static_cast<PokemonSprites::SpriteKind>(
                static_cast<uint16_t>(profile.sleepingBase) + i));
        const auto bounds = PokemonSprites::frameVisibleBounds(frame);
        if (bounds.height > largest.height ||
            (bounds.height == largest.height && bounds.width > largest.width)) {
            expanded = i;
            largest = bounds;
        }
    }
    sleepBreath_.start(nowMs, frameMs, profile.sleepingFrames, expanded);
}

}  // namespace AmoledV1
