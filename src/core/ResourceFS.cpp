#include "core/ResourceFS.h"
#include <Arduino.h>
#include "platform/api/PlatformServices.h"

ResourceFS& ResourceFS::ins() {
    static ResourceFS instance;
    return instance;
}

bool ResourceFS::begin() {
    if (mounted_) return true;

    if (!Platform::resources().mount()) {
        Serial.println("[ResourceFS] resource store mount failed");
        return false;
    }

    mounted_ = true;
    Serial.printf("[ResourceFS] LittleFS mounted total=%u used=%u\n",
                  (unsigned)Platform::resources().totalBytes(),
                  (unsigned)Platform::resources().usedBytes());
    return true;
}

size_t ResourceFS::totalBytes() const {
    return mounted_ ? Platform::resources().totalBytes() : 0;
}

size_t ResourceFS::usedBytes() const {
    return mounted_ ? Platform::resources().usedBytes() : 0;
}

Platform::ResourceFile ResourceFS::open(const char* path) {
    return mounted_ ? Platform::resources().open(path) : Platform::ResourceFile{};
}
