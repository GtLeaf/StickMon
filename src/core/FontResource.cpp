#include "core/FontResource.h"

#include "core/ResourcePack.h"
#include "platform/api/PlatformServices.h"

#include <cstdlib>

namespace {
static constexpr uint32_t FONT_PACK_MAGIC = 0x4E464D53; // SMFN
static constexpr uint16_t FONT_PACK_VERSION = 1;
static constexpr uint16_t MAX_FONT_GLYPHS = 2048;
static constexpr const char* UNSCII_ASCII_FONT_ID = "ascii16-unscii";
static constexpr const char* LARGE_CJK_FONT_ID = "zh32";
static constexpr const char* LARGE_ASCII_FONT_ID = "ascii32";

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

bool readExact(Platform::ResourceFile& file, void* out, size_t length) {
    if (length == 0) return true;
    return file.read(reinterpret_cast<uint8_t*>(out), length) == length;
}

bool validHeader(const PackedFontHeader& h, uint8_t width, uint8_t height,
                 uint8_t bytesPerGlyph) {
    if (h.magic != FONT_PACK_MAGIC || h.version != FONT_PACK_VERSION) return false;
    if (h.glyphCount == 0 || h.glyphCount > MAX_FONT_GLYPHS) return false;
    if (h.width != width || h.height != height) return false;
    return h.bytesPerGlyph == bytesPerGlyph;
}

bool validCodepoint(uint32_t codepoint) {
    if (codepoint < 0x20 || (codepoint >= 0x7F && codepoint < 0xA0)) return false;
    if (codepoint >= 0xD800 && codepoint <= 0xDFFF) return false;
    return codepoint <= 0x10FFFF;
}

const char* faceName(FontFace face) {
    return face == FontFace::UNSCII_ASCII
        ? "unscii-ascii"
        : "sarasa-cjk";
}
}

FontResource& FontResource::ins() {
    static FontResource instance;
    return instance;
}

bool FontResource::begin() {
    if (initialized_) return loaded();
    initialized_ = true;

    if (!ResourcePack::ins().begin()) {
        Platform::logLine("[FontResource] resource pack unavailable");
        return false;
    }

    loadExternal(FontFace::SARASA_CJK, sarasaCjk_);
    loadExternal(FontFace::UNSCII_ASCII, unsciiAscii_);
    Platform::logf("[FontResource] source=%s sarasaCjk=%u unsciiAscii=%u\n",
                  source(), sarasaCjk_.glyphCount, unsciiAscii_.glyphCount);
    return loaded();
}

bool FontResource::loaded() const {
    return sarasaCjk_.loaded && unsciiAscii_.loaded;
}

bool FontResource::loaded(FontFace face) const {
    return dataFor(face).loaded;
}

uint16_t FontResource::glyphCount(FontFace face) const {
    return dataFor(face).glyphCount;
}

const FontResource::FontData& FontResource::dataFor(FontFace face) const {
    return face == FontFace::UNSCII_ASCII
        ? unsciiAscii_
        : sarasaCjk_;
}

const uint8_t* FontResource::findGlyphBitmap(uint32_t codepoint,
                                             FontFace face) {
    begin();
    auto findIn = [codepoint](const FontData& data) -> const uint8_t* {
        if (!data.loaded || !data.glyphs || data.glyphCount == 0) return nullptr;
        int lo = 0;
        int hi = data.glyphCount - 1;
        while (lo <= hi) {
            int mid = (lo + hi) / 2;
            uint32_t value = data.glyphs[mid].codepoint;
            if (value == codepoint) return data.glyphs[mid].bitmap;
            if (value < codepoint) lo = mid + 1;
            else hi = mid - 1;
        }
        return nullptr;
    };

    return face == FontFace::UNSCII_ASCII
        ? findIn(unsciiAscii_)
        : findIn(sarasaCjk_);
}

