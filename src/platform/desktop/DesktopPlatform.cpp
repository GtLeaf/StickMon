#include "platform/desktop/DesktopPlatform.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <new>

namespace {

class DesktopResourceFile final : public Platform::IResourceFile {
public:
    explicit DesktopResourceFile(const std::filesystem::path& path)
        : stream_(path, std::ios::binary) {
        if (!stream_) return;
        stream_.seekg(0, std::ios::end);
        size_ = static_cast<size_t>(stream_.tellg());
        stream_.seekg(0, std::ios::beg);
    }

    bool valid() const override { return stream_.is_open(); }
    size_t size() const override { return valid() ? size_ : 0; }
    size_t position() const override {
        return valid() ? static_cast<size_t>(stream_.tellg()) : 0;
    }
    size_t read(void* output, size_t length) override {
        if (!valid() || !output) return 0;
        stream_.read(static_cast<char*>(output), static_cast<std::streamsize>(length));
        return static_cast<size_t>(stream_.gcount());
    }
    bool seek(size_t position) override {
        if (!valid() || position > size_) return false;
        stream_.clear();
        stream_.seekg(static_cast<std::streamoff>(position), std::ios::beg);
        return static_cast<bool>(stream_);
    }
    void close() override {
        if (stream_.is_open()) stream_.close();
    }

private:
    mutable std::ifstream stream_;
    size_t size_ = 0;
};

}  // namespace

DesktopPlatform::DesktopPlatform(std::string resourceRoot)
    : services_{*this, *this, *this, *this, *this, *this, *this, *this,
                *this, *this, *this, *this},
      resourceRoot_(std::move(resourceRoot)),
      frameBuffer_(static_cast<size_t>(Platform::LOGICAL_DISPLAY_W) *
                   Platform::LOGICAL_DISPLAY_H) {}

bool DesktopPlatform::begin() {
    initialized_ = true;
    return true;
}

uint32_t DesktopPlatform::millis() const { return nowMs_; }

void DesktopPlatform::sleepMs(uint32_t durationMs) { nowMs_ += durationMs; }

void DesktopPlatform::write(const char* text, size_t length) {
    if (text && length > 0) logs_.append(text, length);
}

bool DesktopPlatform::pressed(Platform::InputButton button) const {
    size_t index = static_cast<size_t>(button);
    return index < buttons_.size() && buttons_[index];
}

void DesktopPlatform::setPressed(Platform::InputButton button, bool value) {
    size_t index = static_cast<size_t>(button);
    if (index < buttons_.size()) buttons_[index] = value;
}

Platform::FrameBuffer565 DesktopPlatform::frameBuffer() {
    return {frameBuffer_.data(), Platform::LOGICAL_DISPLAY_W,
            Platform::LOGICAL_DISPLAY_H, false};
}

void DesktopPlatform::setVolume(uint8_t percent) {
    volume_ = std::min<uint8_t>(percent, 100);
}

bool DesktopPlatform::playPcmU8(const uint8_t* data, size_t sampleCount,
                                uint32_t sampleRate) {
    if (!initialized_ || !data || sampleCount == 0 || sampleRate == 0 ||
        volume_ == 0 || microphoneActive_) return false;
    lastAudio_.assign(data, data + sampleCount);
    ++audioPlayCount_;
    playing_ = true;
    return true;
}

bool DesktopPlatform::beginMicrophone() {
    playing_ = false;
    microphoneActive_ = initialized_;
    return microphoneActive_;
}

bool DesktopPlatform::recordMicrophone(int16_t* data, size_t sampleCount,
                                       uint32_t sampleRate) {
    if (!microphoneActive_ || !data || sampleCount == 0 || sampleRate == 0) {
        return false;
    }
    std::fill(data, data + sampleCount, 0);
    return true;
}

bool DesktopPlatform::readAcceleration(float& x, float& y, float& z) {
    x = 0.0f;
    y = 0.0f;
    z = 1.0f;
    return true;
}

