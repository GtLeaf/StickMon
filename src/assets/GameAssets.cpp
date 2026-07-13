#include "assets/GameAssets.h"

#include "core/DeflateDecoder.h"
#include "core/ResourcePack.h"
#include "hardware/Hal.h"
#include "hardware/PixelRenderer.h"

#include <Arduino.h>
#include <FS.h>
#include <cstdlib>
#include <cstring>

namespace GameAssets {
namespace {

constexpr uint32_t PACK_MAGIC = 0x58464753;
constexpr uint16_t PACK_VERSION = 2;
constexpr uint32_t FLAG_RAW_DEFLATE = 1;
constexpr uint8_t FORMAT_INDEXED4_RLE = 1;
constexpr uint16_t MAX_FRAMES = 96;
constexpr uint32_t MAX_DATA_WORDS = 200000;
constexpr uint32_t MAX_PALETTE_WORDS = 2048;
constexpr uint32_t MAX_PAYLOAD_BYTES = 384000;

enum class PackSlot : uint8_t {
    UI,
    BATTLE,
    MAP,
    HATCH,
    COUNT,
};

struct __attribute__((packed)) Header {
    uint32_t magic;
    uint16_t version;
    uint16_t frameCount;
    uint32_t dataWords;
    uint32_t paletteWords;
    uint32_t flags;
    uint32_t payloadRawBytes;
    uint32_t payloadCompressedBytes;
    uint32_t payloadCrc32;
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

static_assert(sizeof(Header) == 32, "Unexpected game asset header layout");
static_assert(sizeof(Frame) == 22, "Unexpected game asset frame layout");

struct AssetPack {
    bool attempted = false;
    bool loaded = false;
    uint8_t* payload = nullptr;
    Frame* frames = nullptr;
    uint16_t frameCount = 0;
    uint16_t* data = nullptr;
    uint32_t dataWords = 0;
    uint16_t* palettes = nullptr;
    uint32_t paletteWords = 0;
    uint32_t packedBytes = 0;
};

struct FrameRef {
    AssetPack* pack = nullptr;
    const Frame* frame = nullptr;
};

bool initialized = false;
bool resourcePackReady = false;
AssetPack packs[static_cast<uint8_t>(PackSlot::COUNT)];
uint16_t* background = nullptr;
Kind cachedBackground = Kind::COUNT;
int cachedBackgroundX = -1;
int cachedBackgroundY = -1;

template <typename T>
T* allocate(size_t count) {
    if (count == 0) return nullptr;
    size_t bytes = sizeof(T) * count;
    return psramFound() ? static_cast<T*>(ps_malloc(bytes)) : static_cast<T*>(malloc(bytes));
}

bool readExact(fs::File& file, void* out, size_t length) {
    return length == 0 || file.read(reinterpret_cast<uint8_t*>(out), length) == length;
}

const char* packName(PackSlot slot) {
    switch (slot) {
    case PackSlot::UI: return "ui";
    case PackSlot::BATTLE: return "battle";
    case PackSlot::MAP: return "map";
    case PackSlot::HATCH: return "hatch";
    default: return "unknown";
    }
}

PackSlot packSlotFor(Kind kind) {
    uint16_t value = static_cast<uint16_t>(kind);
    if (value <= static_cast<uint16_t>(Kind::ITEM_CANDY)) return PackSlot::UI;
    if (kind == Kind::EGG) return PackSlot::HATCH;
    if (kind >= Kind::EXPLORE_TILE_0072 && kind <= Kind::EXPLORE_TILE_1682) {
        return PackSlot::MAP;
    }
    if (kind < Kind::COUNT) return PackSlot::BATTLE;
    return PackSlot::COUNT;
}

bool openPackFile(ResourcePack& pack, PackSlot slot, fs::File& file) {
    switch (slot) {
    case PackSlot::UI: return pack.openUiAssets(file);
    case PackSlot::BATTLE: return pack.openBattleAssets(file);
    case PackSlot::MAP: return pack.openMapAssets(file);
    case PackSlot::HATCH: return pack.openHatchAssets(file);
    default: return false;
    }
}

bool validFrame(const Frame& frame, const Header& header) {
    if (frame.kind >= static_cast<uint16_t>(Kind::COUNT) || frame.width == 0 || frame.height == 0) return false;
    if (frame.format != FORMAT_INDEXED4_RLE || frame.paletteSize == 0 || frame.paletteSize > 16) return false;
    if (frame.reserved != 0 || frame.length == 0) return false;
    if (frame.offset > header.dataWords || frame.length > header.dataWords - frame.offset) return false;
    if (frame.paletteOffset > header.paletteWords || frame.paletteSize > header.paletteWords - frame.paletteOffset) return false;
    return true;
}

bool loadPack(PackSlot slot) {
    if (slot == PackSlot::COUNT) return false;
    AssetPack& loadedPack = packs[static_cast<uint8_t>(slot)];
    if (loadedPack.attempted) return loadedPack.loaded;
    loadedPack.attempted = true;

    if (!initialized && !begin()) return false;
    if (!resourcePackReady) return false;

    ResourcePack& resourcePack = ResourcePack::ins();
    fs::File file;
    if (!openPackFile(resourcePack, slot, file)) {
        Serial.printf("[GameAssets] pack=%s missing\n", packName(slot));
        return false;
    }

    Header header{};
    if (!readExact(file, &header, sizeof(header)) ||
        header.magic != PACK_MAGIC || header.version != PACK_VERSION ||
        header.frameCount == 0 || header.frameCount > MAX_FRAMES ||
        header.dataWords == 0 || header.dataWords > MAX_DATA_WORDS ||
        header.paletteWords == 0 || header.paletteWords > MAX_PALETTE_WORDS ||
        header.flags != FLAG_RAW_DEFLATE ||
        header.payloadRawBytes == 0 || header.payloadRawBytes > MAX_PAYLOAD_BYTES ||
        header.payloadCompressedBytes == 0) {
        Serial.printf("[GameAssets] pack=%s invalid header\n", packName(slot));
        return false;
    }

    uint64_t expectedRaw = static_cast<uint64_t>(header.frameCount) * sizeof(Frame) +
                           static_cast<uint64_t>(header.dataWords + header.paletteWords) * sizeof(uint16_t);
    uint64_t expectedFile = sizeof(Header) + static_cast<uint64_t>(header.payloadCompressedBytes);
    if (expectedRaw != header.payloadRawBytes || expectedFile != file.size()) {
        Serial.printf("[GameAssets] pack=%s size mismatch\n", packName(slot));
        return false;
    }

    uint8_t* payload = allocate<uint8_t>(header.payloadRawBytes);
    if (!payload) {
        Serial.printf("[GameAssets] pack=%s allocation failed bytes=%u\n",
                      packName(slot), header.payloadRawBytes);
        return false;
    }

    DeflateDecoder::Stats stats{};
    bool ok = DeflateDecoder::inflateFile(file,
                                          header.payloadCompressedBytes,
                                          payload,
                                          header.payloadRawBytes,
                                          header.payloadCrc32,
                                          &stats);
    Frame* loadedFrames = reinterpret_cast<Frame*>(payload);
    if (ok) {
        for (uint16_t i = 0; i < header.frameCount; ++i) {
            if (!validFrame(loadedFrames[i], header)) {
                ok = false;
                break;
            }
        }
    }
    if (!ok) {
        free(payload);
        Serial.printf("[GameAssets] pack=%s invalid payload read=%u inflate=%u total=%u\n",
                      packName(slot), stats.readMs, stats.inflateMs, stats.totalMs);
        return false;
    }

    uint8_t* wordData = payload + static_cast<size_t>(header.frameCount) * sizeof(Frame);
    loadedPack.payload = payload;
    loadedPack.frames = loadedFrames;
    loadedPack.frameCount = header.frameCount;
    loadedPack.data = reinterpret_cast<uint16_t*>(wordData);
    loadedPack.dataWords = header.dataWords;
    loadedPack.palettes = loadedPack.data + header.dataWords;
    loadedPack.paletteWords = header.paletteWords;
    loadedPack.packedBytes = static_cast<uint32_t>(expectedFile);
    loadedPack.loaded = true;
    Serial.printf(
        "[GameAssets] pack=%s frames=%u compressed=%u decoded=%u read=%u inflate=%u total=%u psram=%u\n",
        packName(slot), header.frameCount, loadedPack.packedBytes, header.payloadRawBytes,
        stats.readMs, stats.inflateMs, stats.totalMs, psramFound() ? 1 : 0);
    return true;
}

FrameRef findFrame(Kind kind) {
    PackSlot slot = packSlotFor(kind);
    if (!loadPack(slot)) return {};
    AssetPack& pack = packs[static_cast<uint8_t>(slot)];
    uint16_t value = static_cast<uint16_t>(kind);
    for (uint16_t i = 0; i < pack.frameCount; ++i) {
        if (pack.frames[i].kind == value) {
            FrameRef ref;
            ref.pack = &pack;
            ref.frame = &pack.frames[i];
            return ref;
        }
    }
    return {};
}

bool decodeBackgroundViewport(const FrameRef& ref, int cameraX, int cameraY) {
    if (!ref.pack || !ref.frame) return false;
    const Frame& frame = *ref.frame;
    const AssetPack& pack = *ref.pack;
    const uint32_t total = static_cast<uint32_t>(frame.width) * frame.height;
    if (frame.width < Hal::DISPLAY_W || frame.height < Hal::DISPLAY_H || total == 0) return false;
    if (!background) background = allocate<uint16_t>(Hal::DISPLAY_W * Hal::DISPLAY_H);
    if (!background) return false;
    memset(background, 0, Hal::DISPLAY_W * Hal::DISPLAY_H * sizeof(uint16_t));

    uint32_t source = 0;
    uint32_t pixel = 0;
    while (source < frame.length && pixel < total) {
        uint16_t token = pack.data[frame.offset + source++];
        uint16_t run = token & 0x7FFF;
        if (run == 0) continue;
        if (token & 0x8000) {
            pixel += min<uint32_t>(run, total - pixel);
            continue;
        }

        uint32_t runStart = pixel;
        uint32_t runLength = min<uint32_t>(run, total - pixel);
        uint32_t packedWords = (static_cast<uint32_t>(run) + 3) / 4;
        if (source + packedWords > frame.length) return false;
        uint32_t runEnd = runStart + runLength;

        for (int destY = 0; destY < Hal::DISPLAY_H; ++destY) {
            uint32_t rowStart = static_cast<uint32_t>(cameraY + destY) * frame.width + cameraX;
            uint32_t visibleStart = max(runStart, rowStart);
            uint32_t visibleEnd = min(runEnd, rowStart + Hal::DISPLAY_W);
            for (uint32_t absolute = visibleStart; absolute < visibleEnd; ++absolute) {
                uint32_t runOffset = absolute - runStart;
                uint16_t packed = pack.data[frame.offset + source + runOffset / 4];
                uint8_t paletteIndex = (packed >> ((runOffset & 3) * 4)) & 0x0F;
                if (paletteIndex >= frame.paletteSize) return false;
                uint32_t destX = absolute - rowStart;
                background[static_cast<uint32_t>(destY) * Hal::DISPLAY_W + destX] =
                    pack.palettes[frame.paletteOffset + paletteIndex];
            }
        }
        source += packedWords;
        pixel = runEnd;
    }
    return pixel == total;
}

}  // namespace

bool begin() {
    if (initialized) return resourcePackReady;
    initialized = true;
    resourcePackReady = ResourcePack::ins().begin();
    return resourcePackReady;
}

bool available() {
    return initialized ? resourcePackReady : begin();
}

uint32_t compressedBytes() {
    uint32_t total = 0;
    for (const AssetPack& pack : packs) total += pack.packedBytes;
    return total;
}

bool draw(Kind kind, int x, int y, float scale) {
    FrameRef ref = findFrame(kind);
    if (!ref.frame) return false;
    PixelRenderer::drawIndexed4RleScaled(
        x, y, ref.frame->width, ref.frame->height,
        ref.pack->data, ref.frame->offset, ref.frame->length,
        ref.pack->palettes, ref.frame->paletteOffset, ref.frame->paletteSize,
        scale);
    return true;
}

bool drawCentered(Kind kind, int centerX, int centerY, float scale) {
    FrameRef ref = findFrame(kind);
    if (!ref.frame) return false;
    int width = static_cast<int>(ref.frame->width * scale);
    int height = static_cast<int>(ref.frame->height * scale);
    PixelRenderer::drawIndexed4RleScaled(
        centerX - width / 2, centerY - height / 2,
        ref.frame->width, ref.frame->height,
        ref.pack->data, ref.frame->offset, ref.frame->length,
        ref.pack->palettes, ref.frame->paletteOffset, ref.frame->paletteSize,
        scale);
    return true;
}

bool drawBattleBackground(Kind kind) {
    return drawBackgroundViewport(kind, 0, 0);
}

bool drawBackgroundViewport(Kind kind, int cameraX, int cameraY) {
    FrameRef ref = findFrame(kind);
    if (!ref.frame) return false;
    int maxCameraX = max(0, static_cast<int>(ref.frame->width) - Hal::DISPLAY_W);
    int maxCameraY = max(0, static_cast<int>(ref.frame->height) - Hal::DISPLAY_H);
    cameraX = constrain(cameraX, 0, maxCameraX);
    cameraY = constrain(cameraY, 0, maxCameraY);
    if (cachedBackground != kind || cachedBackgroundX != cameraX || cachedBackgroundY != cameraY) {
        if (!decodeBackgroundViewport(ref, cameraX, cameraY)) return false;
        cachedBackground = kind;
        cachedBackgroundX = cameraX;
        cachedBackgroundY = cameraY;
    }
    PixelRenderer::canvas().pushImage(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, background);
    return true;
}

bool drawExploreTile(uint16_t tileId, int x, int y) {
    Kind kind = Kind::COUNT;
    switch (tileId) {
    case 72: kind = Kind::EXPLORE_TILE_0072; break;
    case 144: kind = Kind::EXPLORE_TILE_0144; break;
    case 168: kind = Kind::EXPLORE_TILE_0168; break;
    case 385: kind = Kind::EXPLORE_TILE_0385; break;
    case 386: kind = Kind::EXPLORE_TILE_0386; break;
    case 387: kind = Kind::EXPLORE_TILE_0387; break;
    case 388: kind = Kind::EXPLORE_TILE_0388; break;
    case 389: kind = Kind::EXPLORE_TILE_0389; break;
    case 390: kind = Kind::EXPLORE_TILE_0390; break;
    case 415: kind = Kind::EXPLORE_TILE_0415; break;
    case 537: kind = Kind::EXPLORE_TILE_0537; break;
    case 538: kind = Kind::EXPLORE_TILE_0538; break;
    case 539: kind = Kind::EXPLORE_TILE_0539; break;
    case 540: kind = Kind::EXPLORE_TILE_0540; break;
    case 542: kind = Kind::EXPLORE_TILE_0542; break;
    case 545: kind = Kind::EXPLORE_TILE_0545; break;
    case 546: kind = Kind::EXPLORE_TILE_0546; break;
    case 547: kind = Kind::EXPLORE_TILE_0547; break;
    case 553: kind = Kind::EXPLORE_TILE_0553; break;
    case 554: kind = Kind::EXPLORE_TILE_0554; break;
    case 555: kind = Kind::EXPLORE_TILE_0555; break;
    case 556: kind = Kind::EXPLORE_TILE_0556; break;
    case 558: kind = Kind::EXPLORE_TILE_0558; break;
    case 800: kind = Kind::EXPLORE_TILE_0800; break;
    case 801: kind = Kind::EXPLORE_TILE_0801; break;
    case 802: kind = Kind::EXPLORE_TILE_0802; break;
    case 804: kind = Kind::EXPLORE_TILE_0804; break;
    case 805: kind = Kind::EXPLORE_TILE_0805; break;
    case 808: kind = Kind::EXPLORE_TILE_0808; break;
    case 809: kind = Kind::EXPLORE_TILE_0809; break;
    case 810: kind = Kind::EXPLORE_TILE_0810; break;
    case 811: kind = Kind::EXPLORE_TILE_0811; break;
    case 818: kind = Kind::EXPLORE_TILE_0818; break;
    case 819: kind = Kind::EXPLORE_TILE_0819; break;
    case 1662: kind = Kind::EXPLORE_TILE_1662; break;
    case 1665: kind = Kind::EXPLORE_TILE_1665; break;
    case 1681: kind = Kind::EXPLORE_TILE_1681; break;
    case 1682: kind = Kind::EXPLORE_TILE_1682; break;
    default: return false;
    }
    return draw(kind, x, y);
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
