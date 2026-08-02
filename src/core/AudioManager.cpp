#include "core/AudioManager.h"

#include "core/ResourcePack.h"
#include "platform/api/PlatformServices.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace {
constexpr uint32_t AUDIO_PACK_MAGIC = 0x55414D53UL;  // SMAU
constexpr uint16_t AUDIO_PACK_VERSION = 2;
constexpr uint16_t AUDIO_FLAG_IMA_ADPCM = 1 << 0;
constexpr uint16_t AUDIO_FLAG_MONO = 1 << 1;
constexpr uint16_t AUDIO_FLAG_LOOP = 1 << 2;
constexpr uint16_t AUDIO_REQUIRED_FLAGS =
    AUDIO_FLAG_IMA_ADPCM | AUDIO_FLAG_MONO;
constexpr uint32_t MIN_SAMPLE_RATE = 8000;
constexpr uint32_t MAX_SAMPLE_RATE = 24000;
constexpr uint16_t MAX_BLOCK_BYTES = 2048;
constexpr uint16_t MAX_BLOCK_SAMPLES = 4096;
constexpr uint32_t MAX_SFX_SAMPLES = 22050U * 5U;

struct __attribute__((packed)) PackedAudioHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t flags;
    uint32_t sampleRate;
    uint32_t sampleCount;
    uint16_t blockBytes;
    uint16_t blockSamples;
    uint32_t blockCount;
    uint32_t loopStartSample;
    uint32_t payloadBytes;
    uint32_t payloadCrc32;
};

static_assert(sizeof(PackedAudioHeader) == 36,
              "Unexpected audio pack header layout");

constexpr int IMA_INDEX_TABLE[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8,
};

constexpr int IMA_STEP_TABLE[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130,
    143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449,
    494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411,
    1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026,
    4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
    27086, 29794, 32767,
};

const char* musicId(MusicTrack track) {
    switch (track) {
    case MusicTrack::HOME: return "bgm_home";
    case MusicTrack::EXPLORE: return "bgm_explore";
    case MusicTrack::BATTLE: return "bgm_battle";
    case MusicTrack::BATTLE_SPECIAL: return "bgm_battle_special";
    case MusicTrack::NONE: return nullptr;
    }
    return nullptr;
}

const char* sfxId(SfxCue cue) {
    switch (cue) {
    case SfxCue::UI_CURSOR: return "sfx_ui_cursor";
    case SfxCue::UI_CONFIRM: return "sfx_ui_confirm";
    case SfxCue::UI_CANCEL: return "sfx_ui_cancel";
    case SfxCue::MENU_OPEN: return "sfx_menu_open";
    case SfxCue::MENU_CLOSE: return "sfx_menu_close";
    case SfxCue::DAMAGE_NORMAL: return "sfx_damage_normal";
    case SfxCue::DAMAGE_SUPER: return "sfx_damage_super";
    case SfxCue::DAMAGE_WEAK: return "sfx_damage_weak";
    case SfxCue::THROW: return "sfx_throw";
    case SfxCue::FAINT: return "sfx_faint";
    case SfxCue::EXP_GAIN: return "sfx_exp_gain";
    case SfxCue::EXP_FULL: return "sfx_exp_full";
    case SfxCue::LEVEL_UP: return "sfx_level_up";
    case SfxCue::MOVE_LEARNT: return "sfx_move_learnt";
    case SfxCue::CONTACT: return "sfx_contact";
    }
    return nullptr;
}

uint8_t pcmU8(int predictor) {
    int sample = (predictor >> 8) + 128;
    return static_cast<uint8_t>(std::max(0, std::min(255, sample)));
}

