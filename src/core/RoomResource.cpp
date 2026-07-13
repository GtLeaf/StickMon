#include "core/RoomResource.h"

#include "core/ResourcePack.h"

#include <Arduino.h>
#include <FS.h>
#include <cstdlib>
#include <cstring>

namespace {
static constexpr uint32_t ROOM_PACK_MAGIC = 0x4D4F5253;
static constexpr uint16_t ROOM_PACK_VERSION = 2;
static constexpr const char* DEFAULT_ROOM_ID = "standard";
static constexpr uint16_t MAX_ROOM_W = 240;
static constexpr uint16_t MAX_ROOM_H = 320;
static constexpr uint8_t MAX_POLYGON_POINTS = 32;
static constexpr uint32_t MAX_PATCH_RUNS = 4096;
static constexpr uint32_t MAX_PATCH_PIXELS = 65535;

struct __attribute__((packed)) PackedRoomHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t width;
    uint16_t height;
    int16_t roomY;
    uint32_t baseRawBytes;
    uint32_t baseCompressedLen;
    uint32_t nightPatchRunCount;
    uint32_t nightPatchPixelCount;
    uint8_t walkPolygonCount;
    uint8_t bedPolygonCount;
    uint8_t doorwayPolygonCount;
    uint8_t reservedCount;
    int16_t walkMinX;
    int16_t walkMinY;
    int16_t walkMaxX;
    int16_t walkMaxY;
    int16_t foodX;
    int16_t foodY;
    int16_t bedMinX;
    int16_t bedMinY;
    int16_t bedMaxX;
    int16_t bedMaxY;
    int16_t bedX;
    int16_t bedY;
    int16_t doorwayMinX;
    int16_t doorwayMinY;
    int16_t doorwayMaxX;
    int16_t doorwayMaxY;
    int16_t doorwayInsideX;
    int16_t doorwayInsideY;
    int16_t doorwayOutsideX;
    int16_t doorwayOutsideY;
    uint16_t flags;
    uint16_t reserved;
};
static_assert(sizeof(PackedRoomHeader) == 76, "room pack v2 header size changed");

template <typename T>
T* allocArray(size_t count) {
    if (count == 0) return nullptr;
    size_t bytes = sizeof(T) * count;
    return psramFound() ? static_cast<T*>(ps_malloc(bytes)) : static_cast<T*>(malloc(bytes));
}

bool readExact(fs::File& file, void* out, size_t length) {
    if (length == 0) return true;
    return file.read(reinterpret_cast<uint8_t*>(out), length) == length;
}

bool validHeader(const PackedRoomHeader& h) {
    if (h.magic != ROOM_PACK_MAGIC || h.version != ROOM_PACK_VERSION) return false;
    if (h.width == 0 || h.width > MAX_ROOM_W || h.height == 0 || h.height > MAX_ROOM_H) return false;
    if (h.roomY < 0 || h.roomY >= static_cast<int16_t>(MAX_ROOM_H)) return false;
    if (h.baseRawBytes != (uint32_t)h.width * h.height * sizeof(uint16_t)) return false;
    if (h.baseCompressedLen == 0 || h.baseCompressedLen > h.baseRawBytes) return false;
    if (h.walkPolygonCount < 3 || h.walkPolygonCount > MAX_POLYGON_POINTS ||
        h.bedPolygonCount < 3 || h.bedPolygonCount > MAX_POLYGON_POINTS) return false;
    if (h.doorwayPolygonCount != 0 &&
        (h.doorwayPolygonCount < 3 || h.doorwayPolygonCount > MAX_POLYGON_POINTS)) return false;
    if (h.nightPatchRunCount > MAX_PATCH_RUNS || h.nightPatchPixelCount > MAX_PATCH_PIXELS) return false;
    if ((h.nightPatchRunCount == 0) != (h.nightPatchPixelCount == 0)) return false;
    if (h.flags > 1 || h.reservedCount != 0 || h.reserved != 0) return false;

    auto validBounds = [](int16_t minX, int16_t minY, int16_t maxX, int16_t maxY,
                          uint16_t width, uint16_t height) {
        return minX >= 0 && minY >= 0 && maxX >= minX && maxY >= minY &&
               maxX < static_cast<int16_t>(width) && maxY < static_cast<int16_t>(height);
    };
    auto validPoint = [](int16_t x, int16_t y, uint16_t width, uint16_t height) {
        return x >= 0 && y >= 0 && x < static_cast<int16_t>(width) && y < static_cast<int16_t>(height);
    };
    if (!validBounds(h.walkMinX, h.walkMinY, h.walkMaxX, h.walkMaxY, h.width, h.height) ||
        !validBounds(h.bedMinX, h.bedMinY, h.bedMaxX, h.bedMaxY, h.width, h.height)) {
        return false;
    }
    if (h.doorwayPolygonCount > 0 &&
        !validBounds(h.doorwayMinX, h.doorwayMinY, h.doorwayMaxX, h.doorwayMaxY,
                     h.width, h.height)) return false;
    if (!validPoint(h.foodX, h.foodY, h.width, h.height) ||
        !validPoint(h.bedX, h.bedY, h.width, h.height)) {
        return false;
    }
    if (h.doorwayPolygonCount > 0 &&
        (!validPoint(h.doorwayInsideX, h.doorwayInsideY, h.width, h.height) ||
         !validPoint(h.doorwayOutsideX, h.doorwayOutsideY, h.width, h.height))) return false;
    return true;
}

