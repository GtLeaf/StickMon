// SPDX-License-Identifier: Apache-2.0
#include "RoomSession.h"
#include "PowerActions.h"
#include "SdgoodsPlatform.h"
#include "game/GameRandom.h"
#include "core/UiStrings.h"
#include "core/BuildConfig.h"
#include "presentation/PixelRenderer.h"
#include "StorageMaintenance.h"
#include "core/AudioManager.h"
#include "core/ContentStore.h"
#include "core/ContentValidation.h"
#include "core/FontResource.h"
#include "core/RoomResource.h"
#include "core/ResourcePack.h"
#include "assets/PokemonSprites.h"
#include "esp_ota_ops.h"

namespace {
Sdgoods::SdgoodsPlatform board;
Sdgoods::RoomSession session;
ContentStore catalog;
constexpr uint32_t LOCKED_LOOP_MS = 40;
void recovery(const char* reason) {
    esp_ota_img_states_t state;
    if(esp_ota_get_state_partition(esp_ota_get_running_partition(),&state)==ESP_OK &&
       state==ESP_OTA_IMG_PENDING_VERIFY && esp_ota_check_rollback_is_possible())
        esp_ota_mark_app_invalid_rollback_and_reboot();
    Sdgoods::storageRecovery(board,reason);
    for(;;) {Sdgoods::pollMaintenanceConsole(board,session,false);board.sleepMs(10);}
}
}

extern "C" void app_main() {
    Platform::bind(board.serviceBundle());
    if (!board.begin()) {
        Platform::errorLine("[SDGOODS] board initialization failed; no game started");
        return;
    }
    Sdgoods::beginMaintenanceConsole();
    board.enableSaveWrites(false);
    if(!board.resourcesReady()) recovery("Missing or invalid resources");
    ContentData::bind(&catalog);
    if(!validateContentCatalog() || !catalog.healthy()) recovery("Invalid content catalog");
    Platform::logf("[SDGOODS] build debug=%u trace=%u\n",
        unsigned(STICKMON_ENABLE_DEBUG_FEATURES), unsigned(STICKMON_ENABLE_TRACE_LOGS));
    GameRandom::seed(board.hardwareRandom());
    if (!session.begin(false)) {
        Platform::errorLine("[SDGOODS] save unavailable; original data retained");
        recovery("Save unavailable - retained");
    }
    if(!ResourcePack::ins().active() || !FontResource::ins().loaded() || !RoomResource::ins().available())
        recovery("Critical resources unavailable");
    const auto& state=GameEngine::ins().gameState();
    uint16_t species[Game::TEAM_CAP]{};
    for(unsigned i=0;i<state.teamCount;++i) species[i]=state.team[i].speciesId;
    if(!PokemonSprites::syncTeamCache(species,state.teamCount,Game::TEAM_CAP))
        recovery("Partner sprites unavailable");
    if(!Sdgoods::confirmStorageBoot()) recovery("Boot validation failed");
    board.enableSaveWrites(true);
    if(!session.save()) recovery("Save write failed - retained");
    for (;;) {
        board.pollInputs();
#if STICKMON_ENABLE_ROOM_DIAGNOSTICS
        static uint32_t lastBoardDiagnostic = 0;
        const uint32_t now = board.millis();
        if (now - lastBoardDiagnostic >= 5000) {
            lastBoardDiagnostic = now;
            Platform::logf("[RoomBoard] t=%u held=%u locked=%u\n",
                now, unsigned(board.powerKeyHeld()), unsigned(board.locked()));
        }
#endif
        Sdgoods::processPowerActions(board, session, GameEngine::ins());
        // Suspended I2S rejects PCM; release the stream while locked instead of
        // reopening the track every tick, and restart it on unlock.
        AudioManager::ins().setMusicSuspended(board.locked());
        Sdgoods::pollMaintenanceConsole(board,session,true);
        if (board.powerKeyHeld()) board.sleepMs(5);
        else session.poll();
        // Locked: the engine steps once a second; poll the key at debounce pace.
        if (board.locked()) board.sleepMs(LOCKED_LOOP_MS);
    }
}