size_t decodeImaBlock(const uint8_t* input, size_t inputBytes,
                      uint8_t* output, size_t outputCapacity,
                      size_t wantedSamples) {
    if (!input || inputBytes < 4 || !output || outputCapacity == 0 ||
        wantedSamples == 0) {
        return 0;
    }

    int predictor = static_cast<int16_t>(
        static_cast<uint16_t>(input[0]) |
        (static_cast<uint16_t>(input[1]) << 8));
    int index = input[2];
    if (index < 0 || index > 88 || input[3] != 0) return 0;

    size_t produced = 0;
    output[produced++] = pcmU8(predictor);
    for (size_t byteIndex = 4;
         byteIndex < inputBytes && produced < wantedSamples &&
         produced < outputCapacity; ++byteIndex) {
        for (uint8_t half = 0;
             half < 2 && produced < wantedSamples &&
             produced < outputCapacity; ++half) {
            uint8_t code = half == 0
                ? input[byteIndex] & 0x0F
                : input[byteIndex] >> 4;
            int step = IMA_STEP_TABLE[index];
            int delta = step >> 3;
            if (code & 1) delta += step >> 2;
            if (code & 2) delta += step >> 1;
            if (code & 4) delta += step;
            predictor += (code & 8) ? -delta : delta;
            predictor = std::max(-32768, std::min(32767, predictor));
            index += IMA_INDEX_TABLE[code];
            index = std::max(0, std::min(88, index));
            output[produced++] = pcmU8(predictor);
        }
    }
    return produced;
}

bool readExact(Platform::ResourceFile& file, void* output, size_t length) {
    return length == 0 || file.read(output, length) == length;
}
}  // namespace

AudioManager& AudioManager::ins() {
    static AudioManager instance;
    return instance;
}

void AudioManager::setMusic(MusicTrack track) {
    if (requestedMusic_ == track &&
        (track == MusicTrack::NONE || playingMusic_ == track)) {
        return;
    }
    requestedMusic_ = track;
    if (playingMusic_ != track) releaseMusic();
    if (track != MusicTrack::NONE && Platform::audio().volume() > 0) {
        startRequestedMusic();
    }
}

void AudioManager::stopMusic() {
    requestedMusic_ = MusicTrack::NONE;
    releaseMusic();
}

bool AudioManager::openAudio(const char* id, Platform::ResourceFile& file,
                             AudioHeader& header) {
    if (!id || !ResourcePack::ins().begin() ||
        !ResourcePack::ins().openAudio(id, file)) {
        return false;
    }

    PackedAudioHeader packed{};
    if (!readExact(file, &packed, sizeof(packed)) ||
        packed.magic != AUDIO_PACK_MAGIC ||
        packed.version != AUDIO_PACK_VERSION ||
        (packed.flags & AUDIO_REQUIRED_FLAGS) != AUDIO_REQUIRED_FLAGS ||
        (packed.flags & ~(AUDIO_REQUIRED_FLAGS | AUDIO_FLAG_LOOP)) != 0 ||
        packed.sampleRate < MIN_SAMPLE_RATE ||
        packed.sampleRate > MAX_SAMPLE_RATE ||
        packed.sampleCount == 0 || packed.blockBytes < 5 ||
        packed.blockBytes > MAX_BLOCK_BYTES ||
        packed.blockSamples != 1 + (packed.blockBytes - 4) * 2 ||
        packed.blockSamples > MAX_BLOCK_SAMPLES || packed.blockCount == 0 ||
        ((packed.flags & AUDIO_FLAG_LOOP) != 0 &&
         packed.loopStartSample >= packed.sampleCount) ||
        ((packed.flags & AUDIO_FLAG_LOOP) == 0 &&
         packed.loopStartSample != 0) ||
        packed.payloadBytes != packed.blockCount * packed.blockBytes ||
        sizeof(packed) + packed.payloadBytes != file.size()) {
        Platform::logf("[Audio] invalid pack id=%s bytes=%u\n",
                      id, static_cast<unsigned>(file.size()));
        file.close();
        return false;
    }

    header.sampleRate = packed.sampleRate;
    header.sampleCount = packed.sampleCount;
    header.blockBytes = packed.blockBytes;
    header.blockSamples = packed.blockSamples;
    header.blockCount = packed.blockCount;
    header.loopStartSample = packed.loopStartSample;
    header.payloadBytes = packed.payloadBytes;
    header.looping = (packed.flags & AUDIO_FLAG_LOOP) != 0;
    return true;
}

