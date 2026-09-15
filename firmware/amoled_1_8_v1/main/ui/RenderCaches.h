#pragma once

#include <cstddef>
#include <cstdint>

#include "core/AppSceneFlow.h"

class Canvas565;
namespace Platform { class IMemoryAllocator; }

namespace AmoledV1 {

// identity is the asset kind for backgrounds or the seed for a world map.
// variant distinguishes areas whose maps can share a seed.
struct RenderCacheKey {
    uint32_t identity = 0;
    uint32_t variant = 0;
    int width = 0;
    int height = 0;
    bool byteSwapped = false;

    bool operator==(const RenderCacheKey& other) const;
};

class PixelCache565 {
public:
    PixelCache565() = default;
    ~PixelCache565();
    PixelCache565(const PixelCache565&) = delete;
    PixelCache565& operator=(const PixelCache565&) = delete;

    bool matches(const RenderCacheKey& key) const;
    // Begin invalidates old content. Call commit only after all pixels are ready.
    // The allocator must outlive this cache; release uses the allocating service.
    uint16_t* begin(const RenderCacheKey& key, Platform::IMemoryAllocator& allocator);
    void commit();
    void invalidate();
    void release();
    const uint16_t* data() const { return pixels_; }
    size_t allocatedBytes() const { return pixelCount_ * sizeof(uint16_t); }
    bool copyRowsTo(Canvas565& canvas, const RenderCacheKey& key,
                    uint16_t rowBegin, uint16_t rowEnd) const;

private:
    uint16_t* pixels_ = nullptr;
    size_t pixelCount_ = 0;
    Platform::IMemoryAllocator* allocator_ = nullptr;
    RenderCacheKey key_{};
    bool valid_ = false;
};

// Owned by the application, not by static renderer state.
struct RenderCaches {
    PixelCache565 battleBackground;
    PixelCache565 exploreBackground;
    PixelCache565 exploreWorld;

    void retainForScene(AppSceneFlow::Scene scene);
    void release();
};

}  // namespace AmoledV1
