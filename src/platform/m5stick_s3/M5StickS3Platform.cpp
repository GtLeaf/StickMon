#include "platform/m5stick_s3/M5StickS3Platform.h"

#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>

namespace {

LGFX_Sprite gFrameBuffer;

constexpr uint8_t hardwareVolumeForPercent(uint8_t percent) {
    return static_cast<uint8_t>(
        (static_cast<uint16_t>(percent) * 255U + 50U) / 100U);
}

static_assert(hardwareVolumeForPercent(0) == 0,
              "Muted volume must map to zero");
static_assert(hardwareVolumeForPercent(50) == 128,
              "Half volume mapping changed");
static_assert(hardwareVolumeForPercent(100) == 255,
              "Full volume must map to 255");

constexpr gpio_num_t SECONDARY_WAKE_GPIO = GPIO_NUM_12;

}  // namespace

M5StickS3Platform::M5StickS3Platform()
    : services_{*this, *this, *this, *this, *this, *this, *this} {}

M5StickS3Platform& M5StickS3Platform::instance() {
    static M5StickS3Platform platform;
    return platform;
}

Platform::Services& M5StickS3Platform::serviceBundle() {
    return services_;
}

void bindM5StickS3Platform() {
    Platform::bind(M5StickS3Platform::instance().serviceBundle());
}

bool M5StickS3Platform::begin() {
    if (initialized_) return true;

    auto cfg = M5.config();
    // StickMon targets M5StickS3 only. Pin the board so M5GFX does not run
    // broad hardware autodetection during startup.
    cfg.fallback_board = m5::board_t::board_M5StickS3;
    cfg.internal_spk = true;
    cfg.internal_imu = true;
    cfg.internal_mic = true;
    cfg.external_rtc = false;
    M5.begin(cfg);

    M5.Mic.end();
    if (!M5.Speaker.isRunning()) M5.Speaker.begin();
    WiFi.mode(WIFI_OFF);
    btStop();

    M5.Display.setRotation(1);
    M5.Display.setBrightness(128);

    gFrameBuffer.setColorDepth(16);
    gFrameBuffer.setPsram(psramFound());
    if (!gFrameBuffer.createSprite(Platform::LOGICAL_DISPLAY_W,
                                   Platform::LOGICAL_DISPLAY_H)) {
        Serial.println("[Platform] ERROR: createSprite failed");
        return false;
    }
    gFrameBuffer.setSwapBytes(true);
    setVolume(audioVolume_);
    initialized_ = true;
    Serial.println("[Platform] M5StickS3 ready");
    return true;
}

uint32_t M5StickS3Platform::millis() const {
    return M5.millis();
}

void M5StickS3Platform::sleepMs(uint32_t durationMs) {
    delay(durationMs);
}

void M5StickS3Platform::update() {
    M5.update();
}

bool M5StickS3Platform::pressed(Platform::InputButton button) const {
    switch (button) {
    case Platform::InputButton::PRIMARY:
        return M5.BtnA.isPressed();
    case Platform::InputButton::SECONDARY:
        return M5.BtnB.isPressed() || M5.BtnPWR.isPressed();
    default:
        return false;
    }
}

Platform::FrameBuffer565 M5StickS3Platform::frameBuffer() {
    Platform::FrameBuffer565 result;
    result.pixels = static_cast<uint16_t*>(gFrameBuffer.getBuffer());
    result.width = Platform::LOGICAL_DISPLAY_W;
    result.height = Platform::LOGICAL_DISPLAY_H;
    result.byteSwapped = true;
    return result;
}

void M5StickS3Platform::present() {
    gFrameBuffer.pushSprite(&M5.Display, 0, 0);
}

void M5StickS3Platform::setBrightness(uint8_t value) {
    M5.Display.setBrightness(value);
}

uint8_t M5StickS3Platform::brightness() const {
    return M5.Display.getBrightness();
}

void M5StickS3Platform::sleep() {
    M5.Display.sleep();
    M5.Display.waitDisplay();
}

void M5StickS3Platform::setVolume(uint8_t percent) {
    audioVolume_ = percent > 100 ? 100 : percent;
    M5.Speaker.setVolume(hardwareVolumeForPercent(audioVolume_));
}

