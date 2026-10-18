#pragma once

// Rebuild frozen schema-1/v3 and schema-2/v4 payloads by removing the append-only
// v5 extension (and v4 flag for schema 1), then updating version, length and CRC.

#include "core/SaveCodec.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace SaveTestUtil {

// Bytes written by SaveCodec's writeView (schema-independent): 33 bytes for
// the primary view record plus 51 for the secondary record.
constexpr size_t VIEW_PAYLOAD_BYTES = 84;

inline uint16_t blobCrc16(const uint8_t* bytes, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t index = 0; index < length; ++index) {
        uint8_t value = (index == 14 || index == 15) ? 0 : bytes[index];
        crc ^= value;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 1U) ? static_cast<uint16_t>((crc >> 1) ^ 0xA001)
                             : static_cast<uint16_t>(crc >> 1);
        }
    }
    return crc;
}

inline bool fabricateLegacySchemaBlob(const uint8_t* encoded, size_t length,
                                      std::vector<uint8_t>& out, uint16_t schema) {
    constexpr size_t CARE_EXTENSION_BYTES = 4 + Game::CARE_REWARD_RECORD_CAP * 35;
    const size_t removedBytes = CARE_EXTENSION_BYTES + (schema == 1 ? 1 : 0);
    if (!encoded ||
        length < SaveCodec::HEADER_BYTES + VIEW_PAYLOAD_BYTES + removedBytes + 1) {
        return false;
    }
    const size_t extensionOffset = length - VIEW_PAYLOAD_BYTES - removedBytes;
    out.clear();
    out.reserve(length - removedBytes);
    out.insert(out.end(), encoded, encoded + extensionOffset);
    out.insert(out.end(), encoded + extensionOffset + removedBytes, encoded + length);
    out[4] = static_cast<uint8_t>(schema);
    out[5] = 0;
    // State version lives right after the payload magic.
    out[SaveCodec::HEADER_BYTES + 4] =
        static_cast<uint8_t>(schema == 1 ? 3 : 4);
    out[SaveCodec::HEADER_BYTES + 5] =
        0;
    const uint16_t payloadLength =
        static_cast<uint16_t>(out.size() - SaveCodec::HEADER_BYTES);
    out[12] = static_cast<uint8_t>(payloadLength);
    out[13] = static_cast<uint8_t>(payloadLength >> 8);
    const uint16_t checksum = blobCrc16(out.data(), out.size());
    out[14] = static_cast<uint8_t>(checksum);
    out[15] = static_cast<uint8_t>(checksum >> 8);
    return true;
}

inline bool fabricateSchema1Blob(const uint8_t* encoded, size_t length,
                                 std::vector<uint8_t>& out) {
    return fabricateLegacySchemaBlob(encoded, length, out, 1);
}

inline bool fabricateSchema2Blob(const uint8_t* encoded, size_t length,
                                 std::vector<uint8_t>& out) {
    return fabricateLegacySchemaBlob(encoded, length, out, 2);
}

} // namespace SaveTestUtil
