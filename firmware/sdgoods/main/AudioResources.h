// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ResourceArchive.h"

namespace Sdgoods {
// Embedded resources stay authoritative. An optional, read-only audio archive
// supplies the sounds which do not fit in the 3 MiB application partition.
class AudioResources final : public Platform::IResourceStore {
public:
    AudioResources(const uint8_t* data, size_t size) : embedded_(data, size) {}
    bool attachAudio(const uint8_t* data, size_t size) {
        ResourceArchive candidate(data, size);
        if (!candidate.mount()) return false;
        audio_ = candidate;
        audioReady_ = true;
        return true;
    }
    bool mount() override { return embedded_.mount(); }
    size_t totalBytes() const override {
        return embedded_.totalBytes() + (audioReady_ ? audio_.totalBytes() : 0);
    }
    size_t usedBytes() const override { return totalBytes(); }
    Platform::ResourceFile open(const char* path) override {
        auto file = embedded_.open(path);
        if (file || !audioReady_ || !path) return file;
        const size_t n = std::strlen(path);
        const bool sound = (n >= 10 && std::strcmp(path+n-10, ".smonaudio") == 0) ||
                           (n >= 8 && std::strcmp(path+n-8, ".smoncry") == 0);
        return sound ? audio_.open(path) : Platform::ResourceFile{};
    }
private:
    ResourceArchive embedded_;
    ResourceArchive audio_{nullptr, 0};
    bool audioReady_ = false;
};
}  // namespace Sdgoods
