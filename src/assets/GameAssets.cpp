#include "assets/GameAssets.h"

#include "core/DeflateDecoder.h"
#include "core/MathUtil.h"
#include "core/ResourcePack.h"
#include "hardware/Hal.h"
#include "presentation/PixelRenderer.h"

#include <cstdlib>
#include <cstring>

namespace GameAssets {
namespace {

constexpr uint32_t PACK_MAGIC = 0x58464753;
constexpr uint16_t PACK_VERSION = 2;
constexpr uint32_t FLAG_RAW_DEFLATE = 1;
constexpr uint8_t FORMAT_INDEXED4_RLE = 1;
constexpr uint16_t MAX_FRAMES = 256;
constexpr uint32_t MAX_DATA_WORDS = 200000;
constexpr uint32_t MAX_PALETTE_WORDS = 2048;
constexpr uint32_t MAX_PAYLOAD_BYTES = 384000;
constexpr uint16_t INVALID_FRAME_INDEX = 0xFFFF;

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
uint16_t frameIndices[static_cast<uint16_t>(Kind::COUNT)];
uint16_t* background = nullptr;
Kind cachedBackground = Kind::COUNT;
int cachedBackgroundX = -1;
int cachedBackgroundY = -1;

template <typename T>
T* allocate(size_t count) {
    if (count == 0) return nullptr;
    size_t bytes = sizeof(T) * count;
    return static_cast<T*>(Platform::memory().allocate(bytes, true));
}

bool readExact(Platform::ResourceFile& file, void* out, size_t length) {
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
    if (value <= static_cast<uint16_t>(Kind::SHOWER_BACKGROUND) ||
        kind == Kind::ITEM_HEART_SCALE ||
        kind == Kind::EXPLORE_PICKUP_BALL) {
        return PackSlot::UI;
    }
    if (kind == Kind::EGG) return PackSlot::HATCH;
    if (kind >= Kind::EXPLORE_TILE_0072 && kind <= Kind::EXPLORE_WATERFALL_BOTTOM_F3) {
        return PackSlot::MAP;
    }
    if (kind < Kind::COUNT) return PackSlot::BATTLE;
    return PackSlot::COUNT;
}

bool openPackFile(ResourcePack& pack, PackSlot slot, Platform::ResourceFile& file) {
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
    Platform::ResourceFile file;
    if (!openPackFile(resourcePack, slot, file)) {
        Platform::logf("[GameAssets] pack=%s missing\n", packName(slot));
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
        Platform::logf("[GameAssets] pack=%s invalid header\n", packName(slot));
        return false;
    }

    uint64_t expectedRaw = static_cast<uint64_t>(header.frameCount) * sizeof(Frame) +
                           static_cast<uint64_t>(header.dataWords + header.paletteWords) * sizeof(uint16_t);
    uint64_t expectedFile = sizeof(Header) + static_cast<uint64_t>(header.payloadCompressedBytes);
    if (expectedRaw != header.payloadRawBytes || expectedFile != file.size()) {
        Platform::logf("[GameAssets] pack=%s size mismatch\n", packName(slot));
        return false;
    }

    uint8_t* payload = allocate<uint8_t>(header.payloadRawBytes);
    if (!payload) {
        Platform::logf("[GameAssets] pack=%s allocation failed bytes=%u\n",
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
    bool seenKinds[static_cast<uint16_t>(Kind::COUNT)] = {};
    if (ok) {
        for (uint16_t i = 0; i < header.frameCount; ++i) {
            uint16_t kindValue = loadedFrames[i].kind;
            if (!validFrame(loadedFrames[i], header) ||
                packSlotFor(static_cast<Kind>(kindValue)) != slot ||
                seenKinds[kindValue] ||
                frameIndices[kindValue] != INVALID_FRAME_INDEX) {
                ok = false;
                break;
            }
            seenKinds[kindValue] = true;
        }
    }
    if (!ok) {
        Platform::memory().release(payload);
        Platform::logf("[GameAssets] pack=%s invalid payload read=%u inflate=%u total=%u\n",
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
    for (uint16_t i = 0; i < header.frameCount; ++i) {
        frameIndices[loadedFrames[i].kind] = i;
    }
    loadedPack.loaded = true;
    Platform::logf(
        "[GameAssets] pack=%s frames=%u compressed=%u decoded=%u read=%u inflate=%u total=%u psram=%u\n",
        packName(slot), header.frameCount, loadedPack.packedBytes, header.payloadRawBytes,
        stats.readMs, stats.inflateMs, stats.totalMs,
        Platform::power().externalMemorySize() > 0 ? 1 : 0);
    return true;
}

FrameRef findFrame(Kind kind) {
    uint16_t value = static_cast<uint16_t>(kind);
    if (value >= static_cast<uint16_t>(Kind::COUNT)) return {};
    PackSlot slot = packSlotFor(kind);
    if (!loadPack(slot)) return {};
    AssetPack& pack = packs[static_cast<uint8_t>(slot)];
    uint16_t index = frameIndices[value];
    if (index == INVALID_FRAME_INDEX || index >= pack.frameCount) return {};
    FrameRef ref;
    ref.pack = &pack;
    ref.frame = &pack.frames[index];
    return ref;
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
            pixel += MathUtil::min<uint32_t>(run, total - pixel);
            continue;
        }

        uint32_t runStart = pixel;
        uint32_t runLength = MathUtil::min<uint32_t>(run, total - pixel);
        uint32_t packedWords = (static_cast<uint32_t>(run) + 3) / 4;
        if (source + packedWords > frame.length) return false;
        uint32_t runEnd = runStart + runLength;

        for (int destY = 0; destY < Hal::DISPLAY_H; ++destY) {
            uint32_t rowStart = static_cast<uint32_t>(cameraY + destY) * frame.width + cameraX;
            uint32_t visibleStart = MathUtil::max(runStart, rowStart);
            uint32_t visibleEnd = MathUtil::min(runEnd, rowStart + Hal::DISPLAY_W);
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
    for (uint16_t& index : frameIndices) index = INVALID_FRAME_INDEX;
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

bool drawCenteredAlpha(Kind kind, int centerX, int centerY,
                       float scale, uint8_t alpha) {
    FrameRef ref = findFrame(kind);
    if (!ref.frame || alpha == 0) return false;
    int width = static_cast<int>(ref.frame->width * scale);
    int height = static_cast<int>(ref.frame->height * scale);
    PixelRenderer::drawIndexed4RleScaled(
        centerX - width / 2, centerY - height / 2,
        ref.frame->width, ref.frame->height,
        ref.pack->data, ref.frame->offset, ref.frame->length,
        ref.pack->palettes, ref.frame->paletteOffset, ref.frame->paletteSize,
        scale, false, alpha);
    return true;
}

bool drawBattleBackground(Kind kind) {
    return drawBackgroundViewport(kind, 0, 0);
}

bool drawBackgroundViewport(Kind kind, int cameraX, int cameraY) {
    FrameRef ref = findFrame(kind);
    if (!ref.frame) return false;
    int maxCameraX = MathUtil::max(0, static_cast<int>(ref.frame->width) - Hal::DISPLAY_W);
    int maxCameraY = MathUtil::max(0, static_cast<int>(ref.frame->height) - Hal::DISPLAY_H);
    cameraX = MathUtil::clamp(cameraX, 0, maxCameraX);
    cameraY = MathUtil::clamp(cameraY, 0, maxCameraY);
    if (cachedBackground != kind || cachedBackgroundX != cameraX || cachedBackgroundY != cameraY) {
        if (!decodeBackgroundViewport(ref, cameraX, cameraY)) return false;
        cachedBackground = kind;
        cachedBackgroundX = cameraX;
        cachedBackgroundY = cameraY;
    }
    PixelRenderer::canvas().pushImage(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, background);
    return true;
}

bool drawExploreTile(uint16_t tileId, int x, int y, uint8_t animationFrame) {
    Kind kind = Kind::COUNT;
    uint8_t seaFrame = animationFrame % 8;
    uint8_t waterfallFrame = animationFrame % 4;
    switch (tileId) {
    case 48:
        kind = static_cast<Kind>(static_cast<uint16_t>(Kind::EXPLORE_TILE_0048_F0) + seaFrame);
        break;
    case 64:
        kind = static_cast<Kind>(static_cast<uint16_t>(Kind::EXPLORE_TILE_0064_F0) + seaFrame);
        break;
    case 68:
        kind = static_cast<Kind>(static_cast<uint16_t>(Kind::EXPLORE_TILE_0068_F0) + seaFrame);
        break;
    case 72:
        kind = seaFrame == 0
                   ? Kind::EXPLORE_TILE_0072
                   : static_cast<Kind>(
                         static_cast<uint16_t>(Kind::EXPLORE_TILE_0072_F1) + seaFrame - 1);
        break;
    case 80:
        kind = static_cast<Kind>(static_cast<uint16_t>(Kind::EXPLORE_TILE_0080_F0) + seaFrame);
        break;
    case 84:
        kind = static_cast<Kind>(static_cast<uint16_t>(Kind::EXPLORE_TILE_0084_F0) + seaFrame);
        break;
    case 273: case 283: case 285:
        kind = static_cast<Kind>(
            static_cast<uint16_t>(Kind::EXPLORE_WATERFALL_CREST_F0) + waterfallFrame);
        break;
    case 288: case 304: case 308: case 312: case 316: case 322: case 324:
    case 326: case 328:
        kind = static_cast<Kind>(
            static_cast<uint16_t>(Kind::EXPLORE_WATERFALL_BODY_F0) + waterfallFrame);
        break;
    case 336:
        kind = static_cast<Kind>(
            static_cast<uint16_t>(Kind::EXPLORE_WATERFALL_BOTTOM_F0) + waterfallFrame);
        break;
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
    case 859: kind = Kind::EXPLORE_TILE_0859; break;
    case 1088: kind = Kind::EXPLORE_TILE_1088; break;
    case 1090: kind = Kind::EXPLORE_TILE_1090; break;
    case 1096: kind = Kind::EXPLORE_TILE_1096; break;
    case 1097: kind = Kind::EXPLORE_TILE_1097; break;
    case 1098: kind = Kind::EXPLORE_TILE_1098; break;
    case 1104: kind = Kind::EXPLORE_TILE_1104; break;
    case 1106: kind = Kind::EXPLORE_TILE_1106; break;
    case 1161: kind = Kind::EXPLORE_TILE_1161; break;
    case 1162: kind = Kind::EXPLORE_TILE_1162; break;
    case 1185: kind = Kind::EXPLORE_TILE_1185; break;
    case 1188: kind = Kind::EXPLORE_TILE_1188; break;
    case 1506: kind = Kind::EXPLORE_TILE_1506; break;
    case 1507: kind = Kind::EXPLORE_TILE_1507; break;
    case 1514: kind = Kind::EXPLORE_TILE_1514; break;
    case 1515: kind = Kind::EXPLORE_TILE_1515; break;
    case 1532: kind = Kind::EXPLORE_TILE_1532; break;
    case 1627: kind = Kind::EXPLORE_TILE_1627; break;
    case 1635: kind = Kind::EXPLORE_TILE_1635; break;
    case 1643: kind = Kind::EXPLORE_TILE_1643; break;
    case 1662: kind = Kind::EXPLORE_TILE_1662; break;
    case 1665: kind = Kind::EXPLORE_TILE_1665; break;
    case 1681: kind = Kind::EXPLORE_TILE_1681; break;
    case 1682: kind = Kind::EXPLORE_TILE_1682; break;
    case 4400: kind = Kind::EXPLORE_TILE_4400; break;
    case 4401: kind = Kind::EXPLORE_TILE_4401; break;
    case 4402: kind = Kind::EXPLORE_TILE_4402; break;
    case 4403: kind = Kind::EXPLORE_TILE_4403; break;
    case 1231: kind = Kind::EXPLORE_TILE_1231; break;
    case 4500: kind = Kind::EXPLORE_TILE_4500; break;
    case 4501: kind = Kind::EXPLORE_TILE_4501; break;
    case 4502: kind = Kind::EXPLORE_TILE_4502; break;
    case 4503: kind = Kind::EXPLORE_TILE_4503; break;
    case 4504: kind = Kind::EXPLORE_TILE_4504; break;
    case 4505: kind = Kind::EXPLORE_TILE_4505; break;
    case 4506: kind = Kind::EXPLORE_TILE_4506; break;
    case 4507: kind = Kind::EXPLORE_TILE_4507; break;
    case 4508: kind = Kind::EXPLORE_TILE_4508; break;
    case 4509: kind = Kind::EXPLORE_TILE_4509; break;
    case 4510: kind = Kind::EXPLORE_TILE_4510; break;
    case 4511: kind = Kind::EXPLORE_TILE_4511; break;
    case 4512: kind = Kind::EXPLORE_TILE_4512; break;
    case 4513: kind = Kind::EXPLORE_TILE_4513; break;
    case 4514: kind = Kind::EXPLORE_TILE_4514; break;
    case 4515: kind = Kind::EXPLORE_TILE_4515; break;
    case 4516: kind = Kind::EXPLORE_TILE_4516; break;
    case 4517: kind = Kind::EXPLORE_TILE_4517; break;
    case 4518: kind = Kind::EXPLORE_TILE_4518; break;
    case 4519: kind = Kind::EXPLORE_TILE_4519; break;
    case 4520: kind = Kind::EXPLORE_TILE_4520; break;
    case 4521: kind = Kind::EXPLORE_TILE_4521; break;
    case 4522: kind = Kind::EXPLORE_TILE_4522; break;
    case 4523: kind = Kind::EXPLORE_TILE_4523; break;
    case 4524: kind = Kind::EXPLORE_TILE_4524; break;
    case 4525: kind = Kind::EXPLORE_TILE_4525; break;
    case 4526: kind = Kind::EXPLORE_TILE_4526; break;
    case 4527: kind = Kind::EXPLORE_TILE_4527; break;
    case 4528: kind = Kind::EXPLORE_TILE_4528; break;
    case 4529: kind = Kind::EXPLORE_TILE_4529; break;
    case 4530: kind = Kind::EXPLORE_TILE_4530; break;
    case 4531: kind = Kind::EXPLORE_TILE_4531; break;
    case 4532: kind = Kind::EXPLORE_TILE_4532; break;
    case 4533: kind = Kind::EXPLORE_TILE_4533; break;
    case 4534: kind = Kind::EXPLORE_TILE_4534; break;
    case 4535: kind = Kind::EXPLORE_TILE_4535; break;
    case 4536: kind = Kind::EXPLORE_TILE_4536; break;
    case 4537: kind = Kind::EXPLORE_TILE_4537; break;
    case 4538: kind = Kind::EXPLORE_TILE_4538; break;
    case 4539: kind = Kind::EXPLORE_TILE_4539; break;
    case 4540: kind = Kind::EXPLORE_TILE_4540; break;
    case 4541: kind = Kind::EXPLORE_TILE_4541; break;
    case 4542: kind = Kind::EXPLORE_TILE_4542; break;
    case 4543: kind = Kind::EXPLORE_TILE_4543; break;
    case 4544: kind = Kind::EXPLORE_TILE_4544; break;
    default: return false;
    }
    return draw(kind, x, y);
}

Kind itemKind(Game::ItemId item) {
    switch (item) {
    case Game::ItemId::NORMAL_FOOD: return Kind::ITEM_NORMAL_FOOD;
    case Game::ItemId::POTION: return Kind::ITEM_POTION;
    case Game::ItemId::SUPER_POTION: return Kind::ITEM_SUPER_POTION;
    case Game::ItemId::ANTIDOTE: return Kind::ITEM_ANTIDOTE;
    case Game::ItemId::CANDY: return Kind::ITEM_CANDY;
    case Game::ItemId::TASTY_FOOD: return Kind::ITEM_TASTY_FOOD;
    case Game::ItemId::SWEET_FOOD: return Kind::ITEM_SWEET_FOOD;
    case Game::ItemId::SPICY_FOOD: return Kind::ITEM_SPICY_FOOD;
    case Game::ItemId::SOUR_FOOD: return Kind::ITEM_SOUR_FOOD;
    case Game::ItemId::BITTER_FOOD: return Kind::ITEM_BITTER_FOOD;
    case Game::ItemId::DRY_FOOD: return Kind::ITEM_DRY_FOOD;
    case Game::ItemId::PARALYZE_HEAL: return Kind::ITEM_PARALYZE_HEAL;
    case Game::ItemId::AWAKENING: return Kind::ITEM_AWAKENING;
    case Game::ItemId::BURN_HEAL: return Kind::ITEM_BURN_HEAL;
    case Game::ItemId::ICE_HEAL: return Kind::ITEM_ICE_HEAL;
    case Game::ItemId::MAX_POTION: return Kind::ITEM_MAX_POTION;
    case Game::ItemId::FULL_RESTORE: return Kind::ITEM_FULL_RESTORE;
    case Game::ItemId::FULL_HEAL: return Kind::ITEM_FULL_HEAL;
    case Game::ItemId::FIRE_STONE: return Kind::ITEM_FIRE_STONE;
    case Game::ItemId::WATER_STONE: return Kind::ITEM_WATER_STONE;
    case Game::ItemId::THUNDER_STONE: return Kind::ITEM_THUNDER_STONE;
    case Game::ItemId::REVIVE: return Kind::ITEM_REVIVE;
    case Game::ItemId::MAX_REPEL: return Kind::ITEM_MAX_REPEL;
    case Game::ItemId::HONEY: return Kind::ITEM_HONEY;
    case Game::ItemId::NUGGET: return Kind::ITEM_NUGGET;
    case Game::ItemId::BIG_PEARL: return Kind::ITEM_BIG_PEARL;
    case Game::ItemId::STAR_PIECE: return Kind::ITEM_STAR_PIECE;
    case Game::ItemId::HEART_SCALE: return Kind::ITEM_HEART_SCALE;
    case Game::ItemId::SOAP_0: return Kind::SHOWER_SOAP_0;
    case Game::ItemId::SOAP_1: return Kind::SHOWER_SOAP_1;
    case Game::ItemId::SOAP_2: return Kind::SHOWER_SOAP_2;
    default: return Kind::COUNT;
    }
}

Kind statusKind(Game::MajorStatus status) {
    switch (status) {
    case Game::MajorStatus::POISON: return Kind::STATUS_POISON;
    case Game::MajorStatus::TOXIC: return Kind::STATUS_TOXIC;
    case Game::MajorStatus::PARALYSIS: return Kind::STATUS_PARALYSIS;
    case Game::MajorStatus::SLEEP: return Kind::STATUS_SLEEP;
    case Game::MajorStatus::BURN: return Kind::STATUS_BURN;
    case Game::MajorStatus::FREEZE: return Kind::STATUS_FREEZE;
    default: return Kind::COUNT;
    }
}

}  // namespace GameAssets
