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
        cameraX_ = 0.0f;
        cameraY_ = 0.0f;
        return;
    }

    const float screenX = actorX - cameraX_;
    if (screenX < CAMERA_SAFE_LEFT) cameraX_ = actorX - CAMERA_SAFE_LEFT;
    else if (screenX > CAMERA_SAFE_RIGHT) cameraX_ = actorX - CAMERA_SAFE_RIGHT;

    const float screenY = actorY - cameraY_;
    if (screenY < CAMERA_SAFE_TOP) cameraY_ = actorY - CAMERA_SAFE_TOP;
    else if (screenY > CAMERA_SAFE_BOTTOM) cameraY_ = actorY - CAMERA_SAFE_BOTTOM;

    const float maximumCameraX = std::max<float>(0.0f,
        static_cast<float>(room.width - AmoledUi::WIDTH / AmoledUi::RESOURCE_SCALE));
    const float minimumCameraY = static_cast<float>(room.roomY);
    const float maximumCameraY = std::max<float>(minimumCameraY,
        static_cast<float>(room.roomY + room.height -
            UiMetrics::HOME_ROOM_HEIGHT / AmoledUi::RESOURCE_SCALE));
    // Room sampling, sprite placement and damage use the same logical grid.
    cameraX_ = std::clamp(std::round(cameraX_), 0.0f, maximumCameraX);
    cameraY_ = std::clamp(std::round(cameraY_), minimumCameraY, maximumCameraY);
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
