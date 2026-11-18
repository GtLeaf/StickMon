#include "AdaptiveStandby.h"
#include <cstring>
#include "SdgoodsPlatform.h"
#include "core/ContentStore.h"
#include "core/ContentValidation.h"
#include "core/SaveManager.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "esp_system.h"

namespace Sdgoods {
namespace {
RTC_NOINIT_ATTR uint8_t retainedBytes[sizeof(StandbyCheckpoint)];
struct BootStorage {
    SdgoodsPlatform& board;
    ContentStore& catalog;
    SaveManager saveManager;
    bool begin() { return board.beginStandbyStorage(); }
    bool load(Game::GameState& state, MainSceneViewState& view) {
        // Keep authenticated bank validation; timer boot initializes no UI devices.
        ContentData::bind(&catalog);
        SaveManager::LoadStatus status;
        return validateContentCatalog() && catalog.healthy() && saveManager.begin() &&
            saveManager.load(state, view, nullptr, &status);
    }
    bool save(const Game::GameState& state, const MainSceneViewState& view) {
        return saveManager.saveSnapshot(state, view);
    }
};
}
StandbyCheckpoint StandbyRetention::read() const {
    StandbyCheckpoint c{};
    std::memcpy(&c, retainedBytes, sizeof(c));
    return c;
}
void StandbyRetention::store(const StandbyCheckpoint& c) { std::memcpy(retainedBytes, &c, sizeof(c)); }
void StandbyRetention::clear() { std::memset(retainedBytes, 0, sizeof(retainedBytes)); }
StandbyBoot resumeStandbyBoot(SdgoodsPlatform& board, ContentStore& catalog) {
    const auto cause = esp_sleep_get_wakeup_cause();
    const auto wake = cause == ESP_SLEEP_WAKEUP_TIMER ? StandbyWake::TIMER :
        cause == ESP_SLEEP_WAKEUP_EXT0 ? StandbyWake::KEY : StandbyWake::OTHER;
    StandbyRetention retention;
    BootStorage storage{board, catalog, {}};
    return resumeStandbyBootFlow(board, storage, retention, wake, esp_reset_reason() == ESP_RST_DEEPSLEEP);
}
}
