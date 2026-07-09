#pragma once

#include <FS.h>
#include <cstddef>

class ResourceFS {
public:
    static ResourceFS& ins();

    bool begin();
    bool mounted() const { return mounted_; }
    size_t totalBytes() const;
    size_t usedBytes() const;
    fs::FS& fs();

private:
    ResourceFS() = default;

    bool mounted_ = false;
};