uint32_t DesktopPlatform::hardwareRandom() {
    randomState_ = randomState_ * 1664525UL + 1013904223UL;
    return randomState_;
}

std::string DesktopPlatform::blobKey(const char* nameSpace, const char* key) {
    if (!nameSpace || !key) return {};
    return std::string(nameSpace) + '\n' + key;
}

size_t DesktopPlatform::blobSize(const char* nameSpace, const char* key) {
    auto found = blobs_.find(blobKey(nameSpace, key));
    return found == blobs_.end() ? 0 : found->second.size();
}

bool DesktopPlatform::readBlob(const char* nameSpace, const char* key,
                               void* output, size_t length) {
    auto found = blobs_.find(blobKey(nameSpace, key));
    if (!output || found == blobs_.end() || found->second.size() != length) {
        return false;
    }
    std::memcpy(output, found->second.data(), length);
    return true;
}

bool DesktopPlatform::writeBlob(const char* nameSpace, const char* key,
                                const void* data, size_t length) {
    std::string combined = blobKey(nameSpace, key);
    if (combined.empty() || !data || length == 0) return false;
    const auto* bytes = static_cast<const uint8_t*>(data);
    blobs_[combined] = std::vector<uint8_t>(bytes, bytes + length);
    return true;
}

bool DesktopPlatform::removeBlob(const char* nameSpace, const char* key) {
    blobs_.erase(blobKey(nameSpace, key));
    return true;
}

bool DesktopPlatform::clearNamespace(const char* nameSpace) {
    if (!nameSpace) return false;
    std::string prefix = std::string(nameSpace) + '\n';
    for (auto it = blobs_.begin(); it != blobs_.end();) {
        it = it->first.rfind(prefix, 0) == 0 ? blobs_.erase(it) : std::next(it);
    }
    return true;
}

bool DesktopPlatform::mount() {
    std::error_code error;
    resourcesMounted_ = std::filesystem::is_directory(resourceRoot_, error);
    resourceBytes_ = 0;
    if (!resourcesMounted_) return false;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(resourceRoot_, error)) {
        if (error) break;
        if (entry.is_regular_file(error)) {
            resourceBytes_ += static_cast<size_t>(entry.file_size(error));
        }
    }
    return true;
}

Platform::ResourceFile DesktopPlatform::open(const char* path) {
    if (!resourcesMounted_ || !path) return {};
    std::filesystem::path relative(path[0] == '/' ? path + 1 : path);
    if (relative.empty() || relative.is_absolute()) return {};
    for (const auto& part : relative) {
        if (part == "..") return {};
    }
    auto* file = new (std::nothrow) DesktopResourceFile(
        std::filesystem::path(resourceRoot_) / relative);
    if (!file || !file->valid()) {
        delete file;
        return {};
    }
    return Platform::ResourceFile(file);
}

void* DesktopPlatform::allocate(size_t bytes, bool) {
    return bytes > 0 ? std::malloc(bytes) : nullptr;
}

void DesktopPlatform::release(void* memory) { std::free(memory); }

bool DesktopPlatform::enable() {
    peersActive_ = true;
    peerQueue_.clear();
    return true;
}

void DesktopPlatform::end() {
    peersActive_ = false;
    peerQueue_.clear();
}

bool DesktopPlatform::send(const uint8_t destination[6], const void* data,
                           size_t length) {
    if (!peersActive_ || !destination || !data || length == 0 ||
        length > Platform::PeerPacket::MAX_PAYLOAD_BYTES) return false;
    Platform::PeerPacket packet;
    std::memcpy(packet.source, destination, sizeof(packet.source));
    std::memcpy(packet.payload, data, length);
    packet.length = length;
    peerQueue_.push_back(packet);
    return true;
}

bool DesktopPlatform::receive(Platform::PeerPacket& packet) {
    if (!peersActive_ || peerQueue_.empty()) return false;
    packet = peerQueue_.front();
    peerQueue_.pop_front();
    return true;
}