uint8_t M5StickS3Platform::volume() const {
    return audioVolume_;
}

bool M5StickS3Platform::playPcmU8(const uint8_t* data, size_t sampleCount,
                                  uint32_t sampleRate) {
    if (!initialized_ || audioVolume_ == 0 || !data || sampleCount == 0 ||
        sampleRate == 0 || microphoneMode_ || !M5.Speaker.isEnabled()) {
        return false;
    }
    return M5.Speaker.playRaw(
        data, sampleCount, sampleRate, false, 1, 0, true);
}

bool M5StickS3Platform::playing() const {
    return initialized_ && M5.Speaker.isPlaying(0) != 0;
}

void M5StickS3Platform::stop() {
    if (initialized_) M5.Speaker.stop(0);
}

bool M5StickS3Platform::beginMicrophone() {
    if (!initialized_) return false;
    if (microphoneMode_) return M5.Mic.isRunning();
    M5.Speaker.stop(0);
    M5.Speaker.end();
    delay(2);
    if (!M5.Mic.begin()) {
        M5.Speaker.begin();
        setVolume(audioVolume_);
        return false;
    }
    microphoneMode_ = true;
    Serial.println("[Audio] mode=microphone");
    return true;
}

void M5StickS3Platform::endMicrophone() {
    if (!initialized_ || !microphoneMode_) return;
    while (static_cast<int32_t>(millis() - microphoneRecordingUntilMs_) < 0 ||
           M5.Mic.isRecording()) {
        delay(1);
    }
    M5.Mic.end();
    delay(2);
    M5.Speaker.begin();
    setVolume(audioVolume_);
    microphoneMode_ = false;
    microphoneRecordingUntilMs_ = 0;
    Serial.println("[Audio] mode=speaker");
}

bool M5StickS3Platform::recordMicrophone(
    int16_t* data, size_t sampleCount, uint32_t sampleRate) {
    if (!microphoneMode_ || !data || sampleCount == 0 || sampleRate == 0 ||
        !M5.Mic.record(data, sampleCount, sampleRate, false)) {
        return false;
    }
    uint32_t durationMs = static_cast<uint32_t>(
        (static_cast<uint64_t>(sampleCount) * 1000ULL + sampleRate - 1) /
        sampleRate);
    microphoneRecordingUntilMs_ = millis() + durationMs;
    return true;
}

bool M5StickS3Platform::microphoneRecording() const {
    return initialized_ && microphoneMode_ && M5.Mic.isRecording() != 0;
}

bool M5StickS3Platform::microphoneActive() const {
    return microphoneMode_;
}

bool M5StickS3Platform::readAcceleration(float& x, float& y, float& z) {
    return initialized_ && M5.Imu.getAccel(&x, &y, &z);
}

int M5StickS3Platform::batteryLevel() {
    return M5.Power.getBatteryLevel();
}

Platform::WakeReason M5StickS3Platform::wakeReason() const {
    switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_TIMER:
        return Platform::WakeReason::TIMER;
    case ESP_SLEEP_WAKEUP_UNDEFINED:
        return Platform::WakeReason::NORMAL;
    default:
        return Platform::WakeReason::EXTERNAL_SIGNAL;
    }
}

uint32_t M5StickS3Platform::hardwareRandom() {
    return esp_random();
}

size_t M5StickS3Platform::externalMemorySize() const {
    return ESP.getPsramSize();
}

size_t M5StickS3Platform::externalMemoryFree() const {
    return ESP.getFreePsram();
}

void M5StickS3Platform::restart() {
    ESP.restart();
}

void M5StickS3Platform::enterDeepSleep(
    uint64_t timerWakeUs, bool wakeOnSecondaryButton) {
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    if (wakeOnSecondaryButton) {
        rtc_gpio_pullup_en(SECONDARY_WAKE_GPIO);
        rtc_gpio_pulldown_dis(SECONDARY_WAKE_GPIO);
        esp_sleep_enable_ext1_wakeup(
            1ULL << static_cast<uint8_t>(SECONDARY_WAKE_GPIO),
            ESP_EXT1_WAKEUP_ANY_LOW);
    }
    if (timerWakeUs > 0) {
        esp_sleep_enable_timer_wakeup(timerWakeUs);
    }
    esp_deep_sleep_start();
}
