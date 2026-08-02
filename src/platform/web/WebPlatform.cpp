#include "platform/web/WebPlatform.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

namespace {

// ── JS interop via EM_JS ────────────────────────────────────────────────

#ifdef __EMSCRIPTEN__

EM_JS(void, js_present_frame, (const uint16_t* pixels, int w, int h, int brightness), {
    const canvas = document.getElementById('stickmon-canvas');
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    const img = ctx.createImageData(w, h);
    const data = img.data;
    const src = HEAPU16.buffer.slice(pixels, pixels + w * h * 2);
    const src16 = new Uint16Array(src);
    const b = brightness / 255.0;
    for (let i = 0; i < w * h; i++) {
        const px = src16[i];
        const r5 = (px >> 11) & 0x1F;
        const g6 = (px >> 5) & 0x3F;
        const b5 = px & 0x1F;
        data[i * 4]     = Math.round((r5 << 3 | r5 >> 2) * b);
        data[i * 4 + 1] = Math.round((g6 << 2 | g6 >> 4) * b);
        data[i * 4 + 2] = Math.round((b5 << 3 | b5 >> 2) * b);
        data[i * 4 + 3] = 255;
    }
    ctx.putImageData(img, 0, 0);
});

EM_JS(void, js_audio_init, (int channelCount, int volumePercent), {
    if (window._stickmonAudio) return;
    const AudioContextClass = window.AudioContext || window.webkitAudioContext;
    if (!AudioContextClass) return;
    const context = new AudioContextClass();
    const masterGain = context.createGain();
    masterGain.gain.value = volumePercent / 100.0;
    masterGain.connect(context.destination);
    const channels = [];
    for (let i = 0; i < channelCount; ++i) {
        const gain = context.createGain();
        gain.gain.value = 1.0;
        gain.connect(masterGain);
        channels.push({gain: gain, queue: [], nextStartTime: 0});
    }
    window._stickmonAudio = {
        context: context,
        masterGain: masterGain,
        channels: channels
    };
});

EM_JS(void, js_audio_resume, (), {
    const audio = window._stickmonAudio;
    if (audio && audio.context.state === 'suspended') {
        audio.context.resume();
    }
});

EM_JS(void, js_set_audio_volume, (int volumePercent), {
    const audio = window._stickmonAudio;
    if (!audio) return;
    audio.masterGain.gain.setValueAtTime(
        volumePercent / 100.0, audio.context.currentTime);
});

EM_JS(int, js_play_pcm_channel,
      (const uint8_t* data, int len, int sampleRate, int channel,
       int stopCurrent), {
    const audio = window._stickmonAudio;
    if (!audio || channel < 0 || channel >= audio.channels.length) return 0;
    const actx = audio.context;
    const state = audio.channels[channel];
    if (stopCurrent) {
        for (const entry of state.queue) {
            entry.source.onended = null;
            try { entry.source.stop(); } catch (e) {}
        }
        state.queue = [];
        state.nextStartTime = actx.currentTime;
    } else if (state.queue.length >= 2) {
        return 0;
    }
    const bytes = new Uint8Array(HEAPU8.buffer.slice(data, data + len));
    const float32 = new Float32Array(len);
    for (let i = 0; i < len; i++) {
        float32[i] = (bytes[i] - 128) / 128.0;
    }
    const buf = actx.createBuffer(1, len, sampleRate);
    buf.copyToChannel(float32, 0);
    const src = actx.createBufferSource();
    src.buffer = buf;
    src.connect(state.gain);
    const startTime = Math.max(actx.currentTime, state.nextStartTime);
    const entry = {source: src, endTime: startTime + buf.duration};
    state.queue.push(entry);
    state.nextStartTime = entry.endTime;
    src.onended = function() {
        const index = state.queue.indexOf(entry);
        if (index >= 0) state.queue.splice(index, 1);
        if (state.queue.length === 0) state.nextStartTime = actx.currentTime;
    };
    src.start(startTime);
    if (actx.state === 'suspended') actx.resume();
    return 1;
});

EM_JS(void, js_set_channel_volume, (int channel, int volumePercent), {
    const audio = window._stickmonAudio;
    if (!audio || channel < 0 || channel >= audio.channels.length) return;
    audio.channels[channel].gain.gain.setValueAtTime(
        volumePercent / 100.0, audio.context.currentTime);
});

EM_JS(int, js_audio_playing, (), {
    const audio = window._stickmonAudio;
    if (!audio) return 0;
    return audio.channels.some(channel => channel.queue.length > 0) ? 1 : 0;
});

EM_JS(int, js_queued_pcm, (int channel), {
    const audio = window._stickmonAudio;
    if (!audio || channel < 0 || channel >= audio.channels.length) return 0;
    return audio.channels[channel].queue.length;
});

EM_JS(void, js_stop_audio_channel, (int channel), {
    const audio = window._stickmonAudio;
    if (!audio || channel < 0 || channel >= audio.channels.length) return;
    const state = audio.channels[channel];
    for (const entry of state.queue) {
        entry.source.onended = null;
        try { entry.source.stop(); } catch (e) {}
    }
    state.queue = [];
    state.nextStartTime = audio.context.currentTime;
});

EM_JS(void, js_stop_audio, (), {
    const audio = window._stickmonAudio;
    if (!audio) return;
    for (const state of audio.channels) {
        for (const entry of state.queue) {
            entry.source.onended = null;
            try { entry.source.stop(); } catch (e) {}
        }
        state.queue = [];
        state.nextStartTime = audio.context.currentTime;
    }
});

EM_JS(int, js_ls_get_size, (const char* key), {
    const k = UTF8ToString(key);
    const v = localStorage.getItem(k);
    if (!v) return -1;
    return v.length / 2; // hex string
});

EM_JS(int, js_ls_read, (const char* key, uint8_t* out, int maxLen), {
    const k = UTF8ToString(key);
    const v = localStorage.getItem(k);
    if (!v) return -1;
    const bytes = Math.min(v.length / 2, maxLen);
    for (let i = 0; i < bytes; i++) {
        setValue(out + i, parseInt(v.substr(i * 2, 2), 16), 'i8');
    }
    return bytes;
});

EM_JS(void, js_ls_write, (const char* key, const uint8_t* data, int len), {
    const k = UTF8ToString(key);
    let hex = "";
    const bytes = new Uint8Array(HEAPU8.buffer.slice(data, data + len));
    for (let i = 0; i < len; i++) {
        hex += bytes[i].toString(16).padStart(2, '0');
    }
    try { localStorage.setItem(k, hex); } catch(e) {}
});

EM_JS(void, js_ls_remove, (const char* key), {
    localStorage.removeItem(UTF8ToString(key));
});

EM_JS(void, js_ls_clear_prefix, (const char* prefix), {
    const p = UTF8ToString(prefix);
    const toRemove = [];
    for (let i = 0; i < localStorage.length; i++) {
        const k = localStorage.key(i);
        if (k && k.startsWith(p)) toRemove.push(k);
    }
    toRemove.forEach(k => localStorage.removeItem(k));
});

EM_JS(void, js_console_log, (const char* text), {
    console.log(UTF8ToString(text));
});

#endif // __EMSCRIPTEN__

// ── Web resource file (reads from Emscripten virtual FS) ────────────────

class WebResourceFile final : public Platform::IResourceFile {
public:
    explicit WebResourceFile(const char* path) {
        file_ = std::fopen(path, "rb");
        if (!file_) return;
        std::fseek(file_, 0, SEEK_END);
        size_ = static_cast<size_t>(std::ftell(file_));
        std::fseek(file_, 0, SEEK_SET);
    }
    ~WebResourceFile() override { close(); }

