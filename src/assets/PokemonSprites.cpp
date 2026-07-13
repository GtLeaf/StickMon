#include "assets/PokemonSprites.h"
#include "core/DeflateDecoder.h"
#include "core/ResourcePack.h"
#include <Arduino.h>
#include <FS.h>
#include <cstdlib>
#include <cstring>
#include "hardware/PixelRenderer.h"

namespace PokemonSprites {

namespace {

struct WalkingConfig {
    uint16_t speciesId;
    SpriteKind downBase;
    SpriteKind leftBase;
    SpriteKind upBase;
    SpriteKind rightBase;
    uint8_t frameCount;
    bool rightFlipX;
};

static constexpr WalkingConfig WALKING_CONFIGS[] = {
    {1, SpriteKind::BULBASAUR_WALKING_FRONT_0, SpriteKind::BULBASAUR_WALKING_LEFT_0, SpriteKind::BULBASAUR_WALKING_BACK_0, SpriteKind::BULBASAUR_WALKING_LEFT_0, 3, true},
    {2, SpriteKind::IVYSAUR_WALKING_FRONT_0, SpriteKind::IVYSAUR_WALKING_LEFT_0, SpriteKind::IVYSAUR_WALKING_BACK_0, SpriteKind::IVYSAUR_WALKING_LEFT_0, 3, true},
    {3, SpriteKind::VENUSAUR_WALKING_FRONT_0, SpriteKind::VENUSAUR_WALKING_LEFT_0, SpriteKind::VENUSAUR_WALKING_BACK_0, SpriteKind::VENUSAUR_WALKING_LEFT_0, 3, true},
    {4, SpriteKind::CHARMANDER_WALKING_FRONT_0, SpriteKind::CHARMANDER_WALKING_LEFT_0, SpriteKind::CHARMANDER_WALKING_BACK_0, SpriteKind::CHARMANDER_WALKING_LEFT_0, 3, true},
    {5, SpriteKind::CHARMELEON_WALKING_FRONT_0, SpriteKind::CHARMELEON_WALKING_LEFT_0, SpriteKind::CHARMELEON_WALKING_BACK_0, SpriteKind::CHARMELEON_WALKING_LEFT_0, 3, true},
    {6, SpriteKind::CHARIZARD_WALKING_FRONT_0, SpriteKind::CHARIZARD_WALKING_LEFT_0, SpriteKind::CHARIZARD_WALKING_BACK_0, SpriteKind::CHARIZARD_WALKING_LEFT_0, 3, true},
    {7, SpriteKind::SQUIRTLE_WALKING_FRONT_0, SpriteKind::SQUIRTLE_WALKING_LEFT_0, SpriteKind::SQUIRTLE_WALKING_BACK_0, SpriteKind::SQUIRTLE_WALKING_LEFT_0, 3, true},
    {8, SpriteKind::WARTORTLE_WALKING_FRONT_0, SpriteKind::WARTORTLE_WALKING_LEFT_0, SpriteKind::WARTORTLE_WALKING_BACK_0, SpriteKind::WARTORTLE_WALKING_RIGHT_0, 3, false},
    {9, SpriteKind::BLASTOISE_WALKING_FRONT_0, SpriteKind::BLASTOISE_WALKING_LEFT_0, SpriteKind::BLASTOISE_WALKING_BACK_0, SpriteKind::BLASTOISE_WALKING_LEFT_0, 3, true},
    {10, SpriteKind::CATERPIE_WALKING_FRONT_0, SpriteKind::CATERPIE_WALKING_LEFT_0, SpriteKind::CATERPIE_WALKING_BACK_0, SpriteKind::CATERPIE_WALKING_LEFT_0, 3, true},
    {11, SpriteKind::METAPOD_WALKING_FRONT_0, SpriteKind::METAPOD_WALKING_LEFT_0, SpriteKind::METAPOD_WALKING_BACK_0, SpriteKind::METAPOD_WALKING_LEFT_0, 3, true},
    {12, SpriteKind::BUTTERFREE_WALKING_FRONT_0, SpriteKind::BUTTERFREE_WALKING_LEFT_0, SpriteKind::BUTTERFREE_WALKING_BACK_0, SpriteKind::BUTTERFREE_WALKING_LEFT_0, 3, true},
    {16, SpriteKind::PIDGEY_WALKING_FRONT_0, SpriteKind::PIDGEY_WALKING_LEFT_0, SpriteKind::PIDGEY_WALKING_BACK_0, SpriteKind::PIDGEY_WALKING_LEFT_0, 3, true},
    {17, SpriteKind::PIDGEOTTO_WALKING_FRONT_0, SpriteKind::PIDGEOTTO_WALKING_LEFT_0, SpriteKind::PIDGEOTTO_WALKING_BACK_0, SpriteKind::PIDGEOTTO_WALKING_LEFT_0, 3, true},
    {18, SpriteKind::PIDGEOT_WALKING_FRONT_0, SpriteKind::PIDGEOT_WALKING_LEFT_0, SpriteKind::PIDGEOT_WALKING_BACK_0, SpriteKind::PIDGEOT_WALKING_LEFT_0, 3, true},
    {25, SpriteKind::PIKACHU_WALKING_FRONT_0, SpriteKind::PIKACHU_WALKING_LEFT_0, SpriteKind::PIKACHU_WALKING_BACK_0, SpriteKind::PIKACHU_WALKING_LEFT_0, 3, true},
    {26, SpriteKind::RAICHU_WALKING_FRONT_0, SpriteKind::RAICHU_WALKING_LEFT_0, SpriteKind::RAICHU_WALKING_BACK_0, SpriteKind::RAICHU_WALKING_RIGHT_0, 3, false},
    {74, SpriteKind::GEODUDE_WALKING_FRONT_0, SpriteKind::GEODUDE_WALKING_LEFT_0, SpriteKind::GEODUDE_WALKING_BACK_0, SpriteKind::GEODUDE_WALKING_LEFT_0, 3, true},
    {75, SpriteKind::GRAVELER_WALKING_FRONT_0, SpriteKind::GRAVELER_WALKING_LEFT_0, SpriteKind::GRAVELER_WALKING_BACK_0, SpriteKind::GRAVELER_WALKING_LEFT_0, 3, true},
    {76, SpriteKind::GOLEM_WALKING_FRONT_0, SpriteKind::GOLEM_WALKING_LEFT_0, SpriteKind::GOLEM_WALKING_BACK_0, SpriteKind::GOLEM_WALKING_LEFT_0, 3, true},
    {92, SpriteKind::GASTLY_WALKING_FRONT_0, SpriteKind::GASTLY_WALKING_LEFT_0, SpriteKind::GASTLY_WALKING_BACK_0, SpriteKind::GASTLY_WALKING_LEFT_0, 3, true},
    {93, SpriteKind::HAUNTER_WALKING_FRONT_0, SpriteKind::HAUNTER_WALKING_LEFT_0, SpriteKind::HAUNTER_WALKING_BACK_0, SpriteKind::HAUNTER_WALKING_LEFT_0, 3, true},
    {94, SpriteKind::GENGAR_WALKING_FRONT_0, SpriteKind::GENGAR_WALKING_LEFT_0, SpriteKind::GENGAR_WALKING_BACK_0, SpriteKind::GENGAR_WALKING_LEFT_0, 3, true},
    {123, SpriteKind::SCYTHER_WALKING_FRONT_0, SpriteKind::SCYTHER_WALKING_LEFT_0, SpriteKind::SCYTHER_WALKING_BACK_0, SpriteKind::SCYTHER_WALKING_LEFT_0, 3, true},
    {129, SpriteKind::MAGIKARP_WALKING_FRONT_0, SpriteKind::MAGIKARP_WALKING_LEFT_0, SpriteKind::MAGIKARP_WALKING_BACK_0, SpriteKind::MAGIKARP_WALKING_RIGHT_0, 1, false},
    {130, SpriteKind::GYARADOS_WALKING_FRONT_0, SpriteKind::GYARADOS_WALKING_LEFT_0, SpriteKind::GYARADOS_WALKING_BACK_0, SpriteKind::GYARADOS_WALKING_RIGHT_0, 3, false},
    {133, SpriteKind::EEVEE_WALKING_FRONT_0, SpriteKind::EEVEE_WALKING_LEFT_0, SpriteKind::EEVEE_WALKING_BACK_0, SpriteKind::EEVEE_WALKING_RIGHT_0, 3, false},
    {134, SpriteKind::VAPOREON_WALKING_FRONT_0, SpriteKind::VAPOREON_WALKING_LEFT_0, SpriteKind::VAPOREON_WALKING_BACK_0, SpriteKind::VAPOREON_WALKING_RIGHT_0, 4, false},
    {135, SpriteKind::JOLTEON_WALKING_FRONT_0, SpriteKind::JOLTEON_WALKING_LEFT_0, SpriteKind::JOLTEON_WALKING_BACK_0, SpriteKind::JOLTEON_WALKING_RIGHT_0, 4, false},
    {136, SpriteKind::FLAREON_WALKING_FRONT_0, SpriteKind::FLAREON_WALKING_LEFT_0, SpriteKind::FLAREON_WALKING_BACK_0, SpriteKind::FLAREON_WALKING_RIGHT_0, 4, false},
    {196, SpriteKind::ESPEON_WALKING_FRONT_0, SpriteKind::ESPEON_WALKING_LEFT_0, SpriteKind::ESPEON_WALKING_BACK_0, SpriteKind::ESPEON_WALKING_LEFT_0, 3, true},
    {197, SpriteKind::UMBREON_WALKING_FRONT_0, SpriteKind::UMBREON_WALKING_LEFT_0, SpriteKind::UMBREON_WALKING_BACK_0, SpriteKind::UMBREON_WALKING_LEFT_0, 3, true},
    {143, SpriteKind::SNORLAX_WALKING_FRONT_0, SpriteKind::SNORLAX_WALKING_LEFT_0, SpriteKind::SNORLAX_WALKING_BACK_0, SpriteKind::SNORLAX_WALKING_LEFT_0, 3, true},
    {147, SpriteKind::DRATINI_WALKING_FRONT_0, SpriteKind::DRATINI_WALKING_LEFT_0, SpriteKind::DRATINI_WALKING_BACK_0, SpriteKind::DRATINI_WALKING_RIGHT_0, 3, false},
    {148, SpriteKind::DRAGONAIR_WALKING_FRONT_0, SpriteKind::DRAGONAIR_WALKING_LEFT_0, SpriteKind::DRAGONAIR_WALKING_BACK_0, SpriteKind::DRAGONAIR_WALKING_RIGHT_0, 3, false},
    {149, SpriteKind::DRAGONITE_WALKING_FRONT_0, SpriteKind::DRAGONITE_WALKING_LEFT_0, SpriteKind::DRAGONITE_WALKING_BACK_0, SpriteKind::DRAGONITE_WALKING_RIGHT_0, 2, false},
    {151, SpriteKind::MEW_WALKING_FRONT_0, SpriteKind::MEW_WALKING_LEFT_0, SpriteKind::MEW_WALKING_BACK_0, SpriteKind::MEW_WALKING_RIGHT_0, 2, false},
    {161, SpriteKind::SENTRET_WALKING_FRONT_0, SpriteKind::SENTRET_WALKING_LEFT_0, SpriteKind::SENTRET_WALKING_BACK_0, SpriteKind::SENTRET_WALKING_LEFT_0, 3, true},
    {162, SpriteKind::FURRET_WALKING_FRONT_0, SpriteKind::FURRET_WALKING_LEFT_0, SpriteKind::FURRET_WALKING_BACK_0, SpriteKind::FURRET_WALKING_LEFT_0, 3, true},
    {172, SpriteKind::PICHU_WALKING_FRONT_0, SpriteKind::PICHU_WALKING_LEFT_0, SpriteKind::PICHU_WALKING_BACK_0, SpriteKind::PICHU_WALKING_LEFT_0, 3, true},
    {212, SpriteKind::SCIZOR_WALKING_FRONT_0, SpriteKind::SCIZOR_WALKING_LEFT_0, SpriteKind::SCIZOR_WALKING_BACK_0, SpriteKind::SCIZOR_WALKING_LEFT_0, 3, true},
    {261, SpriteKind::POOCHYENA_WALKING_FRONT_0, SpriteKind::POOCHYENA_WALKING_LEFT_0, SpriteKind::POOCHYENA_WALKING_BACK_0, SpriteKind::POOCHYENA_WALKING_LEFT_0, 3, true},
    {262, SpriteKind::MIGHTYENA_WALKING_FRONT_0, SpriteKind::MIGHTYENA_WALKING_LEFT_0, SpriteKind::MIGHTYENA_WALKING_BACK_0, SpriteKind::MIGHTYENA_WALKING_LEFT_0, 3, true},
    {278, SpriteKind::WINGULL_WALKING_FRONT_0, SpriteKind::WINGULL_WALKING_LEFT_0, SpriteKind::WINGULL_WALKING_BACK_0, SpriteKind::WINGULL_WALKING_LEFT_0, 3, true},
    {279, SpriteKind::PELIPPER_WALKING_FRONT_0, SpriteKind::PELIPPER_WALKING_LEFT_0, SpriteKind::PELIPPER_WALKING_BACK_0, SpriteKind::PELIPPER_WALKING_LEFT_0, 3, true},
    {380, SpriteKind::LATIAS_WALKING_FRONT_0, SpriteKind::LATIAS_WALKING_LEFT_0, SpriteKind::LATIAS_WALKING_BACK_0, SpriteKind::LATIAS_WALKING_LEFT_0, 2, true},
    {381, SpriteKind::LATIOS_WALKING_FRONT_0, SpriteKind::LATIOS_WALKING_LEFT_0, SpriteKind::LATIOS_WALKING_BACK_0, SpriteKind::LATIOS_WALKING_LEFT_0, 2, true},
};

static constexpr uint8_t SPRITE_SOURCE_FILE_BLOCK = 2;
static constexpr uint8_t TEAM_CACHE_CAP = 2;
static constexpr uint8_t CACHE_CAP = 4;
static constexpr uint8_t KNOWN_MISSING_FILE_CAP = 16;
static constexpr uint8_t FRAME_MISSING_CAP = 4;
static constexpr uint8_t KNOWN_MISSING_FRAME_CAP = 16;
static constexpr uint32_t SPRITE_PACK_MAGIC = 0x5350534D;
static constexpr uint16_t SPRITE_PACK_VERSION = 2;
static constexpr uint16_t SPRITE_PACK_FLAG_RAW_DEFLATE = 1;
static constexpr uint16_t MAX_PACK_FRAMES = 256;
static constexpr uint32_t MAX_PACK_PAYLOAD_BYTES = 128000;

struct __attribute__((packed)) PackedSpriteHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t speciesId;
    uint16_t frameCount;
    uint16_t rleWords;
    uint16_t paletteWords;
    uint16_t flags;
    uint32_t payloadRawBytes;
    uint32_t payloadCompressedBytes;
    uint32_t payloadCrc32;
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

static_assert(sizeof(PackedSpriteHeader) == 28, "Unexpected sprite pack header layout");
static_assert(sizeof(PackedSpriteFrame) == 20, "Unexpected sprite pack frame layout");

struct CachedSpecies {
    uint16_t speciesId = 0;
    uint16_t rleWords = 0;
    uint16_t paletteWords = 0;
    uint16_t frameCount = 0;
    SpriteFrame* frames = nullptr;
    uint8_t* payload = nullptr;
    uint32_t payloadBytes = 0;
    uint32_t packedBytes = 0;
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
bool gDynamicLoadingEnabled = true;

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
    if (entry.payload) {
        uint32_t decodedBytes = entry.payloadBytes +
                                static_cast<uint32_t>(entry.frameCount) * sizeof(SpriteFrame);
        gStats.decodedBytes = gStats.decodedBytes >= decodedBytes
            ? gStats.decodedBytes - decodedBytes : 0;
        gStats.compressedBytes = gStats.compressedBytes >= entry.packedBytes
            ? gStats.compressedBytes - entry.packedBytes : 0;
        if (gStats.cachedSpecies > 0) --gStats.cachedSpecies;
        free(entry.payload);
    }
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
        header.rleWords == 0 ||
        header.flags != SPRITE_PACK_FLAG_RAW_DEFLATE ||
        header.payloadRawBytes == 0 ||
        header.payloadRawBytes > MAX_PACK_PAYLOAD_BYTES ||
        header.payloadCompressedBytes == 0) {
        return false;
    }

