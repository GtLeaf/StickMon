#include "scenes/SocialScene.h"
#include <cstdio>
#include <cstring>
#include "core/GameEngine.h"
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "hardware/PixelRenderer.h"

namespace {

constexpr uint32_t DOTS_INTERVAL_MS = 500;
constexpr int TITLE_Y = 34;
constexpr int SUB_Y = 62;
constexpr int SUB2_Y = 84;
constexpr int HINT_Y = 112;

int textPixelWidth(const char* value) {
    int width = 0;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(value);
    while (*p) {
        if (*p < 0x80) {
            width += *p == ' ' ? 5 : 8;
            ++p;
        } else if ((*p & 0xE0) == 0xC0) {
            width += 16;
            p += 2;
        } else if ((*p & 0xF0) == 0xE0) {
            width += 16;
            p += 3;
        } else {
            width += 8;
            ++p;
        }
    }
    return width;
}

void drawCentered(int y, const char* text, uint16_t color) {
    int x = (Hal::DISPLAY_W - textPixelWidth(text)) / 2;
    if (x < 0) x = 0;
    PixelRenderer::text(x, y, text, color, 1);
}

// Title stays centered while 1~3 dots cycle after it.
void drawCenteredWithDots(int y, const char* title, uint32_t elapsedMs, uint16_t color) {
    int titleW = textPixelWidth(title);
    int x = (Hal::DISPLAY_W - titleW) / 2;
    if (x < 0) x = 0;
    PixelRenderer::text(x, y, title, color, 1);
    char dots[4];
    int count = (int)(elapsedMs / DOTS_INTERVAL_MS) % 3 + 1;
    for (int i = 0; i < count; ++i) dots[i] = '.';
    dots[count] = '\0';
    PixelRenderer::text(x + titleW + 2, y, dots, color, 1);
}

} // namespace

void SocialScene::onEnter() {
    EspNowLink::ins().begin();
    cursor = 0;
    joinRequested = false;
    joinRequestedAt = 0;
    syncSent = false;
    acceptPending = false;
    resultText = nullptr;
    phase = GameEngine::ins().visitActive() ? LinkPhase::VISITING : LinkPhase::MENU;
}

void SocialScene::onExit() {
    // A visit in progress keeps the link alive; GameEngine::updateVisit pumps it.
    if (!GameEngine::ins().visitActive()) EspNowLink::ins().end();
}

void SocialScene::update(uint32_t nowMs, float dtSeconds) {
    (void)dtSeconds;
    EspNowLink::ins().update();

    if (GameEngine::ins().takeVisitLinkLost()) {
        enterMenuPhase();
        EspNowLink::ins().stopRoom();
        setToast(Ui::Social::LINK_LOST);
    }

    switch (phase) {
    case LinkPhase::HOST_ADVERTISING:
        updateHostAdvertising(nowMs);
        break;
    case LinkPhase::HOST_WAIT_SYNC:
        updateHostWaitSync(nowMs);
        break;
    case LinkPhase::VISITOR_SEARCHING:
        updateVisitorSearching(nowMs);
        break;
    case LinkPhase::VISITOR_ROOM_LIST:
        updateVisitorRoomList(nowMs);
        break;
    case LinkPhase::VISITOR_JOINING:
        updateVisitorJoining(nowMs);
        break;
    case LinkPhase::VISITOR_WAIT_ACCEPT:
        updateVisitorWaitAccept(nowMs);
        break;
    case LinkPhase::VISITING:
        if (!GameEngine::ins().visitActive()) enterMenuPhase();
        break;
    case LinkPhase::MENU:
    case LinkPhase::RESULT:
    default:
        break;
    }
}

