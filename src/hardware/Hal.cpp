#include "hardware/Hal.h"
#include "core/TraceLog.h"

Hal& Hal::ins() {
    static Hal instance;
    return instance;
}

bool Hal::begin() {
    if (initialized) return true;
    if (!Platform::bound() || !Platform::services().lifecycle.begin()) {
        return false;
    }
    initialized = true;
    Platform::display().setBrightness(brightness);
    Platform::audio().setVolume(audioVolume);
    return true;
}

Platform::FrameBuffer565 Hal::frameBuffer() {
    return Platform::display().frameBuffer();
}

void Hal::flush() {
    Platform::display().present();
}

void Hal::applyBrightness() {
    uint8_t target = idleBrightnessActive ? 16 : brightness;
    STICKMON_TRACEF("[Brightness] apply t=%lu idle=%u configured=%u before=%u target=%u\n",
                    (unsigned long)millis(),
                    idleBrightnessActive ? 1 : 0,
                    brightness,
                    Platform::display().brightness(),
                    target);
    Platform::display().setBrightness(target);
}

void Hal::setBrightness(uint8_t value) {
    STICKMON_TRACEF("[Brightness] set t=%lu old=%u new=%u idle=%u display=%u\n",
                    (unsigned long)millis(), brightness, value,
                    idleBrightnessActive ? 1 : 0,
                    Platform::display().brightness());
    brightness = value;
    applyBrightness();
}

void Hal::setIdleBrightness(bool idle) {
    if (idleBrightnessActive == idle) return;
    STICKMON_TRACEF("[Brightness] idle t=%lu old=%u new=%u configured=%u display=%u\n",
                    (unsigned long)millis(),
                    idleBrightnessActive ? 1 : 0,
                    idle ? 1 : 0,
                    brightness, Platform::display().brightness());
    idleBrightnessActive = idle;
    applyBrightness();
}

void Hal::setAudioVolume(uint8_t percent) {
    audioVolume = percent > 100 ? 100 : percent;
    Platform::audio().setVolume(audioVolume);
    STICKMON_TRACEF("[Audio] volume percent=%u playing=%u\n",
                    audioVolume, audioPlaying() ? 1 : 0);
}

bool Hal::playPcmU8(const uint8_t* data, size_t sampleCount, uint32_t sampleRate) {
    return Platform::audio().playPcmU8(data, sampleCount, sampleRate);
}

bool Hal::audioPlaying() const {
    return initialized && Platform::audio().playing();
}

void Hal::stopAudio() {
    if (initialized) Platform::audio().stop();
}

bool Hal::beginMicrophone() {
    return initialized && Platform::audio().beginMicrophone();
}

void Hal::endMicrophone() {
    if (initialized) Platform::audio().endMicrophone();
}

bool Hal::recordMicrophone(int16_t* data, size_t sampleCount, uint32_t sampleRate) {
    return Platform::audio().recordMicrophone(data, sampleCount, sampleRate);
}

bool Hal::microphoneRecording() const {
    return initialized && Platform::audio().microphoneRecording();
}

bool Hal::microphoneActive() const {
    return initialized && Platform::audio().microphoneActive();
}

uint8_t Hal::getDisplayBrightness() const {
    return Platform::display().brightness();
}

uint32_t Hal::millis() const {
    return Platform::clock().millis();
}

bool Hal::readAccel(float& ax, float& ay, float& az) {
    return initialized && Platform::imu().readAcceleration(ax, ay, az);
}

int Hal::batteryLevel() {
    return Platform::power().batteryLevel();
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
