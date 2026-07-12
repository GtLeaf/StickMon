#include "assets/PokemonSprites.h"
#include "core/ResourcePack.h"
#include <Arduino.h>
#include <FS.h>
#include <cstdlib>
#include <cstring>
#include "hardware/PixelRenderer.h"

namespace PokemonSprites {

namespace {
static constexpr uint8_t SPRITE_SOURCE_FILE_BLOCK = 2;
static constexpr uint8_t TEAM_CACHE_CAP = 2;
static constexpr uint8_t CACHE_CAP = 4;
static constexpr uint8_t KNOWN_MISSING_FILE_CAP = 16;
static constexpr uint8_t FRAME_MISSING_CAP = 4;
static constexpr uint8_t KNOWN_MISSING_FRAME_CAP = 16;
static constexpr uint32_t SPRITE_PACK_MAGIC = 0x5350534D;
static constexpr uint16_t SPRITE_PACK_VERSION = 1;
static constexpr uint16_t MAX_PACK_FRAMES = 256;

struct __attribute__((packed)) PackedSpriteHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t speciesId;
    uint16_t frameCount;
    uint16_t rleWords;
    uint16_t paletteWords;
    uint16_t reserved;
};

struct __attribute__((packed)) PackedSpriteFrame {
    uint16_t kind;
    uint8_t width;
    uint8_t height;
    uint8_t format;
    uint8_t paletteSize;
    uint16_t reserved;
    uint32_t offset;
    uint32_t length;
    uint32_t paletteOffset;
};

struct CachedSpecies {
    uint16_t speciesId = 0;
    uint16_t rleWords = 0;
    uint16_t paletteWords = 0;
    uint16_t frameCount = 0;
    SpriteFrame* frames = nullptr;
    uint16_t* data = nullptr;
    uint16_t* palettes = nullptr;
};

struct MissingFrameKey {
    uint16_t speciesId = 0;
    uint16_t kind = 0;
};

CachedSpecies gCache[CACHE_CAP];
SpriteCacheStats gStats = {};
uint16_t gTeamSignature[CACHE_CAP] = {};
uint16_t gKnownMissingFiles[KNOWN_MISSING_FILE_CAP] = {};
uint16_t gTeamMissing[TEAM_CACHE_CAP] = {};
uint16_t gFrameMissing[FRAME_MISSING_CAP] = {};
MissingFrameKey gKnownMissingFrames[KNOWN_MISSING_FRAME_CAP] = {};
uint8_t gTeamCount = 0;
uint8_t gDynamicSlot = TEAM_CACHE_CAP;

bool containsSpecies(const uint16_t* values, uint8_t count, uint16_t speciesId) {
    for (uint8_t i = 0; i < count; ++i) {
        if (values[i] == speciesId) return true;
    }
    return false;
}

bool appendUniqueSpecies(uint16_t* values, uint8_t count, uint16_t speciesId) {
    if (speciesId == 0 || containsSpecies(values, count, speciesId)) return false;
    for (uint8_t i = 0; i < count; ++i) {
        if (values[i] == 0) {
            values[i] = speciesId;
            return true;
        }
    }
    return false;
}

void refreshMissingStats() {
    uint16_t unique[TEAM_CACHE_CAP + FRAME_MISSING_CAP] = {};
    uint8_t uniqueCount = 0;
    auto collect = [&](const uint16_t* values, uint8_t count) {
        for (uint8_t i = 0; i < count; ++i) {
            uint16_t speciesId = values[i];
            if (speciesId == 0 || containsSpecies(unique, uniqueCount, speciesId)) continue;
            unique[uniqueCount++] = speciesId;
        }
    };
    collect(gTeamMissing, TEAM_CACHE_CAP);
    collect(gFrameMissing, FRAME_MISSING_CAP);
    gStats.missingSpecies = uniqueCount;
    gStats.firstMissingSpecies = uniqueCount > 0 ? unique[0] : 0;
}

void noteMissingSpecies(uint16_t speciesId) {
    if (speciesId == 0) return;
    bool teamSpecies = false;
    for (uint8_t i = 0; i < gTeamCount; ++i) {
        if (gTeamSignature[i] == speciesId) {
            gTeamMissing[i] = speciesId;
            teamSpecies = true;
        }
    }
    if (!teamSpecies) appendUniqueSpecies(gFrameMissing, FRAME_MISSING_CAP, speciesId);
    refreshMissingStats();
}

void releaseEntry(CachedSpecies& entry) {
    if (entry.data) free(entry.data);
    if (entry.frames) free(entry.frames);
    entry = CachedSpecies{};
}

void freeCache() {
    for (auto& entry : gCache) {
        releaseEntry(entry);
    }
    uint32_t reloadCount = gStats.reloadCount;
    gStats = SpriteCacheStats{};
    gStats.reloadCount = reloadCount;
    gStats.cachedSpecies = 0;
    gStats.freePsram = ESP.getFreePsram();
    gStats.psram = psramFound();
    memset(gKnownMissingFiles, 0, sizeof(gKnownMissingFiles));
    memset(gTeamMissing, 0, sizeof(gTeamMissing));
    memset(gFrameMissing, 0, sizeof(gFrameMissing));
    memset(gKnownMissingFrames, 0, sizeof(gKnownMissingFrames));
    gDynamicSlot = TEAM_CACHE_CAP;
}

bool knownMissingFile(uint16_t speciesId) {
    if (speciesId == 0) return true;
    for (uint8_t i = 0; i < KNOWN_MISSING_FILE_CAP; ++i) {
        if (gKnownMissingFiles[i] == speciesId) return true;
    }
    return false;
}

bool rememberMissingFile(uint16_t speciesId) {
    if (speciesId == 0 || knownMissingFile(speciesId)) return false;
    for (uint8_t i = 0; i < KNOWN_MISSING_FILE_CAP; ++i) {
        if (gKnownMissingFiles[i] == 0) {
            gKnownMissingFiles[i] = speciesId;
            return true;
        }
    }
    return false;
}

bool rememberMissingFrame(uint16_t speciesId, uint16_t kind) {
    for (uint8_t i = 0; i < KNOWN_MISSING_FRAME_CAP; ++i) {
        const MissingFrameKey& key = gKnownMissingFrames[i];
        if (key.speciesId == speciesId && key.kind == kind) return false;
        if (key.speciesId == 0) {
            gKnownMissingFrames[i].speciesId = speciesId;
            gKnownMissingFrames[i].kind = kind;
            return true;
        }
    }
    return false;
}

CachedSpecies* cachedSpeciesFor(uint16_t speciesId) {
    for (auto& entry : gCache) {
        if (entry.speciesId == speciesId && entry.data) return &entry;
    }
    return nullptr;
}

bool readExact(fs::File& file, void* out, size_t length) {
    if (length == 0) return true;
    return file.read(reinterpret_cast<uint8_t*>(out), length) == length;
}

bool validPackedFrame(const PackedSpriteFrame& frame, const PackedSpriteHeader& header) {
    if (frame.width == 0 || frame.height == 0 || frame.length == 0) return false;
    if (frame.format != static_cast<uint8_t>(SpriteFormat::RGB565_RLE) &&
        frame.format != static_cast<uint8_t>(SpriteFormat::INDEXED4_RLE)) {
        return false;
    }
    if (frame.offset > header.rleWords || frame.length > header.rleWords - frame.offset) return false;
    if (frame.format == static_cast<uint8_t>(SpriteFormat::INDEXED4_RLE)) {
        if (frame.paletteSize == 0 || frame.paletteSize > 16) return false;
        if (frame.paletteOffset > header.paletteWords ||
            frame.paletteSize > header.paletteWords - frame.paletteOffset) {
            return false;
        }
    }
    return true;
}

bool loadSpeciesFromResourcePack(uint8_t slot, uint16_t speciesId) {
    ResourcePack& pack = ResourcePack::ins();
    if (!pack.begin()) return false;

    fs::File file;
    if (!pack.openSpriteBlock(speciesId, file)) return false;

    PackedSpriteHeader header = {};
    if (!readExact(file, &header, sizeof(header))) return false;
    if (header.magic != SPRITE_PACK_MAGIC ||
        header.version != SPRITE_PACK_VERSION ||
        header.speciesId != speciesId ||
        header.frameCount == 0 ||
        header.frameCount > MAX_PACK_FRAMES ||
        header.rleWords == 0) {
        return false;
    }

    uint32_t frameBytes = static_cast<uint32_t>(header.frameCount) * sizeof(SpriteFrame);
    uint32_t decodedBytes = (static_cast<uint32_t>(header.rleWords) + header.paletteWords) * sizeof(uint16_t);
    SpriteFrame* frames = psramFound()
        ? static_cast<SpriteFrame*>(ps_malloc(frameBytes))
        : static_cast<SpriteFrame*>(malloc(frameBytes));
    uint16_t* decoded = psramFound()
        ? static_cast<uint16_t*>(ps_malloc(decodedBytes))
        : static_cast<uint16_t*>(malloc(decodedBytes));
    if (!frames || !decoded) {
        if (frames) free(frames);
        if (decoded) free(decoded);
        return false;
    }

    for (uint16_t i = 0; i < header.frameCount; ++i) {
        PackedSpriteFrame packed = {};
        if (!readExact(file, &packed, sizeof(packed)) || !validPackedFrame(packed, header)) {
            free(frames);
            free(decoded);
            return false;
        }
        frames[i] = SpriteFrame{
            speciesId,
            packed.kind,
            packed.width,
            packed.height,
            packed.format,
            packed.paletteSize,
            SPRITE_SOURCE_FILE_BLOCK,
            0,
            packed.offset,
            packed.length,
            packed.paletteOffset,
        };
    }

    if (!readExact(file, decoded, decodedBytes)) {
        free(frames);
        free(decoded);
        return false;
    }

    CachedSpecies& entry = gCache[slot];
    bool replacing = entry.data != nullptr;
    releaseEntry(entry);
    entry.speciesId = speciesId;
    entry.rleWords = header.rleWords;
    entry.paletteWords = header.paletteWords;
    entry.frameCount = header.frameCount;
    entry.frames = frames;
    entry.data = decoded;
    entry.palettes = decoded + header.rleWords;

    if (!replacing && gStats.cachedSpecies < 0xFF) ++gStats.cachedSpecies;
    gStats.decodedBytes += decodedBytes;
    gStats.compressedBytes += file.size();
    Serial.printf("[PokemonSprites] source=littlefs species=%u frames=%u bytes=%u\n",
                  speciesId, header.frameCount, static_cast<unsigned>(file.size()));
    return true;
}

bool loadSpeciesIntoCache(uint8_t slot, uint16_t speciesId) {
    if (slot >= CACHE_CAP || speciesId == 0) return false;
    if (loadSpeciesFromResourcePack(slot, speciesId)) {
        if (slot < TEAM_CACHE_CAP) gTeamMissing[slot] = 0;
        refreshMissingStats();
        return true;
    }
    noteMissingSpecies(speciesId);
    if (rememberMissingFile(speciesId)) {
        Serial.printf("[PokemonSprites] missing species=%u\n", speciesId);
    }
    return false;
}

uint8_t dynamicCacheSlot() {
    for (uint8_t i = TEAM_CACHE_CAP; i < CACHE_CAP; ++i) {
        if (gCache[i].speciesId == 0) return i;
    }
    if (gDynamicSlot < TEAM_CACHE_CAP || gDynamicSlot >= CACHE_CAP) gDynamicSlot = TEAM_CACHE_CAP;
    uint8_t slot = gDynamicSlot++;
    if (gDynamicSlot >= CACHE_CAP) gDynamicSlot = TEAM_CACHE_CAP;
    return slot;
}

CachedSpecies* ensureSpeciesLoaded(uint16_t speciesId) {
    CachedSpecies* cached = cachedSpeciesFor(speciesId);
    if (cached) return cached;
    if (knownMissingFile(speciesId)) {
        noteMissingSpecies(speciesId);
        return nullptr;
    }

    uint8_t slot = dynamicCacheSlot();
    if (!loadSpeciesIntoCache(slot, speciesId)) return nullptr;
    return cachedSpeciesFor(speciesId);
}

const SpriteFrame* findFrame(CachedSpecies* cached, SpriteKind kind) {
    if (!cached || !cached->frames) return nullptr;
    uint16_t wantedKind = static_cast<uint16_t>(kind);
    for (uint16_t i = 0; i < cached->frameCount; ++i) {
        if (cached->frames[i].kind == wantedKind) return &cached->frames[i];
    }
    return nullptr;
}
}

