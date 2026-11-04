// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <algorithm>
#include <cstring>
#include <new>
#include "platform/api/PlatformServices.h"

namespace Sdgoods {

class ArchiveFile final : public Platform::IResourceFile {
public:
    ArchiveFile(const uint8_t* bytes, size_t length) : bytes_(bytes), length_(length) {}
    bool valid() const override { return bytes_ != nullptr; }
    size_t size() const override { return length_; }
    size_t position() const override { return position_; }
    size_t read(void* output, size_t length) override {
        if (!valid() || !output) return 0;
        length = std::min(length, length_ - position_);
        std::memcpy(output, bytes_ + position_, length);
        position_ += length;
        return length;
    }
    bool seek(size_t position) override {
        if (!valid() || position > length_) return false;
        position_ = position;
        return true;
    }
    void close() override { bytes_ = nullptr; }
private:
    const uint8_t* bytes_;
    size_t length_, position_ = 0;
};

class ResourceArchive final : public Platform::IResourceStore {
public:
    ResourceArchive(const uint8_t* data, size_t size) : data_(data), size_(size) {}
    bool mount() override {
        ready_ = false;
        if (!data_ || size_ < 8 || std::memcmp(data_, "SMRA", 4)) return false;
        count_ = read32(data_ + 4);
        if (count_ > (size_ - 8) / ENTRY_BYTES) return false;
        const size_t indexEnd = 8 + count_ * ENTRY_BYTES;
        for (uint32_t i = 0; i < count_; ++i) {
            const uint8_t* entry = data_ + 8 + i * ENTRY_BYTES;
            const size_t offset = read32(entry + 192), length = read32(entry + 196);
            if (entry[0] != '/' || !std::memchr(entry, 0, 192) ||
                offset < indexEnd || offset > size_ || length > size_ - offset) return false;
        }
        ready_ = true;
        return true;
    }
    size_t totalBytes() const override { return size_; }
    size_t usedBytes() const override { return size_; }
    Platform::ResourceFile open(const char* path) override {
        if (!ready_ || !path) return {};
        for (uint32_t i = 0; i < count_; ++i) {
            const uint8_t* entry = data_ + 8 + i * ENTRY_BYTES;
            if (std::strcmp(path, reinterpret_cast<const char*>(entry))) continue;
            return Platform::ResourceFile(new (std::nothrow) ArchiveFile(
                data_ + read32(entry + 192), read32(entry + 196)));
        }
        return {};
    }
private:
    static uint32_t read32(const uint8_t* p) {
        return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
    }
    static constexpr size_t ENTRY_BYTES = 200;
    const uint8_t* data_;
    size_t size_;
    uint32_t count_ = 0;
    bool ready_ = false;
};
}  // namespace Sdgoods