    uint32_t frameBytes = static_cast<uint32_t>(header.frameCount) * sizeof(SpriteFrame);
    uint32_t packedFrameBytes = static_cast<uint32_t>(header.frameCount) * sizeof(PackedSpriteFrame);
    uint32_t expectedPayloadBytes = packedFrameBytes +
        (static_cast<uint32_t>(header.rleWords) + header.paletteWords) * sizeof(uint16_t);
    uint32_t fileBytes = static_cast<uint32_t>(file.size());
    if (expectedPayloadBytes != header.payloadRawBytes ||
        static_cast<uint64_t>(sizeof(PackedSpriteHeader)) + header.payloadCompressedBytes != file.size()) {
        return false;
    }

    SpriteFrame* frames = psramFound()
        ? static_cast<SpriteFrame*>(ps_malloc(frameBytes))
        : static_cast<SpriteFrame*>(malloc(frameBytes));
    uint8_t* payload = psramFound()
        ? static_cast<uint8_t*>(ps_malloc(header.payloadRawBytes))
        : static_cast<uint8_t*>(malloc(header.payloadRawBytes));
    if (!frames || !payload) {
        if (frames) free(frames);
        if (payload) free(payload);
        return false;
    }

    DeflateDecoder::Stats decodeStats{};
    if (!DeflateDecoder::inflateFile(file,
                                     header.payloadCompressedBytes,
                                     payload,
                                     header.payloadRawBytes,
                                     header.payloadCrc32,
                                     &decodeStats)) {
        free(frames);
        free(payload);
        Serial.printf(
            "[PokemonSprites] decode failed species=%u read=%u inflate=%u total=%u\n",
            speciesId, decodeStats.readMs, decodeStats.inflateMs, decodeStats.totalMs);
        return false;
    }

