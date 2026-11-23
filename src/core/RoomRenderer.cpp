#include "core/RoomRenderer.h"

#include "core/MathUtil.h"
#include "core/DeflateDecoder.h"
#include "core/RoomResource.h"
#include "game/MarketWorkshopService.h"
#include "hardware/Hal.h"
#include "platform/api/PlatformServices.h"
#include "presentation/PixelRenderer.h"
#include "presentation/FurnitureArt.h"

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
uint16_t roomBufferFurniture = 0;
Wallpaper roomBufferWallpaper = Wallpaper::ORIGINAL;
bool roomBufferWallpaperApplied = false;

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

bool prepare(bool night, uint16_t placedFurniture, Wallpaper wallpaper) {
    placedFurniture &= Game::MarketWorkshop::PHOTO_FRAME;
    if (!ensureRoomBuffer()) return false;
    RoomResource& room = RoomResource::ins();
    if (roomBufferValid && roomBufferNight == night && roomBufferRevision == room.revision() &&
        roomBufferFurniture == placedFurniture && roomBufferWallpaper == wallpaper) return true;
    uint32_t started = Platform::clock().millis();
    if (!decodeBase()) {
        Platform::errorLine("[RoomRenderer] base decode failed");
        roomBufferValid = false;
        return false;
    }
    if (night && room.layered() && room.furniture().nightBaseLength() &&
        !inflateRawDeflate(room.furniture().nightBase(), room.furniture().nightBaseLength(),
            reinterpret_cast<uint8_t*>(roomBuffer), room.baseRawBytes())) {
        roomBufferValid = false;
        return false;
    }
    if (night && !room.layered()) applyNightPatch();
    // Home can still draw the original surface if an overlay is unavailable.
    // The shop consults this result before showing a preview or charging coins.
    roomBufferWallpaperApplied = room.applyWallpaper(roomBuffer, night, wallpaper);
    if (room.layered()) {
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
    }
    if (placedFurniture & Game::MarketWorkshop::PHOTO_FRAME) {
        const int scale = room.artScale();
        const int x = FurnitureArt::FRAME_WORLD_X * scale, y = FurnitureArt::frameTop(scale, scale);
        const int size = FurnitureArt::FRAME_WORLD_SIZE * scale;
        // Project the front, narrow wooden side and contact shadow onto the sloping wall.
        for (int col = 0; col < size + 2; ++col) {
            const int px = x + col;
            if (px >= room.artWidth()) break;
            const int top = y + std::min(col, size - 1) / 3;
            for (int row = 0; row < size + scale + 1 && top + row < room.artHeight(); ++row) {
                uint16_t& pixel = roomBuffer[(top + row) * room.artWidth() + px];
                if (col < size && row < size) {
                    pixel = FurnitureArt::framePixel(col, row, size);
                } else if (col <= size && row >= 1 && row < size + scale) {
                    pixel = PixelRenderer::rgb(100, 62, 38);
                } else if (col >= 1 && row >= 2) {
                    // Retain three quarters of the underlying day or night wall color.
                    pixel = ((pixel >> 1) & 0x7bef) + ((pixel >> 2) & 0x39e7);
                }
            }
        }
    }
    roomBufferFurniture = placedFurniture;
    roomBufferWallpaper = wallpaper;
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

bool draw(float cameraY, bool night, float cameraX, uint16_t placedFurniture, Wallpaper wallpaper) {
    RoomResource::ins().begin();
    if (!prepare(night, placedFurniture, wallpaper)) {
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
                  uint8_t pixelScale, uint16_t placedFurniture, Wallpaper wallpaper) {
    RoomResource::ins().begin();
    if (!prepare(night, placedFurniture, wallpaper)) return false;
    return drawViewportBuffer(destinationX, destinationY,
                              viewportWidth, viewportHeight,
                              cameraX, cameraY, pixelScale);
}

bool preparePreview(bool night, uint16_t placedFurniture, Wallpaper wallpaper) {
    RoomResource::ins().begin();
    return prepare(night, placedFurniture, wallpaper) && roomBufferWallpaperApplied;
}

bool drawPreview(int16_t x, int16_t y, uint16_t width, uint16_t height, bool night,
                 uint16_t placedFurniture, const uint16_t* wallPhoto, Wallpaper wallpaper) {
    if (!width || !height || width > 480 || !preparePreview(night, placedFurniture, wallpaper)) return false;
    const auto& room = RoomResource::ins();
    auto& canvas = PixelRenderer::canvas();
    const int drawWidth = std::min<int>(width, int(height) * room.artWidth() / room.artHeight());
    const int drawHeight = drawWidth * room.artHeight() / room.artWidth();
    const int left = x + (width - drawWidth) / 2, top = y + (height - drawHeight) / 2;
    const int scale = room.artScale();
    constexpr int photoSize = FurnitureArt::WALL_PHOTO_SIZE;
    constexpr int inset = (FurnitureArt::FRAME_WORLD_SIZE - photoSize) / 2;
    const int frameX = FurnitureArt::FRAME_WORLD_X * scale;
    const int photoX = frameX + inset * scale;
    const int photoY = FurnitureArt::frameTop(scale, scale) + inset * scale;
    // Only sample the photo within its projected columns. The shared room cache
    // keeps the empty frame so changing a binding never requires a room decode.
    const int firstPhotoCol = (photoX * drawWidth + room.artWidth() - 1) / room.artWidth();
    const int endPhotoCol = std::min(drawWidth,
        ((photoX + photoSize * scale) * drawWidth + room.artWidth() - 1) / room.artWidth());
    uint16_t row[480];
    for (int dy = std::max(0, canvas.clipTop() - top);
         dy < std::min(drawHeight, canvas.clipBottom() - top); ++dy) {
        const int sy = dy * room.artHeight() / drawHeight;
        const uint16_t* source = roomBuffer + sy * room.artWidth();
        for (int dx = 0; dx < drawWidth; ++dx) row[dx] = source[dx * room.artWidth() / drawWidth];
        if (wallPhoto && (placedFurniture & Game::MarketWorkshop::PHOTO_FRAME)) {
            for (int dx = firstPhotoCol; dx < endPhotoCol; ++dx) {
                const int sx = dx * room.artWidth() / drawWidth;
                const int photoRow = sy - photoY - (sx - frameX) / 3;
                if (photoRow >= 0 && photoRow < photoSize * scale)
                    row[dx] = wallPhoto[photoRow / scale * photoSize + (sx - photoX) / scale];
            }
        }
        canvas.pushImage(left, top + dy, drawWidth, 1, row);
    }
    return true;
}

}  // namespace RoomRenderer
