#include "scenes/SettingsScene.h"
#include "presentation/TouchUi.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "core/CryPlayer.h"
#include "core/GameEngine.h"
#include "core/MathUtil.h"
#include "core/TraceLog.h"
#include "core/UiStrings.h"
#include "core/UiMotion.h"
#include "hardware/Hal.h"
#include "presentation/PixelRenderer.h"

constexpr uint8_t SettingsScene::TOUCH_ORDER[];

namespace {
constexpr int SLIDER_LEFT = 120;
constexpr int SLIDER_RIGHT = Hal::DISPLAY_W - 24;
constexpr int HELP_PAGE_COUNT = sizeof(Ui::Settings::HELP_PAGE_TITLES) / sizeof(Ui::Settings::HELP_PAGE_TITLES[0]);
}

void SettingsScene::onEnter() {
    cancelTouch();
    touchScroll = 0;
    GameEngine::ins().wakeFromIdle();
    cursor = 0;
    viewMode = ViewMode::MENU;
    resetConfirmYes = false;
    menuScroll = 0.0f;
    helpPage = 0;
    normalizeVolumeSetting();
}

SceneUpdateResult SettingsScene::update(uint32_t nowMs, float dtSeconds) {
    (void)dtSeconds;
    RenderDemand demand;

    if (!TouchUi::enabled() && viewMode == ViewMode::MENU) {
        static constexpr int ROW_H = 24;
        static constexpr int START_Y = 6;
        const int contentH = START_Y * 2 + COUNT * ROW_H;
        const int maxScroll =
            contentH > TouchUi::CONTENT_H ? contentH - TouchUi::CONTENT_H : 0;
        int targetScroll =
            START_Y + cursor * ROW_H + ROW_H / 2 - TouchUi::CONTENT_H / 2;
        targetScroll = MathUtil::clamp(targetScroll, 0, maxScroll);
        UiMotion::StepResult step = UiMotion::lerp(
            menuScroll, static_cast<float>(targetScroll), 0.25f, 0.5f);
        demand.changed(step.changed);
        demand.animate(step.active);
    }

    if (demand.expired(toast != nullptr, nowMs, toastUntil)) toast = nullptr;
    return demand.result();
}

bool SettingsScene::touchNavigation() const { return !TouchUi::enabled(); }

void SettingsScene::cancelTouch() {
    Scene::cancelTouch();
    pageTouch.cancel();
    sliderCandidate = -1;
    sliderDragging = sliderChanged = false;
}

bool SettingsScene::onTouch(const Platform::IInputDevice::TouchEvent& event) {
    if (!TouchUi::enabled()) return Scene::onTouch(event);
    using Type = Platform::IInputDevice::TouchType;
    const int target = touchTarget(event.x, event.y);
    if (event.type == Type::CANCEL) {
        const bool changed = pageTouch.pressed() >= 0 || sliderDragging;
        cancelTouch();
        return changed;
    }
    if (event.type == Type::DOWN) {
        cancelTouch();
        sliderStartX = event.x; sliderStartY = event.y;
        if (viewMode == ViewMode::MENU && (target == 16 + BRIGHTNESS || target == 16 + VOLUME))
            sliderCandidate = target - 16;
    }
    if (event.type == Type::MOVE && sliderCandidate >= 0 && !sliderDragging) {
        const int dx = std::abs(event.x - sliderStartX), dy = std::abs(event.y - sliderStartY);
        if (std::max(dx, dy) > 10) {
            if (dx > dy) { sliderDragging = true; pageTouch.cancel(); }
            else sliderCandidate = -1;
        }
    }
    if (sliderDragging && (event.type == Type::MOVE || event.type == Type::UP)) {
        sliderChanged = setSliderValue(sliderCandidate, event.x) || sliderChanged;
        if (event.type == Type::UP) {
            if (sliderCandidate == VOLUME && sliderChanged) previewVolume();
            cancelTouch();
        }
        return true;
    }
    const auto layout = TouchUi::listLayout();
    const auto result = pageTouch.update(event, target, touchScroll, layout.maxScroll(COUNT),
        viewMode == ViewMode::MENU && layout.viewport.contains(event.x, event.y));
    if (result.tap == 16 + BRIGHTNESS || result.tap == 16 + VOLUME) {
        const int item = result.tap - 16;
        const bool changed = setSliderValue(item, event.x);
        if (changed && item == VOLUME) previewVolume();
        cancelTouch();
        return true;
    }
    const bool activated = result.tap >= 0 && onTap(result.tap);
    if (event.type == Type::UP) sliderCandidate = -1;
    return result.changed || activated;
}

