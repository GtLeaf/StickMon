#include "core/CryPlayer.h"

#include "core/AudioManager.h"
#include "core/DeflateDecoder.h"
#include "core/ImaAdpcm.h"
#include "core/ResourcePack.h"
#include "platform/api/PlatformServices.h"

#include <cstdlib>

namespace {
constexpr uint32_t CRY_PACK_MAGIC = 0x52434D53UL;  // SMCR
constexpr uint16_t CRY_PACK_VERSION = 1;
constexpr uint16_t CRY_ADPCM_VERSION = 2;
constexpr uint16_t CRY_ADPCM_FLAGS = 4;
constexpr size_t CRY_BLOCK_BYTES = 1024;
constexpr size_t CRY_BLOCK_SAMPLES = 2041;
constexpr uint16_t CRY_PACK_FLAG_RAW_DEFLATE = 1 << 0;
constexpr uint16_t CRY_PACK_FLAG_PCM_U8_MONO = 1 << 1;
constexpr uint16_t CRY_PACK_FLAGS =
    CRY_PACK_FLAG_RAW_DEFLATE | CRY_PACK_FLAG_PCM_U8_MONO;
constexpr uint32_t CRY_SAMPLE_RATE = PcmMixer::OUTPUT_SAMPLE_RATE;
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

bool readExact(Platform::ResourceFile& file, void* output, size_t length) {
    if (length == 0) return true;
    if (!output) return false;
    auto* bytes = static_cast<uint8_t*>(output);
    size_t remaining = length;
    while (remaining > 0) {
        size_t received = file.read(bytes, remaining);
        if (received == 0 || received > remaining) return false;
        bytes += received;
        remaining -= received;
    }
    return true;
}

bool decodeAdpcm(Platform::ResourceFile& file, uint8_t* pcm,
                 const PackedCryHeader& header) {
    uint8_t block[CRY_BLOCK_BYTES];
    uint32_t crc = 0xFFFFFFFFU;
    size_t produced = 0;
    for (size_t offset = 0; offset < header.payloadCompressedBytes;
         offset += sizeof(block)) {
        if (!readExact(file, block, sizeof(block))) return false;
        for (uint8_t byte : block) {
            crc ^= byte;
            for (unsigned bit = 0; bit < 8; ++bit)
                crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
        }
        const size_t wanted = std::min<size_t>(CRY_BLOCK_SAMPLES,
                                               header.sampleCount - produced);
        if (ImaAdpcm::decodeImaBlock(block, sizeof(block), pcm + produced,
                                    header.sampleCount - produced, wanted) != wanted)
            return false;
        produced += wanted;
    }
    return produced == header.sampleCount && ~crc == header.payloadCrc32;
}
}  // namespace

CryPlayer& CryPlayer::ins() {
    static CryPlayer instance;
    return instance;
}

bool CryPlayer::play(uint16_t speciesId) {
    update();
    if (speciesId == 0 || Platform::audio().volume() == 0) return false;
    if (pcm_ ||
        Platform::audio().queuedPcm(AudioManager::CRY_CHANNEL) > 0) return false;

    ResourcePack& pack = ResourcePack::ins();
    if (!pack.begin()) return false;
    Platform::ResourceFile file;
    if (!pack.openCry(speciesId, file)) {
        Platform::errorf("[CryPlayer] missing species=%u\n", speciesId);
        return false;
    }

    PackedCryHeader header{};
    if (!readExact(file, &header, sizeof(header))) return false;
    const bool adpcm = header.version == CRY_ADPCM_VERSION;
    const uint32_t expectedBlocks =
        (header.sampleCount + uint64_t(CRY_BLOCK_SAMPLES) - 1) / CRY_BLOCK_SAMPLES;
    if (
        header.magic != CRY_PACK_MAGIC ||
        (!adpcm && header.version != CRY_PACK_VERSION) ||
        header.speciesId != speciesId ||
        header.sampleRate != CRY_SAMPLE_RATE ||
        header.sampleCount != header.payloadRawBytes ||
        header.payloadRawBytes == 0 ||
        header.payloadRawBytes > MAX_CRY_PCM_BYTES ||
        header.payloadCompressedBytes == 0 ||
        header.flags != (adpcm ? CRY_ADPCM_FLAGS : CRY_PACK_FLAGS) ||
        (adpcm && uint64_t(expectedBlocks) * CRY_BLOCK_BYTES != header.payloadCompressedBytes) ||
        header.reserved != 0 ||
        static_cast<uint64_t>(sizeof(header)) + header.payloadCompressedBytes !=
            file.size()) {
        Platform::errorf("[CryPlayer] invalid species=%u bytes=%u\n",
                      speciesId, static_cast<unsigned>(file.size()));
        return false;
    }

    uint8_t* pcm = static_cast<uint8_t*>(
        Platform::memory().allocate(header.payloadRawBytes, true));
    if (!pcm) {
        Platform::errorf("[CryPlayer] allocation failed species=%u bytes=%u\n",
                      speciesId, header.payloadRawBytes);
        return false;
    }

    DeflateDecoder::Stats stats{};
    const bool decoded = adpcm ? decodeAdpcm(file, pcm, header)
        : DeflateDecoder::inflateFile(file,
                                     header.payloadCompressedBytes,
                                     pcm,
                                     header.payloadRawBytes,
                                     header.payloadCrc32,
                                     &stats);
    if (!decoded) {
        Platform::memory().release(pcm);
        Platform::errorf("[CryPlayer] decode failed species=%u read=%u inflate=%u\n",
                      speciesId, stats.readMs, stats.inflateMs);
        return false;
    }

    pcm_ = pcm;
    pcmBytes_ = header.payloadRawBytes;
    speciesId_ = speciesId;
    if (!Platform::audio().playPcmU8Channel(
            pcm_, pcmBytes_, header.sampleRate,
            AudioManager::CRY_CHANNEL, true)) {
        releaseBuffer();
        Platform::errorf("[CryPlayer] playback failed species=%u\n", speciesId);
        return false;
    }

    Platform::logf(
        "[CryPlayer] play species=%u compressed=%u decoded=%u read=%u inflate=%u\n",
        speciesId, header.payloadCompressedBytes, header.payloadRawBytes,
        stats.readMs, stats.inflateMs);
    return true;
}

bool CryPlayer::replay(uint16_t speciesId) {
    if (speciesId == 0 || Platform::audio().volume() == 0) return false;
    if (pcm_ && speciesId_ == speciesId) {
        return Platform::audio().playPcmU8Channel(
            pcm_, pcmBytes_, CRY_SAMPLE_RATE,
            AudioManager::CRY_CHANNEL, true);
    }
    return play(speciesId);
}

void CryPlayer::update() {
    if (pcm_ &&
        Platform::audio().queuedPcm(AudioManager::CRY_CHANNEL) == 0) {
        releaseBuffer();
    }
}

void CryPlayer::stop() {
    if (pcm_) Platform::audio().stopChannel(AudioManager::CRY_CHANNEL);
}

void CryPlayer::releaseBuffer() {
    if (pcm_) Platform::memory().release(pcm_);
    pcm_ = nullptr;
    pcmBytes_ = 0;
    speciesId_ = 0;
}
