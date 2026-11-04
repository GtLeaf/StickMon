// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstring>
#include "platform/api/PlatformServices.h"

namespace Sdgoods {
// Explicit little-endian float32 record, separate from the game/save schema.
class TouchCalibrationStore {
public:
    static constexpr size_t SIZE = 32;
    static constexpr const char* NAMESPACE = "touchcal";
    static constexpr const char* KEY = "affine_v1";
    static Platform::TouchTransform load(Platform::IBlobStore& store, bool* restored = nullptr) {
        uint8_t data[SIZE]{};
        Platform::TouchTransform result;
        const bool valid = store.blobSize(NAMESPACE, KEY) == SIZE &&
            store.readBlob(NAMESPACE, KEY, data, SIZE) && decode(data, result);
        if (restored) *restored = valid;
        return result;
    }
    static bool save(Platform::IBlobStore& store, const Platform::TouchTransform& transform) {
        if (!transform.valid(360, 360)) return false;
        uint8_t data[SIZE] = {'S', 'G', 'T', 1};
        static_assert(sizeof(float) == 4, "Calibration record requires float32");
        for (int i = 0; i < 6; ++i) {
            uint32_t bits;
            std::memcpy(&bits, &transform.values[i], 4);
            put(data + 4 + i*4, bits);
        }
        put(data + 28, checksum(data, 28));
        return store.writeBlob(NAMESPACE, KEY, data, SIZE);
    }
    static bool decode(const uint8_t (&data)[SIZE], Platform::TouchTransform& output) {
        if (data[0] != 'S' || data[1] != 'G' || data[2] != 'T' || data[3] != 1 ||
            get(data + 28) != checksum(data, 28)) return false;
        Platform::TouchTransform candidate;
        for (int i = 0; i < 6; ++i) {
            const uint32_t bits = get(data + 4 + i*4);
            std::memcpy(&candidate.values[i], &bits, 4);
        }
        if (!candidate.valid(360, 360)) return false;
        output = candidate;
        return true;
    }
private:
    static uint32_t checksum(const uint8_t* data, size_t size) {
        uint32_t value = 2166136261u;
        for (size_t i = 0; i < size; ++i) value = (value ^ data[i]) * 16777619u;
        return value;
    }
    static void put(uint8_t* p, uint32_t v) {
        for (int i = 0; i < 4; ++i) p[i] = uint8_t(v >> (i*8));
    }
    static uint32_t get(const uint8_t* p) {
        return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
    }
};
}  // namespace Sdgoods
