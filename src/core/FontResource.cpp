#include "core/FontResource.h"

#include "core/ResourcePack.h"
#include "platform/api/PlatformServices.h"

#include <cstdlib>

namespace {
static constexpr uint32_t FONT_PACK_MAGIC = 0x4E464D53; // SMFN
static constexpr uint16_t FONT_PACK_VERSION = 1;
static constexpr uint16_t MAX_FONT_GLYPHS = 2048;
static constexpr const char* UNSCII_ASCII_FONT_ID = "ascii16-unscii";

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

bool validHeader(const PackedFontHeader& h) {
    if (h.magic != FONT_PACK_MAGIC || h.version != FONT_PACK_VERSION) return false;
    if (h.glyphCount == 0 || h.glyphCount > MAX_FONT_GLYPHS) return false;
    if (h.width != FontResource::GLYPH_W || h.height != FontResource::GLYPH_H) return false;
    return h.bytesPerGlyph == FontResource::GLYPH_BYTES;
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
    if (!validHeader(header)) {
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

void FontResource::releaseExternal(FontData& data) {
    if (data.glyphs) Platform::memory().release(data.glyphs);
    data.glyphs = nullptr;
    data.glyphCount = 0;
    data.loaded = false;
}