    bool valid() const override { return file_ != nullptr; }
    size_t size() const override { return size_; }
    size_t position() const override {
        return file_ ? static_cast<size_t>(std::ftell(file_)) : 0;
    }
    size_t read(void* output, size_t length) override {
        if (!file_ || !output) return 0;
        return std::fread(output, 1, length, file_);
    }
    bool seek(size_t position) override {
        if (!file_ || position > size_) return false;
        return std::fseek(file_, static_cast<long>(position), SEEK_SET) == 0;
    }
    void close() override {
        if (file_) { std::fclose(file_); file_ = nullptr; }
    }

private:
    std::FILE* file_ = nullptr;
    size_t size_ = 0;
};

} // namespace

// ── WebPlatform implementation ──────────────────────────────────────────

WebPlatform::WebPlatform()
    : services_{*this, *this, *this, *this, *this, *this, *this, *this,
                *this, *this, *this, *this},
      frameBuffer_(static_cast<size_t>(Platform::LOGICAL_DISPLAY_W) *
                   Platform::LOGICAL_DISPLAY_H) {}

bool WebPlatform::begin() {
    initialized_ = true;
#ifdef __EMSCRIPTEN__
    js_audio_init(Platform::IAudioDevice::CHANNEL_COUNT, volume_);
#endif
    return true;
}