void SocialScene::updateHostAdvertising(uint32_t nowMs) {
    EspNowLink::RoomPurpose purpose;
    uint16_t requestSeq = 0;
    if (EspNowLink::ins().takeJoinRequest(joinMac, purpose, requestSeq)) {
        EspNowLink::ins().sendJoinAck(joinMac, true, requestSeq);
        acceptPending = false;
        phase = LinkPhase::HOST_WAIT_SYNC;
        phaseStartedMs = nowMs;
        return;
    }

    if (nowMs - phaseStartedMs >= HOST_ADVERTISE_TIMEOUT_MS) {
        showResult(Ui::Social::HOST_TIMEOUT);
    }
}

void SocialScene::updateVisitorSearching(uint32_t nowMs) {
    if (EspNowLink::ins().roomCount() > 0) {
        roomCursor = 0;
        phase = LinkPhase::VISITOR_ROOM_LIST;
        return;
    }

    if (nowMs - phaseStartedMs >= SEARCH_TIMEOUT_MS) {
        showResult(Ui::Social::SEARCH_TIMEOUT);
    }
}

void SocialScene::updateVisitorRoomList(uint32_t nowMs) {
    (void)nowMs;
    uint8_t count = EspNowLink::ins().roomCount();
    if (count == 0) {
        // All rooms expired; rescan with a fresh budget.
        phase = LinkPhase::VISITOR_SEARCHING;
        phaseStartedMs = Hal::ins().millis();
        return;
    }
    // Rows are rooms plus a trailing back entry.
    if (roomCursor > count) roomCursor = 0;
}

void SocialScene::updateVisitorJoining(uint32_t nowMs) {
    bool accepted = false;
    if (EspNowLink::ins().takeJoinAck(accepted)) {
        joinRequested = false;
        if (!accepted) {
            // Multi-device: let the user pick another room instead of bailing out.
            phase = LinkPhase::VISITOR_ROOM_LIST;
            setToast(Ui::Social::JOIN_REJECTED);
        } else {
            const Game::MonsterRuntime& mon = GameEngine::ins().activeMonster();
            pendingSync.speciesId = mon.speciesId;
            pendingSync.level = mon.level;
            pendingSync.nature = mon.nature;
            pendingSync.satiety = mon.satiety;
            pendingSync.mood = mon.mood;
            pendingSync.affection = mon.affection;
            syncSent = false;
            lastSendTryMs = 0;
            phase = LinkPhase::VISITOR_WAIT_ACCEPT;
            phaseStartedMs = nowMs;
        }
        return;
    }

    if (nowMs - joinRequestedAt >= JOIN_ACK_TIMEOUT_MS) {
        joinRequested = false;
        // Host may have accepted another device; go back to the room list.
        phase = LinkPhase::VISITOR_ROOM_LIST;
        setToast(Ui::Social::LINK_FAILED);
    }
}

void SocialScene::updateHostWaitSync(uint32_t nowMs) {
    if (acceptPending) {
        if (nowMs - lastSendTryMs >= SEND_RETRY_MS) {
            lastSendTryMs = nowMs;
            if (EspNowLink::ins().sendSessionMessage(LinkMessageType::VISIT_ACCEPT,
                                                     &pendingAccept, sizeof(pendingAccept))) {
                acceptPending = false;
                if (pendingAccept.accepted) {
                    // 链接成功,房主直接回主界面(访客精灵已在队伍中)
                    GameEngine::ins().requestScene(SceneID::MAIN);
                } else {
                    showResult(Ui::Social::HOST_FULL);
                }
                return;
            }
        }
    } else {
        LinkMessageType type;
        uint8_t payload[24];
        uint8_t payloadLen = 0;
        if (EspNowLink::ins().takeSessionMessage(type, payload, payloadLen) &&
            type == LinkMessageType::VISIT_SYNC && payloadLen >= sizeof(VisitSyncPayload)) {
            VisitSyncPayload sync{};
            memcpy(&sync, payload, sizeof(sync));
            uint8_t reason = GameEngine::ins().beginVisitAsHost(
                sync.speciesId, sync.level, sync.nature, sync.satiety, sync.mood, sync.affection);
            pendingAccept.accepted = reason == 0 ? 1 : 0;
            pendingAccept.reason = reason;
            acceptPending = true;
            lastSendTryMs = 0;
            phaseStartedMs = nowMs;
        }
    }

    if (nowMs - phaseStartedMs >= HANDSHAKE_TIMEOUT_MS) {
        if (GameEngine::ins().visitActive()) {
            phase = LinkPhase::VISITING;
        } else {
            showResult(Ui::Social::LINK_FAILED);
        }
    }
}

