#include "core/ResourceFS.h"
#include <Arduino.h>
#include <LittleFS.h>

namespace {
constexpr const char* BASE_PATH = "/assets";
constexpr const char* PARTITION_LABEL = "spiffs";
constexpr uint8_t MAX_OPEN_FILES = 8;
}

ResourceFS& ResourceFS::ins() {
    static ResourceFS instance;
    return instance;
}

bool ResourceFS::begin() {
    if (mounted_) return true;

    if (!LittleFS.begin(true, BASE_PATH, MAX_OPEN_FILES, PARTITION_LABEL)) {
        Serial.println("[ResourceFS] LittleFS mount failed");
        return false;
    }

    mounted_ = true;
    Serial.printf("[ResourceFS] LittleFS mounted total=%u used=%u\n",
                  (unsigned)LittleFS.totalBytes(),
                  (unsigned)LittleFS.usedBytes());
    return true;
}

size_t ResourceFS::totalBytes() const {
    return mounted_ ? LittleFS.totalBytes() : 0;
}

size_t ResourceFS::usedBytes() const {
    return mounted_ ? LittleFS.usedBytes() : 0;
}

fs::FS& ResourceFS::fs() {
    return LittleFS;
}
