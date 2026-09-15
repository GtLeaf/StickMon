#include "ui/RenderCaches.h"

#include <algorithm>
#include <cstring>
#include <limits>

#include "platform/api/PlatformServices.h"
#include "presentation/Canvas565.h"

namespace AmoledV1 {

bool RenderCacheKey::operator==(const RenderCacheKey& other) const {
    return identity == other.identity && variant == other.variant &&
           width == other.width && height == other.height &&
           byteSwapped == other.byteSwapped;
}

PixelCache565::~PixelCache565() {
    release();
}

bool PixelCache565::matches(const RenderCacheKey& key) const {
    return valid_ && pixels_ && key_ == key;
}

uint16_t* PixelCache565::begin(const RenderCacheKey& key,
                              Platform::IMemoryAllocator& allocator) {
    invalidate();
    if (key.width <= 0 || key.height <= 0 ||
        static_cast<size_t>(key.width) >
            std::numeric_limits<size_t>::max() / sizeof(uint16_t) /
                static_cast<size_t>(key.height)) {
        release();
        return nullptr;
    }
    const size_t count = static_cast<size_t>(key.width) * key.height;
    if (pixelCount_ != count || allocator_ != &allocator) release();
    if (!pixels_) {
        pixels_ = static_cast<uint16_t*>(allocator.allocate(count * sizeof(uint16_t), true));
        if (!pixels_) return nullptr;
        pixelCount_ = count;
        allocator_ = &allocator;
    }
    key_ = key;
    return pixels_;
}

void PixelCache565::commit() {
    valid_ = pixels_ != nullptr;
}

void PixelCache565::invalidate() {
    valid_ = false;
}

void PixelCache565::release() {
    if (pixels_) allocator_->release(pixels_);
    pixels_ = nullptr;
    pixelCount_ = 0;
    allocator_ = nullptr;
    key_ = {};
    valid_ = false;
}

bool PixelCache565::copyRowsTo(Canvas565& canvas, const RenderCacheKey& key,
                              uint16_t rowBegin, uint16_t rowEnd) const {
    if (!matches(key) || !canvas.attached() ||
        key.width != canvas.physicalWidth() ||
        key.height != canvas.physicalHeight() ||
        key.byteSwapped != canvas.byteSwapped()) return false;
    const int bottom = std::min<int>(rowEnd, key.height);
    for (int row = rowBegin; row < bottom; ++row) {
        const size_t offset = static_cast<size_t>(row) * key.width;
        std::memcpy(canvas.rawPixels() + offset, pixels_ + offset,
                    static_cast<size_t>(key.width) * sizeof(uint16_t));
    }
    return true;
}

void RenderCaches::retainForScene(AppSceneFlow::Scene scene) {
    using AppSceneFlow::Scene;
    if (scene != Scene::BATTLE) battleBackground.release();
    if (scene != Scene::EXPLORE_AREAS) exploreBackground.release();
    // Keep the world through route menus, battles and progression dialogs.
    if (scene == Scene::HOME || scene == Scene::EXPLORE_AREAS) exploreWorld.release();
}

void RenderCaches::release() {
    battleBackground.release();
    exploreBackground.release();
    exploreWorld.release();
}

}  // namespace AmoledV1
