#pragma once

// Rebuild frozen schema-1/v3, schema-2/v4 and schema-3/v5 payloads by removing the
// append-only v6/v5 extensions (and v4 flag for schema 1), then updating version,
// length and CRC.

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
    // Schema 4 (v6) appends the market tutor record after the care records.
    constexpr size_t MARKET_EXTENSION_BYTES = 4 + 2 + 2 + 1 + 1 + Game::MARKET_TUTOR_TRAIN_CAP * 8;
    // Schema 5 (v7) appends the Kecleon shelf after the tutor record.
    constexpr size_t SHOP_EXTENSION_BYTES = 4 + 2 * Game::MARKET_SHELF_SLOTS;
    // Schema 6 (v8) appends the Chansey stall record after the shelf.
    constexpr size_t CARE_STALL_EXTENSION_BYTES = 4 + 1 + Game::MARKET_CARE_PERIOD_CAP * 8;
    constexpr size_t MYSTERY_BOX_EXTENSION_BYTES = 2;
    constexpr size_t SEASON_EXTENSION_BYTES = 4 + 8 + 8 + 1 + 17 + 1;
    const size_t removedBytes = (schema <= 7 ? SEASON_EXTENSION_BYTES : 0) +
                                (schema <= 6 ? MYSTERY_BOX_EXTENSION_BYTES : 0) +
                                (schema <= 5 ? CARE_STALL_EXTENSION_BYTES : 0) +
                                (schema <= 4 ? SHOP_EXTENSION_BYTES : 0) +
                                (schema <= 3 ? MARKET_EXTENSION_BYTES : 0) +
                                (schema <= 2 ? CARE_EXTENSION_BYTES : 0) + (schema == 1 ? 1 : 0);
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
        static_cast<uint8_t>(schema == 1 ? 3 : schema == 2 ? 4 : schema == 3 ? 5 : schema == 4 ? 6 : schema == 5 ? 7 : schema <= 7 ? 8 : 9);
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

inline bool fabricateSchema5Blob(const uint8_t* encoded, size_t length,
                                 std::vector<uint8_t>& out) {
    return fabricateLegacySchemaBlob(encoded, length, out, 5);
}

inline bool fabricateSchema4Blob(const uint8_t* encoded, size_t length,
                                 std::vector<uint8_t>& out) {
    return fabricateLegacySchemaBlob(encoded, length, out, 4);
}

inline bool fabricateSchema3Blob(const uint8_t* encoded, size_t length,
                                 std::vector<uint8_t>& out) {
    return fabricateLegacySchemaBlob(encoded, length, out, 3);
}

inline bool fabricateSchema2Blob(const uint8_t* encoded, size_t length,
                                 std::vector<uint8_t>& out) {
    return fabricateLegacySchemaBlob(encoded, length, out, 2);
}

} // namespace SaveTestUtil