// Clock — real time
uint32_t WebPlatform::millis() const {
#ifdef __EMSCRIPTEN__
    return static_cast<uint32_t>(emscripten_get_now());
#else
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
#endif
}

uint32_t WebPlatform::micros() const {
#ifdef __EMSCRIPTEN__
    return static_cast<uint32_t>(emscripten_get_now() * 1000.0);
#else
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(now).count());
#endif
}

void WebPlatform::sleepMs(uint32_t durationMs) {
    // In browser, we don't actually sleep — the main loop is driven by rAF.
    // This is a no-op; the game loop will be called again on next frame.
    (void)durationMs;
}

// Logger
void WebPlatform::write(const char* text, size_t length) {
    if (!text || length == 0) return;
#ifdef __EMSCRIPTEN__
    // Copy to null-terminated buffer for JS
    std::string msg(text, length);
    js_console_log(msg.c_str());
#else
    std::fwrite(text, 1, length, stderr);
#endif
}

void WebPlatform::flush() {}

// Input
bool WebPlatform::pressed(Platform::InputButton button) const {
    size_t index = static_cast<size_t>(button);
    return index < buttons_.size() && buttons_[index];
}

void WebPlatform::setPressed(Platform::InputButton button, bool value) {
    size_t index = static_cast<size_t>(button);
    if (index < buttons_.size()) buttons_[index] = value;
#ifdef __EMSCRIPTEN__
    if (value) js_audio_resume();
#endif
}

// Display
Platform::FrameBuffer565 WebPlatform::frameBuffer() {
    return {frameBuffer_.data(), Platform::LOGICAL_DISPLAY_W,
            Platform::LOGICAL_DISPLAY_H, false};
}

void WebPlatform::present() {
    if (displaySleeping_) return;
#ifdef __EMSCRIPTEN__
    js_present_frame(frameBuffer_.data(),
                     Platform::LOGICAL_DISPLAY_W,
                     Platform::LOGICAL_DISPLAY_H,
                     brightness_);
#endif
}

// Audio
void WebPlatform::setVolume(uint8_t percent) {
    volume_ = std::min<uint8_t>(percent, 100);
#ifdef __EMSCRIPTEN__
    js_set_audio_volume(volume_);
#endif
}

bool WebPlatform::playPcmU8(const uint8_t* data, size_t sampleCount,
                            uint32_t sampleRate) {
    return playPcmU8Channel(data, sampleCount, sampleRate, 0, true);
}

bool WebPlatform::playPcmU8Channel(const uint8_t* data, size_t sampleCount,
                                   uint32_t sampleRate, uint8_t channel,
                                   bool stopCurrent) {
    if (!initialized_ || !data || sampleCount == 0 || sampleRate == 0 ||
        volume_ == 0 || channel >= audioQueueDepth_.size()) return false;
#ifdef __EMSCRIPTEN__
    return js_play_pcm_channel(
        data, static_cast<int>(sampleCount), static_cast<int>(sampleRate),
        channel, stopCurrent ? 1 : 0) != 0;
#else
    if (!stopCurrent && audioQueueDepth_[channel] >= 2) return false;
    audioQueueDepth_[channel] = stopCurrent
        ? 1
        : static_cast<uint8_t>(audioQueueDepth_[channel] + 1);
    return true;
#endif
}

void WebPlatform::setChannelVolume(uint8_t channel, uint8_t percent) {
    if (channel >= audioChannelVolume_.size()) return;
    audioChannelVolume_[channel] = std::min<uint8_t>(percent, 100);
#ifdef __EMSCRIPTEN__
    js_set_channel_volume(channel, audioChannelVolume_[channel]);
#endif
}

bool WebPlatform::playing() const {
#ifdef __EMSCRIPTEN__
    return js_audio_playing() != 0;
#else
    return std::any_of(audioQueueDepth_.begin(), audioQueueDepth_.end(),
                       [](uint8_t depth) { return depth > 0; });
#endif
}

uint8_t WebPlatform::queuedPcm(uint8_t channel) const {
    if (channel >= audioQueueDepth_.size()) return 0;
#ifdef __EMSCRIPTEN__
    return static_cast<uint8_t>(js_queued_pcm(channel));
#else
    return audioQueueDepth_[channel];
#endif
}

void WebPlatform::stop() {
#ifdef __EMSCRIPTEN__
    js_stop_audio();
#endif
    audioQueueDepth_.fill(0);
}

