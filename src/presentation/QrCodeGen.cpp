#include "presentation/QrCodeGen.h"

#include <algorithm>
#include <cstring>

// Minimal QR encoder derived from the classic Nayuki qrcodegen algorithms,
// reduced to what the device UI needs: byte mode, ECC level L, mask 0, and
// versions 1..5 (all single-block at ECC L, so no interleaving and no version
// info field are required). Encoding follows ISO/IEC 18004.

namespace Stickmon {
namespace QrCodeGen {
namespace {

// ECC level L, versions 1..5: data codewords, ECC codewords (single block).
constexpr int NUM_DATA_CODEWORDS[MAX_VERSION] = {19, 34, 55, 80, 108};
constexpr int NUM_ECC_CODEWORDS[MAX_VERSION] = {7, 10, 15, 20, 26};

// Alignment pattern centers per version (version 1 has none).
constexpr int ALIGN_CENTERS[MAX_VERSION][2] = {
    {0, 0}, {6, 18}, {6, 22}, {6, 26}, {6, 30},
};

constexpr int MAX_DATA_CODEWORDS = 108;
constexpr int MAX_ECC_CODEWORDS = 26;

// Non-null while encode() runs; keeps the drawing helpers stateless-looking.
uint8_t* s_modules = nullptr;
bool* s_isFunction = nullptr;
int s_size = 0;

uint8_t gfMultiply(uint8_t x, uint8_t y) {
    uint8_t z = 0;
    for (int i = 7; i >= 0; --i) {
        z = static_cast<uint8_t>((z << 1) ^ ((z >> 7) * 0x11D));
        z = static_cast<uint8_t>(z ^ (((y >> i) & 1) * x));
    }
    return z;
}

void rsComputeDivisor(int degree, uint8_t* result) {
    std::memset(result, 0, static_cast<size_t>(degree));
    result[degree - 1] = 1;
    uint8_t root = 1;
    for (int i = 0; i < degree; ++i) {
        for (int j = 0; j < degree; ++j) {
            result[j] = gfMultiply(result[j], root);
            if (j + 1 < degree) result[j] ^= result[j + 1];
        }
        root = gfMultiply(root, 0x02);
    }
}

void rsComputeRemainder(const uint8_t* data, int dataLen,
                        const uint8_t* divisor, int degree, uint8_t* result) {
    std::memset(result, 0, static_cast<size_t>(degree));
    for (int i = 0; i < dataLen; ++i) {
        const uint8_t factor = static_cast<uint8_t>(data[i] ^ result[0]);
        std::memmove(result, result + 1, static_cast<size_t>(degree - 1));
        result[degree - 1] = 0;
        for (int j = 0; j < degree; ++j) {
            result[j] ^= gfMultiply(divisor[j], factor);
        }
    }
}

void appendBits(uint32_t value, int count, uint8_t* buffer, int& bitLength) {
    for (int i = count - 1; i >= 0; --i) {
        if (((value >> i) & 1U) != 0) {
            buffer[bitLength >> 3] |=
                static_cast<uint8_t>(0x80 >> (bitLength & 7));
        }
        ++bitLength;
    }
}

void setFunctionModule(int x, int y, bool dark) {
    s_modules[y * s_size + x] = dark ? 1 : 0;
    s_isFunction[y * s_size + x] = true;
}

void drawFinderPattern(int centerX, int centerY) {
    for (int dy = -4; dy <= 4; ++dy) {
        for (int dx = -4; dx <= 4; ++dx) {
            const int x = centerX + dx;
            const int y = centerY + dy;
            if (x < 0 || x >= s_size || y < 0 || y >= s_size) continue;
            const int dist = std::max(dx < 0 ? -dx : dx, dy < 0 ? -dy : dy);
            setFunctionModule(x, y, dist != 2 && dist != 4);
        }
    }
}

void drawAlignmentPattern(int centerX, int centerY) {
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            const int dist = std::max(dx < 0 ? -dx : dx, dy < 0 ? -dy : dy);
            setFunctionModule(centerX + dx, centerY + dy, dist != 1);
        }
    }
}

int formatBits() {
    // ECC level L has format field 01; mask pattern 0.
    const int data = (0x1 << 3) | 0;
    int rem = data;
    for (int i = 0; i < 10; ++i) {
        rem = (rem << 1) ^ ((rem >> 9) * 0x537);
    }
    return ((data << 10) | rem) ^ 0x5412;
}

void drawFormatModules(int bits) {
    // First copy around the top-left finder pattern.
    for (int i = 0; i <= 5; ++i) {
        setFunctionModule(8, i, ((bits >> i) & 1) != 0);
    }
    setFunctionModule(8, 7, ((bits >> 6) & 1) != 0);
    setFunctionModule(8, 8, ((bits >> 7) & 1) != 0);
    setFunctionModule(7, 8, ((bits >> 8) & 1) != 0);
    for (int i = 9; i < 15; ++i) {
        setFunctionModule(14 - i, 8, ((bits >> i) & 1) != 0);
    }
    // Second copy split between the other two finder patterns.
    for (int i = 0; i < 8; ++i) {
        setFunctionModule(s_size - 1 - i, 8, ((bits >> i) & 1) != 0);
    }
    for (int i = 8; i < 15; ++i) {
        setFunctionModule(8, s_size - 15 + i, ((bits >> i) & 1) != 0);
    }
    // The dark module is always set.
    setFunctionModule(8, s_size - 8, true);
}

void drawFunctionPatterns(int version) {
    // Timing patterns.
    for (int i = 0; i < s_size; ++i) {
        setFunctionModule(6, i, i % 2 == 0);
        setFunctionModule(i, 6, i % 2 == 0);
    }
    // Finder patterns with separators.
    drawFinderPattern(3, 3);
    drawFinderPattern(s_size - 4, 3);
    drawFinderPattern(3, s_size - 4);
    // Alignment patterns, skipping the three finder corners.
    if (version > 1) {
        const int* centers = ALIGN_CENTERS[version - 1];
        for (int cy = 0; cy < 2; ++cy) {
            for (int cx = 0; cx < 2; ++cx) {
                const int x = centers[cx];
                const int y = centers[cy];
                const bool overlapsFinder =
                    (x == 6 && y == 6) || (x == 6 && y == s_size - 7) ||
                    (x == s_size - 7 && y == 6);
                if (!overlapsFinder) drawAlignmentPattern(x, y);
            }
        }
    }
    // Reserve the format info areas (real bits are drawn after masking).
    drawFormatModules(0);
}

void drawCodewords(const uint8_t* codewords, int count) {
    int bitIndex = 0;
    for (int right = s_size - 1; right >= 1; right -= 2) {
        if (right == 6) right = 5;  // Skip the vertical timing pattern.
        for (int vert = 0; vert < s_size; ++vert) {
            for (int j = 0; j < 2; ++j) {
                const int x = right - j;
                const bool upward = ((right + 1) & 2) == 0;
                const int y = upward ? s_size - 1 - vert : vert;
                if (s_isFunction[y * s_size + x] || bitIndex >= count * 8) {
                    continue;
                }
                s_modules[y * s_size + x] =
                    (codewords[bitIndex >> 3] >> (7 - (bitIndex & 7))) & 1;
                ++bitIndex;
            }
        }
    }
}

void applyMaskZero() {
    for (int y = 0; y < s_size; ++y) {
        for (int x = 0; x < s_size; ++x) {
            if (!s_isFunction[y * s_size + x] && (x + y) % 2 == 0) {
                s_modules[y * s_size + x] ^= 1;
            }
        }
    }
}

}  // namespace