const SpriteFrame* findSpeciesSprite(uint16_t speciesId, SpriteKind kind) {
    CachedSpecies* cached = ensureSpeciesLoaded(speciesId);
    const SpriteFrame* frame = findFrame(cached, kind);
    if (frame) return frame;
    if (!cached || speciesId == 0) return nullptr;
    noteMissingSpecies(speciesId);
    uint16_t kindValue = static_cast<uint16_t>(kind);
    if (rememberMissingFrame(speciesId, kindValue)) {
        Serial.printf("[PokemonSprites] missing frame species=%u kind=%u\n",
                      speciesId, static_cast<unsigned>(kindValue));
    }
    return nullptr;
}

void syncTeamCache(const uint16_t* speciesIds, uint8_t count) {
    if (!speciesIds) count = 0;
    if (count > TEAM_CACHE_CAP) count = TEAM_CACHE_CAP;

    uint16_t next[CACHE_CAP] = {};
    for (uint8_t i = 0; i < count; ++i) next[i] = speciesIds[i];
    if (count == gTeamCount) {
        bool same = true;
        for (uint8_t i = 0; i < CACHE_CAP; ++i) {
            if (next[i] != gTeamSignature[i]) {
                same = false;
                break;
            }
        }
        if (same) return;
    }

    uint32_t start = millis();
    freeCache();
    gTeamCount = count;
    for (uint8_t i = 0; i < CACHE_CAP; ++i) gTeamSignature[i] = next[i];
    for (uint8_t i = 0; i < count; ++i) {
        if (next[i] == 0) continue;
        if (!loadSpeciesIntoCache(i, next[i])) {
            Serial.printf("[PokemonSprites] cache miss species=%u\n", next[i]);
        }
    }
    gStats.reloadCount++;
    gStats.lastReloadMs = millis() - start;
    gStats.freePsram = ESP.getFreePsram();
    gStats.psram = psramFound();
    Serial.printf(
        "[PokemonSprites] cache reload species=%u,%u cached=%u missing=%u decoded=%u compressed=%u ms=%u psram=%u free=%u\n",
        gTeamSignature[0], gTeamSignature[1], gStats.cachedSpecies, gStats.missingSpecies,
        gStats.decodedBytes, gStats.compressedBytes, gStats.lastReloadMs,
        gStats.psram ? 1 : 0, gStats.freePsram);
}

