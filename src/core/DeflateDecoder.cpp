#include "core/DeflateDecoder.h"

#include "third_party/uzlib/uzlib.h"

#include <algorithm>
#include "platform/api/PlatformServices.h"

namespace DeflateDecoder {
namespace {

constexpr size_t INPUT_BUFFER_BYTES = 512;
constexpr uint32_t CRC32_NIBBLE_TABLE[16] = {
    0x00000000UL, 0x1DB71064UL, 0x3B6E20C8UL, 0x26D930ACUL,
    0x76DC4190UL, 0x6B6B51F4UL, 0x4DB26158UL, 0x5005713CUL,
    0xEDB88320UL, 0xF00F9344UL, 0xD6D6A3E8UL, 0xCB61B38CUL,
    0x9B64C2B0UL, 0x86D3D2D4UL, 0xA00AE278UL, 0xBDBDF21CUL,
};

struct FileInflateContext {
    TINF_DATA state{};
    Platform::ResourceFile* file = nullptr;
    uint32_t remaining = 0;
    uint32_t bytesRead = 0;
    uint32_t readMs = 0;
    bool readFailed = false;
    uint8_t input[INPUT_BUFFER_BYTES] = {};
};

static_assert(offsetof(FileInflateContext, state) == 0,
              "TINF_DATA must remain the first callback context member");

int refillSource(TINF_DATA* state) {
    auto* context = reinterpret_cast<FileInflateContext*>(state);
    if (!context->file || context->remaining == 0) return -1;

    size_t wanted = std::min<size_t>(sizeof(context->input), context->remaining);
    uint32_t started = Platform::clock().millis();
    size_t received = context->file->read(context->input, wanted);
    context->readMs += Platform::clock().millis() - started;
    if (received == 0) {
        context->readFailed = true;
        return -1;
    }

    context->remaining -= received;
    context->bytesRead += received;
    state->source = context->input + 1;
    state->source_limit = context->input + received;
    return context->input[0];
}

uint32_t crc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        crc = (crc >> 4) ^ CRC32_NIBBLE_TABLE[crc & 0x0F];
        crc = (crc >> 4) ^ CRC32_NIBBLE_TABLE[crc & 0x0F];
    }
    return ~crc;
}

}  // namespace

bool inflateFile(Platform::ResourceFile& file,
                 uint32_t compressedBytes,
                 uint8_t* output,
                 size_t outputBytes,
                 uint32_t expectedCrc32,
                 Stats* stats) {
    Stats result{};
    result.inputBytes = compressedBytes;
    result.outputBytes = static_cast<uint32_t>(outputBytes);
    uint32_t started = Platform::clock().millis();

    if (!file || !output || compressedBytes == 0 || outputBytes == 0 ||
        outputBytes > UINT32_MAX) {
        if (stats) *stats = result;
        return false;
    }

    FileInflateContext context{};
    context.file = &file;
    context.remaining = compressedBytes;
    context.state.source = context.input;
    context.state.source_limit = context.input;
    context.state.source_read_cb = refillSource;
    context.state.dest_start = output;
    context.state.dest = output;
    context.state.dest_limit = output + outputBytes;
    uzlib_init();
    uzlib_uncompress_init(&context.state, nullptr, 0);

    int status = TINF_OK;
    while (context.state.dest < context.state.dest_limit && status == TINF_OK) {
        status = uzlib_uncompress(&context.state);
    }

    size_t produced = static_cast<size_t>(context.state.dest - output);
    uint32_t actualCrc32 = produced == outputBytes ? crc32(output, outputBytes) : 0;

    result.readMs = context.readMs;
    result.totalMs = Platform::clock().millis() - started;
    result.inflateMs = result.totalMs >= result.readMs ? result.totalMs - result.readMs : 0;
    if (stats) *stats = result;

    bool streamComplete = status == TINF_DONE ||
                          (status == TINF_OK && context.state.dest == context.state.dest_limit);
    return streamComplete && !context.readFailed &&
           context.bytesRead <= compressedBytes && produced == outputBytes &&
           actualCrc32 == expectedCrc32;
}

}  // namespace DeflateDecoder
