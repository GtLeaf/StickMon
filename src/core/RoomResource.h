#pragma once

#include <cstddef>
#include <cstdint>

class RoomResource {
public:
    struct Point {
        int16_t x;
        int16_t y;
    };

    struct __attribute__((packed)) PatchRun {
        uint16_t y;
        uint16_t x;
        uint16_t len;
        uint32_t colorOffset;
    };

    static RoomResource& ins();

    bool begin();
    bool available() const { return loaded_; }
    bool missing() const { return initialized_ && !loaded_; }
    bool external() const { return loaded_; }
    const char* source() const { return loaded_ ? "littlefs" : "missing"; }

    uint16_t width() const { return width_; }
    uint16_t height() const { return height_; }
    int16_t roomY() const { return roomY_; }
    uint32_t pixelCount() const { return (uint32_t)width_ * height_; }
    uint32_t baseRawBytes() const { return baseRawBytes_; }
    uint32_t baseCompressedLen() const { return baseCompressedLen_; }
    uint32_t nightPatchRunCount() const { return nightPatchRunCount_; }
    uint32_t nightPatchPixelCount() const { return nightPatchPixelCount_; }

    uint8_t baseCompressedByte(uint32_t index) const;
    PatchRun nightPatchRun(uint32_t index) const;
    uint16_t nightPatchPixel(uint32_t index) const;

    uint8_t walkPolygonCount() const { return walkPolygonCount_; }
    int16_t walkMinX() const { return walkMinX_; }
    int16_t walkMinY() const { return walkMinY_; }
    int16_t walkMaxX() const { return walkMaxX_; }
    int16_t walkMaxY() const { return walkMaxY_; }
    Point walkPoint(uint8_t index) const;

    int16_t foodX() const { return foodX_; }
    int16_t foodY() const { return foodY_; }

    uint8_t bedPolygonCount() const { return bedPolygonCount_; }
    int16_t bedMinX() const { return bedMinX_; }
    int16_t bedMinY() const { return bedMinY_; }
    int16_t bedMaxX() const { return bedMaxX_; }
    int16_t bedMaxY() const { return bedMaxY_; }
    int16_t bedX() const { return bedX_; }
    int16_t bedY() const { return bedY_; }
    Point bedPoint(uint8_t index) const;

private:
    RoomResource() = default;
    ~RoomResource() = default;

    bool loadExternal();
    void clearMeta();
    void releaseExternal();

    bool initialized_ = false;
    bool loaded_ = false;

    uint16_t width_ = 0;
    uint16_t height_ = 0;
    int16_t roomY_ = 0;
    uint32_t baseRawBytes_ = 0;
    uint32_t baseCompressedLen_ = 0;
    uint32_t nightPatchRunCount_ = 0;
    uint32_t nightPatchPixelCount_ = 0;

    uint8_t walkPolygonCount_ = 0;
    int16_t walkMinX_ = 0;
    int16_t walkMinY_ = 0;
    int16_t walkMaxX_ = 0;
    int16_t walkMaxY_ = 0;
    int16_t foodX_ = 0;
    int16_t foodY_ = 0;

    uint8_t bedPolygonCount_ = 0;
    int16_t bedMinX_ = 0;
    int16_t bedMinY_ = 0;
    int16_t bedMaxX_ = 0;
    int16_t bedMaxY_ = 0;
    int16_t bedX_ = 0;
    int16_t bedY_ = 0;

    uint8_t* baseCompressed_ = nullptr;
    PatchRun* nightPatchRuns_ = nullptr;
    uint16_t* nightPatchPixels_ = nullptr;
    Point* walkPolygon_ = nullptr;
    Point* bedPolygon_ = nullptr;
};