void beginRenderFrame() {
    memset(gFrameMissing, 0, sizeof(gFrameMissing));
    refreshMissingStats();
}

const SpriteCacheStats& cacheStats() {
    return gStats;
}

bool drawFrame(const SpriteFrame* frame, int x, int y, bool flipX) {
    if (!frame) return false;

    uint8_t width = pgm_read_byte(&frame->width);
    uint8_t height = pgm_read_byte(&frame->height);
    uint32_t offset = pgm_read_dword(&frame->offset);
    uint32_t length = pgm_read_dword(&frame->length);
    if (width == 0 || height == 0 || length == 0) return false;

    uint8_t format = pgm_read_byte(&frame->format);
    uint8_t source = pgm_read_byte(&frame->source);
    if (source != SPRITE_SOURCE_FILE_BLOCK) return false;

    uint16_t speciesId = pgm_read_word(&frame->speciesId);
    CachedSpecies* cached = cachedSpeciesFor(speciesId);
    if (!cached || offset + length > cached->rleWords) return false;
    const uint16_t* rle = cached->data;
    const uint16_t* palettes = cached->palettes;

    if (format == static_cast<uint8_t>(SpriteFormat::INDEXED4_RLE)) {
        uint32_t paletteOffset = pgm_read_dword(&frame->paletteOffset);
        uint8_t paletteSize = pgm_read_byte(&frame->paletteSize);
        if (paletteOffset + paletteSize > cached->paletteWords) return false;
        PixelRenderer::drawIndexed4Rle(
            x, y, width, height, rle, offset, length, palettes,
            paletteOffset,
            paletteSize,
            flipX);
        return true;
    }

    PixelRenderer::drawRgb565Rle(x, y, width, height, rle, offset, length, flipX);
    return true;
}