void WebPlatform::stopChannel(uint8_t channel) {
    if (channel >= audioQueueDepth_.size()) return;
#ifdef __EMSCRIPTEN__
    js_stop_audio_channel(channel);
#endif
    audioQueueDepth_[channel] = 0;
}

// IMU
bool WebPlatform::readAcceleration(float& x, float& y, float& z) {
    x = 0.0f;
    y = 0.0f;
    z = 1.0f;
    return true;
}

// Power
uint32_t WebPlatform::hardwareRandom() {
    return static_cast<uint32_t>(std::rand());
}

// BlobStore — localStorage
std::string WebPlatform::blobKey(const char* nameSpace, const char* key) {
    if (!nameSpace || !key) return {};
    return std::string("stickmon_") + nameSpace + "_" + key;
}

size_t WebPlatform::blobSize(const char* nameSpace, const char* key) {
    std::string k = blobKey(nameSpace, key);
    if (k.empty()) return 0;
#ifdef __EMSCRIPTEN__
    int sz = js_ls_get_size(k.c_str());
    return sz >= 0 ? static_cast<size_t>(sz) : 0;
#else
    auto it = blobs_.find(k);
    return it == blobs_.end() ? 0 : it->second.size();
#endif
}

bool WebPlatform::readBlob(const char* nameSpace, const char* key,
                           void* output, size_t length) {
    if (!output || length == 0) return false;
    std::string k = blobKey(nameSpace, key);
    if (k.empty()) return false;
#ifdef __EMSCRIPTEN__
    int rd = js_ls_read(k.c_str(), static_cast<uint8_t*>(output),
                        static_cast<int>(length));
    return rd == static_cast<int>(length);
#else
    auto it = blobs_.find(k);
    if (it == blobs_.end() || it->second.size() != length) return false;
    std::memcpy(output, it->second.data(), length);
    return true;
#endif
}

bool WebPlatform::writeBlob(const char* nameSpace, const char* key,
                            const void* data, size_t length) {
    if (!data || length == 0) return false;
    std::string k = blobKey(nameSpace, key);
    if (k.empty()) return false;
#ifdef __EMSCRIPTEN__
    js_ls_write(k.c_str(), static_cast<const uint8_t*>(data),
                static_cast<int>(length));
#else
    const auto* bytes = static_cast<const uint8_t*>(data);
    blobs_[k] = std::vector<uint8_t>(bytes, bytes + length);
#endif
    return true;
}

bool WebPlatform::removeBlob(const char* nameSpace, const char* key) {
    std::string k = blobKey(nameSpace, key);
    if (k.empty()) return false;
#ifdef __EMSCRIPTEN__
    js_ls_remove(k.c_str());
#else
    blobs_.erase(k);
#endif
    return true;
}

bool WebPlatform::clearNamespace(const char* nameSpace) {
    if (!nameSpace) return false;
    std::string prefix = std::string("stickmon_") + nameSpace + "_";
#ifdef __EMSCRIPTEN__
    js_ls_clear_prefix(prefix.c_str());
#else
    for (auto it = blobs_.begin(); it != blobs_.end();) {
        it = it->first.rfind(prefix, 0) == 0 ? blobs_.erase(it) : std::next(it);
    }
#endif
    return true;
}

// ResourceStore — Emscripten virtual FS
bool WebPlatform::mount() {
    resourcesMounted_ = true;
    resourceBytes_ = 0;
    return true;
}

Platform::ResourceFile WebPlatform::open(const char* path) {
    if (!resourcesMounted_ || !path) return {};
    // Emscripten virtual FS paths are relative to / or absolute
    auto* file = new (std::nothrow) WebResourceFile(path);
    if (!file || !file->valid()) {
        delete file;
        return {};
    }
    return Platform::ResourceFile(file);
}

// Memory
size_t WebPlatform::externalMemoryFree() const {
    return externalUsed_ < kExternalMemSize ? kExternalMemSize - externalUsed_ : 0;
}

void* WebPlatform::allocate(size_t bytes, bool preferExternal) {
    if (bytes == 0) return nullptr;
    if (preferExternal && externalUsed_ + bytes > kExternalMemSize) {
        return nullptr;
    }
    void* ptr = std::malloc(bytes);
    if (ptr && preferExternal) {
        externalAllocs_[ptr] = bytes;
        externalUsed_ += bytes;
    }
    return ptr;
}

void WebPlatform::release(void* memory) {
    if (!memory) return;
    auto it = externalAllocs_.find(memory);
    if (it != externalAllocs_.end()) {
        externalUsed_ -= it->second;
        externalAllocs_.erase(it);
    }
    std::free(memory);
}
