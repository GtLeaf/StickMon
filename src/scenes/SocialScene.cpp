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
    joinRequested = false;
    joinRequestedAt = 0;
}

void SocialScene::onExit() {
    EspNowLink::ins().end();
}

void SocialScene::update(uint32_t nowMs, float dtSeconds) {
    (void)nowMs;
    (void)dtSeconds;
    EspNowLink::ins().update();

    EspNowLink::RoomPurpose purpose;
    uint16_t requestSeq = 0;
    if (EspNowLink::ins().takeJoinRequest(joinMac, purpose, requestSeq)) {
        EspNowLink::ins().sendJoinAck(joinMac, true, requestSeq);
        toast = Ui::Social::ACCEPTED;
        toastUntil = Hal::ins().millis() + 1500;
    }

    bool accepted = false;
    if (EspNowLink::ins().takeJoinAck(accepted)) {
        joinRequested = false;
        if (!accepted) EspNowLink::ins().stopRoom();
        toast = accepted ? Ui::Social::CONNECT_OK : Ui::Social::CONNECT_REJECTED;
        toastUntil = Hal::ins().millis() + 1500;
    }

    if (joinRequested && nowMs - joinRequestedAt >= 3000) joinRequested = false;
    if (!joinRequested && EspNowLink::ins().currentMode() == EspNowLink::Mode::SEARCHING &&
        EspNowLink::ins().roomCount() > 0 && EspNowLink::ins().sendJoinRequest(0)) {
        joinRequested = true;
        joinRequestedAt = nowMs;
        toast = Ui::Social::JOIN_REQUESTED;
        toastUntil = nowMs + 1400;
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
        joinRequested = false;
        EspNowLink::ins().startHost(selectedPurpose);
        toast = hostToast(selectedPurpose);
        break;
    case ACTION_SEARCH:
        joinRequested = false;
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

    toastUntil = Hal::ins().millis() + 1400;
}

void SocialScene::render() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb(7, 9, 14));

    renderMenu();
    renderToast();
}

void SocialScene::renderMenu() {
    auto& c = PixelRenderer::canvas();
    const int count = viewMode == ViewMode::PURPOSE ? (int)PURPOSE_COUNT : (int)ACTION_COUNT;
    const int rowH = 22;
    const int startY = 20;

    for (int i = 0; i < count; ++i) {
        int y = startY + i * rowH;
        uint16_t color = (i == cursor) ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);
        if (i == cursor) {
            c.fillRect(8, y + 3, 4, 14, PixelRenderer::rgb(255, 216, 72));
        }
        const char* label = viewMode == ViewMode::PURPOSE ? Ui::Social::PURPOSE_ITEMS[i] : Ui::Social::ACTION_ITEMS[i];
        c.fillRect(22, y + 3, 14, 14, PixelRenderer::rgb(135, 214, 238));
        PixelRenderer::text(48, y + 2, label, color, 1);
        if (i < count - 1) {
            c.drawFastHLine(48, y + rowH - 1, 150, PixelRenderer::rgb(70, 74, 84));
        }
    }
}

void SocialScene::renderToast() {
    if (!toast) return;
    if ((int32_t)(Hal::ins().millis() - toastUntil) >= 0) {
        toast = nullptr;
        return;
    }
    auto& c = PixelRenderer::canvas();
    c.fillRect(52, 108, 136, 20, PixelRenderer::rgb(34, 39, 47));
    PixelRenderer::text(60, 110, toast, PixelRenderer::rgb(255, 255, 255), 1);
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
