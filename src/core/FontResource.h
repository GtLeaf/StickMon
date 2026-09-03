#pragma once

#include <cstdint>

enum class FontFace : uint8_t {
    SARASA_CJK = 0,
    UNSCII_ASCII,
};

class FontResource {
public:
    static constexpr uint8_t GLYPH_W = 16;
    static constexpr uint8_t GLYPH_H = 16;
    static constexpr uint8_t GLYPH_BYTES = 32;
    static constexpr uint8_t LARGE_GLYPH_W = 32;
    static constexpr uint8_t LARGE_GLYPH_H = 32;
    static constexpr uint8_t LARGE_GLYPH_BYTES = 128;

    static FontResource& ins();

    bool begin();
    bool loaded() const;
    bool loaded(FontFace face) const;
    const char* source() const { return loaded() ? "littlefs" : "missing"; }
    uint16_t glyphCount(FontFace face) const;
    const uint8_t* findGlyphBitmap(uint32_t codepoint, FontFace face);
    const uint8_t* findLargeGlyphBitmap(uint32_t codepoint, FontFace face);

private:
    struct __attribute__((packed)) Glyph {
        uint32_t codepoint;
        uint8_t bitmap[GLYPH_BYTES];
    };

    struct FontData {
        bool loaded = false;
        uint16_t glyphCount = 0;
        Glyph* glyphs = nullptr;
    };

    struct __attribute__((packed)) LargeGlyph {
        uint32_t codepoint;
        uint8_t bitmap[LARGE_GLYPH_BYTES];
    };

    struct LargeFontData {
        bool loaded = false;
        uint16_t glyphCount = 0;
        LargeGlyph* glyphs = nullptr;
    };

    FontResource() = default;
    ~FontResource() = default;

    const FontData& dataFor(FontFace face) const;
    bool loadExternal(FontFace face, FontData& data);
    void releaseExternal(FontData& data);
    const LargeFontData& largeDataFor(FontFace face) const;
    bool beginLarge();
    bool loadExternalLarge(FontFace face, LargeFontData& data);
    void releaseExternalLarge(LargeFontData& data);

    bool initialized_ = false;
    bool largeInitialized_ = false;
    FontData sarasaCjk_;
    FontData unsciiAscii_;
    LargeFontData sarasaCjkLarge_;
    LargeFontData unsciiAsciiLarge_;
};
