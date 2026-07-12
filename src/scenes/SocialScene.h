#pragma once

#include "core/Scene.h"
#include "hardware/EspNowLink.h"

class SocialScene : public Scene {
public:
    void onEnter() override;
    void onExit() override;
    void update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;

private:
    enum class ViewMode : uint8_t {
        PURPOSE,
        ACTION,
    };

    enum PurposeItem : uint8_t {
        PURPOSE_BATTLE = 0,
        PURPOSE_TRADE,
        PURPOSE_GIFT,
        PURPOSE_BACK,
        PURPOSE_COUNT,
    };

    enum ActionItem : uint8_t {
        ACTION_HOST = 0,
        ACTION_SEARCH,
        ACTION_BACK,
        ACTION_COUNT,
    };

    uint8_t cursor = 0;
    ViewMode viewMode = ViewMode::PURPOSE;
    EspNowLink::RoomPurpose selectedPurpose = EspNowLink::RoomPurpose::BATTLE;
    uint32_t toastUntil = 0;
    const char* toast = nullptr;
    uint8_t joinMac[6] = {};
    bool joinRequested = false;
    uint32_t joinRequestedAt = 0;

    void activateCurrent();
    void renderMenu();
    void renderToast();
    static const char* purposeName(EspNowLink::RoomPurpose purpose);
    static const char* hostToast(EspNowLink::RoomPurpose purpose);
    static const char* searchToast(EspNowLink::RoomPurpose purpose);
};
