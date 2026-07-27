#include "hardware/Hal.h"
#include <Arduino.h>
#include <WiFi.h>
#include "core/TraceLog.h"

namespace {
constexpr uint8_t hardwareVolumeForPercent(uint8_t percent) {
    return static_cast<uint8_t>(
        (static_cast<uint16_t>(percent) * 255U + 50U) / 100U);
}

static_assert(hardwareVolumeForPercent(0) == 0, "Muted volume must map to zero");
static_assert(hardwareVolumeForPercent(50) == 128, "Half volume mapping changed");
static_assert(hardwareVolumeForPercent(100) == 255, "Full volume must map to 255");
}  // namespace

Hal& Hal::ins() {
    static Hal instance;
    return instance;
}

bool Hal::begin() {
    if (initialized) return true;

    auto cfg = M5.config();
    // StickMon targets M5StickS3 hardware only. Pinning the board keeps the
    // display path deterministic and avoids M5GFX's broad board autodetection.
    cfg.fallback_board = m5::board_t::board_M5StickS3;
    cfg.internal_spk = true;
    cfg.internal_imu = true;
    cfg.internal_mic = true;
    cfg.external_rtc = false;
    M5.begin(cfg);

    // StickS3 shares audio control lines between input and output. Keep the
    // device in speaker mode until a caller explicitly requests the mic.
    M5.Mic.end();
    if (!M5.Speaker.isRunning()) M5.Speaker.begin();

    WiFi.mode(WIFI_OFF);
    btStop();

    M5.Display.setRotation(1);
    M5.Display.setBrightness(brightness);

    sprite.setColorDepth(16);
    sprite.setPsram(psramFound());
    if (!sprite.createSprite(DISPLAY_W, DISPLAY_H)) {
        Serial.println("[Hal] ERROR: createSprite failed");
        return false;
    }
    sprite.setSwapBytes(true);

    initialized = true;
    Serial.println("[Hal] Init complete");
    return true;
}

LGFX_Sprite& Hal::canvas() {
    return sprite;
}

void Hal::flush() {
    sprite.pushSprite(&M5.Display, 0, 0);
}

void Hal::applyBrightness() {
    uint8_t target = idleBrightnessActive ? 16 : brightness;
    STICKMON_TRACEF("[Brightness] apply t=%lu idle=%u configured=%u before=%u target=%u\n",
                    (unsigned long)millis(),
                    idleBrightnessActive ? 1 : 0,
                    brightness,
                    M5.Display.getBrightness(),
                    target);
    M5.Display.setBrightness(target);
}

void Hal::setBrightness(uint8_t value) {
    STICKMON_TRACEF("[Brightness] set t=%lu old=%u new=%u idle=%u display=%u\n",
                    (unsigned long)millis(), brightness, value,
                    idleBrightnessActive ? 1 : 0, M5.Display.getBrightness());
    brightness = value;
    applyBrightness();
}

void Hal::setIdleBrightness(bool idle) {
    if (idleBrightnessActive == idle) return;
    STICKMON_TRACEF("[Brightness] idle t=%lu old=%u new=%u configured=%u display=%u\n",
                    (unsigned long)millis(),
                    idleBrightnessActive ? 1 : 0,
                    idle ? 1 : 0,
                    brightness,
                    M5.Display.getBrightness());
    idleBrightnessActive = idle;
    applyBrightness();
}

void Hal::setAudioVolume(uint8_t percent) {
    audioVolume = min<uint8_t>(100, percent);
    uint8_t hardwareVolume = hardwareVolumeForPercent(audioVolume);
    M5.Speaker.setVolume(hardwareVolume);
    STICKMON_TRACEF("[Audio] volume percent=%u hardware=%u playing=%u\n",
                    audioVolume, hardwareVolume, audioPlaying() ? 1 : 0);
}

bool Hal::playPcmU8(const uint8_t* data, size_t sampleCount, uint32_t sampleRate) {
    if (!initialized || audioVolume == 0 || !data || sampleCount == 0 ||
        sampleRate == 0 || microphoneMode || !M5.Speaker.isEnabled()) {
        return false;
    }
    return M5.Speaker.playRaw(data, sampleCount, sampleRate, false, 1, 0, true);
}

bool Hal::audioPlaying() const {
    return initialized && M5.Speaker.isPlaying(0) != 0;
}

void Hal::stopAudio() {
    if (initialized) M5.Speaker.stop(0);
}

bool Hal::beginMicrophone() {
    if (!initialized) return false;
    if (microphoneMode) return M5.Mic.isRunning();
    M5.Speaker.stop(0);
    M5.Speaker.end();
    delay(2);
    if (!M5.Mic.begin()) {
        M5.Speaker.begin();
        M5.Speaker.setVolume(hardwareVolumeForPercent(audioVolume));
        return false;
    }
    microphoneMode = true;
    Serial.println("[Audio] mode=microphone");
    return true;
}

void Hal::endMicrophone() {
    if (!initialized || !microphoneMode) return;
    // Mic_Class::end() does not clear a partially filled recording slot. Let
    // the queued block complete so the next microphone session cannot stall.
    while ((int32_t)(millis() - microphoneRecordingUntilMs) < 0 ||
           M5.Mic.isRecording()) {
        delay(1);
    }
    M5.Mic.end();
    delay(2);
    M5.Speaker.begin();
    M5.Speaker.setVolume(hardwareVolumeForPercent(audioVolume));
    microphoneMode = false;
    microphoneRecordingUntilMs = 0;
    Serial.println("[Audio] mode=speaker");
}

bool Hal::recordMicrophone(int16_t* data, size_t sampleCount, uint32_t sampleRate) {
    if (!microphoneMode || !data || sampleCount == 0 || sampleRate == 0 ||
        !M5.Mic.record(data, sampleCount, sampleRate, false)) {
        return false;
    }
    uint32_t durationMs = static_cast<uint32_t>(
        (static_cast<uint64_t>(sampleCount) * 1000ULL + sampleRate - 1) / sampleRate);
    microphoneRecordingUntilMs = millis() + durationMs;
    return true;
}

bool Hal::microphoneRecording() const {
    return initialized && microphoneMode && M5.Mic.isRecording() != 0;
}

uint32_t Hal::millis() const {
    return M5.millis();
}

bool Hal::btnA_raw() const {
    return M5.BtnA.isPressed();
}

bool Hal::btnB_raw() const {
    return M5.BtnB.isPressed() || M5.BtnPWR.isPressed();
}

bool Hal::readAccel(float& ax, float& ay, float& az) {
    if (!initialized) return false;
    return M5.Imu.getAccel(&ax, &ay, &az);
}

int Hal::batteryLevel() {
    return M5.Power.getBatteryLevel();
}

int Hal::filteredBatteryLevel() {
    uint32_t now = millis();
    if (batteryFiltered >= 0 && now - lastBatterySampleMs < 2000) {
        return batteryFiltered;
    }

    int raw = batteryLevel();
    if (raw < 0) raw = 0;
    if (raw > 100) raw = 100;
    if (batteryFiltered < 0) {
        batteryFiltered = raw;
    } else {
        batteryFiltered = (batteryFiltered * 7 + raw * 3 + 5) / 10;
    }
    lastBatterySampleMs = now;
    return batteryFiltered;
}
