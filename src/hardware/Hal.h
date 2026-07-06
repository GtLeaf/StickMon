#pragma once

#include <M5Unified.h>

class Hal {
public:
    static Hal& ins();

    bool begin();
    LGFX_Sprite& canvas();
    void flush();
    void setBrightness(uint8_t value);
    void setIdleBrightness(bool idle);
    uint8_t getBrightness() const { return brightness; }
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
    int batteryFiltered = -1;
    uint32_t lastBatterySampleMs = 0;
};