int encode(const char* text, uint8_t* outModules, size_t outSize) {
    if (!text || !outModules) return 0;
    const size_t length = std::strlen(text);

    int version = 0;
    for (int candidate = 1; candidate <= MAX_VERSION; ++candidate) {
        const int capacityBits = NUM_DATA_CODEWORDS[candidate - 1] * 8;
        // 4-bit mode indicator + 8-bit length + payload bytes.
        if (4 + 8 + static_cast<int>(length) * 8 <= capacityBits) {
            version = candidate;
            break;
        }
    }
    if (version == 0) return 0;

    const int size = 21 + 4 * (version - 1);
    if (outSize < static_cast<size_t>(size) * size) return 0;

    const int numData = NUM_DATA_CODEWORDS[version - 1];
    const int numEcc = NUM_ECC_CODEWORDS[version - 1];
    const int capacityBits = numData * 8;

    uint8_t data[MAX_DATA_CODEWORDS] = {};
    int bitLength = 0;
    appendBits(0x4, 4, data, bitLength);  // Byte mode indicator.
    appendBits(static_cast<uint32_t>(length), 8, data, bitLength);
    for (size_t i = 0; i < length; ++i) {
        appendBits(static_cast<uint8_t>(text[i]), 8, data, bitLength);
    }
    const int terminator = capacityBits - bitLength < 4
        ? capacityBits - bitLength : 4;
    appendBits(0, terminator, data, bitLength);
    while (bitLength % 8 != 0) appendBits(0, 1, data, bitLength);
    for (uint8_t pad = 0xEC; bitLength < capacityBits;
         pad = static_cast<uint8_t>(pad ^ 0xEC ^ 0x11)) {
        appendBits(pad, 8, data, bitLength);
    }

    uint8_t divisor[MAX_ECC_CODEWORDS];
    rsComputeDivisor(numEcc, divisor);
    uint8_t ecc[MAX_ECC_CODEWORDS];
    rsComputeRemainder(data, numData, divisor, numEcc, ecc);

    uint8_t codewords[MAX_DATA_CODEWORDS + MAX_ECC_CODEWORDS];
    std::memcpy(codewords, data, static_cast<size_t>(numData));
    std::memcpy(codewords + numData, ecc, static_cast<size_t>(numEcc));

    bool isFunction[MAX_MATRIX_BYTES] = {};
    s_modules = outModules;
    s_isFunction = isFunction;
    s_size = size;
    std::memset(outModules, 0, static_cast<size_t>(size) * size);

    drawFunctionPatterns(version);
    drawCodewords(codewords, numData + numEcc);
    applyMaskZero();
    drawFormatModules(formatBits());

    s_modules = nullptr;
    s_isFunction = nullptr;
    s_size = 0;
    return size;
}

}  // namespace QrCodeGen
}  // namespace Stickmon
