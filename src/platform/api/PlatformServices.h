#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

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
    virtual uint32_t micros() const = 0;
    virtual void sleepMs(uint32_t durationMs) = 0;
};

class ILogger {
public:
    virtual ~ILogger() = default;
    virtual void write(const char* text, size_t length) = 0;
    virtual void flush() = 0;
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
    static constexpr uint8_t CHANNEL_COUNT = 3;

    virtual ~IAudioDevice() = default;
    virtual void setVolume(uint8_t percent) = 0;
    virtual uint8_t volume() const = 0;
    virtual bool playPcmU8(const uint8_t* data, size_t sampleCount,
                           uint32_t sampleRate) = 0;
    virtual bool playPcmU8Channel(const uint8_t* data, size_t sampleCount,
                                  uint32_t sampleRate, uint8_t channel,
                                  bool stopCurrent) = 0;
    virtual void setChannelVolume(uint8_t channel, uint8_t percent) = 0;
    virtual bool playing() const = 0;
    virtual uint8_t queuedPcm(uint8_t channel) const = 0;
    virtual void stop() = 0;
    virtual void stopChannel(uint8_t channel) = 0;
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

class IBlobStore {
public:
    virtual ~IBlobStore() = default;
    virtual bool initialize() = 0;
    virtual size_t blobSize(const char* nameSpace, const char* key) = 0;
    virtual bool readBlob(const char* nameSpace, const char* key,
                          void* output, size_t length) = 0;
    virtual bool writeBlob(const char* nameSpace, const char* key,
                           const void* data, size_t length) = 0;
    virtual bool removeBlob(const char* nameSpace, const char* key) = 0;
    virtual bool clearNamespace(const char* nameSpace) = 0;
};

class IResourceFile {
public:
    virtual ~IResourceFile() = default;
    virtual bool valid() const = 0;
    virtual size_t size() const = 0;
    virtual size_t position() const = 0;
    virtual size_t read(void* output, size_t length) = 0;
    virtual bool seek(size_t position) = 0;
    virtual void close() = 0;
};

class ResourceFile {
public:
    ResourceFile() = default;
    explicit ResourceFile(IResourceFile* implementation)
        : implementation_(implementation) {}
    ~ResourceFile() { reset(); }

    ResourceFile(const ResourceFile&) = delete;
    ResourceFile& operator=(const ResourceFile&) = delete;
    ResourceFile(ResourceFile&& other) noexcept
        : implementation_(other.implementation_) {
        other.implementation_ = nullptr;
    }
    ResourceFile& operator=(ResourceFile&& other) noexcept {
        if (this != &other) {
            reset();
            implementation_ = other.implementation_;
            other.implementation_ = nullptr;
        }
        return *this;
    }

    explicit operator bool() const {
        return implementation_ && implementation_->valid();
    }
    size_t size() const { return implementation_ ? implementation_->size() : 0; }
    size_t position() const {
        return implementation_ ? implementation_->position() : 0;
    }
    size_t read(void* output, size_t length) {
        return implementation_ ? implementation_->read(output, length) : 0;
    }
    size_t read(uint8_t* output, size_t length) {
        return read(static_cast<void*>(output), length);
    }
    bool seek(size_t position) {
        return implementation_ && implementation_->seek(position);
    }
    void close() {
        if (implementation_) implementation_->close();
    }
    void reset(IResourceFile* implementation = nullptr) {
        if (implementation_) {
            implementation_->close();
            delete implementation_;
        }
        implementation_ = implementation;
    }

private:
    IResourceFile* implementation_ = nullptr;
};

class IResourceStore {
public:
    virtual ~IResourceStore() = default;
    virtual bool mount() = 0;
    virtual size_t totalBytes() const = 0;
    virtual size_t usedBytes() const = 0;
    virtual ResourceFile open(const char* path) = 0;
};

class IMemoryAllocator {
public:
    virtual ~IMemoryAllocator() = default;
    virtual void* allocate(size_t bytes, bool preferExternal) = 0;
    virtual void release(void* memory) = 0;
    virtual size_t externalFree() const = 0;
};

struct PeerPacket {
    static constexpr size_t MAX_PAYLOAD_BYTES = 250;
    uint8_t source[6] = {};
    uint8_t payload[MAX_PAYLOAD_BYTES] = {};
    size_t length = 0;
};

class IPeerTransport {
public:
    virtual ~IPeerTransport() = default;
    virtual bool enable() = 0;
    virtual void end() = 0;
    virtual bool active() const = 0;
    virtual bool send(const uint8_t destination[6], const void* data,
                      size_t length) = 0;
    virtual bool receive(PeerPacket& packet) = 0;
};

struct Services {
    IPlatformLifecycle& lifecycle;
    IClock& clock;
    ILogger& logger;
    IInputDevice& input;
    IDisplayDevice& display;
    IAudioDevice& audio;
    IImuDevice& imu;
    IPowerDevice& power;
    IBlobStore& blobs;
    IResourceStore& resources;
    IMemoryAllocator& memory;
    IPeerTransport& peers;
};

void bind(Services& services);
bool bound();
Services& services();

inline IClock& clock() { return services().clock; }
inline ILogger& logger() { return services().logger; }
inline IInputDevice& input() { return services().input; }
inline IDisplayDevice& display() { return services().display; }
inline IAudioDevice& audio() { return services().audio; }
inline IImuDevice& imu() { return services().imu; }
inline IPowerDevice& power() { return services().power; }
inline IBlobStore& blobs() { return services().blobs; }
inline IResourceStore& resources() { return services().resources; }
inline IMemoryAllocator& memory() { return services().memory; }
inline IPeerTransport& peers() { return services().peers; }

void logf(const char* format, ...);
void logLine(const char* text);

}  // namespace Platform