bool SettingsScene::setSliderValue(int item, int x) {
    if (viewMode != ViewMode::MENU || (item != BRIGHTNESS && item != VOLUME)) return false;
    const int minimum = item == BRIGHTNESS ? 32 : 0;
    const int maximum = item == BRIGHTNESS ? 255 : 100;
    const int width = SLIDER_RIGHT - SLIDER_LEFT;
    const uint8_t value = minimum + ((std::clamp(x, SLIDER_LEFT, SLIDER_RIGHT) - SLIDER_LEFT) * (maximum - minimum) + width / 2) / width;
    auto& engine = GameEngine::ins();
    auto& setting = item == BRIGHTNESS ? engine.gameState().settings.brightness : engine.gameState().settings.volume;
    cursor = item;
    if (setting == value) return false;
    setting = value;
    if (item == BRIGHTNESS) { engine.wakeFromIdle(); Hal::ins().setBrightness(value); }
    else { Hal::ins().setAudioVolume(value); if (!value) CryPlayer::ins().stop(); }
    markSettingsDirty();
    return true;
}

void SettingsScene::previewVolume() {
    auto& engine = GameEngine::ins();
    if (engine.gameState().settings.volume && engine.gameState().teamCount)
        CryPlayer::ins().replay(engine.activeMonster().speciesId);
}

int SettingsScene::touchTarget(int16_t x, int16_t y) {
    if (TouchUi::enabled()) {
        if (x < 0 || x >= Hal::DISPLAY_W || y < 0 || y >= Hal::DISPLAY_H) return -1;
        const int mode = static_cast<int>(viewMode) * 256;
        if (TouchUi::pageBackHit(x, y)) return mode + 230;
        if (viewMode != ViewMode::MENU) {
            if (y >= Hal::DISPLAY_H - 40 && y < Hal::DISPLAY_H - 6) {
                if (x >= 8 && x < Hal::DISPLAY_W / 2 - 6) return mode + 200;
                if (x >= Hal::DISPLAY_W / 2 + 6 && x < Hal::DISPLAY_W - 8) return mode + 201;
            }
            return -1;
        }
        const int row = TouchUi::listLayout().itemAt(x, y, touchScroll, COUNT);
        if (row < 0) return -1;
        const int item = TOUCH_ORDER[row];
        if ((item == BRIGHTNESS || item == VOLUME) && x >= SLIDER_LEFT - 12) return 16 + item;
        return item;
    }
    if (viewMode != ViewMode::MENU || y < 6 || x < 8 || y >= TouchUi::CONTENT_H) return -1;
    const int row = (y - 6 + int(menuScroll)) / 24;
    return row < COUNT ? row : -1;
}

bool SettingsScene::onTap(int target) {
    if (TouchUi::enabled()) {
        if (target / 256 != static_cast<int>(viewMode)) return false;
        const int item = target % 256;
        if (item == 230) return onButton({1, BtnAction::LONG_PRESS});
        if (viewMode == ViewMode::HELP) {
            if (item == 200) { helpPage = (helpPage + HELP_PAGE_COUNT - 1) % HELP_PAGE_COUNT; return true; }
            if (item == 201) return onButton({uint8_t(helpPage + 1 == HELP_PAGE_COUNT ? 0 : 1), BtnAction::PRESSED});
            return false;
        }
        if (viewMode == ViewMode::RESET_CONFIRM) {
            if (item != 200 && item != 201) return false;
            resetConfirmYes = item == 201;
            return onButton({0, BtnAction::PRESSED});
        }
    }
    if (viewMode != ViewMode::MENU || target < 0 || target >= COUNT) return false;
    cursor = target;
    return onButton({0, BtnAction::PRESSED});
}

