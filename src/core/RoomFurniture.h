#pragma once

#include <cstdint>
#include "platform/api/PlatformServices.h"

// Room-authored placement and lighting, independent of reusable textures.
// Only the selected textures are held in memory. Rendering performs no IO.
class RoomFurniture {
public:
    static constexpr uint16_t MAX_PLACEMENTS = 32;
    RoomFurniture() = default;
    ~RoomFurniture() { clear(); }
    RoomFurniture(const RoomFurniture&) = delete;
    RoomFurniture& operator=(const RoomFurniture&) = delete;

    bool load(Platform::ResourceFile& file, uint16_t width, uint16_t height);
    bool replace(const char* slot, const char* assetId);
    const char* selected(const char* slot) const;
    void composite(uint8_t* rgb, uint16_t width, uint16_t height, bool night) const;
    void clear();
    void swap(RoomFurniture& other);
    uint16_t count() const { return count_; }
    const uint8_t* nightBase() const { return nightBase_; }
    uint32_t nightBaseLength() const { return nightBaseLength_; }

private:
    struct Texture {
        uint16_t width = 0, height = 0;
        uint8_t* rgba = nullptr;
    };
    struct Placement {
        char slot[24] = {}, asset[32] = {};
        int16_t x = 0, y = 0, shadowX = 0, shadowY = 0;
        uint16_t width = 0, height = 0, flags = 0;
        uint16_t shadowWidth = 0, shadowHeight = 0;
        uint8_t* shadowDay = nullptr;
        uint8_t* shadowNight = nullptr;
        Texture texture;
    };
    static bool loadTexture(const char* assetId, Texture& texture);
    Placement* placements_ = nullptr;
    uint16_t count_ = 0;
    uint8_t* nightBase_ = nullptr;
    uint32_t nightBaseLength_ = 0;
};
