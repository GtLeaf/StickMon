#pragma once

// Test helper: rebuilds a schema-1 (v3 state) SaveCodec blob from a blob
// produced by the current encoder. Schema 2 appended exactly one byte
// (GameState::debugMotionFlags) at the end of the state payload, so the
// legacy blob is the same bytes minus that one, with the header schema,
// payload length, state version and CRC patched accordingly.

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

inline bool fabricateSchema1Blob(const uint8_t* encoded, size_t length,
                                 std::vector<uint8_t>& out) {
    if (!encoded ||
        length < SaveCodec::HEADER_BYTES + VIEW_PAYLOAD_BYTES + 2) {
        return false;
    }
    const size_t debugFlagsOffset = length - VIEW_PAYLOAD_BYTES - 1;
    out.clear();
    out.reserve(length - 1);
    out.insert(out.end(), encoded, encoded + debugFlagsOffset);
    out.insert(out.end(), encoded + debugFlagsOffset + 1, encoded + length);
    out[4] = 1; // schema 1
    out[5] = 0;
    // State version lives right after the payload magic.
    out[SaveCodec::HEADER_BYTES + 4] =
        static_cast<uint8_t>(SaveCodec::SCHEMA_1_STATE_VERSION);
    out[SaveCodec::HEADER_BYTES + 5] =
        static_cast<uint8_t>(SaveCodec::SCHEMA_1_STATE_VERSION >> 8);
    const uint16_t payloadLength =
        static_cast<uint16_t>(out.size() - SaveCodec::HEADER_BYTES);
    out[12] = static_cast<uint8_t>(payloadLength);
    out[13] = static_cast<uint8_t>(payloadLength >> 8);
    const uint16_t checksum = blobCrc16(out.data(), out.size());
    out[14] = static_cast<uint8_t>(checksum);
    out[15] = static_cast<uint8_t>(checksum >> 8);
    return true;
}

} // namespace SaveTestUtil
