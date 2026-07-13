#pragma once

#include <FS.h>
#include <cstddef>
#include <cstdint>

namespace DeflateDecoder {

struct Stats {
    uint32_t inputBytes = 0;
    uint32_t outputBytes = 0;
    uint32_t readMs = 0;
    uint32_t inflateMs = 0;
    uint32_t totalMs = 0;
};

// Inflates a raw DEFLATE stream from the file's current position into output.
bool inflateFile(fs::File& file,
                 uint32_t compressedBytes,
                 uint8_t* output,
                 size_t outputBytes,
                 uint32_t expectedCrc32,
                 Stats* stats = nullptr);

}  // namespace DeflateDecoder
