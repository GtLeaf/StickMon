#include "core/RoomFurniture.h"
#include "core/DeflateDecoder.h"
#include "core/ResourcePack.h"
#include <algorithm>
#include <cstring>
#include <new>
#include <utility>

namespace {
struct __attribute__((packed)) FurnitureHeader {
    uint32_t magic;
    uint16_t version, width, height, flags;
    uint32_t rawBytes, compressedBytes, crc;
};
struct __attribute__((packed)) LayoutHeader {
    uint32_t nightBaseBytes;
    uint16_t count, reserved;
};
struct __attribute__((packed)) PlacementHeader {
    char slot[24], asset[32];
    int16_t x, y;
    uint16_t width, height, flags;
    int16_t shadowX, shadowY;
    uint16_t shadowWidth, shadowHeight;
    uint32_t dayBytes, nightBytes;
};
static_assert(sizeof(FurnitureHeader) == 24, "furniture header layout");
static_assert(sizeof(PlacementHeader) == 82, "placement header layout");

bool read(Platform::ResourceFile& file, void* out, size_t bytes) {
    return bytes <= file.size() - file.position() && file.read(out, bytes) == bytes;
}
uint8_t* allocate(size_t bytes) {
    return bytes ? static_cast<uint8_t*>(Platform::memory().allocate(bytes, true)) : nullptr;
}
bool safeName(const char* name, size_t size) {
    const char* end = static_cast<const char*>(std::memchr(name, 0, size));
    if (!end || end == name) return false;
    for (const char* p = name; p < end; ++p)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') || *p == '_' || *p == '-')) return false;
    return true;
}
bool loadAlpha(Platform::ResourceFile& file, uint32_t compressedBytes,
               uint32_t pixels, uint8_t*& out) {
    if (!compressedBytes || compressedBytes > pixels + 1024 ||
        compressedBytes > file.size() - file.position()) return false;
    auto* compressed = allocate(compressedBytes);
    out = allocate(pixels);
    const bool ok = compressed && out && read(file, compressed, compressedBytes) &&
        DeflateDecoder::inflateMemory(compressed, compressedBytes, out, pixels);
    if (compressed) Platform::memory().release(compressed);
    return ok;
}
uint8_t mul255(unsigned a, unsigned b) { return (a * b + 127) / 255; }
void blend(uint8_t* dst, const uint8_t* src, unsigned alpha) {
    for (int c = 0; c < 3; ++c) dst[c] = (src[c] * alpha + dst[c] * (255 - alpha) + 127) / 255;
}
void darken(uint8_t* dst, unsigned alpha) {
    for (int c = 0; c < 3; ++c) dst[c] = mul255(dst[c], 255 - alpha);
}
}

bool RoomFurniture::loadTexture(const char* id, Texture& texture) {
    Platform::ResourceFile file;
    if (!ResourcePack::ins().openFurniture(id, file)) return false;
    FurnitureHeader h{};
    if (!read(file, &h, sizeof(h)) || h.magic != 0x4e525546 || h.version != 1 || h.flags ||
        !h.width || !h.height || h.width > 480 || h.height > 640 ||
        h.rawBytes != static_cast<uint32_t>(h.width) * h.height * 3 ||
        !h.compressedBytes || h.compressedBytes > h.rawBytes + 1024 ||
        file.size() != sizeof(h) + h.compressedBytes) return false;
    auto* raw = allocate(h.rawBytes);
    auto* rgba = allocate(static_cast<uint32_t>(h.width) * h.height * 4);
    if (!raw || !rgba || !DeflateDecoder::inflateFile(file, h.compressedBytes, raw, h.rawBytes, h.crc)) {
        if (raw) Platform::memory().release(raw);
        if (rgba) Platform::memory().release(rgba);
        return false;
    }
    for (uint32_t i = 0; i < static_cast<uint32_t>(h.width) * h.height; ++i) {
        const uint16_t color = raw[i * 3] | (static_cast<uint16_t>(raw[i * 3 + 1]) << 8);
        rgba[i * 4] = ((color >> 11) * 255 + 15) / 31;
        rgba[i * 4 + 1] = (((color >> 5) & 63) * 255 + 31) / 63;
        rgba[i * 4 + 2] = ((color & 31) * 255 + 15) / 31;
        rgba[i * 4 + 3] = raw[i * 3 + 2];
    }
    Platform::memory().release(raw);
    texture.width = h.width;
    texture.height = h.height;
    texture.rgba = rgba;
    return true;
}