bool drawFrameScaled(const SpriteFrame* frame, int x, int y, float scale, bool flipX) {
    if (!frame) return false;
    if (scale == 1.0f) return drawFrame(frame, x, y, flipX);

    uint8_t width = pgm_read_byte(&frame->width);
    uint8_t height = pgm_read_byte(&frame->height);
    uint32_t offset = pgm_read_dword(&frame->offset);
    uint32_t length = pgm_read_dword(&frame->length);
    if (width == 0 || height == 0 || length == 0 || scale <= 0.0f) return false;

    uint8_t format = pgm_read_byte(&frame->format);
    uint8_t source = pgm_read_byte(&frame->source);
    if (source != SPRITE_SOURCE_FILE_BLOCK) return false;

    uint16_t speciesId = pgm_read_word(&frame->speciesId);
    CachedSpecies* cached = cachedSpeciesFor(speciesId);
    if (!cached || offset + length > cached->rleWords) return false;
    const uint16_t* rle = cached->data;
    const uint16_t* palettes = cached->palettes;

    if (format == static_cast<uint8_t>(SpriteFormat::INDEXED4_RLE)) {
        uint32_t paletteOffset = pgm_read_dword(&frame->paletteOffset);
        uint8_t paletteSize = pgm_read_byte(&frame->paletteSize);
        if (paletteOffset + paletteSize > cached->paletteWords) return false;
        PixelRenderer::drawIndexed4RleScaled(
            x, y, width, height, rle, offset, length, palettes,
            paletteOffset,
            paletteSize,
            scale,
            flipX);
        return true;
    }

    PixelRenderer::drawRgb565RleScaled(x, y, width, height, rle, offset, length, scale, flipX);
    return true;
}

}  // namespace PokemonSprites
