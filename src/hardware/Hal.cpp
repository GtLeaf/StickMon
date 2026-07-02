#include "hardware/Hal.h"
#include <Arduino.h>
#include <WiFi.h>

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
    cfg.internal_spk = false;
    cfg.internal_imu = true;
    cfg.internal_mic = false;
    cfg.external_rtc = false;
    M5.begin(cfg);
    M5.Speaker.end();

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

void Hal::setBrightness(uint8_t value) {
    brightness = value;
    if (!idleBrightnessActive) {
        M5.Display.setBrightness(value);
    }
}

void Hal::setIdleBrightness(bool idle) {
    if (idle == idleBrightnessActive) return;
    idleBrightnessActive = idle;
    M5.Display.setBrightness(idle ? 16 : brightness);
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