const FontResource::LargeFontData& FontResource::largeDataFor(FontFace face) const {
    return face == FontFace::UNSCII_ASCII
        ? unsciiAsciiLarge_
        : sarasaCjkLarge_;
}

bool FontResource::beginLarge() {
    if (largeInitialized_) {
        return sarasaCjkLarge_.loaded && unsciiAsciiLarge_.loaded;
    }
    largeInitialized_ = true;
    if (!ResourcePack::ins().begin()) return false;

    loadExternalLarge(FontFace::SARASA_CJK, sarasaCjkLarge_);
    loadExternalLarge(FontFace::UNSCII_ASCII, unsciiAsciiLarge_);
    Platform::logf("[FontResource] large cjk=%u ascii=%u\n",
                  sarasaCjkLarge_.glyphCount, unsciiAsciiLarge_.glyphCount);
    return sarasaCjkLarge_.loaded && unsciiAsciiLarge_.loaded;
}

const uint8_t* FontResource::findLargeGlyphBitmap(uint32_t codepoint,
                                                  FontFace face) {
    beginLarge();
    const LargeFontData& data = largeDataFor(face);
    if (!data.loaded || !data.glyphs || data.glyphCount == 0) return nullptr;
    int lo = 0;
    int hi = data.glyphCount - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        uint32_t value = data.glyphs[mid].codepoint;
        if (value == codepoint) return data.glyphs[mid].bitmap;
        if (value < codepoint) lo = mid + 1;
        else hi = mid - 1;
    }
    return nullptr;
}

bool FontResource::loadExternal(FontFace face, FontData& data) {
    ResourcePack& pack = ResourcePack::ins();
    releaseExternal(data);

    Platform::ResourceFile file;
    bool opened = face == FontFace::UNSCII_ASCII
        ? pack.openFont(UNSCII_ASCII_FONT_ID, file)
        : pack.openDefaultFont(file);
    if (!opened) {
        Platform::logf("[FontResource] %s font file unavailable\n",
                      faceName(face));
        return false;
    }

    PackedFontHeader header{};
    if (!readExact(file, &header, sizeof(header))) {
        Platform::logf("[FontResource] %s font header truncated\n",
                      faceName(face));
        return false;
    }
    if (!validHeader(header, FontResource::GLYPH_W,
                     FontResource::GLYPH_H, FontResource::GLYPH_BYTES)) {
        Platform::logf("[FontResource] %s font header invalid magic=%08lx version=%u glyphs=%u size=%ux%u bytes=%u\n",
                      faceName(face),
                      static_cast<unsigned long>(header.magic),
                      header.version,
                      header.glyphCount,
                      header.width,
                      header.height,
                      header.bytesPerGlyph);
        return false;
    }

    size_t bytes = sizeof(Glyph) * header.glyphCount;
    size_t expectedSize = sizeof(PackedFontHeader) + bytes;
    if (file.size() != expectedSize) {
        Platform::logf("[FontResource] %s font size invalid actual=%u expected=%u\n",
                      faceName(face),
                      static_cast<unsigned>(file.size()),
                      static_cast<unsigned>(expectedSize));
        return false;
    }
    Glyph* glyphs = static_cast<Glyph*>(
        Platform::memory().allocate(bytes, true));
    if (!glyphs) {
        Platform::logf("[FontResource] %s font allocation failed bytes=%u\n",
                      faceName(face),
                      static_cast<unsigned>(bytes));
        return false;
    }

    if (!readExact(file, glyphs, bytes)) {
        Platform::logf("[FontResource] %s font glyph payload truncated\n",
                      faceName(face));
        Platform::memory().release(glyphs);
        return false;
    }

    uint32_t previous = 0;
    for (uint16_t i = 0; i < header.glyphCount; ++i) {
        uint32_t codepoint = glyphs[i].codepoint;
        if (!validCodepoint(codepoint)) {
            Platform::logf("[FontResource] %s font codepoint invalid index=%u value=U+%04lX\n",
                          faceName(face), i,
                          static_cast<unsigned long>(codepoint));
            Platform::memory().release(glyphs);
            return false;
        }
        if (i > 0 && codepoint <= previous) {
            Platform::logf("[FontResource] %s font codepoints unsorted index=%u previous=U+%04lX value=U+%04lX\n",
                          faceName(face),
                          i,
                          static_cast<unsigned long>(previous),
                          static_cast<unsigned long>(codepoint));
            Platform::memory().release(glyphs);
            return false;
        }
        previous = codepoint;
    }

    data.glyphs = glyphs;
    data.glyphCount = header.glyphCount;
    data.loaded = true;
    return true;
}

