#pragma once

#include <cstddef>
#include <cstdint>

namespace Stickmon {
namespace QrCodeGen {

// Encodes text as a QR code: byte mode, ECC level L, mask pattern 0,
// auto-selected version 1..5 (21x21 up to 37x37 modules). outModules receives
// size*size bytes in row-major order, 1 = dark module. Returns the module
// count per side, or 0 on failure (null args, text too long, or outSize
// smaller than size*size).
int encode(const char* text, uint8_t* outModules, size_t outSize);

inline constexpr int MAX_VERSION = 5;
inline constexpr int MAX_MODULES = 21 + 4 * (MAX_VERSION - 1);  // 37
inline constexpr size_t MAX_MATRIX_BYTES =
    static_cast<size_t>(MAX_MODULES) * MAX_MODULES;

}  // namespace QrCodeGen
}  // namespace Stickmon