bool SettingsScene::onButton(const ButtonEvent& event) {
    if (viewMode == ViewMode::RESET_CONFIRM) {
        if ((event.btn == 0 || event.btn == 1) && event.action == BtnAction::LONG_PRESS) {
            viewMode = ViewMode::MENU;
            resetConfirmYes = false;
            return true;
        }
        if (event.btn == 1 && event.action == BtnAction::PRESSED) {
            resetConfirmYes = !resetConfirmYes;
            return true;
        }
        if (event.btn == 0 && event.action == BtnAction::PRESSED) {
            if (!resetConfirmYes) {
                viewMode = ViewMode::MENU;
                return true;
            }
            if (!GameEngine::ins().resetGame()) {
                viewMode = ViewMode::MENU;
                toast = Ui::Settings::RESET_FAILED;
                toastUntil = Hal::ins().millis() + 1500;
                return true;
            }
            GameEngine::ins().requestScene(SceneID::HATCH);
            return true;
        }
        return false;
    }

    if (viewMode == ViewMode::HELP) {
        static constexpr uint8_t HELP_PAGE_COUNT =
            sizeof(Ui::Settings::HELP_PAGE_TITLES) /
            sizeof(Ui::Settings::HELP_PAGE_TITLES[0]);
        if (event.btn == 1 && event.action == BtnAction::PRESSED) {
            helpPage = (helpPage + 1) % HELP_PAGE_COUNT;
            return true;
        }
        if (event.btn == 0 && event.action == BtnAction::PRESSED) {
            if (helpPage + 1 == HELP_PAGE_COUNT) {
                GameEngine::ins().resetTutorial();
                GameEngine::ins().requestScene(SceneID::MAIN);
            }
            return true;
        }
        if (event.btn == 1 && event.action == BtnAction::LONG_PRESS) {
            viewMode = ViewMode::MENU;
            return true;
        }
        return false;
    }

    if ((event.btn == 0 || event.btn == 1) && event.action == BtnAction::LONG_PRESS) {
        GameEngine::ins().requestScene(SceneID::MENU);
        return true;
    }
    if (event.btn == 1 && event.action == BtnAction::PRESSED) {
        if (TouchUi::enabled()) {
            int index = 0;
            while (index < COUNT && TOUCH_ORDER[index] != cursor) ++index;
            index = (index + 1) % COUNT;
            cursor = TOUCH_ORDER[index];
            touchScroll = TouchUi::listLayout().reveal(index, touchScroll, COUNT);
        } else cursor = (cursor + 1) % COUNT;
        if (cursor == 0) menuScroll = 0.0f;
        return true;
    }
    if (event.btn == 0 && event.action == BtnAction::PRESSED) {
        activateCurrent();
        return true;
    }
    return false;
}

void SettingsScene::activateCurrent() {
    toast = nullptr;
    toastUntil = 0;
    switch (cursor) {
    case BRIGHTNESS:
        cycleBrightness();
        break;
    case GAME_SPEED:
        GameEngine::ins().cycleGameSpeed();
        markSettingsDirty();
        break;
    case VOLUME: {
        auto& engine = GameEngine::ins();
        auto& state = engine.gameState();
        auto& settings = state.settings;
        settings.volume = settings.volume >= 100 ? 0 : std::min(100, settings.volume + 10);
        Hal::ins().setAudioVolume(settings.volume);
        if (settings.volume == 0) {
            CryPlayer::ins().stop();
        } else if (state.teamCount > 0) {
            CryPlayer::ins().replay(engine.activeMonster().speciesId);
        }
        markSettingsDirty();
        break;
    }
    case POWER_SAVE:
        GameEngine::ins().cycleIdleTimeout();
        markSettingsDirty();
        return;
        break;
    case HELP:
        viewMode = ViewMode::HELP;
        helpPage = 0;
        toast = nullptr;
        return;
    case RESET_GAME:
        viewMode = ViewMode::RESET_CONFIRM;
        resetConfirmYes = false;
        toast = nullptr;
        return;
    default:
        break;
    }
}

void SettingsScene::cycleBrightness() {
    GameEngine::ins().wakeFromIdle();
    uint8_t cur = Hal::ins().getBrightness();
    static constexpr uint8_t LEVELS[] = {32, 64, 128, 192, 255};
    uint8_t next = LEVELS[0];
    for (uint8_t level : LEVELS) {
        if (level > cur) {
            next = level;
            break;
        }
    }
    STICKMON_TRACEF("[Settings] brightness t=%lu current=%u next=%u display=%u\n",
                    (unsigned long)Hal::ins().millis(), cur, next,
                    Hal::ins().getDisplayBrightness());
    Hal::ins().setBrightness(next);
    GameEngine::ins().gameState().settings.brightness = next;
    markSettingsDirty();
}

