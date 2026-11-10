// SPDX-License-Identifier: Apache-2.0
#include "RoomSession.h"

#include "core/SaveManager.h"
#include "hardware/Hal.h"
#include "ui/round_touch/RoomControls.h"
#include "platform/api/PlatformServices.h"

namespace Sdgoods {
bool RoomSession::begin(bool persist) {
    SaveManager verifier;
    Game::GameState prior{};
    MainSceneViewState view{};
    SaveManager::LoadStatus status = SaveManager::LoadStatus::NOT_FOUND;
    if (!verifier.begin()) return false;
    bool normalized=false;
    verifier.load(prior, view, &normalized, &status);
    // A trial application must not rewrite data its fallback still owns.
    if(!persist && normalized) return false;
    if (status == SaveManager::LoadStatus::INVALID ||
        status == SaveManager::LoadStatus::NEWER_VERSION) {
        Platform::errorf("[SDGOODS] refusing unreadable save: status=%u\n", unsigned(status));
        return false;
    }
    if (status == SaveManager::LoadStatus::NOT_FOUND &&
        (Platform::blobs().blobSize("stickmon", "state_a") ||
         Platform::blobs().blobSize("stickmon", "state_b"))) {
        Platform::errorLine("[SDGOODS] invalid snapshot slots retained; no fresh game created");
        return false;
    }
    // Seed HAL before begin() applies its default volume to the audio device.
    if (status == SaveManager::LoadStatus::LOADED) Hal::ins().setAudioVolume(prior.settings.volume);
    auto& engine = GameEngine::ins();
    if (!engine.begin()) return false;
    ready_ = true;
    if (persist && !save()) { ready_ = false; return false; }
    if(!persist) savedFood_=foodState();
    const auto& state = engine.gameState();
    Platform::logf("[SDGOODS] session ready: restored=%u hatched=%u species=%u stock=%u bowl=%u bites=%u satiety=%u\n",
        status == SaveManager::LoadStatus::LOADED, state.oobeDone,
        state.teamCount ? state.team[0].speciesId : 0,
        engine.selectedFoodCount(), engine.bowlFoodCount(), engine.bowlFoodBitesRemaining(),
        state.teamCount ? state.team[0].satiety : 0);
    return true;
}

RoomSession::FoodState RoomSession::foodState() const {
    const auto& state = GameEngine::ins().gameState();
    FoodState result{};
    for (unsigned i = 0; i < Game::ROOM_FOOD_COUNT; ++i) result[i] = state.room.food[i];
    result[Game::ROOM_FOOD_COUNT] = state.room.bowlCount;
    result[Game::ROOM_FOOD_COUNT + 1] = state.room.bowlBitesRemaining;
    result[Game::ROOM_FOOD_COUNT + 2] = state.room.bowlFood;
    result[Game::ROOM_FOOD_COUNT + 3] = state.teamCount ? state.team[0].satiety : 0;
    result[Game::ROOM_FOOD_COUNT + 4] = state.room.selectedFood;
    return result;
}

bool RoomSession::save() {
    if (!ready_) return false;
    lastAttemptMs_ = Platform::clock().millis();
    lastSaveOk_ = GameEngine::ins().saveNow();
    if (lastSaveOk_) savedFood_ = foodState();
    Ui::RoundRoom::lastSaveOk = lastSaveOk_;
    GameEngine::ins().invalidateScene();
    if (lastSaveOk_) Platform::logLine("[SDGOODS] save ok");
    else Platform::errorLine("[SDGOODS] save FAILED");
    return lastSaveOk_;
}

void RoomSession::poll() {
    if (!ready_) return;
    GameEngine::ins().run();
    if (foodState() != savedFood_ &&
        (lastSaveOk_ || Platform::clock().millis() - lastAttemptMs_ >= 2000)) save();
}

bool RoomSession::restart() {
    if (!save()) return false;
    Platform::power().restart();
    return true;
}
}  // namespace Sdgoods
