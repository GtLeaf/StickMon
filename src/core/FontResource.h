#pragma once

#include <cstdint>

class FontResource {
public:
    static constexpr uint8_t GLYPH_W = 16;
    static constexpr uint8_t GLYPH_H = 16;
    static constexpr uint8_t GLYPH_BYTES = 32;

    static FontResource& ins();

    bool begin();
    bool loaded() const { return loaded_; }
    const char* source() const { return loaded_ ? "littlefs" : "missing"; }
    uint16_t glyphCount() const { return glyphCount_; }
    const uint8_t* findGlyphBitmap(uint32_t codepoint);

private:
    struct __attribute__((packed)) Glyph {
        uint32_t codepoint;
        uint8_t bitmap[GLYPH_BYTES];
    };

    FontResource() = default;
    ~FontResource() = default;

    bool loadExternal();
    void releaseExternal();

    bool initialized_ = false;
    bool loaded_ = false;
    uint16_t glyphCount_ = 0;
    Glyph* glyphs_ = nullptr;
};
