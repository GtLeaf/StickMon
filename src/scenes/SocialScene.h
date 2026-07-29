#pragma once

#include "core/Scene.h"
#include "hardware/EspNowLink.h"

class SocialScene : public Scene {
public:
    void onEnter() override;
    void onExit() override;
    SceneUpdateResult update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;

private:
    enum class LinkPhase : uint8_t {
        MENU,
        HOST_ADVERTISING,
        HOST_WAIT_SYNC,
        VISITOR_SEARCHING,
        VISITOR_ROOM_LIST,
        VISITOR_JOINING,
        VISITOR_WAIT_ACCEPT,
        VISITING,
        RESULT,
    };

    enum MenuItem : uint8_t {
        ITEM_INVITE = 0,
        ITEM_VISIT,
        ITEM_BACK,
        ITEM_COUNT,
    };

    static constexpr uint32_t HANDSHAKE_TIMEOUT_MS = 5000;
    static constexpr uint32_t HOST_ADVERTISE_TIMEOUT_MS = 30000;
    static constexpr uint32_t SEARCH_TIMEOUT_MS = 8000;
    static constexpr uint32_t JOIN_ACK_TIMEOUT_MS = 3000;
    static constexpr uint32_t SEND_RETRY_MS = 200;

    uint8_t cursor = 0;
    uint32_t toastUntil = 0;
    const char* toast = nullptr;
    uint8_t joinMac[6] = {};
    bool joinRequested = false;
    uint32_t joinRequestedAt = 0;
    LinkPhase phase = LinkPhase::MENU;
    uint32_t phaseStartedMs = 0;
    const char* resultText = nullptr;
    VisitSyncPayload pendingSync{};
    bool syncSent = false;
    VisitAcceptPayload pendingAccept{};
    bool acceptPending = false;
    uint32_t lastSendTryMs = 0;
    uint8_t roomCursor = 0;
    uint32_t lastVisualTick = UINT32_MAX;
    uint8_t lastRoomCount = 0;

    void setToast(const char* text, uint32_t durationMs = 1500);
    void enterMenuPhase();
    void showResult(const char* text);
    void updateHostAdvertising(uint32_t nowMs);
    void updateHostWaitSync(uint32_t nowMs);
    void updateVisitorSearching(uint32_t nowMs);
    void updateVisitorRoomList(uint32_t nowMs);
    void updateVisitorJoining(uint32_t nowMs);
    void updateVisitorWaitAccept(uint32_t nowMs);
    void activateCurrent();
    void renderMenu();
    void renderHostAdvertising();
    void renderHostWaitSync();
    void renderVisitorSearching();
    void renderVisitorRoomList();
    void renderStatusPage(const char* title, const char* subLine = nullptr);
    void renderResult();
    void renderVisiting();
    void renderToast();
};
