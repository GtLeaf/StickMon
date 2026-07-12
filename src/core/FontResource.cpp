#include "core/FontResource.h"

#include "core/ResourcePack.h"

#include <Arduino.h>
#include <FS.h>
#include <cstdlib>

namespace {
static constexpr uint32_t FONT_PACK_MAGIC = 0x4E464D53; // SMFN
static constexpr uint16_t FONT_PACK_VERSION = 1;
static constexpr uint16_t MAX_FONT_GLYPHS = 2048;

struct __attribute__((packed)) PackedFontHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t glyphCount;
    uint8_t width;
    uint8_t height;
    uint8_t bytesPerGlyph;
    uint8_t reserved;
    uint32_t flags;
};

bool readExact(fs::File& file, void* out, size_t length) {
    if (length == 0) return true;
    return file.read(reinterpret_cast<uint8_t*>(out), length) == length;
}

bool validHeader(const PackedFontHeader& h) {
    if (h.magic != FONT_PACK_MAGIC || h.version != FONT_PACK_VERSION) return false;
    if (h.glyphCount == 0 || h.glyphCount > MAX_FONT_GLYPHS) return false;
    if (h.width != FontResource::GLYPH_W || h.height != FontResource::GLYPH_H) return false;
    return h.bytesPerGlyph == FontResource::GLYPH_BYTES;
}
}

FontResource& FontResource::ins() {
    static FontResource instance;
    return instance;
}

bool FontResource::begin() {
    if (initialized_) return loaded_;
    initialized_ = true;
    loaded_ = loadExternal();
    Serial.printf("[FontResource] source=%s glyphs=%u\n", source(), glyphCount_);
    return loaded_;
}

const uint8_t* FontResource::findGlyphBitmap(uint32_t codepoint) {
    begin();
    if (!loaded_ || !glyphs_ || glyphCount_ == 0) return nullptr;

    int lo = 0;
    int hi = glyphCount_ - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        uint32_t value = glyphs_[mid].codepoint;
        if (value == codepoint) return glyphs_[mid].bitmap;
        if (value < codepoint) lo = mid + 1;
        else hi = mid - 1;
    }
    return nullptr;
}

bool FontResource::loadExternal() {
    ResourcePack& pack = ResourcePack::ins();
    if (!pack.begin()) return false;

    fs::File file;
    if (!pack.openDefaultFont(file)) return false;

    PackedFontHeader header{};
    if (!readExact(file, &header, sizeof(header)) || !validHeader(header)) return false;

    size_t bytes = sizeof(Glyph) * header.glyphCount;
    Glyph* glyphs = psramFound()
        ? static_cast<Glyph*>(ps_malloc(bytes))
        : static_cast<Glyph*>(malloc(bytes));
    if (!glyphs) return false;

    if (!readExact(file, glyphs, bytes)) {
        free(glyphs);
        return false;
    }

    uint32_t previous = 0;
    for (uint16_t i = 0; i < header.glyphCount; ++i) {
        uint32_t codepoint = glyphs[i].codepoint;
        if (codepoint < 0x80 || codepoint > 0x10FFFF) {
            free(glyphs);
            return false;
        }
        if (i > 0 && codepoint <= previous) {
            free(glyphs);
            return false;
        }
        previous = codepoint;
    }

    releaseExternal();
    glyphs_ = glyphs;
    glyphCount_ = header.glyphCount;
    return true;
}

void FontResource::releaseExternal() {
    if (glyphs_) free(glyphs_);
    glyphs_ = nullptr;
    glyphCount_ = 0;
    loaded_ = false;
}
