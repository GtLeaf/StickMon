#pragma once

#include <cstddef>
#include <cstdint>
#include "platform/api/PlatformServices.h"

class Hal {
public:
    static Hal& ins();

    bool begin();
    Platform::FrameBuffer565 frameBuffer();
    void flush();
    void setBrightness(uint8_t value);
    void setIdleBrightness(bool idle);
    void setAudioVolume(uint8_t percent);
    bool playPcmU8(const uint8_t* data, size_t sampleCount, uint32_t sampleRate);
    bool audioPlaying() const;
    void stopAudio();
    bool beginMicrophone();
    void endMicrophone();
    bool recordMicrophone(int16_t* data, size_t sampleCount, uint32_t sampleRate);
    bool microphoneRecording() const;
    bool microphoneActive() const;
    uint8_t getBrightness() const { return brightness; }
    uint8_t getDisplayBrightness() const;
    bool isIdleBrightnessActive() const { return idleBrightnessActive; }
    uint8_t getAudioVolume() const { return audioVolume; }
    uint32_t millis() const;
    bool readAccel(float& ax, float& ay, float& az);
    int batteryLevel();
    int filteredBatteryLevel();

    static constexpr int DISPLAY_W = Platform::LOGICAL_DISPLAY_W;
    static constexpr int DISPLAY_H = Platform::LOGICAL_DISPLAY_H;

private:
    Hal() = default;

    void applyBrightness();

    bool initialized = false;
    bool idleBrightnessActive = false;
    uint8_t brightness = 128;
    uint8_t audioVolume = 50;
    int batteryFiltered = -1;
    uint32_t lastBatterySampleMs = 0;
};
