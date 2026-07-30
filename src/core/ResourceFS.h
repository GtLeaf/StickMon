#pragma once

#include <cstddef>
#include "platform/api/PlatformServices.h"

class ResourceFS {
public:
    static ResourceFS& ins();

    bool begin();
    bool mounted() const { return mounted_; }
    size_t totalBytes() const;
    size_t usedBytes() const;
    Platform::ResourceFile open(const char* path);

private:
    ResourceFS() = default;

    bool mounted_ = false;
};
