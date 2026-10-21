#include "core/RoomRenderer.h"

#include "core/MathUtil.h"
#include "core/DeflateDecoder.h"
#include "core/RoomResource.h"
#include "hardware/Hal.h"
#include "platform/api/PlatformServices.h"
#include "presentation/PixelRenderer.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace RoomRenderer {
namespace {

uint16_t* roomBuffer = nullptr;
uint32_t roomBufferPixels = 0;
bool roomBufferValid = false;
bool roomBufferNight = false;
uint32_t roomBufferRevision = 0;

bool ensureRoomBuffer() {
    RoomResource& room = RoomResource::ins();
    if (!room.available()) return false;
    uint32_t pixels = room.artPixelCount();
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
    return DeflateDecoder::inflateMemory(compressed, compressedSize, out, outSize);
}

bool decodeBase() {
    RoomResource& room = RoomResource::ins();
    if (!roomBuffer || room.baseRawBytes() != roomBufferPixels * sizeof(uint16_t)) return false;

    const uint32_t compressedLen = room.baseCompressedLen();
    const uint8_t* compressed = room.baseCompressedData();
    if (!compressed) return false;
    if (!room.referenceCompressedData()) {
        return inflateRawDeflate(compressed, compressedLen,
            reinterpret_cast<uint8_t*>(roomBuffer), room.baseRawBytes());
    }
    const uint32_t maskBytes = (roomBufferPixels + 7) / 8;
    const uint32_t patchBytes = room.baseRawBytes() + maskBytes;
    auto* patch = static_cast<uint8_t*>(Platform::memory().allocate(patchBytes, true));
    if (!patch) return false;
    const bool ok = inflateRawDeflate(compressed, compressedLen, patch, patchBytes) &&
        inflateRawDeflate(room.referenceCompressedData(), room.referenceCompressedLen(),
            reinterpret_cast<uint8_t*>(roomBuffer), room.baseRawBytes());
    if (ok) {
        const auto* colors = reinterpret_cast<const uint16_t*>(patch);
        const auto* mask = patch + room.baseRawBytes();
        for (uint32_t pixel = 0; pixel < roomBufferPixels; ++pixel) {
            if (mask[pixel / 8] & (1U << (pixel % 8))) roomBuffer[pixel] = colors[pixel];
        }
    }
    Platform::memory().release(patch);
    return ok;
}

void applyNightPatch() {
    RoomResource& room = RoomResource::ins();
    for (uint32_t runIndex = 0; runIndex < room.nightPatchRunCount(); ++runIndex) {
        RoomResource::PatchRun run = room.nightPatchRun(runIndex);
        uint32_t dst = static_cast<uint32_t>(run.y) * room.artWidth() + run.x;
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
    if (roomBufferValid && roomBufferNight == night &&
        roomBufferRevision == RoomResource::ins().revision()) return true;
    uint32_t started = Platform::clock().millis();
    if (!decodeBase()) {
        Platform::logLine("[RoomRenderer] base decode failed");
        roomBufferValid = false;
        return false;
    }
    RoomResource& room = RoomResource::ins();
    if (room.layered()) {
        if (night && room.furniture().nightBaseLength() &&
            !inflateRawDeflate(room.furniture().nightBase(), room.furniture().nightBaseLength(),
                reinterpret_cast<uint8_t*>(roomBuffer), room.baseRawBytes())) {
            roomBufferValid = false;
            return false;
        }
        auto* rgb = static_cast<uint8_t*>(Platform::memory().allocate(roomBufferPixels * 3, true));
        if (!rgb) { roomBufferValid = false; return false; }
        for (uint32_t i = 0; i < roomBufferPixels; ++i) {
            const uint16_t color = roomBuffer[i];
            rgb[i * 3] = ((color >> 11) * 255 + 15) / 31;
            rgb[i * 3 + 1] = (((color >> 5) & 63) * 255 + 31) / 63;
            rgb[i * 3 + 2] = ((color & 31) * 255 + 15) / 31;
        }
        room.furniture().composite(rgb, room.artWidth(), room.artHeight(), night);
        for (uint32_t i = 0; i < roomBufferPixels; ++i)
            roomBuffer[i] = ((rgb[i * 3] & 0xf8) << 8) |
                           ((rgb[i * 3 + 1] & 0xfc) << 3) | (rgb[i * 3 + 2] >> 3);
        Platform::memory().release(rgb);
    } else if (night) applyNightPatch();
    roomBufferNight = night;
    roomBufferRevision = RoomResource::ins().revision();
    roomBufferValid = true;
    Platform::logf("[RoomRenderer] prepared mode=%s bytes=%u ms=%u\n",
                  night ? "night" : "day",
                  static_cast<unsigned>(roomBufferPixels * sizeof(uint16_t)),
                  Platform::clock().millis() - started);
    return true;
}

bool drawViewportBuffer(int16_t destinationX, int16_t destinationY,
                        uint16_t viewportWidth, uint16_t viewportHeight,
                        int16_t cameraX, int16_t cameraY, uint8_t pixelScale) {
    RoomResource& room = RoomResource::ins();
    if (!roomBuffer || pixelScale == 0 ||
        viewportWidth == 0 || viewportHeight == 0) return false;
    const int artScale = MathUtil::max<int>(1, room.artScale());
    const int worldViewportWidth =
        (static_cast<int>(viewportWidth) + pixelScale - 1) / pixelScale;
    const int worldViewportHeight =
        (static_cast<int>(viewportHeight) + pixelScale - 1) / pixelScale;
    const int maximumWorldX = MathUtil::max<int>(
        0, room.width() - worldViewportWidth);
    const int maximumWorldY = MathUtil::max<int>(
        0, room.height() - worldViewportHeight);
    const int worldCameraX = MathUtil::clamp<int>(cameraX, 0, maximumWorldX);
    const int worldCameraY = MathUtil::clamp<int>(
        cameraY - room.roomY(), 0, maximumWorldY);
    const int sourceX = worldCameraX * artScale;
    const int sourceY = worldCameraY * artScale;
    const int sourceWidth = MathUtil::min<int>(
        room.artWidth() - sourceX, worldViewportWidth * artScale);
    const int sourceHeight = MathUtil::min<int>(
        room.artHeight() - sourceY, worldViewportHeight * artScale);
    if (sourceWidth <= 0 || sourceHeight <= 0) return false;

    Canvas565& canvas = PixelRenderer::canvas();
    // Fast path: the art and destination are both native
    // pixels, so each source row can be sent to the panel without fillRect.
    if (artScale == pixelScale && sourceWidth >= viewportWidth &&
        sourceHeight >= viewportHeight) {
        for (int row = 0; row < viewportHeight; ++row) {
            canvas.pushImage(
                destinationX, destinationY + row, viewportWidth, 1,
                &roomBuffer[static_cast<uint32_t>(sourceY + row) * room.artWidth() +
                            sourceX]);
        }
        return true;
    }

    // Generic nearest-neighbor mapping keeps old v3 resources usable while
    // allowing a v4 high-resolution room to be sampled by the 240px display.
    // Build one output row at a time so the low-resolution display still uses
    // the panel's bulk transfer path instead of issuing one draw call/pixel.
    uint16_t rowBuffer[480] = {};
    for (int row = 0; row < viewportHeight; ++row) {
        int sourceRow = sourceY + (row * artScale) / pixelScale;
        sourceRow = MathUtil::clamp<int>(sourceRow, sourceY,
                                         room.artHeight() - 1);
        for (int column = 0; column < viewportWidth; ++column) {
            int sourceColumn = sourceX + (column * artScale) / pixelScale;
            sourceColumn = MathUtil::clamp<int>(sourceColumn, sourceX,
                                                room.artWidth() - 1);
            uint16_t color = roomBuffer[static_cast<uint32_t>(sourceRow) *
                                        room.artWidth() + sourceColumn];
            if (column < static_cast<int>(sizeof(rowBuffer) / sizeof(rowBuffer[0]))) {
                rowBuffer[column] = color;
            } else {
                canvas.drawPixel(destinationX + column, destinationY + row, color);
            }
        }
        if (viewportWidth <= sizeof(rowBuffer) / sizeof(rowBuffer[0])) {
            canvas.pushImage(destinationX, destinationY + row,
                             viewportWidth, 1, rowBuffer);
        }
    }
    return true;
}

}  // namespace

Viewport viewport() {
    const auto& room = RoomResource::ins();
    const int width = room.available()
        ? MathUtil::min<int>(Hal::DISPLAY_W, room.width() * PIXEL_SCALE) : Hal::DISPLAY_W;
    const int height = room.available()
        ? MathUtil::min<int>(Hal::DISPLAY_H, room.height() * PIXEL_SCALE) : Hal::DISPLAY_H;
    return {static_cast<int16_t>((Hal::DISPLAY_W - width) / 2),
            static_cast<int16_t>((Hal::DISPLAY_H - height) / 2),
            static_cast<uint16_t>(width), static_cast<uint16_t>(height)};
}

int16_t clampCameraX(float cameraX) {
    const auto& room = RoomResource::ins();
    const int width = (Hal::DISPLAY_W + PIXEL_SCALE - 1) / PIXEL_SCALE;
    return MathUtil::clamp<int>(static_cast<int>(roundf(cameraX)), 0,
        MathUtil::max<int>(0, room.width() - width));
}

int16_t clampCameraY(float cameraY) {
    const auto& room = RoomResource::ins();
    const int height = (Hal::DISPLAY_H + PIXEL_SCALE - 1) / PIXEL_SCALE;
    return MathUtil::clamp<int>(static_cast<int>(roundf(cameraY)), room.roomY(),
        room.roomY() + MathUtil::max<int>(0, room.height() - height));
}

int16_t screenX(float worldX, float cameraX) {
    return viewport().x + static_cast<int16_t>(roundf((worldX - clampCameraX(cameraX)) * PIXEL_SCALE));
}

int16_t screenY(float worldY, float cameraY) {
    return viewport().y + static_cast<int16_t>(roundf((worldY - clampCameraY(cameraY)) * PIXEL_SCALE));
}

bool draw(float cameraY, bool night, float cameraX) {
    RoomResource::ins().begin();
    if (!prepare(night)) {
        PixelRenderer::clear(PixelRenderer::rgb(16, 18, 24));
        return false;
    }
    const auto view = viewport();
    PixelRenderer::clear(0);
    return drawViewportBuffer(view.x, view.y, view.width, view.height,
                              clampCameraX(cameraX), clampCameraY(cameraY), PIXEL_SCALE);
}

bool drawViewport(int16_t destinationX, int16_t destinationY,
                  uint16_t viewportWidth, uint16_t viewportHeight,
                  int16_t cameraX, int16_t cameraY, bool night,
                  uint8_t pixelScale) {
    RoomResource::ins().begin();
    if (!prepare(night)) return false;
    return drawViewportBuffer(destinationX, destinationY,
                              viewportWidth, viewportHeight,
                              cameraX, cameraY, pixelScale);
}

}  // namespace RoomRenderer
