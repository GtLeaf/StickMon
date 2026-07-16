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

    static FontResource& ins();

    bool begin();
    bool loaded() const;
    bool loaded(FontFace face) const;
    const char* source() const { return loaded() ? "littlefs" : "missing"; }
    uint16_t glyphCount(FontFace face) const;
    const uint8_t* findGlyphBitmap(uint32_t codepoint, FontFace face);

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

    FontResource() = default;
    ~FontResource() = default;

    const FontData& dataFor(FontFace face) const;
    bool loadExternal(FontFace face, FontData& data);
    void releaseExternal(FontData& data);

    bool initialized_ = false;
    FontData sarasaCjk_;
    FontData unsciiAscii_;
};