void SocialScene::updateVisitorWaitAccept(uint32_t nowMs) {
    if (!syncSent && nowMs - lastSendTryMs >= SEND_RETRY_MS) {
        lastSendTryMs = nowMs;
        syncSent = EspNowLink::ins().sendSessionMessage(LinkMessageType::VISIT_SYNC,
                                                        &pendingSync, sizeof(pendingSync));
    }

    LinkMessageType type;
    uint8_t payload[24];
    uint8_t payloadLen = 0;
    if (EspNowLink::ins().takeSessionMessage(type, payload, payloadLen) &&
        type == LinkMessageType::VISIT_ACCEPT && payloadLen >= sizeof(VisitAcceptPayload)) {
        VisitAcceptPayload accept{};
        memcpy(&accept, payload, sizeof(accept));
        if (accept.accepted) {
            GameEngine::ins().beginVisitAsVisitor();
            phase = LinkPhase::VISITING;
            setToast(Ui::Social::ENTERED_ROOM);
        } else {
            showResult(accept.reason == 1 ? Ui::Social::REJECT_STORAGE_FULL
                                          : Ui::Social::REJECT_NO_MONSTER);
        }
        return;
    }

    if (nowMs - phaseStartedMs >= HANDSHAKE_TIMEOUT_MS) {
        showResult(Ui::Social::LINK_FAILED);
    }
}

bool SocialScene::onButton(const ButtonEvent& event) {
    if ((event.btn == 0 || event.btn == 1) && event.action == BtnAction::LONG_PRESS) {
        GameEngine::ins().requestScene(SceneID::MENU);
        return true;
    }

    if (phase == LinkPhase::VISITING) {
        if (event.btn == 0 && event.action == BtnAction::PRESSED) {
            if (GameEngine::ins().visitAsHost()) {
                VisitEndPayload end{0};
                EspNowLink::ins().sendSessionMessage(LinkMessageType::VISIT_END,
                                                     &end, sizeof(end));
            } else {
                EspNowLink::ins().sendSessionMessage(LinkMessageType::VISIT_RECALL,
                                                     nullptr, 0);
            }
            GameEngine::ins().endVisit();
            enterMenuPhase();
            return true;
        }
        return false;
    }

    if (phase == LinkPhase::MENU) {
        if (event.btn == 1 && event.action == BtnAction::PRESSED) {
            cursor = (cursor + 1) % ITEM_COUNT;
            return true;
        }
        if (event.btn == 0 && event.action == BtnAction::PRESSED) {
            activateCurrent();
            return true;
        }
        return false;
    }

    if (phase == LinkPhase::VISITOR_ROOM_LIST) {
        uint8_t count = EspNowLink::ins().roomCount();
        uint8_t rows = count + 1; // trailing back row
        if (event.btn == 1 && event.action == BtnAction::PRESSED) {
            roomCursor = (roomCursor + 1) % rows;
            return true;
        }
        if (event.btn == 0 && event.action == BtnAction::PRESSED) {
            if (roomCursor >= count) {
                EspNowLink::ins().stopRoom();
                enterMenuPhase();
                return true;
            }
            if (EspNowLink::ins().sendJoinRequest(roomCursor)) {
                joinRequested = true;
                joinRequestedAt = Hal::ins().millis();
                phase = LinkPhase::VISITOR_JOINING;
            }
            return true;
        }
        return false;
    }

    // Flow and result pages: A cancels back to the social menu.
    if (event.btn == 0 && event.action == BtnAction::PRESSED) {
        EspNowLink::ins().stopRoom();
        enterMenuPhase();
        return true;
    }
    return false;
}

