#pragma once

#include <cstddef>
#include <cstdint>

#include "platform/api/PlatformServices.h"

enum class MusicTrack : uint8_t {
    NONE = 0,
    HOME,
    EXPLORE,
    BATTLE,
    BATTLE_SPECIAL,
};

enum class SfxCue : uint8_t {
    UI_CURSOR = 0,
    UI_CONFIRM,
    UI_CANCEL,
    MENU_OPEN,
    MENU_CLOSE,
    DAMAGE_NORMAL,
    DAMAGE_SUPER,
    DAMAGE_WEAK,
    THROW,
    FAINT,
    EXP_GAIN,
    EXP_FULL,
    LEVEL_UP,
    MOVE_LEARNT,
    CONTACT,
};

class AudioManager {
public:
    static constexpr uint8_t MUSIC_CHANNEL = 0;
    static constexpr uint8_t SFX_CHANNEL = 1;
    static constexpr uint8_t CRY_CHANNEL = 2;

    static AudioManager& ins();

    void setMusic(MusicTrack track);
    void stopMusic();
    void setPowerSave(bool active);
    bool playSfx(SfxCue cue);
    void update();
    void stopAll();

    MusicTrack requestedMusic() const { return requestedMusic_; }
    MusicTrack playingMusic() const { return playingMusic_; }
    bool musicFadeActive() const {
        return musicChannelVolume_ != musicFadeTargetVolume_;
    }

private:
    AudioManager() = default;

    struct AudioHeader {
        uint32_t sampleRate = 0;
        uint32_t sampleCount = 0;
        uint16_t blockBytes = 0;
        uint16_t blockSamples = 0;
        uint32_t blockCount = 0;
        uint32_t loopStartSample = 0;
        uint32_t payloadBytes = 0;
        bool looping = false;
    };

    bool startRequestedMusic();
    bool openAudio(const char* id, Platform::ResourceFile& file,
                   AudioHeader& header);
    bool decodeMusicBlock(uint8_t bufferIndex);
    bool queueNextMusicBlock(bool stopCurrent);
    void releaseMusic();
    void releaseSfx();
    void updateMusicFade(uint32_t nowMs);

    Platform::ResourceFile musicFile_;
    AudioHeader musicHeader_{};
    uint8_t* musicCompressed_ = nullptr;
    uint8_t* musicPcm_[2] = {};
    uint16_t musicPcmSamples_[2] = {};
    uint8_t nextMusicBuffer_ = 0;
    uint32_t nextMusicBlock_ = 0;
    uint16_t nextMusicSkipSamples_ = 0;

    uint8_t* sfxPcm_ = nullptr;
    size_t sfxPcmBytes_ = 0;

    MusicTrack requestedMusic_ = MusicTrack::NONE;
    MusicTrack playingMusic_ = MusicTrack::NONE;
    uint32_t musicFadeStartedMs_ = 0;
    uint8_t musicFadeStartVolume_ = 100;
    uint8_t musicChannelVolume_ = 100;
    uint8_t musicFadeTargetVolume_ = 100;
    bool powerSaveActive_ = false;
    bool musicPaused_ = false;
};