    const auto* packedFrames = reinterpret_cast<const PackedSpriteFrame*>(payload);
    for (uint16_t i = 0; i < header.frameCount; ++i) {
        const PackedSpriteFrame& packed = packedFrames[i];
        if (!validPackedFrame(packed, header)) {
            free(frames);
            free(payload);
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

    uint16_t* decoded = reinterpret_cast<uint16_t*>(payload + packedFrameBytes);

    CachedSpecies& entry = gCache[slot];
    releaseEntry(entry);
    entry.speciesId = speciesId;
    entry.rleWords = header.rleWords;
    entry.paletteWords = header.paletteWords;
    entry.frameCount = header.frameCount;
    entry.frames = frames;
    entry.payload = payload;
    entry.payloadBytes = header.payloadRawBytes;
    entry.packedBytes = fileBytes;
    entry.data = decoded;
    entry.palettes = decoded + header.rleWords;

    if (gStats.cachedSpecies < 0xFF) ++gStats.cachedSpecies;
    gStats.decodedBytes += header.payloadRawBytes + frameBytes;
    gStats.compressedBytes += fileBytes;
    Serial.printf(
        "[PokemonSprites] source=littlefs species=%u frames=%u compressed=%u decoded=%u read=%u inflate=%u total=%u\n",
        speciesId, header.frameCount, fileBytes, header.payloadRawBytes + frameBytes,
        decodeStats.readMs, decodeStats.inflateMs, decodeStats.totalMs);
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
    if (!gDynamicLoadingEnabled) return nullptr;
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


bool walkingAnimation(uint16_t speciesId, WalkDirection direction, WalkingAnimation& animation) {
    for (const auto& config : WALKING_CONFIGS) {
        if (config.speciesId != speciesId) continue;
        animation.frameCount = config.frameCount;
        animation.flipX = false;
        switch (direction) {
        case WalkDirection::DOWN: animation.base = config.downBase; break;
        case WalkDirection::LEFT: animation.base = config.leftBase; break;
        case WalkDirection::UP: animation.base = config.upBase; break;
        case WalkDirection::RIGHT:
            animation.base = config.rightBase;
            animation.flipX = config.rightFlipX;
            break;
        }
        return animation.frameCount > 0;
    }
    return false;
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

bool syncTeamCache(const uint16_t* speciesIds, uint8_t count, uint8_t loadBudget) {
    if (!speciesIds) count = 0;
    if (count > TEAM_CACHE_CAP) count = TEAM_CACHE_CAP;

    uint16_t next[CACHE_CAP] = {};
    for (uint8_t i = 0; i < count; ++i) next[i] = speciesIds[i];
    bool signatureChanged = count != gTeamCount;
    if (count == gTeamCount) {
        for (uint8_t i = 0; i < CACHE_CAP; ++i) {
            if (next[i] != gTeamSignature[i]) {
                signatureChanged = true;
                break;
            }
        }
    }

    uint32_t start = millis();
    if (signatureChanged) {
        freeCache();
        gTeamCount = count;
        for (uint8_t i = 0; i < CACHE_CAP; ++i) gTeamSignature[i] = next[i];
        ++gStats.reloadCount;
    }

    uint8_t loadedThisCall = 0;
    for (uint8_t i = 0; i < count; ++i) {
        if (next[i] == 0 || gCache[i].speciesId == next[i]) continue;
        if (loadedThisCall >= loadBudget) continue;
        if (!loadSpeciesIntoCache(i, next[i])) {
            Serial.printf("[PokemonSprites] cache miss species=%u\n", next[i]);
        }
        ++loadedThisCall;
    }

    bool ready = true;
    for (uint8_t i = 0; i < count; ++i) {
        if (next[i] != 0 && gCache[i].speciesId != next[i]) {
            ready = false;
            break;
        }
    }
    if (!signatureChanged && loadedThisCall == 0) return ready;

    gStats.lastReloadMs = millis() - start;
    gStats.freePsram = ESP.getFreePsram();
    gStats.psram = psramFound();
    Serial.printf(
        "[PokemonSprites] cache sync species=%u,%u loaded=%u ready=%u cached=%u missing=%u decoded=%u compressed=%u ms=%u psram=%u free=%u\n",
        gTeamSignature[0], gTeamSignature[1], loadedThisCall, ready ? 1 : 0,
        gStats.cachedSpecies, gStats.missingSpecies,
        gStats.decodedBytes, gStats.compressedBytes, gStats.lastReloadMs,
        gStats.psram ? 1 : 0, gStats.freePsram);
    return ready;
}

void setDynamicLoadingEnabled(bool enabled) {
    gDynamicLoadingEnabled = enabled;
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
