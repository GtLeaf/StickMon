#pragma once

#include <FS.h>
#include <cstdint>

class ResourcePack {
public:
    static constexpr uint16_t SUPPORTED_SCHEMA = 1;

    static ResourcePack& ins();

    bool begin();
    bool active() const { return active_; }
    const char* root() const { return root_; }
    const char* id() const { return id_; }
    const char* version() const { return version_; }
    uint16_t schema() const { return schema_; }

    bool openSpriteBlock(uint16_t speciesId, fs::File& file) const;
    bool openRoom(const char* roomId, fs::File& file) const;
    bool openFont(const char* fontId, fs::File& file) const;
    bool openDefaultFont(fs::File& file) const;
    bool openGameAssets(fs::File& file) const;

private:
    ResourcePack() = default;

    bool loadActiveConfig();
    bool loadManifest();
    bool openRelative(const char* relativePath, fs::File& file) const;
    void setDefaultRoot();

    bool initialized_ = false;
    bool active_ = false;
    uint16_t schema_ = 0;
    char root_[64] = {};
    char id_[32] = {};
    char version_[24] = {};
    char spritesDir_[32] = {};
    char roomsDir_[32] = {};
    char fontsDir_[32] = {};
    char defaultRoomPath_[64] = {};
    char defaultFontPath_[64] = {};
    char gameAssetsPath_[64] = {};
};
