#include "scenes/SocialScene.h"
#include <cstdio>
#include <cstring>
#include "core/GameEngine.h"
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "hardware/PixelRenderer.h"

void SocialScene::onEnter() {
    EspNowLink::ins().begin();
    viewMode = ViewMode::PURPOSE;
    cursor = 0;
}

void SocialScene::onExit() {
    EspNowLink::ins().end();
}

void SocialScene::update(uint32_t nowMs, float dtSeconds) {
    (void)nowMs;
    (void)dtSeconds;
    EspNowLink::ins().update();

    EspNowLink::RoomPurpose purpose;
    if (EspNowLink::ins().takeJoinRequest(joinMac, purpose)) {
        EspNowLink::ins().sendJoinAck(joinMac, true);
        toast = Ui::Social::ACCEPTED;
        toastUntil = Hal::ins().millis() + 1500;
    }

    bool accepted = false;
    if (EspNowLink::ins().takeJoinAck(accepted)) {
        toast = accepted ? Ui::Social::CONNECT_OK : Ui::Social::CONNECT_REJECTED;
        toastUntil = Hal::ins().millis() + 1500;
    }
}

bool SocialScene::onButton(const ButtonEvent& event) {
    if ((event.btn == 0 || event.btn == 1) && event.action == BtnAction::LONG_PRESS) {
        if (viewMode == ViewMode::ACTION) {
            viewMode = ViewMode::PURPOSE;
            cursor = 0;
        } else {
            GameEngine::ins().requestScene(SceneID::MENU);
        }
        return true;
    }

    if (event.btn == 1 && event.action == BtnAction::PRESSED) {
        const uint8_t count = viewMode == ViewMode::PURPOSE ? (uint8_t)PURPOSE_COUNT : (uint8_t)ACTION_COUNT;
        cursor = (cursor + 1) % count;
        return true;
    }

    if (event.btn == 0 && event.action == BtnAction::PRESSED) {
        activateCurrent();
        return true;
    }
    return false;
}

void SocialScene::activateCurrent() {
    if (viewMode == ViewMode::PURPOSE) {
        switch (cursor) {
        case PURPOSE_BATTLE: selectedPurpose = EspNowLink::RoomPurpose::BATTLE; break;
        case PURPOSE_TRADE: selectedPurpose = EspNowLink::RoomPurpose::TRADE; break;
        case PURPOSE_GIFT: selectedPurpose = EspNowLink::RoomPurpose::GIFT; break;
        case PURPOSE_BACK:
            GameEngine::ins().requestScene(SceneID::MENU);
            return;
        default:
            break;
        }
        viewMode = ViewMode::ACTION;
        cursor = 0;
        return;
    }

    switch (cursor) {
    case ACTION_HOST:
        EspNowLink::ins().startHost(selectedPurpose);
        toast = hostToast(selectedPurpose);
        break;
    case ACTION_SEARCH:
        EspNowLink::ins().startSearch(selectedPurpose);
        toast = searchToast(selectedPurpose);
        break;
    case ACTION_BACK:
        viewMode = ViewMode::PURPOSE;
        cursor = 0;
        return;
    default:
        return;
    }

    if (EspNowLink::ins().currentMode() == EspNowLink::Mode::SEARCHING &&
        EspNowLink::ins().roomCount() > 0) {
        EspNowLink::ins().sendJoinRequest(0);
        toast = Ui::Social::JOIN_REQUESTED;
    }
    toastUntil = Hal::ins().millis() + 1400;
}

void SocialScene::render() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb(7, 9, 14));
    c.fillRect(0, 0, Hal::DISPLAY_W, 24, PixelRenderer::rgb(25, 25, 40));
    PixelRenderer::text(4, 4, Ui::SOCIAL, PixelRenderer::rgb(67, 213, 224), 1);

    renderMenu();
    renderToast();
}

void SocialScene::renderMenu() {
    auto& c = PixelRenderer::canvas();
    const int count = viewMode == ViewMode::PURPOSE ? (int)PURPOSE_COUNT : (int)ACTION_COUNT;
    const int rowH = 31;
    const int startY = 42;

    for (int i = 0; i < count; ++i) {
        int y = startY + i * rowH;
        uint16_t color = (i == cursor) ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);
        if (i == cursor) {
            c.fillRect(4, y + 5, 5, 18, PixelRenderer::rgb(255, 216, 72));
        }
        const char* label = viewMode == ViewMode::PURPOSE ? Ui::Social::PURPOSE_ITEMS[i] : Ui::Social::ACTION_ITEMS[i];
        PixelRenderer::text(16, y + 6, label, color, 1);
        if (i < count - 1) {
            c.drawFastHLine(4, y + rowH - 1, Hal::DISPLAY_W - 8, PixelRenderer::rgb(70, 74, 84));
        }
    }
}

void SocialScene::renderToast() {
    if (!toast || Hal::ins().millis() > toastUntil) return;
    auto& c = PixelRenderer::canvas();
    c.fillRect(14, 216, 107, 20, PixelRenderer::rgb(34, 39, 47));
    PixelRenderer::text(22, 220, toast, PixelRenderer::rgb(255, 255, 255), 1);
}

const char* SocialScene::purposeName(EspNowLink::RoomPurpose purpose) {
    switch (purpose) {
    case EspNowLink::RoomPurpose::BATTLE: return Ui::BATTLE;
    case EspNowLink::RoomPurpose::TRADE: return Ui::TRADE;
    case EspNowLink::RoomPurpose::EVOLVE: return Ui::EVOLVE;
    case EspNowLink::RoomPurpose::GIFT: return Ui::GIFT;
    default: return "--";
    }
}

const char* SocialScene::hostToast(EspNowLink::RoomPurpose purpose) {
    switch (purpose) {
    case EspNowLink::RoomPurpose::BATTLE: return Ui::Social::HOSTING_BATTLE;
    case EspNowLink::RoomPurpose::TRADE: return Ui::Social::HOSTING_TRADE;
    case EspNowLink::RoomPurpose::GIFT: return Ui::Social::HOSTING_GIFT;
    default: return Ui::Social::HOSTING_TRADE;
    }
}

const char* SocialScene::searchToast(EspNowLink::RoomPurpose purpose) {
    switch (purpose) {
    case EspNowLink::RoomPurpose::BATTLE: return Ui::Social::SEARCHING_BATTLE;
    case EspNowLink::RoomPurpose::TRADE: return Ui::Social::SEARCHING_TRADE;
    case EspNowLink::RoomPurpose::GIFT: return Ui::Social::SEARCHING_GIFT;
    default: return Ui::Social::SEARCHING_TRADE;
    }
}