bool validPayload(const PackedRoomHeader& h, const RoomResource::PatchRun* runs,
                  const RoomResource::Point* walk, const RoomResource::Point* bed,
                  const RoomResource::Point* doorway) {
    uint32_t expectedColorOffset = 0;
    for (uint32_t i = 0; i < h.nightPatchRunCount; ++i) {
        const RoomResource::PatchRun& run = runs[i];
        if (run.len == 0 || run.y >= h.height || run.x >= h.width || run.len > h.width - run.x) return false;
        if (run.colorOffset != expectedColorOffset ||
            run.colorOffset > h.nightPatchPixelCount ||
            run.len > h.nightPatchPixelCount - run.colorOffset) {
            return false;
        }
        expectedColorOffset += run.len;
    }
    if (expectedColorOffset != h.nightPatchPixelCount) return false;

    auto validatePolygon = [&](const RoomResource::Point* points, uint8_t count,
                               int16_t expectedMinX, int16_t expectedMinY,
                               int16_t expectedMaxX, int16_t expectedMaxY) {
        int16_t minX = points[0].x;
        int16_t minY = points[0].y;
        int16_t maxX = points[0].x;
        int16_t maxY = points[0].y;
        for (uint8_t i = 0; i < count; ++i) {
            const RoomResource::Point& point = points[i];
            if (point.x < 0 || point.y < 0 ||
                point.x >= static_cast<int16_t>(h.width) || point.y >= static_cast<int16_t>(h.height)) {
                return false;
            }
            minX = min(minX, point.x);
            minY = min(minY, point.y);
            maxX = max(maxX, point.x);
            maxY = max(maxY, point.y);
        }
        return minX == expectedMinX && minY == expectedMinY && maxX == expectedMaxX && maxY == expectedMaxY;
    };

    bool valid = validatePolygon(walk, h.walkPolygonCount,
                                 h.walkMinX, h.walkMinY, h.walkMaxX, h.walkMaxY) &&
                 validatePolygon(bed, h.bedPolygonCount,
                                 h.bedMinX, h.bedMinY, h.bedMaxX, h.bedMaxY);
    if (!valid || h.doorwayPolygonCount == 0) return valid;
    return validatePolygon(doorway, h.doorwayPolygonCount,
                           h.doorwayMinX, h.doorwayMinY,
                           h.doorwayMaxX, h.doorwayMaxY);
}
}

RoomResource& RoomResource::ins() {
    static RoomResource instance;
    return instance;
}

bool RoomResource::begin() {
    if (initialized_) return loaded_;
    initialized_ = true;
    clearMeta();
    loaded_ = loadExternal();
    Serial.printf("[RoomResource] source=%s size=%ux%u base=%u patchRuns=%u patchPixels=%u "
                  "door=%u inside=%d,%d outside=%d,%d\n",
                  source(), width_, height_, baseCompressedLen_,
                  nightPatchRunCount_, nightPatchPixelCount_, doorwayPolygonCount_,
                  doorwayInsideX_, doorwayInsideY_, doorwayOutsideX_, doorwayOutsideY_);
    return loaded_;
}

