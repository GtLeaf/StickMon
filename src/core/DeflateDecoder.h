#pragma once

#include <cstddef>
#include <cstdint>
#include "platform/api/PlatformServices.h"

namespace DeflateDecoder {

bool inflateMemory(const uint8_t* compressed, size_t compressedBytes,
                   uint8_t* output, size_t outputBytes);

struct Stats {
    uint32_t inputBytes = 0;
    uint32_t outputBytes = 0;
    uint32_t readMs = 0;
    uint32_t inflateMs = 0;
    uint32_t totalMs = 0;
};

// Inflates a raw DEFLATE stream from the file's current position into output.
bool inflateFile(Platform::ResourceFile& file,
                 uint32_t compressedBytes,
                 uint8_t* output,
                 size_t outputBytes,
                 uint32_t expectedCrc32,
                 Stats* stats = nullptr);

// Strict variant: requires the final DEFLATE marker and exact input length.
// outputCapacity must include at least one guard byte beyond outputBytes;
// an extra decoded byte is rejected and never exposed as valid output.
bool inflateFileExact(Platform::ResourceFile& file, uint32_t compressedBytes,
                      uint8_t* output, size_t outputCapacity, size_t outputBytes,
                      uint32_t expectedCrc32, Stats* stats = nullptr);

}  // namespace DeflateDecoder
