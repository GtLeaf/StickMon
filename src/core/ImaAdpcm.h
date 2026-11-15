#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace ImaAdpcm {
constexpr int IMA_INDEX_TABLE[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8,
};

constexpr int IMA_STEP_TABLE[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130,
    143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449,
    494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411,
    1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026,
    4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
    27086, 29794, 32767,
};

inline uint8_t pcmU8(int predictor) {
    int sample = (predictor >> 8) + 128;
    return static_cast<uint8_t>(std::max(0, std::min(255, sample)));
}

inline size_t decodeImaBlock(const uint8_t* input, size_t inputBytes,
                      uint8_t* output, size_t outputCapacity,
                      size_t wantedSamples) {
    if (!input || inputBytes < 4 || !output || outputCapacity == 0 ||
        wantedSamples == 0) {
        return 0;
    }

    int predictor = static_cast<int16_t>(
        static_cast<uint16_t>(input[0]) |
        (static_cast<uint16_t>(input[1]) << 8));
    int index = input[2];
    if (index < 0 || index > 88 || input[3] != 0) return 0;

    size_t produced = 0;
    output[produced++] = pcmU8(predictor);
    for (size_t byteIndex = 4;
         byteIndex < inputBytes && produced < wantedSamples &&
         produced < outputCapacity; ++byteIndex) {
        for (uint8_t half = 0;
             half < 2 && produced < wantedSamples &&
             produced < outputCapacity; ++half) {
            uint8_t code = half == 0
                ? input[byteIndex] & 0x0F
                : input[byteIndex] >> 4;
            int step = IMA_STEP_TABLE[index];
            int delta = step >> 3;
            if (code & 1) delta += step >> 2;
            if (code & 2) delta += step >> 1;
            if (code & 4) delta += step;
            predictor += (code & 8) ? -delta : delta;
            predictor = std::max(-32768, std::min(32767, predictor));
            index += IMA_INDEX_TABLE[code];
            index = std::max(0, std::min(88, index));
            output[produced++] = pcmU8(predictor);
        }
    }
    return produced;
}

} // namespace ImaAdpcm