void SocialScene::activateCurrent() {
    switch (cursor) {
    case ITEM_INVITE:
        joinRequested = false;
        EspNowLink::ins().startHost(EspNowLink::RoomPurpose::VISIT);
        phase = LinkPhase::HOST_ADVERTISING;
        phaseStartedMs = Hal::ins().millis();
        break;
    case ITEM_VISIT:
        joinRequested = false;
        EspNowLink::ins().startSearch(EspNowLink::RoomPurpose::VISIT);
        phase = LinkPhase::VISITOR_SEARCHING;
        phaseStartedMs = Hal::ins().millis();
        break;
    case ITEM_BACK:
    default:
        GameEngine::ins().requestScene(SceneID::MENU);
        return;
    }
}

void SocialScene::setToast(const char* text, uint32_t durationMs) {
    toast = text;
    toastUntil = Hal::ins().millis() + durationMs;
}

void SocialScene::enterMenuPhase() {
    phase = LinkPhase::MENU;
    joinRequested = false;
    syncSent = false;
    acceptPending = false;
    resultText = nullptr;
}

void SocialScene::showResult(const char* text) {
    EspNowLink::ins().stopRoom();
    joinRequested = false;
    syncSent = false;
    acceptPending = false;
    resultText = text;
    phase = LinkPhase::RESULT;
}

void SocialScene::render() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb(7, 9, 14));

    switch (phase) {
    case LinkPhase::HOST_ADVERTISING:
        renderHostAdvertising();
        break;
    case LinkPhase::HOST_WAIT_SYNC:
        renderHostWaitSync();
        break;
    case LinkPhase::VISITOR_SEARCHING:
        renderVisitorSearching();
        break;
    case LinkPhase::VISITOR_ROOM_LIST:
        renderVisitorRoomList();
        break;
    case LinkPhase::VISITOR_JOINING:
        renderStatusPage(Ui::Social::REQUESTING_JOIN);
        break;
    case LinkPhase::VISITOR_WAIT_ACCEPT:
        renderStatusPage(Ui::Social::SYNCING);
        break;
    case LinkPhase::VISITING:
        renderVisiting();
        break;
    case LinkPhase::RESULT:
        renderResult();
        break;
    case LinkPhase::MENU:
    default:
        renderMenu();
        break;
    }
    renderToast();
}

void SocialScene::renderMenu() {
    auto& c = PixelRenderer::canvas();
    const int rowH = 22;
    const int startY = 20;

    for (int i = 0; i < (int)ITEM_COUNT; ++i) {
        int y = startY + i * rowH;
        uint16_t color = (i == cursor) ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);
        if (i == cursor) {
            c.fillRect(8, y + 3, 4, 14, PixelRenderer::rgb(255, 216, 72));
        }
        PixelRenderer::text(22, y + 2, Ui::Social::MENU_ITEMS[i], color, 1);
        if (i < (int)ITEM_COUNT - 1) {
            c.drawFastHLine(22, y + rowH - 1, 150, PixelRenderer::rgb(70, 74, 84));
        }
    }
}

void SocialScene::renderHostAdvertising() {
    uint32_t elapsed = Hal::ins().millis() - phaseStartedMs;
    drawCenteredWithDots(TITLE_Y, Ui::Social::HOSTING_VISIT, elapsed,
                         PixelRenderer::rgb(241, 242, 232));
    drawCentered(SUB_Y, Ui::Social::WAIT_GUEST_SUB, PixelRenderer::rgb(160, 164, 174));
    char buf[24];
    snprintf(buf, sizeof(buf), Ui::Social::WAIT_SECONDS_FMT, (unsigned)(elapsed / 1000));
    drawCentered(SUB2_Y, buf, PixelRenderer::rgb(160, 164, 174));
    drawCentered(HINT_Y, Ui::Social::CANCEL_HINT, PixelRenderer::rgb(120, 124, 134));
}

