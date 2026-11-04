#include "scenes/SettingsScene.h"

#include <algorithm>
#include "core/CryPlayer.h"
#include "core/GameEngine.h"
#include "core/TraceLog.h"
#include "hardware/Hal.h"

void SettingsScene::onEnter() { ui.onEnter(); }
void SettingsScene::onExit() { ui.onExit(); }
SceneUpdateResult SettingsScene::update(uint32_t nowMs, float dtSeconds) {
    return ui.update(nowMs, dtSeconds);
}
void SettingsScene::render() { ui.render(); }
bool SettingsScene::onButton(const ButtonEvent& event) { return ui.onButton(event); }
bool SettingsScene::touchNavigation() const { return ui.touchNavigation(); }
bool SettingsScene::onTouch(const Platform::IInputDevice::TouchEvent& event) {
    return ui.onTouch(event);
}
void SettingsScene::cancelTouch() { Scene::cancelTouch(); ui.cancelTouch(); }
int SettingsScene::touchTarget(int16_t x, int16_t y) { return ui.touchTarget(x, y); }
bool SettingsScene::onTap(int target) { return ui.onTap(target); }

const Game::GameState& SettingsScene::settingsState() const {
    return GameEngine::ins().gameState();
}
uint8_t SettingsScene::settingsBrightness() const { return Hal::ins().getBrightness(); }
float SettingsScene::settingsGameSpeed() const { return GameEngine::ins().gameSpeed(); }
const char* SettingsScene::settingsIdleLabel() const {
    return GameEngine::ins().idleTimeoutLabel();
}

void SettingsScene::initializeSettings(bool continuousVolume) {
    auto& engine = GameEngine::ins();
    engine.wakeFromIdle();
    auto& volume = engine.gameState().settings.volume;
    const uint8_t normalized = volume > 100 ? 100 : continuousVolume ? volume :
        static_cast<uint8_t>((volume / 10) * 10);
    if (volume != normalized) {
        volume = normalized;
        markSettingsDirty();
    }
    Hal::ins().setAudioVolume(volume);
}

void SettingsScene::cycleSetting(Ui::SettingsPage::Item item) {
    using Page = Ui::SettingsPage;
    auto& engine = GameEngine::ins();
    switch (item) {
    case Page::BRIGHTNESS: {
        engine.wakeFromIdle();
        const uint8_t current = Hal::ins().getBrightness();
        static constexpr uint8_t LEVELS[] = {32, 64, 128, 192, 255};
        uint8_t next = LEVELS[0];
        for (uint8_t level : LEVELS) {
            if (level > current) { next = level; break; }
        }
        STICKMON_TRACEF("[Settings] brightness t=%lu current=%u next=%u display=%u\n",
                        static_cast<unsigned long>(Hal::ins().millis()), current, next,
                        Hal::ins().getDisplayBrightness());
        Hal::ins().setBrightness(next);
        engine.gameState().settings.brightness = next;
        break;
    }
    case Page::GAME_SPEED:
        engine.cycleGameSpeed();
        break;
    case Page::VOLUME: {
        auto& volume = engine.gameState().settings.volume;
        volume = volume >= 100 ? 0 : std::min(100, volume + 10);
        Hal::ins().setAudioVolume(volume);
        if (volume == 0) CryPlayer::ins().stop();
        else previewSettingsVolume();
        break;
    }
    case Page::POWER_SAVE:
        engine.cycleIdleTimeout();
        break;
    default:
        return;
    }
    markSettingsDirty();
}

bool SettingsScene::setSettingValue(Ui::SettingsPage::Item item, uint8_t value) {
    using Page = Ui::SettingsPage;
    if (item != Page::BRIGHTNESS && item != Page::VOLUME) return false;
    auto& engine = GameEngine::ins();
    auto& setting = item == Page::BRIGHTNESS ? engine.gameState().settings.brightness :
        engine.gameState().settings.volume;
    if (setting == value) return false;
    setting = value;
    if (item == Page::BRIGHTNESS) {
        engine.wakeFromIdle();
        Hal::ins().setBrightness(value);
    } else {
        Hal::ins().setAudioVolume(value);
        if (!value) CryPlayer::ins().stop();
    }
    markSettingsDirty();
    return true;
}

void SettingsScene::previewSettingsVolume() {
    auto& engine = GameEngine::ins();
    if (engine.gameState().settings.volume && engine.gameState().teamCount)
        CryPlayer::ins().replay(engine.activeMonster().speciesId);
}
void SettingsScene::leaveSettings() { GameEngine::ins().requestScene(SceneID::MENU); }
void SettingsScene::replaySettingsTutorial() {
    auto& engine = GameEngine::ins();
    engine.resetTutorial();
    engine.requestScene(SceneID::MAIN);
}
bool SettingsScene::resetSettingsGame() { return GameEngine::ins().resetGame(); }
void SettingsScene::finishSettingsReset() { GameEngine::ins().requestScene(SceneID::HATCH); }
void SettingsScene::markSettingsDirty() { GameEngine::ins().markDirty(SaveUrgency::SOON); }