bool AudioManager::startRequestedMusic() {
    const char* id = musicId(requestedMusic_);
    if (!id || playingMusic_ != MusicTrack::NONE ||
        Platform::audio().volume() == 0) {
        return false;
    }

    AudioHeader header{};
    Platform::ResourceFile file;
    if (!openAudio(id, file, header) || !header.looping) return false;

    uint8_t* compressed = static_cast<uint8_t*>(
        Platform::memory().allocate(header.blockBytes, true));
    uint8_t* first = static_cast<uint8_t*>(
        Platform::memory().allocate(header.blockSamples, true));
    uint8_t* second = static_cast<uint8_t*>(
        Platform::memory().allocate(header.blockSamples, true));
    if (!compressed || !first || !second) {
        if (compressed) Platform::memory().release(compressed);
        if (first) Platform::memory().release(first);
        if (second) Platform::memory().release(second);
        Platform::logf("[Audio] music allocation failed id=%s\n", id);
        return false;
    }

    musicFile_ = std::move(file);
    musicHeader_ = header;
    musicCompressed_ = compressed;
    musicPcm_[0] = first;
    musicPcm_[1] = second;
    nextMusicBuffer_ = 0;
    nextMusicBlock_ = 0;
    nextMusicSkipSamples_ = 0;
    playingMusic_ = requestedMusic_;
    if (!queueNextMusicBlock(true) || !queueNextMusicBlock(false)) {
        releaseMusic();
        return false;
    }
    Platform::logf("[Audio] music=%s rate=%u samples=%u blocks=%u\n",
                  id, header.sampleRate, header.sampleCount, header.blockCount);
    return true;
}

bool AudioManager::decodeMusicBlock(uint8_t bufferIndex) {
    if (!musicFile_ || !musicCompressed_ || bufferIndex >= 2 ||
        !musicPcm_[bufferIndex]) {
        return false;
    }
    if (nextMusicBlock_ >= musicHeader_.blockCount) {
        if (!musicHeader_.looping) return false;
        nextMusicBlock_ =
            musicHeader_.loopStartSample / musicHeader_.blockSamples;
        nextMusicSkipSamples_ = static_cast<uint16_t>(
            musicHeader_.loopStartSample % musicHeader_.blockSamples);
    }

    size_t offset = sizeof(PackedAudioHeader) +
        static_cast<size_t>(nextMusicBlock_) * musicHeader_.blockBytes;
    if (!musicFile_.seek(offset) ||
        !readExact(musicFile_, musicCompressed_, musicHeader_.blockBytes)) {
        return false;
    }
    uint32_t blockStart = nextMusicBlock_ * musicHeader_.blockSamples;
    uint32_t samplesRemaining = musicHeader_.sampleCount > blockStart
        ? musicHeader_.sampleCount - blockStart : 0;
    size_t wanted = std::min<uint32_t>(
        musicHeader_.blockSamples, samplesRemaining);
    if (wanted == 0 && musicHeader_.looping) {
        nextMusicBlock_ =
            musicHeader_.loopStartSample / musicHeader_.blockSamples;
        nextMusicSkipSamples_ = static_cast<uint16_t>(
            musicHeader_.loopStartSample % musicHeader_.blockSamples);
        return decodeMusicBlock(bufferIndex);
    }
    size_t decoded = decodeImaBlock(
        musicCompressed_, musicHeader_.blockBytes, musicPcm_[bufferIndex],
        musicHeader_.blockSamples, wanted);
    if (decoded != wanted || nextMusicSkipSamples_ >= decoded) return false;
    if (nextMusicSkipSamples_ > 0) {
        decoded -= nextMusicSkipSamples_;
        std::memmove(
            musicPcm_[bufferIndex],
            musicPcm_[bufferIndex] + nextMusicSkipSamples_, decoded);
        nextMusicSkipSamples_ = 0;
    }
    musicPcmSamples_[bufferIndex] = static_cast<uint16_t>(decoded);
    ++nextMusicBlock_;
    return true;
}