RoomResource::PatchRun RoomResource::nightPatchRun(uint32_t index) const {
    PatchRun out{};
    if (index >= nightPatchRunCount_) return out;
    return loaded_ ? nightPatchRuns_[index] : out;
}

uint16_t RoomResource::nightPatchPixel(uint32_t index) const {
    if (index >= nightPatchPixelCount_) return 0;
    return loaded_ ? nightPatchPixels_[index] : 0;
}

RoomResource::Point RoomResource::walkPoint(uint8_t index) const {
    Point out{};
    if (index >= walkPolygonCount_) return out;
    return loaded_ ? walkPolygon_[index] : out;
}

RoomResource::Point RoomResource::bedPoint(uint8_t index) const {
    Point out{};
    if (index >= bedPolygonCount_) return out;
    return loaded_ ? bedPolygon_[index] : out;
}

RoomResource::Point RoomResource::doorwayPoint(uint8_t index) const {
    Point out{};
    if (index >= doorwayPolygonCount_) return out;
    return loaded_ ? doorwayPolygon_[index] : out;
}

bool RoomResource::loadExternal() {
    ResourcePack& pack = ResourcePack::ins();
    if (!pack.begin()) return false;

    fs::File file;
    if (!pack.openRoom(DEFAULT_ROOM_ID, file)) return false;

    PackedRoomHeader header{};
    if (!readExact(file, &header, sizeof(header)) || !validHeader(header)) {
        Serial.println("[RoomResource] invalid room header");
        return false;
    }

    uint64_t expectedSize = sizeof(header) + static_cast<uint64_t>(header.baseCompressedLen) +
                            static_cast<uint64_t>(header.nightPatchRunCount) * sizeof(PatchRun) +
                            static_cast<uint64_t>(header.nightPatchPixelCount) * sizeof(uint16_t) +
                            static_cast<uint64_t>(header.walkPolygonCount + header.bedPolygonCount +
                                                  header.doorwayPolygonCount) * sizeof(Point);
    if (expectedSize != file.size()) {
        Serial.println("[RoomResource] room payload size mismatch");
        return false;
    }

    uint8_t* baseCompressed = allocArray<uint8_t>(header.baseCompressedLen);
    PatchRun* patchRuns = allocArray<PatchRun>(header.nightPatchRunCount);
    uint16_t* patchPixels = allocArray<uint16_t>(header.nightPatchPixelCount);
    Point* walk = allocArray<Point>(header.walkPolygonCount);
    Point* bed = allocArray<Point>(header.bedPolygonCount);
    Point* doorway = allocArray<Point>(header.doorwayPolygonCount);

    if (!baseCompressed ||
        (header.nightPatchRunCount && !patchRuns) ||
        (header.nightPatchPixelCount && !patchPixels) ||
        (header.walkPolygonCount && !walk) ||
        (header.bedPolygonCount && !bed) ||
        (header.doorwayPolygonCount && !doorway)) {
        if (baseCompressed) free(baseCompressed);
        if (patchRuns) free(patchRuns);
        if (patchPixels) free(patchPixels);
        if (walk) free(walk);
        if (bed) free(bed);
        if (doorway) free(doorway);
        return false;
    }

    bool ok =
        readExact(file, baseCompressed, header.baseCompressedLen) &&
        readExact(file, patchRuns, sizeof(PatchRun) * header.nightPatchRunCount) &&
        readExact(file, patchPixels, sizeof(uint16_t) * header.nightPatchPixelCount) &&
        readExact(file, walk, sizeof(Point) * header.walkPolygonCount) &&
        readExact(file, bed, sizeof(Point) * header.bedPolygonCount) &&
        readExact(file, doorway, sizeof(Point) * header.doorwayPolygonCount);
    if (!ok) {
        free(baseCompressed);
        if (patchRuns) free(patchRuns);
        if (patchPixels) free(patchPixels);
        if (walk) free(walk);
        if (bed) free(bed);
        if (doorway) free(doorway);
        return false;
    }

    if (!validPayload(header, patchRuns, walk, bed, doorway)) {
        free(baseCompressed);
        if (patchRuns) free(patchRuns);
        if (patchPixels) free(patchPixels);
        if (walk) free(walk);
        if (bed) free(bed);
        if (doorway) free(doorway);
        Serial.println("[RoomResource] invalid room metadata");
        return false;
    }

    releaseExternal();
    width_ = header.width;
    height_ = header.height;
    roomY_ = header.roomY;
    baseRawBytes_ = header.baseRawBytes;
    baseCompressedLen_ = header.baseCompressedLen;
    nightPatchRunCount_ = header.nightPatchRunCount;
    nightPatchPixelCount_ = header.nightPatchPixelCount;
    walkPolygonCount_ = header.walkPolygonCount;
    bedPolygonCount_ = header.bedPolygonCount;
    doorwayPolygonCount_ = header.doorwayPolygonCount;
    walkMinX_ = header.walkMinX;
    walkMinY_ = header.walkMinY;
    walkMaxX_ = header.walkMaxX;
    walkMaxY_ = header.walkMaxY;
    foodX_ = header.foodX;
    foodY_ = header.foodY;
    bedMinX_ = header.bedMinX;
    bedMinY_ = header.bedMinY;
    bedMaxX_ = header.bedMaxX;
    bedMaxY_ = header.bedMaxY;
    bedX_ = header.bedX;
    bedY_ = header.bedY;
    doorwayMinX_ = header.doorwayMinX;
    doorwayMinY_ = header.doorwayMinY;
    doorwayMaxX_ = header.doorwayMaxX;
    doorwayMaxY_ = header.doorwayMaxY;
    doorwayInsideX_ = header.doorwayInsideX;
    doorwayInsideY_ = header.doorwayInsideY;
    doorwayOutsideX_ = header.doorwayOutsideX;
    doorwayOutsideY_ = header.doorwayOutsideY;
    baseCompressed_ = baseCompressed;
    nightPatchRuns_ = patchRuns;
    nightPatchPixels_ = patchPixels;
    walkPolygon_ = walk;
    bedPolygon_ = bed;
    doorwayPolygon_ = doorway;
    return true;
}