bool RoomFurniture::load(Platform::ResourceFile& file, uint16_t width, uint16_t height) {
    LayoutHeader h{};
    if (!read(file, &h, sizeof(h)) || h.reserved || h.count > MAX_PLACEMENTS ||
        h.nightBaseBytes > static_cast<uint32_t>(width) * height * 2 + 1024) return false;
    RoomFurniture next;
    if (h.nightBaseBytes) {
        next.nightBase_ = allocate(h.nightBaseBytes);
        next.nightBaseLength_ = h.nightBaseBytes;
        if (!next.nightBase_ || !read(file, next.nightBase_, h.nightBaseBytes)) return false;
        auto* decoded = allocate(static_cast<uint32_t>(width) * height * 2);
        const bool valid = decoded && DeflateDecoder::inflateMemory(next.nightBase_, h.nightBaseBytes,
            decoded, static_cast<uint32_t>(width) * height * 2);
        if (decoded) Platform::memory().release(decoded);
        if (!valid) return false;
    }
    if (h.count) {
        void* memory = Platform::memory().allocate(h.count * sizeof(Placement), true);
        if (!memory) return false;
        next.placements_ = static_cast<Placement*>(memory);
        for (uint16_t i = 0; i < h.count; ++i) new (&next.placements_[i]) Placement();
        next.count_ = h.count;
    }
    for (uint16_t i = 0; i < h.count; ++i) {
        PlacementHeader p{};
        if (!read(file, &p, sizeof(p)) || !safeName(p.slot, sizeof(p.slot)) ||
            !safeName(p.asset, sizeof(p.asset)) || !(p.flags & 3) || p.flags > 7 ||
            !p.width || !p.height || p.width > 480 || p.height > 640 ||
            p.x <= -static_cast<int>(p.width) || p.y <= -static_cast<int>(p.height) ||
            p.x >= width || p.y >= height ||
            p.shadowX < 0 || p.shadowY < 0 ||
            p.shadowWidth > width || p.shadowHeight > height ||
            p.shadowX + p.shadowWidth > width || p.shadowY + p.shadowHeight > height ||
            ((p.shadowWidth == 0) != (p.shadowHeight == 0)) ||
            ((p.dayBytes == 0) != (p.shadowWidth == 0)) || (!p.dayBytes && p.nightBytes)) return false;
        for (uint16_t j = 0; j < i; ++j)
            if (std::strcmp(next.placements_[j].slot, p.slot) == 0) return false;
        Placement& out = next.placements_[i];
        std::memcpy(out.slot, p.slot, sizeof(p.slot));
        std::memcpy(out.asset, p.asset, sizeof(p.asset));
        out.x = p.x; out.y = p.y; out.width = p.width; out.height = p.height; out.flags = p.flags;
        out.shadowX = p.shadowX; out.shadowY = p.shadowY;
        out.shadowWidth = p.shadowWidth; out.shadowHeight = p.shadowHeight;
        const uint32_t pixels = static_cast<uint32_t>(p.shadowWidth) * p.shadowHeight;
        if ((p.dayBytes && !loadAlpha(file, p.dayBytes, pixels, out.shadowDay)) ||
            (p.nightBytes && !loadAlpha(file, p.nightBytes, pixels, out.shadowNight)) ||
            !loadTexture(p.asset, out.texture)) return false;
    }
    if (file.position() != file.size()) return false;
    swap(next);
    return true;
}