void SettingsScene::normalizeVolumeSetting() {
    auto& volume = GameEngine::ins().gameState().settings.volume;
    uint8_t normalized = volume > 100 ? 100 : TouchUi::enabled() ? volume : (uint8_t)((volume / 10) * 10);

    if (volume != normalized) {
        volume = normalized;
        markSettingsDirty();
    }
    Hal::ins().setAudioVolume(volume);
}

void SettingsScene::markSettingsDirty() {
    GameEngine::ins().markDirty(SaveUrgency::SOON);
}

void SettingsScene::render() {
    if (TouchUi::enabled()) { renderTouch(); return; }
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, TouchUi::CONTENT_H, PixelRenderer::rgb(7, 9, 14));
    if (viewMode == ViewMode::HELP) {
        renderHelp();
    } else {
        renderMenu();
        if (viewMode == ViewMode::RESET_CONFIRM) renderResetConfirm();
    }
    renderToast();
}

void SettingsScene::renderTouch() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, 0);
    const int mode = static_cast<int>(viewMode) * 256;
    const auto& settings = GameEngine::ins().gameState().settings;
    if (viewMode == ViewMode::MENU) {
        const auto layout = TouchUi::listLayout();
        c.setClipRect(0, TouchUi::PAGE_HEADER_H, Hal::DISPLAY_W, Hal::DISPLAY_H - TouchUi::PAGE_HEADER_H);
        for (int row = 0; row < COUNT; ++row) {
            const auto rect = layout.cell(row, touchScroll);
            if (rect.y + rect.height <= TouchUi::PAGE_HEADER_H || rect.y >= Hal::DISPLAY_H) continue;
            const int item = TOUCH_ORDER[row];
            const bool pressed = pageTouch.pressed() == item || pageTouch.pressed() == 16 + item ||
                (sliderDragging && sliderCandidate == item);
            MenuGrid::card(c, rect, pressed, item == cursor);
            PixelRenderer::text(rect.x + 10, rect.y + 14, Ui::Settings::ITEMS[item], item == RESET_GAME ? 0xf471 : 0xffff);
            if (item == BRIGHTNESS || item == VOLUME) {
                TouchUi::slider(SLIDER_LEFT, SLIDER_RIGHT, rect.y + 22,
                    item == BRIGHTNESS ? Hal::ins().getBrightness() : settings.volume,
                    item == BRIGHTNESS ? 32 : 0, item == BRIGHTNESS ? 255 : 100, pressed);
            } else {
                const char* value = ">";
                char speed[12];
                if (item == GAME_SPEED) { std::snprintf(speed, sizeof(speed), "%.0fx", GameEngine::ins().gameSpeed()); value = speed; }
                if (item == POWER_SAVE) value = GameEngine::ins().idleTimeoutLabel();
                MenuGrid::label(c, Hal::DISPLAY_W - 48, rect.y + 14, value, 72, 0xffe0);
            }
        }
        c.clearClipRect();
    } else if (viewMode == ViewMode::HELP) {
        helpPage %= HELP_PAGE_COUNT;
        MenuGrid::card(c, {8, 44, Hal::DISPLAY_W - 16, 144}, false);
        PixelRenderer::text(18, 54, Ui::Settings::HELP_PAGE_TITLES[helpPage], 0x77b6);
        char page[12]; std::snprintf(page, sizeof(page), "%u/%u", helpPage + 1, HELP_PAGE_COUNT);
        PixelRenderer::text(Hal::DISPLAY_W - 44, 54, page, 0x7bef);
        for (int i = 0; i < 4; ++i)
            PixelRenderer::text(18, 84 + i * 24, Ui::Settings::TOUCH_HELP_PAGE_LINES[helpPage][i], 0xffff);
        const int width = Hal::DISPLAY_W / 2 - 14;
        TouchUi::action(8, Hal::DISPLAY_H - 40, width, "<", pageTouch.pressed() == mode + 200);
        TouchUi::action(Hal::DISPLAY_W / 2 + 6, Hal::DISPLAY_H - 40, width,
            helpPage + 1 == HELP_PAGE_COUNT ? Ui::Settings::HELP_PAGE_TITLES[HELP_PAGE_COUNT - 1] : ">",
            pageTouch.pressed() == mode + 201);
    } else {
        MenuGrid::card(c, {8, 50, Hal::DISPLAY_W - 16, 128}, false);
        MenuGrid::label(c, Hal::DISPLAY_W / 2, 80, Ui::Settings::RESET_WARNING, Hal::DISPLAY_W - 32, 0xf471);
        MenuGrid::label(c, Hal::DISPLAY_W / 2, 120, Ui::Settings::RESET_QUESTION, Hal::DISPLAY_W - 32, 0xffff);
        const int width = Hal::DISPLAY_W / 2 - 14;
        TouchUi::action(8, Hal::DISPLAY_H - 40, width, Ui::Settings::RESET_NO, pageTouch.pressed() == mode + 200);
        const int x = Hal::DISPLAY_W / 2 + 6, y = Hal::DISPLAY_H - 40;
        c.fillRoundRect(x, y, width, 34, 4,
            pageTouch.pressed() == mode + 201 ? PixelRenderer::rgb(129, 59, 68) : PixelRenderer::rgb(91, 49, 55));
        c.drawRoundRect(resetConfirmYes ? x : 8, y, width, 34, 4, 0xffe0);
        MenuGrid::label(c, x + width / 2, y + 9, Ui::Settings::RESET_YES, width - 8, 0xf471);
    }
    TouchUi::pageHeader(viewMode == ViewMode::HELP ? Ui::Settings::HELP :
        viewMode == ViewMode::RESET_CONFIRM ? Ui::Settings::RESET_GAME : Ui::SETTINGS,
        pageTouch.pressed() == mode + 230);
    if (toast) {
        c.fillRoundRect(8, Hal::DISPLAY_H - 72, Hal::DISPLAY_W - 16, 26, 4, PixelRenderer::rgb(34, 39, 47));
        MenuGrid::label(c, Hal::DISPLAY_W / 2, Hal::DISPLAY_H - 67, toast, Hal::DISPLAY_W - 32, 0xffff);
    }
}

