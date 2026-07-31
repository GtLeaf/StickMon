#include "core/ResourcePack.h"

#include "core/ResourceFS.h"
#include "platform/api/PlatformServices.h"

#include <cstdio>
#include <cstring>

namespace {
constexpr const char* ACTIVE_CONFIG_PATH = "/active.json";
constexpr const char* DEFAULT_PACK_ROOT = "/packs/dev";
constexpr const char* PACK_ROOT_PREFIX = "/packs/";
constexpr const char* PACK_FORMAT = "smon-resource-pack-v1";
constexpr const char* DEFAULT_SPRITES_DIR = "sprites";
constexpr const char* DEFAULT_CRIES_DIR = "cries";
constexpr const char* DEFAULT_ROOMS_DIR = "rooms";
constexpr const char* DEFAULT_FONTS_DIR = "fonts";
constexpr const char* DEFAULT_ROOM_PATH = "rooms/standard.smonroom";
constexpr const char* DEFAULT_FONT_PATH = "fonts/zh16.smonfont";
constexpr const char* DEFAULT_UI_ASSETS_PATH = "game/ui.smonfx";
constexpr const char* DEFAULT_BATTLE_ASSETS_PATH = "game/battle.smonfx";
constexpr const char* DEFAULT_MAP_ASSETS_PATH = "game/maps.smonfx";
constexpr const char* DEFAULT_HATCH_ASSETS_PATH = "game/hatch.smonfx";
constexpr size_t CONFIG_CAP = 1024;

void copyText(char* dest, size_t cap, const char* value) {
    if (!dest || cap == 0) return;
    if (!value) value = "";
    std::snprintf(dest, cap, "%s", value);
}

bool readTextFile(const char* path, char* buffer, size_t cap) {
    if (!path || !buffer || cap == 0) return false;
    Platform::ResourceFile file = ResourceFS::ins().open(path);
    if (!file) return false;
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

int hexDigitValue(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

bool parseJsonHex4(const char*& pos, uint16_t& value) {
    value = 0;
    for (uint8_t i = 0; i < 4; ++i) {
        if (pos[i] == '\0') return false;
        int digit = hexDigitValue(pos[i]);
        if (digit < 0) return false;
        value = static_cast<uint16_t>((value << 4) | digit);
    }
    pos += 4;
    return true;
}

bool appendUtf8Codepoint(uint32_t codepoint, char* out, size_t cap, size_t& len) {
    if (codepoint == 0 || codepoint > 0x10FFFF ||
        (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
        return false;
    }
    uint8_t bytes = codepoint <= 0x7F ? 1 :
                    codepoint <= 0x7FF ? 2 :
                    codepoint <= 0xFFFF ? 3 : 4;
    if (len + bytes >= cap) return false;
    if (bytes == 1) {
        out[len++] = static_cast<char>(codepoint);
    } else if (bytes == 2) {
        out[len++] = static_cast<char>(0xC0 | (codepoint >> 6));
        out[len++] = static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (bytes == 3) {
        out[len++] = static_cast<char>(0xE0 | (codepoint >> 12));
        out[len++] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        out[len++] = static_cast<char>(0x80 | (codepoint & 0x3F));
    } else {
        out[len++] = static_cast<char>(0xF0 | (codepoint >> 18));
        out[len++] = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
        out[len++] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        out[len++] = static_cast<char>(0x80 | (codepoint & 0x3F));
    }
    return true;
}

bool extractJsonString(const char* json, const char* key, char* out, size_t cap) {
    const char* pos = findJsonValue(json, key);
    if (!pos || *pos != '"' || cap == 0) return false;
    ++pos;
    size_t len = 0;
    while (*pos && *pos != '"') {
        uint8_t value = static_cast<uint8_t>(*pos++);
        if (value < 0x20) return false;
        if (value != '\\') {
            if (len + 1 >= cap) return false;
            out[len++] = static_cast<char>(value);
            continue;
        }

        char escaped = *pos++;
        if (escaped == '\0') return false;
        switch (escaped) {
        case '"': value = '"'; break;
        case '\\': value = '\\'; break;
        case '/': value = '/'; break;
        case 'b': value = '\b'; break;
        case 'f': value = '\f'; break;
        case 'n': value = '\n'; break;
        case 'r': value = '\r'; break;
        case 't': value = '\t'; break;
        case 'u': {
            uint16_t first = 0;
            if (!parseJsonHex4(pos, first)) return false;
            uint32_t codepoint = first;
            if (first >= 0xD800 && first <= 0xDBFF) {
                if (pos[0] != '\\' || pos[1] != 'u') return false;
                pos += 2;
                uint16_t second = 0;
                if (!parseJsonHex4(pos, second) ||
                    second < 0xDC00 || second > 0xDFFF) {
                    return false;
                }
                codepoint = 0x10000UL +
                    ((static_cast<uint32_t>(first) - 0xD800UL) << 10) +
                    (static_cast<uint32_t>(second) - 0xDC00UL);
            } else if (first >= 0xDC00 && first <= 0xDFFF) {
                return false;
            }
            if (!appendUtf8Codepoint(codepoint, out, cap, len)) return false;
            continue;
        }
        default:
            return false;
        }
        if (len + 1 >= cap) return false;
        out[len++] = static_cast<char>(value);
    }
    out[len] = '\0';
    return len > 0 && *pos == '"';
}

enum class JsonStringStatus : uint8_t {
    MISSING,
    VALID,
    INVALID,
};

JsonStringStatus extractJsonStringAlias(const char* json,
                                        const char* primary,
                                        const char* fallback,
                                        char* out,
                                        size_t cap) {
    const char* key = findJsonValue(json, primary) ? primary :
                      findJsonValue(json, fallback) ? fallback : nullptr;
    if (!key) return JsonStringStatus::MISSING;
    return extractJsonString(json, key, out, cap)
        ? JsonStringStatus::VALID
        : JsonStringStatus::INVALID;
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

bool isSafeVersionText(const char* value) {
    if (!value || value[0] == '\0') return false;
    for (const char* pos = value; *pos; ++pos) {
        char ch = *pos;
        bool allowed = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                       (ch >= '0' && ch <= '9') || ch == '.' || ch == '-' ||
                       ch == '_' || ch == '+';
        if (!allowed) return false;
    }
    return true;
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
        Platform::logLine("[ResourcePack] resource FS unavailable");
        return false;
    }

    loadActiveConfig();
    active_ = loadManifest();
    if (active_) {
        Platform::logf("[ResourcePack] active id=%s version=%s schema=%u root=%s\n",
                      id_, version_, schema_, root_);
    } else {
        Platform::logf("[ResourcePack] no active pack root=%s\n", root_);
    }
    return active_;
}

bool ResourcePack::openSpriteBlock(uint16_t speciesId, Platform::ResourceFile& file) const {
    if (!active_) return false;
    char relative[64];
    int written = std::snprintf(relative, sizeof(relative), "%s/%03u.smonsp", spritesDir_, speciesId);
    return written > 0 && static_cast<size_t>(written) < sizeof(relative) && openRelative(relative, file);
}

bool ResourcePack::openCry(uint16_t speciesId, Platform::ResourceFile& file) const {
    if (!active_) return false;
    char relative[64];
    int written = std::snprintf(
        relative, sizeof(relative), "%s/%03u.smoncry", criesDir_, speciesId);
    return written > 0 && static_cast<size_t>(written) < sizeof(relative) &&
           openRelative(relative, file);
}

bool ResourcePack::openRoom(const char* roomId, Platform::ResourceFile& file) const {
    if (!active_ || !isSafePackName(roomId)) return false;
    if (std::strcmp(roomId, "standard") == 0) return openRelative(defaultRoomPath_, file);
    char relative[64];
    int written = std::snprintf(relative, sizeof(relative), "%s/%s.smonroom", roomsDir_, roomId);
    return written > 0 && static_cast<size_t>(written) < sizeof(relative) && openRelative(relative, file);
}

bool ResourcePack::openFont(const char* fontId, Platform::ResourceFile& file) const {
    if (!active_ || !isSafePackName(fontId)) return false;
    char relative[64];
    int written = std::snprintf(relative, sizeof(relative), "%s/%s.smonfont", fontsDir_, fontId);
    return written > 0 && static_cast<size_t>(written) < sizeof(relative) && openRelative(relative, file);
}

bool ResourcePack::openDefaultFont(Platform::ResourceFile& file) const {
    return active_ && openRelative(defaultFontPath_, file);
}

bool ResourcePack::openUiAssets(Platform::ResourceFile& file) const {
    return active_ && openRelative(uiAssetsPath_, file);
}

bool ResourcePack::openBattleAssets(Platform::ResourceFile& file) const {
    return active_ && openRelative(battleAssetsPath_, file);
}

bool ResourcePack::openMapAssets(Platform::ResourceFile& file) const {
    return active_ && openRelative(mapAssetsPath_, file);
}

bool ResourcePack::openHatchAssets(Platform::ResourceFile& file) const {
    return active_ && openRelative(hatchAssetsPath_, file);
}

bool ResourcePack::openRelative(const char* relativePath, Platform::ResourceFile& file) const {
    if (!active_ || !isSafePathText(relativePath, false)) return false;
    char path[128];
    int written = std::snprintf(path, sizeof(path), "%s/%s", root_, relativePath);
    if (written <= 0 || static_cast<size_t>(written) >= sizeof(path)) return false;
    file = ResourceFS::ins().open(path);
    return static_cast<bool>(file);
}

void ResourcePack::setDefaultRoot() {
    schema_ = 0;
    copyText(root_, sizeof(root_), DEFAULT_PACK_ROOT);
    id_[0] = '\0';
    version_[0] = '\0';
    copyText(spritesDir_, sizeof(spritesDir_), DEFAULT_SPRITES_DIR);
    copyText(criesDir_, sizeof(criesDir_), DEFAULT_CRIES_DIR);
    copyText(roomsDir_, sizeof(roomsDir_), DEFAULT_ROOMS_DIR);
    copyText(fontsDir_, sizeof(fontsDir_), DEFAULT_FONTS_DIR);
    copyText(defaultRoomPath_, sizeof(defaultRoomPath_), DEFAULT_ROOM_PATH);
    copyText(defaultFontPath_, sizeof(defaultFontPath_), DEFAULT_FONT_PATH);
    copyText(uiAssetsPath_, sizeof(uiAssetsPath_), DEFAULT_UI_ASSETS_PATH);
    copyText(battleAssetsPath_, sizeof(battleAssetsPath_), DEFAULT_BATTLE_ASSETS_PATH);
    copyText(mapAssetsPath_, sizeof(mapAssetsPath_), DEFAULT_MAP_ASSETS_PATH);
    copyText(hatchAssetsPath_, sizeof(hatchAssetsPath_), DEFAULT_HATCH_ASSETS_PATH);
}

bool ResourcePack::loadActiveConfig() {
    char json[CONFIG_CAP];
    if (!readTextFile(ACTIVE_CONFIG_PATH, json, sizeof(json))) return false;

    char path[sizeof(root_)] = {};
    JsonStringStatus pathStatus =
        extractJsonStringAlias(json, "packPath", "path", path, sizeof(path));
    if (pathStatus == JsonStringStatus::INVALID) {
        Platform::logLine("[ResourcePack] invalid pack root config");
        return false;
    }
    if (pathStatus == JsonStringStatus::VALID) {
        if (!isSafePathText(path, true)) {
            Platform::logf("[ResourcePack] invalid pack root: %s\n", path);
            return false;
        }
        copyText(root_, sizeof(root_), path);
        return true;
    }

    char packName[32] = {};
    JsonStringStatus packStatus =
        extractJsonStringAlias(json, "activePack", "pack",
                               packName, sizeof(packName));
    if (packStatus == JsonStringStatus::INVALID) {
        Platform::logLine("[ResourcePack] invalid active pack config");
        return false;
    }
    if (packStatus == JsonStringStatus::VALID) {
        if (!isSafePackName(packName)) {
            Platform::logf("[ResourcePack] invalid pack id: %s\n", packName);
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
        Platform::logf("[ResourcePack] manifest format missing path=%s\n", path);
        return false;
    }
    if (std::strcmp(format, PACK_FORMAT) != 0) {
        Platform::logf("[ResourcePack] unsupported manifest format=%s expected=%s path=%s\n",
                      format,
                      PACK_FORMAT,
                      path);
        return false;
    }
    if (!extractJsonUint16(json, "schema", schema_) || schema_ != SUPPORTED_SCHEMA) {
        Platform::logf("[ResourcePack] unsupported schema=%u\n", schema_);
        return false;
    }

    bool hasId = findJsonValue(json, "id") != nullptr;
    bool hasVersion = findJsonValue(json, "version") != nullptr;
    if ((hasId && !extractJsonString(json, "id", id_, sizeof(id_))) ||
        (hasVersion && !extractJsonString(json, "version", version_, sizeof(version_)))) {
        Platform::logLine("[ResourcePack] invalid manifest metadata");
        return false;
    }
    if (id_[0] == '\0') copyText(id_, sizeof(id_), "dev");
    if (version_[0] == '\0') copyText(version_, sizeof(version_), "0.0.0");
    if (!isSafePackName(id_) || !isSafeVersionText(version_)) {
        Platform::logLine("[ResourcePack] unsafe manifest metadata");
        return false;
    }

    if (!copyOptionalPath(json, "sprites", spritesDir_, sizeof(spritesDir_)) ||
        !copyOptionalPath(json, "cries", criesDir_, sizeof(criesDir_)) ||
        !copyOptionalPath(json, "rooms", roomsDir_, sizeof(roomsDir_)) ||
        !copyOptionalPath(json, "fonts", fontsDir_, sizeof(fontsDir_)) ||
        !copyOptionalPath(json, "room", defaultRoomPath_, sizeof(defaultRoomPath_)) ||
        !copyOptionalPath(json, "font", defaultFontPath_, sizeof(defaultFontPath_)) ||
        !copyOptionalPath(json, "uiAssets", uiAssetsPath_, sizeof(uiAssetsPath_)) ||
        !copyOptionalPath(json, "battleAssets", battleAssetsPath_, sizeof(battleAssetsPath_)) ||
        !copyOptionalPath(json, "mapAssets", mapAssetsPath_, sizeof(mapAssetsPath_)) ||
        !copyOptionalPath(json, "hatchAssets", hatchAssetsPath_, sizeof(hatchAssetsPath_))) {
        Platform::logLine("[ResourcePack] invalid resource path in manifest");
        return false;
    }
    return true;
}