bool RoomFurniture::replace(const char* slot, const char* assetId) {
    if (!slot || !assetId) return false;
    for (uint16_t i = 0; i < count_; ++i) {
        auto& p = placements_[i];
        if (std::strcmp(slot, p.slot) != 0) continue;
        if (std::strlen(assetId) >= sizeof(p.asset)) return false;
        if (std::strcmp(assetId, p.asset) == 0) return true;
        Texture next;
        if (!loadTexture(assetId, next)) return false;
        if (p.texture.rgba) Platform::memory().release(p.texture.rgba);
        p.texture = next;
        std::strcpy(p.asset, assetId);
        return true;
    }
    return false;
}
const char* RoomFurniture::selected(const char* slot) const {
    if (slot) for (uint16_t i = 0; i < count_; ++i)
        if (std::strcmp(slot, placements_[i].slot) == 0) return placements_[i].asset;
    return nullptr;
}

void RoomFurniture::composite(uint8_t* rgb, uint16_t width, uint16_t height, bool night) const {
    auto visible = [night](const Placement& p) { return (p.flags & (night ? 2 : 1)) != 0; };
    auto shadowAt = [night](const Placement& p, int x, int y) -> uint8_t {
        const uint8_t* alpha = night && p.shadowNight ? p.shadowNight : p.shadowDay;
        x -= p.shadowX; y -= p.shadowY;
        return alpha && x >= 0 && y >= 0 && x < p.shadowWidth && y < p.shadowHeight
            ? alpha[y * p.shadowWidth + x] : 0;
    };
    // Projected shadows affect the floor before any objects are drawn.
    for (uint16_t i = 0; i < count_; ++i) {
        const auto& p = placements_[i];
        if (!visible(p)) continue;
        for (int y = p.shadowY; y < p.shadowY + p.shadowHeight; ++y)
            for (int x = p.shadowX; x < p.shadowX + p.shadowWidth; ++x)
                darken(rgb + (y * width + x) * 3, shadowAt(p, x, y));
    }
    for (uint16_t i = 0; i < count_; ++i) {
        const auto& p = placements_[i];
        if (!visible(p)) continue;
        const auto& texture = p.texture;
        for (int y = std::max<int>(0, p.y); y < std::min<int>(height, p.y + p.height); ++y) {
            const int sy = (y - p.y) * texture.height / p.height;
            for (int x = std::max<int>(0, p.x); x < std::min<int>(width, p.x + p.width); ++x) {
                const int sx = (x - p.x) * texture.width / p.width;
                const uint8_t* pixel = texture.rgba + (sy * texture.width + sx) * 4;
                if (!pixel[3]) continue;
                uint8_t* dst = rgb + (y * width + x) * 3;
                blend(dst, pixel, pixel[3]);
                if (p.flags & 4) {
                    uint8_t shadow = 0;
                    for (uint16_t j = 0; j < count_; ++j) {
                        if (i != j && visible(placements_[j])) {
                            const unsigned a = shadowAt(placements_[j], x, y);
                            shadow = a + mul255(shadow, 255 - a);
                        }
                    }
                    darken(dst, mul255(shadow, pixel[3]));
                }
            }
        }
    }
}

void RoomFurniture::clear() {
    for (uint16_t i = 0; i < count_; ++i) {
        auto& p = placements_[i];
        if (p.texture.rgba) Platform::memory().release(p.texture.rgba);
        if (p.shadowDay) Platform::memory().release(p.shadowDay);
        if (p.shadowNight) Platform::memory().release(p.shadowNight);
        p.~Placement();
    }
    if (placements_) Platform::memory().release(placements_);
    if (nightBase_) Platform::memory().release(nightBase_);
    placements_ = nullptr; count_ = 0; nightBase_ = nullptr; nightBaseLength_ = 0;
}
void RoomFurniture::swap(RoomFurniture& other) {
    std::swap(placements_, other.placements_); std::swap(count_, other.count_);
    std::swap(nightBase_, other.nightBase_); std::swap(nightBaseLength_, other.nightBaseLength_);
}
