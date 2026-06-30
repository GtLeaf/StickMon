#include "core/SaveManager.h"
#include <Arduino.h>
#include <Preferences.h>
#include <cstring>

namespace {
constexpr const char* NVS_NS = "stickmon";
constexpr const char* NVS_KEY = "state";
constexpr const char* HATCH_KEY = "hatch";
constexpr const char* CLOCK_KEY = "clock_min";
}

bool SaveManager::begin() {
    Preferences prefs;
    bool ok = prefs.begin(NVS_NS, false);
    prefs.end();
    return ok;
}

bool SaveManager::load(Game::GameState& state) {
    Preferences prefs;
    if (!prefs.begin(NVS_NS, true)) return false;
    size_t len = prefs.getBytesLength(NVS_KEY);
    if (len != sizeof(Game::GameState)) {
        Serial.printf("[SaveManager] state size mismatch: %u != %u\n", (unsigned)len, (unsigned)sizeof(Game::GameState));
        prefs.end();
        reset(state);
        return false;
    }

    Game::GameState loaded;
    size_t read = prefs.getBytes(NVS_KEY, &loaded, sizeof(loaded));
    prefs.end();
    if (read != sizeof(loaded) ||
        loaded.magic != Game::SAVE_MAGIC ||
        loaded.version != Game::SAVE_VERSION ||
        loaded.checksum != checksum(loaded)) {
        Serial.printf("[SaveManager] state invalid read=%u magic=%08lx version=%u checksum=%04x/%04x\n",
                      (unsigned)read,
                      (unsigned long)loaded.magic,
                      loaded.version,
                      loaded.checksum,
                      checksum(loaded));
        reset(state);
        return false;
    }

    state = loaded;
    return true;
}

bool SaveManager::save(const Game::GameState& state) {
    Game::GameState copy = state;
    copy.magic = Game::SAVE_MAGIC;
    copy.version = Game::SAVE_VERSION;
    copy.checksum = checksum(copy);

    Preferences prefs;
    if (!prefs.begin(NVS_NS, false)) return false;
    size_t written = prefs.putBytes(NVS_KEY, &copy, sizeof(copy));
    prefs.end();
    return written == sizeof(copy);
}

void SaveManager::reset(Game::GameState& state) {
    state = Game::GameState{};
}

bool SaveManager::loadClock(uint32_t& gameMinutesTotal) {
    Preferences prefs;
    if (!prefs.begin(NVS_NS, true)) return false;
    bool exists = prefs.isKey(CLOCK_KEY);
    if (exists) gameMinutesTotal = prefs.getUInt(CLOCK_KEY, 0);
    prefs.end();
    return exists;
}

bool SaveManager::saveClock(uint32_t gameMinutesTotal) {
    Preferences prefs;
    if (!prefs.begin(NVS_NS, false)) return false;
    size_t written = prefs.putUInt(CLOCK_KEY, gameMinutesTotal);
    prefs.end();
    return written == sizeof(gameMinutesTotal);
}

bool SaveManager::loadHatchProgress(Game::HatchProgress& progress) {
    Preferences prefs;
    if (!prefs.begin(NVS_NS, true)) return false;
    size_t len = prefs.getBytesLength(HATCH_KEY);
    if (len != sizeof(Game::HatchProgress)) {
        prefs.end();
        progress = Game::HatchProgress{};
        return false;
    }

    Game::HatchProgress loaded;
    size_t read = prefs.getBytes(HATCH_KEY, &loaded, sizeof(loaded));
    prefs.end();
    if (read != sizeof(loaded) ||
        loaded.magic != Game::HATCH_MAGIC ||
        loaded.checksum != checksum(loaded)) {
        progress = Game::HatchProgress{};
        return false;
    }

    progress = loaded;
    return true;
}

bool SaveManager::saveHatchProgress(const Game::HatchProgress& progress) {
    Game::HatchProgress copy = progress;
    copy.magic = Game::HATCH_MAGIC;
    copy.checksum = checksum(copy);

    Preferences prefs;
    if (!prefs.begin(NVS_NS, false)) return false;
    size_t written = prefs.putBytes(HATCH_KEY, &copy, sizeof(copy));
    prefs.end();
    return written == sizeof(copy);
}

void SaveManager::clearHatchProgress() {
    Preferences prefs;
    if (!prefs.begin(NVS_NS, false)) return;
    prefs.remove(HATCH_KEY);
    prefs.end();
}

uint16_t SaveManager::checksum(const Game::GameState& state) {
    Game::GameState copy = state;
    copy.checksum = 0;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&copy);
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < sizeof(copy); ++i) {
        crc ^= bytes[i];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
        }
    }
    return crc;
}

uint16_t SaveManager::checksum(const Game::HatchProgress& progress) {
    Game::HatchProgress copy = progress;
    copy.checksum = 0;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&copy);
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < sizeof(copy); ++i) {
        crc ^= bytes[i];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
        }
    }
    return crc;
}