void SettingsScene::renderMenu() {
    auto& c = PixelRenderer::canvas();
    static_assert(sizeof(Ui::Settings::ITEMS) / sizeof(Ui::Settings::ITEMS[0]) == COUNT,
                  "Settings labels must match SettingsScene::Item");
    static constexpr int ROW_H = 24;
    static constexpr int START_Y = 6;
    static constexpr int TEXT_Y_OFFSET = 4;
    static constexpr int INDICATOR_X = 8;
    static constexpr int CONTENT_X = 20;
    static constexpr int VALUE_X = 156;
    static constexpr int SEPARATOR_W = Hal::DISPLAY_W - CONTENT_X * 2;
    for (int i = 0; i < COUNT; ++i) {
        int y = START_Y + i * ROW_H - (int)menuScroll;
        if (y + ROW_H <= 0 || y >= TouchUi::CONTENT_H) continue;
        bool selected = i == cursor;
        uint16_t fg = selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);
        int textY = y + TEXT_Y_OFFSET;
        if (selected) {
            c.fillRect(INDICATOR_X, textY, 4, 16, PixelRenderer::rgb(255, 216, 72));
        }
        PixelRenderer::text(CONTENT_X, textY, Ui::Settings::ITEMS[i], fg, 1);

        char value[16] = "";
        const auto& settings = GameEngine::ins().gameState().settings;
        if (i == BRIGHTNESS) snprintf(value, sizeof(value), "%u", Hal::ins().getBrightness());
        if (i == GAME_SPEED) snprintf(value, sizeof(value), "%.0fx", GameEngine::ins().gameSpeed());
        if (i == VOLUME) snprintf(value, sizeof(value), "%u%%", settings.volume);
        if (i == POWER_SAVE) snprintf(value, sizeof(value), "%s", GameEngine::ins().idleTimeoutLabel());
        if (value[0]) PixelRenderer::text(VALUE_X, textY, value, PixelRenderer::rgb(135, 214, 238), 1);

        if (i < COUNT - 1) {
            c.drawFastHLine(CONTENT_X, y + ROW_H - 1, SEPARATOR_W,
                            PixelRenderer::rgb(70, 74, 84));
        }
    }
}