bool AudioManager::queueNextMusicBlock(bool stopCurrent) {
    uint8_t bufferIndex = nextMusicBuffer_;
    if (!decodeMusicBlock(bufferIndex)) return false;
    size_t sampleCount = musicPcmSamples_[bufferIndex];
    if (!Platform::audio().playPcmU8Channel(
            musicPcm_[bufferIndex], sampleCount, musicHeader_.sampleRate,
            MUSIC_CHANNEL, stopCurrent)) {
        return false;
    }
    nextMusicBuffer_ ^= 1;
    return true;
}

bool AudioManager::playSfx(SfxCue cue) {
    if (Platform::audio().volume() == 0) return false;
    const char* id = sfxId(cue);
    AudioHeader header{};
    Platform::ResourceFile file;
    if (!openAudio(id, file, header) || header.looping ||
        header.sampleCount > MAX_SFX_SAMPLES) {
        return false;
    }

    Platform::audio().stopChannel(SFX_CHANNEL);
    releaseSfx();
    uint8_t* compressed = static_cast<uint8_t*>(
        Platform::memory().allocate(header.blockBytes, true));
    uint8_t* pcm = static_cast<uint8_t*>(
        Platform::memory().allocate(header.sampleCount, true));
    if (!compressed || !pcm) {
        if (compressed) Platform::memory().release(compressed);
        if (pcm) Platform::memory().release(pcm);
        return false;
    }

    size_t produced = 0;
    for (uint32_t block = 0; block < header.blockCount; ++block) {
        if (!readExact(file, compressed, header.blockBytes)) break;
        size_t wanted = std::min<size_t>(
            header.blockSamples, header.sampleCount - produced);
        size_t decoded = decodeImaBlock(
            compressed, header.blockBytes, pcm + produced,
            header.sampleCount - produced, wanted);
        if (decoded != wanted) break;
        produced += decoded;
    }
    Platform::memory().release(compressed);
    if (produced != header.sampleCount ||
        !Platform::audio().playPcmU8Channel(
            pcm, produced, header.sampleRate, SFX_CHANNEL, true)) {
        Platform::memory().release(pcm);
        return false;
    }

    sfxPcm_ = pcm;
    sfxPcmBytes_ = produced;
    return true;
}

void AudioManager::update() {
    if (sfxPcm_ && Platform::audio().queuedPcm(SFX_CHANNEL) == 0) {
        releaseSfx();
    }
    if (requestedMusic_ == MusicTrack::NONE ||
        Platform::audio().volume() == 0) {
        if (playingMusic_ != MusicTrack::NONE) releaseMusic();
        return;
    }
    if (playingMusic_ != requestedMusic_) {
        releaseMusic();
        startRequestedMusic();
        return;
    }
    if (playingMusic_ == MusicTrack::NONE) {
        startRequestedMusic();
        return;
    }
    while (Platform::audio().queuedPcm(MUSIC_CHANNEL) < 2) {
        if (!queueNextMusicBlock(false)) {
            Platform::logLine("[Audio] music stream stopped");
            releaseMusic();
            break;
        }
    }
}

void AudioManager::releaseMusic() {
    Platform::audio().stopChannel(MUSIC_CHANNEL);
    musicFile_.reset();
    if (musicCompressed_) Platform::memory().release(musicCompressed_);
    if (musicPcm_[0]) Platform::memory().release(musicPcm_[0]);
    if (musicPcm_[1]) Platform::memory().release(musicPcm_[1]);
    musicCompressed_ = nullptr;
    musicPcm_[0] = nullptr;
    musicPcm_[1] = nullptr;
    musicPcmSamples_[0] = 0;
    musicPcmSamples_[1] = 0;
    musicHeader_ = AudioHeader{};
    nextMusicBuffer_ = 0;
    nextMusicBlock_ = 0;
    nextMusicSkipSamples_ = 0;
    playingMusic_ = MusicTrack::NONE;
}

void AudioManager::releaseSfx() {
    if (sfxPcm_) Platform::memory().release(sfxPcm_);
    sfxPcm_ = nullptr;
    sfxPcmBytes_ = 0;
}

void AudioManager::stopAll() {
    requestedMusic_ = MusicTrack::NONE;
    Platform::audio().stop();
    releaseMusic();
    releaseSfx();
}
