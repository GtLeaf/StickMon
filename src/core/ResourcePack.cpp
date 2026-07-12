#include "core/ResourcePack.h"

#include "core/ResourceFS.h"

#include <Arduino.h>
#include <cstdio>
#include <cstring>

namespace {
constexpr const char* ACTIVE_CONFIG_PATH = "/active.json";
constexpr const char* DEFAULT_PACK_ROOT = "/packs/dev";
constexpr const char* PACK_ROOT_PREFIX = "/packs/";
constexpr const char* PACK_FORMAT = "smon-resource-pack-v1";
constexpr const char* DEFAULT_SPRITES_DIR = "sprites";
constexpr const char* DEFAULT_ROOMS_DIR = "rooms";
constexpr const char* DEFAULT_FONTS_DIR = "fonts";
constexpr const char* DEFAULT_ROOM_PATH = "rooms/standard.smonroom";
constexpr const char* DEFAULT_FONT_PATH = "fonts/zh16.smonfont";
constexpr const char* DEFAULT_GAME_ASSETS_PATH = "game/game.smonfx";
constexpr size_t CONFIG_CAP = 1024;

void copyText(char* dest, size_t cap, const char* value) {
    if (!dest || cap == 0) return;
    if (!value) value = "";
    std::snprintf(dest, cap, "%s", value);
}

bool readTextFile(const char* path, char* buffer, size_t cap) {
    if (!path || !buffer || cap == 0) return false;
    fs::File file = ResourceFS::ins().fs().open(path, "r");
    if (!file || file.isDirectory()) return false;
    size_t size = file.size();
    if (size == 0 || size >= cap) return false;
    size_t read = file.read(reinterpret_cast<uint8_t*>(buffer), size);
    buffer[read] = '\0';
    return read == size;
}

const char* skipWhitespace(const char* pos) {
    while (pos && (*pos == ' ' || *pos == '\n' || *pos == '\r' || *pos == '\t')) ++pos;
    return pos;
}

const char* findJsonValue(const char* json, const char* key) {
    if (!json || !key) return nullptr;
    size_t keyLen = std::strlen(key);
    uint16_t depth = 0;
    const char* pos = json;
    while (*pos) {
        if (*pos == '{' || *pos == '[') {
            ++depth;
            ++pos;
            continue;
        }
        if (*pos == '}' || *pos == ']') {
            if (depth > 0) --depth;
            ++pos;
            continue;
        }
        if (*pos != '"') {
            ++pos;
            continue;
        }

        const char* start = ++pos;
        bool escaped = false;
        while (*pos) {
            if (!escaped && *pos == '"') break;
            if (!escaped && *pos == '\\') escaped = true;
            else escaped = false;
            ++pos;
        }
        if (*pos != '"') return nullptr;

        const char* after = skipWhitespace(pos + 1);
        bool isTopLevelKey = depth == 1 && *after == ':';
        if (isTopLevelKey && static_cast<size_t>(pos - start) == keyLen &&
            std::strncmp(start, key, keyLen) == 0) {
            return skipWhitespace(after + 1);
        }
        ++pos;
    }
    return nullptr;
}

bool extractJsonString(const char* json, const char* key, char* out, size_t cap) {
    const char* pos = findJsonValue(json, key);
    if (!pos || *pos != '"' || cap == 0) return false;
    ++pos;
    size_t len = 0;
    while (*pos && *pos != '"') {
        char value = *pos++;
        if (value == '\\') {
            char escaped = *pos++;
            if (escaped == '"' || escaped == '\\' || escaped == '/') value = escaped;
            else return false;
        }
        if (len + 1 >= cap) return false;
        out[len++] = value;
    }
    out[len] = '\0';
    return len > 0 && *pos == '"';
}

bool extractJsonUint16(const char* json, const char* key, uint16_t& out) {
    const char* pos = findJsonValue(json, key);
    if (!pos || *pos < '0' || *pos > '9') return false;
    uint32_t value = 0;
    while (*pos >= '0' && *pos <= '9') {
        value = value * 10 + static_cast<uint32_t>(*pos - '0');
        if (value > 0xFFFF) return false;
        ++pos;
    }
    pos = skipWhitespace(pos);
    if (*pos != ',' && *pos != '}') return false;
    out = static_cast<uint16_t>(value);
    return true;
}

bool isSafePathText(const char* value, bool absolutePackRoot) {
    if (!value || value[0] == '\0') return false;
    if (absolutePackRoot) {
        if (std::strncmp(value, PACK_ROOT_PREFIX, std::strlen(PACK_ROOT_PREFIX)) != 0) return false;
    } else if (value[0] == '/') {
        return false;
    }

    const char* segment = absolutePackRoot ? value + 1 : value;
    size_t segmentLen = 0;
    for (const char* pos = segment; ; ++pos) {
        char ch = *pos;
        if (ch == '/' || ch == '\0') {
            if (segmentLen == 0) return false;
            const char* segmentStart = pos - segmentLen;
            if ((segmentLen == 1 && segmentStart[0] == '.') ||
                (segmentLen == 2 && segmentStart[0] == '.' && segmentStart[1] == '.')) {
                return false;
            }
            if (ch == '\0') break;
            segmentLen = 0;
            continue;
        }
        bool allowed = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                       (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '.';
        if (!allowed) return false;
        ++segmentLen;
    }
    return true;
}

bool isSafePackName(const char* value) {
    return isSafePathText(value, false) && std::strchr(value, '/') == nullptr;
}

bool copyOptionalPath(const char* json, const char* key, char* dest, size_t cap) {
    if (!findJsonValue(json, key)) return true;
    char value[64] = {};
    if (!extractJsonString(json, key, value, sizeof(value))) return false;
    if (!isSafePathText(value, false) || std::strlen(value) >= cap) return false;
    copyText(dest, cap, value);
    return true;
}
}

ResourcePack& ResourcePack::ins() {
    static ResourcePack instance;
    return instance;
}

bool ResourcePack::begin() {
    if (initialized_) return active_;
    initialized_ = true;
    setDefaultRoot();

    if (!ResourceFS::ins().begin()) {
        Serial.println("[ResourcePack] resource FS unavailable");
        return false;
    }

    loadActiveConfig();
    active_ = loadManifest();
    if (active_) {
        Serial.printf("[ResourcePack] active id=%s version=%s schema=%u root=%s\n",
                      id_, version_, schema_, root_);
    } else {
        Serial.printf("[ResourcePack] no active pack root=%s\n", root_);
    }
    return active_;
}

bool ResourcePack::openSpriteBlock(uint16_t speciesId, fs::File& file) const {
    if (!active_) return false;
    char relative[64];
    int written = std::snprintf(relative, sizeof(relative), "%s/%03u.smonsp", spritesDir_, speciesId);
    return written > 0 && static_cast<size_t>(written) < sizeof(relative) && openRelative(relative, file);
}

bool ResourcePack::openRoom(const char* roomId, fs::File& file) const {
    if (!active_ || !isSafePackName(roomId)) return false;
    if (std::strcmp(roomId, "standard") == 0) return openRelative(defaultRoomPath_, file);
    char relative[64];
    int written = std::snprintf(relative, sizeof(relative), "%s/%s.smonroom", roomsDir_, roomId);
    return written > 0 && static_cast<size_t>(written) < sizeof(relative) && openRelative(relative, file);
}

bool ResourcePack::openFont(const char* fontId, fs::File& file) const {
    if (!active_ || !isSafePackName(fontId)) return false;
    char relative[64];
    int written = std::snprintf(relative, sizeof(relative), "%s/%s.smonfont", fontsDir_, fontId);
    return written > 0 && static_cast<size_t>(written) < sizeof(relative) && openRelative(relative, file);
}

bool ResourcePack::openDefaultFont(fs::File& file) const {
    return active_ && openRelative(defaultFontPath_, file);
}

bool ResourcePack::openGameAssets(fs::File& file) const {
    return active_ && openRelative(gameAssetsPath_, file);
}

bool ResourcePack::openRelative(const char* relativePath, fs::File& file) const {
    if (!active_ || !isSafePathText(relativePath, false)) return false;
    char path[128];
    int written = std::snprintf(path, sizeof(path), "%s/%s", root_, relativePath);
    if (written <= 0 || static_cast<size_t>(written) >= sizeof(path)) return false;
    file = ResourceFS::ins().fs().open(path, "r");
    return file && !file.isDirectory();
}

void ResourcePack::setDefaultRoot() {
    schema_ = 0;
    copyText(root_, sizeof(root_), DEFAULT_PACK_ROOT);
    id_[0] = '\0';
    version_[0] = '\0';
    copyText(spritesDir_, sizeof(spritesDir_), DEFAULT_SPRITES_DIR);
    copyText(roomsDir_, sizeof(roomsDir_), DEFAULT_ROOMS_DIR);
    copyText(fontsDir_, sizeof(fontsDir_), DEFAULT_FONTS_DIR);
    copyText(defaultRoomPath_, sizeof(defaultRoomPath_), DEFAULT_ROOM_PATH);
    copyText(defaultFontPath_, sizeof(defaultFontPath_), DEFAULT_FONT_PATH);
    copyText(gameAssetsPath_, sizeof(gameAssetsPath_), DEFAULT_GAME_ASSETS_PATH);
}

bool ResourcePack::loadActiveConfig() {
    char json[CONFIG_CAP];
    if (!readTextFile(ACTIVE_CONFIG_PATH, json, sizeof(json))) return false;

    char path[sizeof(root_)] = {};
    if (extractJsonString(json, "packPath", path, sizeof(path)) ||
        extractJsonString(json, "path", path, sizeof(path))) {
        if (!isSafePathText(path, true)) {
            Serial.printf("[ResourcePack] invalid pack root: %s\n", path);
            return false;
        }
        copyText(root_, sizeof(root_), path);
        return true;
    }

    char packName[32] = {};
    if (extractJsonString(json, "activePack", packName, sizeof(packName)) ||
        extractJsonString(json, "pack", packName, sizeof(packName))) {
        if (!isSafePackName(packName)) {
            Serial.printf("[ResourcePack] invalid pack id: %s\n", packName);
            return false;
        }
        int written = std::snprintf(root_, sizeof(root_), "/packs/%s", packName);
        return written > 0 && static_cast<size_t>(written) < sizeof(root_);
    }

    return false;
}

bool ResourcePack::loadManifest() {
    if (!isSafePathText(root_, true)) return false;
    char path[96];
    int written = std::snprintf(path, sizeof(path), "%s/manifest.json", root_);
    if (written <= 0 || static_cast<size_t>(written) >= sizeof(path)) return false;

    char json[CONFIG_CAP];
    if (!readTextFile(path, json, sizeof(json))) return false;

    char format[40] = {};
    if (!extractJsonString(json, "format", format, sizeof(format))) {
        Serial.printf("[ResourcePack] manifest format missing path=%s\n", path);
        return false;
    }
    if (std::strcmp(format, PACK_FORMAT) != 0) {
        Serial.printf("[ResourcePack] unsupported manifest format=%s expected=%s path=%s\n",
                      format,
                      PACK_FORMAT,
                      path);
        return false;
    }
    if (!extractJsonUint16(json, "schema", schema_) || schema_ != SUPPORTED_SCHEMA) {
        Serial.printf("[ResourcePack] unsupported schema=%u\n", schema_);
        return false;
    }

    extractJsonString(json, "id", id_, sizeof(id_));
    extractJsonString(json, "version", version_, sizeof(version_));
    if (id_[0] == '\0') copyText(id_, sizeof(id_), "dev");
    if (version_[0] == '\0') copyText(version_, sizeof(version_), "0.0.0");

    if (!copyOptionalPath(json, "sprites", spritesDir_, sizeof(spritesDir_)) ||
        !copyOptionalPath(json, "rooms", roomsDir_, sizeof(roomsDir_)) ||
        !copyOptionalPath(json, "fonts", fontsDir_, sizeof(fontsDir_)) ||
        !copyOptionalPath(json, "room", defaultRoomPath_, sizeof(defaultRoomPath_)) ||
        !copyOptionalPath(json, "font", defaultFontPath_, sizeof(defaultFontPath_)) ||
        !copyOptionalPath(json, "gameAssets", gameAssetsPath_, sizeof(gameAssetsPath_))) {
        Serial.println("[ResourcePack] invalid resource path in manifest");
        return false;
    }
    return true;
}