bool FontResource::loadExternalLarge(FontFace face, LargeFontData& data) {
    ResourcePack& pack = ResourcePack::ins();
    releaseExternalLarge(data);

    Platform::ResourceFile file;
    const char* id = face == FontFace::UNSCII_ASCII
        ? LARGE_ASCII_FONT_ID : LARGE_CJK_FONT_ID;
    if (!pack.openFont(id, file)) {
        Platform::logf("[FontResource] large %s font file unavailable\n",
                      faceName(face));
        return false;
    }

    PackedFontHeader header{};
    if (!readExact(file, &header, sizeof(header))) {
        Platform::logf("[FontResource] large %s font header truncated\n",
                      faceName(face));
        return false;
    }
    if (!validHeader(header, FontResource::LARGE_GLYPH_W,
                     FontResource::LARGE_GLYPH_H,
                     FontResource::LARGE_GLYPH_BYTES)) {
        Platform::logf(
            "[FontResource] large %s font header invalid magic=%08lx version=%u glyphs=%u size=%ux%u bytes=%u\n",
            faceName(face), static_cast<unsigned long>(header.magic),
            header.version, header.glyphCount, header.width, header.height,
            header.bytesPerGlyph);
        return false;
    }

    size_t bytes = sizeof(LargeGlyph) * header.glyphCount;
    size_t expectedSize = sizeof(PackedFontHeader) + bytes;
    if (file.size() != expectedSize) {
        Platform::logf(
            "[FontResource] large %s font size invalid actual=%u expected=%u\n",
            faceName(face), static_cast<unsigned>(file.size()),
            static_cast<unsigned>(expectedSize));
        return false;
    }
    LargeGlyph* glyphs = static_cast<LargeGlyph*>(
        Platform::memory().allocate(bytes, true));
    if (!glyphs) {
        Platform::logf("[FontResource] large %s font allocation failed bytes=%u\n",
                      faceName(face), static_cast<unsigned>(bytes));
        return false;
    }
    if (!readExact(file, glyphs, bytes)) {
        Platform::logf("[FontResource] large %s font glyph payload truncated\n",
                      faceName(face));
        Platform::memory().release(glyphs);
        return false;
    }

    uint32_t previous = 0;
    for (uint16_t i = 0; i < header.glyphCount; ++i) {
        uint32_t codepoint = glyphs[i].codepoint;
        if (!validCodepoint(codepoint) ||
            (i > 0 && codepoint <= previous)) {
            Platform::logf("[FontResource] large %s font codepoints invalid index=%u\n",
                          faceName(face), i);
            Platform::memory().release(glyphs);
            return false;
        }
        previous = codepoint;
    }

    data.glyphs = glyphs;
    data.glyphCount = header.glyphCount;
    data.loaded = true;
    return true;
}

void FontResource::releaseExternal(FontData& data) {
    if (data.glyphs) Platform::memory().release(data.glyphs);
    data.glyphs = nullptr;
    data.glyphCount = 0;
    data.loaded = false;
}

void FontResource::releaseExternalLarge(LargeFontData& data) {
    if (data.glyphs) Platform::memory().release(data.glyphs);
    data.glyphs = nullptr;
    data.glyphCount = 0;
    data.loaded = false;
}