void SocialScene::renderHostWaitSync() {
    // Before the guest's sync packet arrives the guest is still connecting;
    // once we answer with VISIT_ACCEPT we are finalizing the sync.
    renderStatusPage(acceptPending ? Ui::Social::SYNCING : Ui::Social::GUEST_CONNECTING);
}

void SocialScene::renderVisitorSearching() {
    uint32_t elapsed = Hal::ins().millis() - phaseStartedMs;
    drawCenteredWithDots(TITLE_Y, Ui::Social::SEARCHING_VISIT, elapsed,
                         PixelRenderer::rgb(241, 242, 232));
    char buf[24];
    snprintf(buf, sizeof(buf), Ui::Social::ROOMS_FOUND_FMT,
             (unsigned)EspNowLink::ins().roomCount());
    drawCentered(SUB_Y, buf, PixelRenderer::rgb(160, 164, 174));
    drawCentered(HINT_Y, Ui::Social::CANCEL_HINT, PixelRenderer::rgb(120, 124, 134));
}

void SocialScene::renderVisitorRoomList() {
    auto& c = PixelRenderer::canvas();
    uint8_t count = EspNowLink::ins().roomCount();
    drawCentered(TITLE_Y - 14, Ui::Social::SELECT_ROOM, PixelRenderer::rgb(255, 216, 72));

    const int rowH = 20;
    const int startY = 40;
    uint8_t rows = count + 1;
    for (uint8_t i = 0; i < rows; ++i) {
        int y = startY + i * rowH;
        bool selected = i == roomCursor;
        uint16_t color = selected ? PixelRenderer::rgb(255, 216, 72)
                                  : PixelRenderer::rgb(241, 242, 232);
        if (selected) {
            c.fillRect(40, y + 1, 4, 14, PixelRenderer::rgb(255, 216, 72));
        }
        if (i < count) {
            EspNowLink::RoomEntry room;
            char buf[24];
            buf[0] = '\0';
            if (EspNowLink::ins().copyRoomAt(i, room)) {
                snprintf(buf, sizeof(buf), Ui::Social::ROOM_ROW_FMT,
                         static_cast<unsigned>(room.roomId));
            }
            PixelRenderer::text(56, y, buf, color, 1);
        } else {
            PixelRenderer::text(56, y, Ui::BACK, color, 1);
        }
    }
}

void SocialScene::renderStatusPage(const char* title, const char* subLine) {
    uint32_t elapsed = Hal::ins().millis() - phaseStartedMs;
    drawCenteredWithDots(TITLE_Y, title, elapsed, PixelRenderer::rgb(241, 242, 232));
    if (subLine) drawCentered(SUB_Y, subLine, PixelRenderer::rgb(160, 164, 174));
    drawCentered(HINT_Y, Ui::Social::CANCEL_HINT, PixelRenderer::rgb(120, 124, 134));
}

void SocialScene::renderResult() {
    drawCentered(TITLE_Y + 10, resultText ? resultText : Ui::Social::LINK_FAILED,
                 PixelRenderer::rgb(255, 216, 72));
    drawCentered(HINT_Y, Ui::Social::BACK_HINT, PixelRenderer::rgb(120, 124, 134));
}

void SocialScene::renderVisiting() {
    bool asHost = GameEngine::ins().visitAsHost();
    PixelRenderer::text(20, 24, Ui::Social::VISITING_TITLE, PixelRenderer::rgb(255, 216, 72), 1);
    PixelRenderer::text(20, 52,
                        asHost ? Ui::Social::VISITING_HOST_HINT : Ui::Social::VISITING_GUEST_HINT,
                        PixelRenderer::rgb(241, 242, 232), 1);
    PixelRenderer::text(20, 110, Ui::Social::RECALL_HINT, PixelRenderer::rgb(160, 164, 174), 1);
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
