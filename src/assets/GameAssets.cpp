#include "assets/GameAssets.h"

#include "core/ResourcePack.h"
#include "hardware/Hal.h"
#include "hardware/PixelRenderer.h"

#include <Arduino.h>
#include <FS.h>
#include <cstdlib>

namespace GameAssets {
namespace {

constexpr uint32_t PACK_MAGIC = 0x58464753;
constexpr uint16_t PACK_VERSION = 1;
constexpr uint8_t FORMAT_INDEXED4_RLE = 1;
constexpr uint16_t MAX_FRAMES = 96;
constexpr uint32_t MAX_DATA_WORDS = 100000;
constexpr uint32_t MAX_PALETTE_WORDS = 2048;

struct __attribute__((packed)) Header {
    uint32_t magic;
    uint16_t version;
    uint16_t frameCount;
    uint32_t dataWords;
    uint32_t paletteWords;
};

struct __attribute__((packed)) Frame {
    uint16_t kind;
    uint16_t width;
    uint16_t height;
    uint8_t format;
    uint8_t paletteSize;
    uint16_t reserved;
    uint32_t offset;
    uint32_t length;
    uint32_t paletteOffset;
};

bool initialized = false;
bool loaded = false;
Frame* frames = nullptr;
uint16_t frameCount = 0;
uint16_t* data = nullptr;
uint32_t dataWords = 0;
uint16_t* palettes = nullptr;
uint32_t paletteWords = 0;
uint16_t* background = nullptr;
Kind cachedBackground = Kind::COUNT;
uint32_t packedBytes = 0;

template <typename T>
T* allocate(size_t count) {
    if (count == 0) return nullptr;
    size_t bytes = sizeof(T) * count;
    return psramFound() ? static_cast<T*>(ps_malloc(bytes)) : static_cast<T*>(malloc(bytes));
}

bool readExact(fs::File& file, void* out, size_t length) {
    return length == 0 || file.read(reinterpret_cast<uint8_t*>(out), length) == length;
}

const Frame* findFrame(Kind kind) {
    if (!loaded && !begin()) return nullptr;
    uint16_t value = static_cast<uint16_t>(kind);
    for (uint16_t i = 0; i < frameCount; ++i) {
        if (frames[i].kind == value) return &frames[i];
    }
    return nullptr;
}

bool validFrame(const Frame& frame, const Header& header) {
    if (frame.kind >= static_cast<uint16_t>(Kind::COUNT) || frame.width == 0 || frame.height == 0) return false;
    if (frame.format != FORMAT_INDEXED4_RLE || frame.paletteSize == 0 || frame.paletteSize > 16) return false;
    if (frame.reserved != 0 || frame.length == 0) return false;
    if (frame.offset > header.dataWords || frame.length > header.dataWords - frame.offset) return false;
    if (frame.paletteOffset > header.paletteWords || frame.paletteSize > header.paletteWords - frame.paletteOffset) return false;
    return true;
}

bool decodeBackground(const Frame& frame) {
    const uint32_t total = static_cast<uint32_t>(frame.width) * frame.height;
    if (frame.width != Hal::DISPLAY_W || frame.height != Hal::DISPLAY_H || total != 32400) return false;
    if (!background) background = allocate<uint16_t>(total);
    if (!background) return false;

    uint32_t source = 0;
    uint32_t pixel = 0;
    while (source < frame.length && pixel < total) {
        uint16_t token = data[frame.offset + source++];
        uint16_t run = token & 0x7FFF;
        if (run == 0) continue;
        if (token & 0x8000) {
            while (run-- && pixel < total) background[pixel++] = 0;
            continue;
        }

        uint16_t packed = 0;
        for (uint16_t i = 0; i < run && pixel < total; ++i) {
            if ((i & 3) == 0) {
                if (source >= frame.length) return false;
                packed = data[frame.offset + source++];
            }
            uint8_t paletteIndex = (packed >> ((i & 3) * 4)) & 0x0F;
            if (paletteIndex >= frame.paletteSize) return false;
            background[pixel++] = palettes[frame.paletteOffset + paletteIndex];
        }
    }
    return pixel == total;
}

}  // namespace

bool begin() {
    if (initialized) return loaded;
    initialized = true;

    ResourcePack& pack = ResourcePack::ins();
    if (!pack.begin()) return false;
    fs::File file;
    if (!pack.openGameAssets(file)) {
        Serial.println("[GameAssets] pack missing");
        return false;
    }

    Header header{};
    if (!readExact(file, &header, sizeof(header)) ||
        header.magic != PACK_MAGIC || header.version != PACK_VERSION ||
        header.frameCount == 0 || header.frameCount > MAX_FRAMES ||
        header.dataWords == 0 || header.dataWords > MAX_DATA_WORDS ||
        header.paletteWords == 0 || header.paletteWords > MAX_PALETTE_WORDS) {
        Serial.println("[GameAssets] invalid header");
        return false;
    }

    uint64_t expected = sizeof(Header) + static_cast<uint64_t>(header.frameCount) * sizeof(Frame) +
                        static_cast<uint64_t>(header.dataWords + header.paletteWords) * sizeof(uint16_t);
    if (expected != file.size()) {
        Serial.println("[GameAssets] size mismatch");
        return false;
    }

    Frame* loadedFrames = allocate<Frame>(header.frameCount);
    uint16_t* loadedData = allocate<uint16_t>(header.dataWords);
    uint16_t* loadedPalettes = allocate<uint16_t>(header.paletteWords);
    if (!loadedFrames || !loadedData || !loadedPalettes) {
        if (loadedFrames) free(loadedFrames);
        if (loadedData) free(loadedData);
        if (loadedPalettes) free(loadedPalettes);
        return false;
    }

    bool ok = readExact(file, loadedFrames, sizeof(Frame) * header.frameCount) &&
              readExact(file, loadedData, sizeof(uint16_t) * header.dataWords) &&
              readExact(file, loadedPalettes, sizeof(uint16_t) * header.paletteWords);
    if (ok) {
        for (uint16_t i = 0; i < header.frameCount; ++i) {
            if (!validFrame(loadedFrames[i], header)) {
                ok = false;
                break;
            }
        }
    }
    if (!ok) {
        free(loadedFrames);
        free(loadedData);
        free(loadedPalettes);
        Serial.println("[GameAssets] invalid payload");
        return false;
    }

    frames = loadedFrames;
    frameCount = header.frameCount;
    data = loadedData;
    dataWords = header.dataWords;
    palettes = loadedPalettes;
    paletteWords = header.paletteWords;
    packedBytes = static_cast<uint32_t>(expected);
    loaded = true;
    Serial.printf("[GameAssets] frames=%u bytes=%u psram=%u\n",
                  frameCount, packedBytes, psramFound() ? 1 : 0);
    return true;
}

bool available() {
    return loaded || begin();
}

uint32_t compressedBytes() {
    return packedBytes;
}

bool draw(Kind kind, int x, int y, float scale) {
    const Frame* frame = findFrame(kind);
    if (!frame) return false;
    PixelRenderer::drawIndexed4RleScaled(
        x, y, frame->width, frame->height,
        data, frame->offset, frame->length,
        palettes, frame->paletteOffset, frame->paletteSize,
        scale);
    return true;
}

bool drawCentered(Kind kind, int centerX, int centerY, float scale) {
    const Frame* frame = findFrame(kind);
    if (!frame) return false;
    int width = static_cast<int>(frame->width * scale);
    int height = static_cast<int>(frame->height * scale);
    return draw(kind, centerX - width / 2, centerY - height / 2, scale);
}

bool drawBattleBackground(Kind kind) {
    const Frame* frame = findFrame(kind);
    if (!frame) return false;
    if (cachedBackground != kind) {
        if (!decodeBackground(*frame)) return false;
        cachedBackground = kind;
    }
    PixelRenderer::canvas().pushImage(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, background);
    return true;
}

Kind itemKind(Game::ItemId item) {
    switch (item) {
    case Game::ItemId::POKE_BALL: return Kind::ITEM_POKE_BALL;
    case Game::ItemId::GREAT_BALL: return Kind::ITEM_GREAT_BALL;
    case Game::ItemId::HEAVY_BALL: return Kind::ITEM_HEAVY_BALL;
    case Game::ItemId::TIMER_BALL: return Kind::ITEM_TIMER_BALL;
    case Game::ItemId::NORMAL_FOOD: return Kind::ITEM_NORMAL_FOOD;
    case Game::ItemId::POTION: return Kind::ITEM_POTION;
    case Game::ItemId::SUPER_POTION: return Kind::ITEM_SUPER_POTION;
    case Game::ItemId::ANTIDOTE: return Kind::ITEM_ANTIDOTE;
    case Game::ItemId::CANDY: return Kind::ITEM_CANDY;
    default: return Kind::COUNT;
    }
}

Kind ballFrameKind(Game::ItemId item, uint8_t frame) {
    frame %= 8;
    uint16_t base = static_cast<uint16_t>(Kind::BALL_POKE_BALL_0);
    switch (item) {
    case Game::ItemId::GREAT_BALL: base = static_cast<uint16_t>(Kind::BALL_GREAT_BALL_0); break;
    case Game::ItemId::HEAVY_BALL: base = static_cast<uint16_t>(Kind::BALL_HEAVY_BALL_0); break;
    case Game::ItemId::TIMER_BALL: base = static_cast<uint16_t>(Kind::BALL_TIMER_BALL_0); break;
    default: break;
    }
    return static_cast<Kind>(base + frame);
}

Kind ballOpenKind(Game::ItemId item) {
    switch (item) {
    case Game::ItemId::GREAT_BALL: return Kind::BALL_GREAT_BALL_OPEN;
    case Game::ItemId::HEAVY_BALL: return Kind::BALL_HEAVY_BALL_OPEN;
    case Game::ItemId::TIMER_BALL: return Kind::BALL_TIMER_BALL_OPEN;
    default: return Kind::BALL_POKE_BALL_OPEN;
    }
}

}  // namespace GameAssets
