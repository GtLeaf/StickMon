#include "OstbPlatform.h"
#include "core/GameEngine.h"
#include "core/SaveManager.h"
#include "game/GameRandom.h"
#include "esp_log.h"

extern "C" void app_main() {
    Ostb::bindOstbPlatform();
    auto& board = Ostb::OstbPlatform::instance();
    if (!board.begin()) { ESP_LOGE("OSTB", "Board initialization failed"); return; }
    GameRandom::seed(board.hardwareRandom());
    auto& engine = GameEngine::ins();
    if (!engine.begin()) { ESP_LOGE("OSTB", "Game initialization failed"); return; }
    SaveManager verifier;
    Game::GameState saved{};
    MainSceneViewState view{};
    const bool verified = verifier.begin() && verifier.load(saved, view);
    ESP_LOGI("OSTB", "Game ready; saved snapshot readable=%u, hatched=%u; room=%u position=(%.2f,%.2f)",
             verified, saved.oobeDone, view.valid, double(view.monsterX), double(view.monsterY));

    bool setupWasActive = board.touchSetupActive();
    while (true) {
        if (board.powerKeyHeld()) {
            // Keep the last displayed position equal to the saved position
            // while the hardware power controller counts the held key.
            board.update();
            board.sleepMs(5);
        } else if (board.touchSetupActive()) {
            board.update();
            if (!board.locked()) board.drawTouchSetup();
            board.sleepMs(5);
        } else {
            if (setupWasActive) {
                setupWasActive = false;
                engine.wakeFromIdle();
                engine.invalidateScene();
            }
            engine.run();
        }
        if (board.takeSaveRequest()) {
            if (engine.saveNow()) {
                const auto& room = engine.mainSceneViewState();
                ESP_LOGI("OSTB", "Power key snapshot saved before hardware cutoff; room=%u position=(%.2f,%.2f)",
                         room.valid, double(room.monsterX), double(room.monsterY));
            } else ESP_LOGE("OSTB", "Power key snapshot save failed");
        }
        if (board.takeUnlockRequest()) {
            engine.wakeFromIdle();
            engine.invalidateScene();
        }
        if (board.takeShutdownRequest()) {
            if (engine.saveNow()) board.enterDeepSleep(0, true);
            else ESP_LOGE("OSTB", "Save failed; staying awake to protect progress");
        }
    }
}
