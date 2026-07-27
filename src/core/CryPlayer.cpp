#include "core/CryPlayer.h"

#include "core/DeflateDecoder.h"
#include "core/ResourcePack.h"
#include "hardware/Hal.h"

#include <Arduino.h>
#include <cstdlib>

namespace {
constexpr uint32_t CRY_PACK_MAGIC = 0x52434D53UL;  // SMCR
constexpr uint16_t CRY_PACK_VERSION = 1;
constexpr uint16_t CRY_PACK_FLAG_RAW_DEFLATE = 1 << 0;
constexpr uint16_t CRY_PACK_FLAG_PCM_U8_MONO = 1 << 1;
constexpr uint16_t CRY_PACK_FLAGS =
    CRY_PACK_FLAG_RAW_DEFLATE | CRY_PACK_FLAG_PCM_U8_MONO;
constexpr uint32_t CRY_SAMPLE_RATE = 22050;
constexpr uint32_t MAX_CRY_PCM_BYTES = CRY_SAMPLE_RATE * 4U;

struct __attribute__((packed)) PackedCryHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t speciesId;
    uint32_t sampleRate;
    uint32_t sampleCount;
    uint32_t payloadRawBytes;
    uint32_t payloadCompressedBytes;
    uint32_t payloadCrc32;
    uint16_t flags;
    uint16_t reserved;
};

static_assert(sizeof(PackedCryHeader) == 32, "Unexpected cry pack header layout");

bool readExact(fs::File& file, void* output, size_t length) {
    return length == 0 ||
           file.read(reinterpret_cast<uint8_t*>(output), length) == length;
}
}  // namespace

CryPlayer& CryPlayer::ins() {
    static CryPlayer instance;
    return instance;
}

bool CryPlayer::play(uint16_t speciesId) {
    update();
    if (speciesId == 0 || Hal::ins().getAudioVolume() == 0) return false;
    if (pcm_ || Hal::ins().audioPlaying()) return false;

    ResourcePack& pack = ResourcePack::ins();
    if (!pack.begin()) return false;
    fs::File file;
    if (!pack.openCry(speciesId, file)) {
        Serial.printf("[CryPlayer] missing species=%u\n", speciesId);
        return false;
    }

    PackedCryHeader header{};
    if (!readExact(file, &header, sizeof(header)) ||
        header.magic != CRY_PACK_MAGIC ||
        header.version != CRY_PACK_VERSION ||
        header.speciesId != speciesId ||
        header.sampleRate != CRY_SAMPLE_RATE ||
        header.sampleCount != header.payloadRawBytes ||
        header.payloadRawBytes == 0 ||
        header.payloadRawBytes > MAX_CRY_PCM_BYTES ||
        header.payloadCompressedBytes == 0 ||
        header.flags != CRY_PACK_FLAGS ||
        header.reserved != 0 ||
        static_cast<uint64_t>(sizeof(header)) + header.payloadCompressedBytes !=
            file.size()) {
        Serial.printf("[CryPlayer] invalid species=%u bytes=%u\n",
                      speciesId, static_cast<unsigned>(file.size()));
        return false;
    }

    uint8_t* pcm = psramFound()
        ? static_cast<uint8_t*>(ps_malloc(header.payloadRawBytes))
        : static_cast<uint8_t*>(malloc(header.payloadRawBytes));
    if (!pcm) {
        Serial.printf("[CryPlayer] allocation failed species=%u bytes=%u\n",
                      speciesId, header.payloadRawBytes);
        return false;
    }

    DeflateDecoder::Stats stats{};
    if (!DeflateDecoder::inflateFile(file,
                                     header.payloadCompressedBytes,
                                     pcm,
                                     header.payloadRawBytes,
                                     header.payloadCrc32,
                                     &stats)) {
        free(pcm);
        Serial.printf("[CryPlayer] decode failed species=%u read=%u inflate=%u\n",
                      speciesId, stats.readMs, stats.inflateMs);
        return false;
    }

    pcm_ = pcm;
    pcmBytes_ = header.payloadRawBytes;
    speciesId_ = speciesId;
    if (!Hal::ins().playPcmU8(pcm_, pcmBytes_, header.sampleRate)) {
        releaseBuffer();
        Serial.printf("[CryPlayer] playback failed species=%u\n", speciesId);
        return false;
    }

    Serial.printf(
        "[CryPlayer] play species=%u compressed=%u decoded=%u read=%u inflate=%u\n",
        speciesId, header.payloadCompressedBytes, header.payloadRawBytes,
        stats.readMs, stats.inflateMs);
    return true;
}

bool CryPlayer::replay(uint16_t speciesId) {
    if (speciesId == 0 || Hal::ins().getAudioVolume() == 0) return false;
    if (pcm_ && speciesId_ == speciesId) {
        return Hal::ins().playPcmU8(pcm_, pcmBytes_, CRY_SAMPLE_RATE);
    }
    return play(speciesId);
}

void CryPlayer::update() {
    if (pcm_ && !Hal::ins().audioPlaying()) releaseBuffer();
}

void CryPlayer::stop() {
    if (pcm_) Hal::ins().stopAudio();
}

void CryPlayer::releaseBuffer() {
    if (pcm_) free(pcm_);
    pcm_ = nullptr;
    pcmBytes_ = 0;
    speciesId_ = 0;
}
