#pragma once

#include <M5Unified.h>
#include <cstddef>

class Hal {
public:
    static Hal& ins();

    bool begin();
    LGFX_Sprite& canvas();
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
    bool microphoneActive() const { return microphoneMode; }
    uint8_t getBrightness() const { return brightness; }
    uint8_t getDisplayBrightness() const { return M5.Display.getBrightness(); }
    bool isIdleBrightnessActive() const { return idleBrightnessActive; }
    uint8_t getAudioVolume() const { return audioVolume; }
    uint32_t millis() const;
    bool btnA_raw() const;
    bool btnB_raw() const;
    bool readAccel(float& ax, float& ay, float& az);
    int batteryLevel();
    int filteredBatteryLevel();

    static constexpr int DISPLAY_W = 240;
    static constexpr int DISPLAY_H = 135;

private:
    Hal() = default;

    void applyBrightness();

    LGFX_Sprite sprite;
    bool initialized = false;
    bool idleBrightnessActive = false;
    uint8_t brightness = 128;
    uint8_t audioVolume = 50;
    bool microphoneMode = false;
    uint32_t microphoneRecordingUntilMs = 0;
    int batteryFiltered = -1;
    uint32_t lastBatterySampleMs = 0;
};
