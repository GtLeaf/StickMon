#pragma once

#include <cstddef>
#include <cstdint>

namespace Platform {

static constexpr uint16_t LOGICAL_DISPLAY_W = 240;
static constexpr uint16_t LOGICAL_DISPLAY_H = 135;

enum class InputButton : uint8_t {
    PRIMARY = 0,
    SECONDARY = 1,
    COUNT,
};

enum class WakeReason : uint8_t {
    NORMAL,
    TIMER,
    EXTERNAL_SIGNAL,
};

struct FrameBuffer565 {
    uint16_t* pixels = nullptr;
    uint16_t width = 0;
    uint16_t height = 0;
    bool byteSwapped = false;
};

class IPlatformLifecycle {
public:
    virtual ~IPlatformLifecycle() = default;
    virtual bool begin() = 0;
};

class IClock {
public:
    virtual ~IClock() = default;
    virtual uint32_t millis() const = 0;
    virtual void sleepMs(uint32_t durationMs) = 0;
};

class IInputDevice {
public:
    virtual ~IInputDevice() = default;
    virtual void update() = 0;
    virtual bool pressed(InputButton button) const = 0;
};

class IDisplayDevice {
public:
    virtual ~IDisplayDevice() = default;
    virtual FrameBuffer565 frameBuffer() = 0;
    virtual void present() = 0;
    virtual void setBrightness(uint8_t value) = 0;
    virtual uint8_t brightness() const = 0;
    virtual void sleep() = 0;
};

class IAudioDevice {
public:
    virtual ~IAudioDevice() = default;
    virtual void setVolume(uint8_t percent) = 0;
    virtual uint8_t volume() const = 0;
    virtual bool playPcmU8(const uint8_t* data, size_t sampleCount,
                           uint32_t sampleRate) = 0;
    virtual bool playing() const = 0;
    virtual void stop() = 0;
    virtual bool beginMicrophone() = 0;
    virtual void endMicrophone() = 0;
    virtual bool recordMicrophone(int16_t* data, size_t sampleCount,
                                  uint32_t sampleRate) = 0;
    virtual bool microphoneRecording() const = 0;
    virtual bool microphoneActive() const = 0;
};

class IImuDevice {
public:
    virtual ~IImuDevice() = default;
    virtual bool readAcceleration(float& x, float& y, float& z) = 0;
};

class IPowerDevice {
public:
    virtual ~IPowerDevice() = default;
    virtual int batteryLevel() = 0;
    virtual WakeReason wakeReason() const = 0;
    virtual uint32_t hardwareRandom() = 0;
    virtual size_t externalMemorySize() const = 0;
    virtual size_t externalMemoryFree() const = 0;
    virtual void restart() = 0;
    virtual void enterDeepSleep(uint64_t timerWakeUs,
                                bool wakeOnSecondaryButton) = 0;
};

struct Services {
    IPlatformLifecycle& lifecycle;
    IClock& clock;
    IInputDevice& input;
    IDisplayDevice& display;
    IAudioDevice& audio;
    IImuDevice& imu;
    IPowerDevice& power;
};

void bind(Services& services);
bool bound();
Services& services();

inline IClock& clock() { return services().clock; }
inline IInputDevice& input() { return services().input; }
inline IDisplayDevice& display() { return services().display; }
inline IAudioDevice& audio() { return services().audio; }
inline IImuDevice& imu() { return services().imu; }
inline IPowerDevice& power() { return services().power; }

}  // namespace Platform