void RoomResource::clearMeta() {
    releaseExternal();
    loaded_ = false;
    width_ = 0;
    height_ = 0;
    roomY_ = 0;
    baseRawBytes_ = 0;
    baseCompressedLen_ = 0;
    nightPatchRunCount_ = 0;
    nightPatchPixelCount_ = 0;
    walkPolygonCount_ = 0;
    walkMinX_ = 0;
    walkMinY_ = 0;
    walkMaxX_ = 0;
    walkMaxY_ = 0;
    foodX_ = 0;
    foodY_ = 0;
    bedPolygonCount_ = 0;
    bedMinX_ = 0;
    bedMinY_ = 0;
    bedMaxX_ = 0;
    bedMaxY_ = 0;
    bedX_ = 0;
    bedY_ = 0;
    doorwayPolygonCount_ = 0;
    doorwayMinX_ = 0;
    doorwayMinY_ = 0;
    doorwayMaxX_ = 0;
    doorwayMaxY_ = 0;
    doorwayInsideX_ = 0;
    doorwayInsideY_ = 0;
    doorwayOutsideX_ = 0;
    doorwayOutsideY_ = 0;
}

void RoomResource::releaseExternal() {
    if (baseCompressed_) free(baseCompressed_);
    if (nightPatchRuns_) free(nightPatchRuns_);
    if (nightPatchPixels_) free(nightPatchPixels_);
    if (walkPolygon_) free(walkPolygon_);
    if (bedPolygon_) free(bedPolygon_);
    if (doorwayPolygon_) free(doorwayPolygon_);
    baseCompressed_ = nullptr;
    nightPatchRuns_ = nullptr;
    nightPatchPixels_ = nullptr;
    walkPolygon_ = nullptr;
    bedPolygon_ = nullptr;
    doorwayPolygon_ = nullptr;
}