void SettingsScene::renderHelp() {
    auto& c = PixelRenderer::canvas();
    static constexpr uint8_t HELP_PAGE_COUNT =
        sizeof(Ui::Settings::HELP_PAGE_TITLES) /
        sizeof(Ui::Settings::HELP_PAGE_TITLES[0]);
    static constexpr uint8_t HELP_LINE_COUNT =
        sizeof(Ui::Settings::HELP_PAGE_LINES[0]) /
        sizeof(Ui::Settings::HELP_PAGE_LINES[0][0]);
    static_assert(
        sizeof(Ui::Settings::HELP_PAGE_LINES) /
                sizeof(Ui::Settings::HELP_PAGE_LINES[0]) ==
            HELP_PAGE_COUNT,
        "help page titles and content must match");
    if (helpPage >= HELP_PAGE_COUNT) helpPage = 0;

    c.fillRect(0, 0, Hal::DISPLAY_W, TouchUi::CONTENT_H, PixelRenderer::rgb(10, 14, 20));
    PixelRenderer::text(12, 8, Ui::Settings::HELP_PAGE_TITLES[helpPage],
                        PixelRenderer::rgb(67, 213, 224), 1);
    char page[12];
    snprintf(page, sizeof(page), "%u/%u", helpPage + 1, HELP_PAGE_COUNT);
    PixelRenderer::text(198, 8, page,
                        PixelRenderer::rgb(156, 164, 176), 1);
    c.drawFastHLine(8, 27, 216, PixelRenderer::rgb(55, 63, 76));
    for (uint8_t line = 0; line < HELP_LINE_COUNT; ++line) {
        PixelRenderer::text(
            12, 31 + line * 19,
            Ui::Settings::HELP_PAGE_LINES[helpPage][line],
            line == 2 && helpPage + 1 == HELP_PAGE_COUNT
                ? PixelRenderer::rgb(255, 216, 72)
                : PixelRenderer::rgb(241, 242, 232),
            1);
    }
    const char* footer = helpPage + 1 == HELP_PAGE_COUNT
        ? Ui::Settings::HELP_REPLAY
        : Ui::Settings::HELP_NEXT;
    PixelRenderer::text(88, 114, footer,
                        PixelRenderer::rgb(135, 214, 238), 1);
}

void SettingsScene::renderResetConfirm() {
    auto& c = PixelRenderer::canvas();
    static constexpr int POP_X = 20;
    static constexpr int POP_Y = 18;
    static constexpr int POP_W = 200;
    static constexpr int POP_H = 98;
    c.fillRect(POP_X, POP_Y, POP_W, POP_H, PixelRenderer::rgb(24, 28, 36));
    c.drawRect(POP_X, POP_Y, POP_W, POP_H, PixelRenderer::rgb(241, 242, 232));

    PixelRenderer::text(88, POP_Y + 8, Ui::Settings::RESET_GAME,
                        PixelRenderer::rgb(255, 216, 72), 1);
    PixelRenderer::text(48, POP_Y + 32, Ui::Settings::RESET_WARNING,
                        PixelRenderer::rgb(241, 242, 232), 1);
    PixelRenderer::text(72, POP_Y + 51, Ui::Settings::RESET_QUESTION,
                        PixelRenderer::rgb(156, 164, 176), 1);

    uint16_t yesColor = resetConfirmYes ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(156, 164, 176);
    uint16_t noColor = !resetConfirmYes ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(156, 164, 176);
    PixelRenderer::text(62, POP_Y + 74, Ui::Settings::RESET_YES, yesColor, 1);
    PixelRenderer::text(150, POP_Y + 74, Ui::Settings::RESET_NO, noColor, 1);
}

void SettingsScene::renderToast() {
    if (!toast) return;
    auto& c = PixelRenderer::canvas();
    c.fillRect(70, 108, 100, 20, PixelRenderer::rgb(34, 39, 47));
    PixelRenderer::text(78, 110, toast, PixelRenderer::rgb(255, 255, 255), 1);
}
