#include "core/RoomRenderer.h"

#include "core/MathUtil.h"
#include "core/RoomResource.h"
#include "hardware/Hal.h"
#include "platform/api/PlatformServices.h"
#include "presentation/PixelRenderer.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

extern "C" {
#include "third_party/uzlib/uzlib.h"
}

namespace RoomRenderer {
namespace {

uint16_t* roomBuffer = nullptr;
uint32_t roomBufferPixels = 0;
bool roomBufferValid = false;
bool roomBufferNight = false;

bool ensureRoomBuffer() {
    RoomResource& room = RoomResource::ins();
    if (!room.available()) return false;
    uint32_t pixels = room.pixelCount();
    if (pixels == 0) return false;
    if (roomBuffer && roomBufferPixels == pixels) return true;

    if (roomBuffer) Platform::memory().release(roomBuffer);
    roomBuffer = nullptr;
    roomBufferPixels = 0;
    roomBufferValid = false;

    if (Platform::power().externalMemorySize() == 0) return false;
    roomBuffer = static_cast<uint16_t*>(Platform::memory().allocate(
        static_cast<size_t>(pixels) * sizeof(uint16_t), true));
    roomBufferPixels = roomBuffer ? pixels : 0;
    return roomBuffer != nullptr;
}

bool inflateRawDeflate(const uint8_t* compressed, uint32_t compressedSize,
                       uint8_t* out, uint32_t outSize) {
    TINF_DATA state;
    memset(&state, 0, sizeof(state));
    uzlib_init();
    uzlib_uncompress_init(&state, nullptr, 0);
    state.source = compressed;
    state.source_limit = compressed + compressedSize;
    state.dest_start = out;
    state.dest = out;
    state.dest_limit = out + outSize;

    int result = TINF_OK;
    while (state.dest < state.dest_limit) {
        result = uzlib_uncompress(&state);
        if (result == TINF_DONE) break;
        if (result != TINF_OK) return false;
    }
    return result == TINF_DONE || state.dest == state.dest_limit;
}

bool decodeBase() {
    RoomResource& room = RoomResource::ins();
    if (!roomBuffer || room.baseRawBytes() != roomBufferPixels * sizeof(uint16_t)) return false;

    const uint32_t compressedLen = room.baseCompressedLen();
    const uint8_t* compressed = room.baseCompressedData();
    if (!compressed) return false;
    return inflateRawDeflate(
        compressed,
        compressedLen,
        reinterpret_cast<uint8_t*>(roomBuffer),
        room.baseRawBytes());
}

void applyNightPatch() {
    RoomResource& room = RoomResource::ins();
    for (uint32_t runIndex = 0; runIndex < room.nightPatchRunCount(); ++runIndex) {
        RoomResource::PatchRun run = room.nightPatchRun(runIndex);
        uint32_t dst = static_cast<uint32_t>(run.y) * room.width() + run.x;
        if (dst >= roomBufferPixels) continue;
        uint16_t len = run.len;
        uint32_t available = roomBufferPixels - dst;
        if (len > available) len = static_cast<uint16_t>(available);
        for (uint16_t i = 0; i < len && run.colorOffset + i < room.nightPatchPixelCount(); ++i) {
            roomBuffer[dst + i] = room.nightPatchPixel(run.colorOffset + i);
        }
    }
}

bool prepare(bool night) {
    if (!ensureRoomBuffer()) return false;
    if (roomBufferValid && roomBufferNight == night) return true;
    uint32_t started = Platform::clock().millis();
    if (!decodeBase()) {
        Platform::logLine("[RoomRenderer] base decode failed");
        roomBufferValid = false;
        return false;
    }
    if (night) applyNightPatch();
    roomBufferNight = night;
    roomBufferValid = true;
    Platform::logf("[RoomRenderer] prepared mode=%s bytes=%u ms=%u\n",
                  night ? "night" : "day",
                  static_cast<unsigned>(roomBufferPixels * sizeof(uint16_t)),
                  Platform::clock().millis() - started);
    return true;
}

void drawBuffer(float cameraY) {
    RoomResource& room = RoomResource::ins();
    int16_t roomScreenY = room.roomY() - static_cast<int16_t>(roundf(cameraY));
    int16_t srcY = roomScreenY < 0 ? static_cast<int16_t>(-roomScreenY) : 0;
    int16_t dstY = roomScreenY > 0 ? roomScreenY : 0;
    int16_t drawH = static_cast<int16_t>(room.height()) - srcY;
    int16_t screenRemaining = Hal::DISPLAY_H - dstY;
    if (drawH > screenRemaining) drawH = screenRemaining;
    if (drawH <= 0) return;

    PixelRenderer::canvas().pushImage(
        0,
        dstY,
        room.width(),
        drawH,
        &roomBuffer[static_cast<uint32_t>(srcY) * room.width()]);
}

bool drawViewportBuffer(int16_t destinationX, int16_t destinationY,
                        uint16_t viewportWidth, uint16_t viewportHeight,
                        int16_t cameraX, int16_t cameraY) {
    RoomResource& room = RoomResource::ins();
    if (!roomBuffer || viewportWidth == 0 || viewportHeight == 0) return false;

    int sourceX = cameraX;
    int sourceY = cameraY - room.roomY();
    int maximumX = MathUtil::max<int>(0, room.width() - viewportWidth);
    int maximumY = MathUtil::max<int>(0, room.height() - viewportHeight);
    sourceX = MathUtil::clamp(sourceX, 0, maximumX);
    sourceY = MathUtil::clamp(sourceY, 0, maximumY);
    int drawWidth = MathUtil::min<int>(viewportWidth, room.width() - sourceX);
    int drawHeight = MathUtil::min<int>(viewportHeight, room.height() - sourceY);
    if (drawWidth <= 0 || drawHeight <= 0) return false;

    Canvas565& canvas = PixelRenderer::canvas();
    for (int row = 0; row < drawHeight; ++row) {
        const uint16_t* source =
            &roomBuffer[static_cast<uint32_t>(sourceY + row) * room.width() +
                        sourceX];
        canvas.pushImage(destinationX, destinationY + row,
                         drawWidth, 1, source);
    }
    return true;
}

}  // namespace

bool draw(float cameraY, bool night) {
    RoomResource::ins().begin();
    if (!prepare(night)) {
        PixelRenderer::clear(PixelRenderer::rgb(16, 18, 24));
        return false;
    }
    drawBuffer(cameraY);
    return true;
}

bool drawViewport(int16_t destinationX, int16_t destinationY,
                  uint16_t viewportWidth, uint16_t viewportHeight,
                  int16_t cameraX, int16_t cameraY, bool night) {
    RoomResource::ins().begin();
    if (!prepare(night)) return false;
    return drawViewportBuffer(destinationX, destinationY,
                              viewportWidth, viewportHeight,
                              cameraX, cameraY);
}

}  // namespace RoomRenderer
