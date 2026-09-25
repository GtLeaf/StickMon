#include "scenes/SocialScene.h"

#include <cstdio>
#include <cstring>

#include "core/GameEngine.h"
#include "core/TraceLog.h"
#include "core/UiStrings.h"
#include "hardware/EspNowLink.h"
#include "hardware/Hal.h"
#include "presentation/PixelRenderer.h"

namespace {

constexpr uint32_t DOTS_INTERVAL_MS = 500;

int textPixelWidth(const char* value) {
    int width = 0;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(value);
    while (*bytes) {
        if (*bytes < 0x80) {
            width += *bytes == ' ' ? 5 : 8;
            ++bytes;
        } else if ((*bytes & 0xE0) == 0xC0) {
            width += 16;
            bytes += 2;
        } else if ((*bytes & 0xF0) == 0xE0) {
            width += 16;
            bytes += 3;
        } else {
            width += 8;
            ++bytes;
        }
    }
    return width;
}

void drawCentered(int top, const char* text, uint16_t color) {
    int left = (Hal::DISPLAY_W - textPixelWidth(text)) / 2;
    PixelRenderer::text(left < 0 ? 0 : left, top, text, color, 1);
}

}

void SocialScene::onEnter() {
    auto& service = GameEngine::ins().visitService();
    service.attach(&GameEngine::ins().gameState());
    if (!service.active() && !service.busy()) service.stop();
    EspNowLink::ins().begin();
    cursor = 0;
    roomCursor = 0;
    lastState = service.state();
    lastRoomCount = service.viewModel().roomCount;
    stateStartedMs = Hal::ins().millis();
    lastVisualTick = UINT32_MAX;
}

void SocialScene::onExit() {
    auto& service = GameEngine::ins().visitService();
    if (!service.active() && service.state() != State::ENDING) {
        service.stop();
        EspNowLink::ins().end();
    }
}

SceneUpdateResult SocialScene::update(uint32_t nowMs, float dtSeconds) {
    (void)dtSeconds;
    RenderDemand demand;
    auto& service = GameEngine::ins().visitService();
    const auto model = service.viewModel();
    if (model.state != lastState) {
        STICKMON_TRACEF("[VisitStick] state=%u->%u rooms=%u\n",
                        static_cast<unsigned>(lastState),
                        static_cast<unsigned>(model.state), model.roomCount);
        lastState = model.state;
        stateStartedMs = nowMs;
        roomCursor = 0;
        if (model.state == State::WAITING_HOST_DECISION) cursor = 0;
        demand.redraw();
        if (model.state == State::ACTIVE) {
            GameEngine::ins().requestScene(SceneID::MAIN);
            return demand.result();
        }
    }
    if (lastRoomCount != model.roomCount) {
        lastRoomCount = model.roomCount;
        demand.redraw();
    }
    if (model.state == State::HOSTING || model.state == State::SEARCHING ||
        model.state == State::JOINING || model.state == State::SYNCING ||
        model.state == State::WAITING_ACCEPT ||
        model.state == State::WAITING_HOST_DECISION) {
        uint32_t visualTick = nowMs / DOTS_INTERVAL_MS;
        if (visualTick != lastVisualTick) {
            lastVisualTick = visualTick;
            demand.redraw();
        }
        demand.wakeIn(66);
    } else if (model.state == State::ROOM_LIST ||
               model.state == State::ACTIVE || model.state == State::ENDING) {
        demand.wakeIn(200);
    }
    return demand.result();
}

bool SocialScene::onButton(const ButtonEvent& event) {
    auto& service = GameEngine::ins().visitService();
    const auto model = service.viewModel();
    if ((event.btn == 0 || event.btn == 1) &&
        event.action == BtnAction::LONG_PRESS) {
        GameEngine::ins().requestScene(SceneID::MENU);
        return true;
    }
    if (event.action != BtnAction::PRESSED) return false;

    if (model.state == State::IDLE) {
        if (event.btn == 1) {
            cursor = (cursor + 1) % 3;
        } else if (event.btn == 0) {
            if (cursor == 0) service.startHost();
            else if (cursor == 1) service.startSearch();
            else GameEngine::ins().requestScene(SceneID::MENU);
        }
        return true;
    }
    if (model.state == State::ROOM_LIST) {
        if (event.btn == 1) {
            roomCursor = (roomCursor + 1) % (model.roomCount + 1);
        } else if (event.btn == 0) {
            if (roomCursor == model.roomCount) service.stop();
            else service.selectRoom(roomCursor);
        }
        return true;
    }
    if (model.state == State::WAITING_HOST_DECISION) {
        if (event.btn == 1) cursor = (cursor + 1) % 2;
        else if (event.btn == 0) service.acceptIncoming(cursor == 0);
        return true;
    }
    if (event.btn == 0) {
        if (model.state == State::ACTIVE) service.endVisit();
        else if (model.state != State::ENDING) service.stop();
        return true;
    }
    return false;
}

void SocialScene::render() {
    auto& canvas = PixelRenderer::canvas();
    canvas.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H,
                    PixelRenderer::rgb(7, 9, 14));
    const auto model = GameEngine::ins().visitService().viewModel();
    const uint32_t nowMs = Hal::ins().millis();
    switch (model.state) {
    case State::IDLE:
        renderMenu();
        break;
    case State::HOSTING: {
        char room[24];
        std::snprintf(room, sizeof(room), Ui::Social::ROOM_ROW_FMT,
                      static_cast<unsigned>(model.roomId));
        renderStatus(Ui::Social::HOSTING_VISIT, nowMs, room);
        break;
    }
    case State::SEARCHING:
        renderStatus(Ui::Social::SEARCHING_VISIT, nowMs);
        break;
    case State::ROOM_LIST:
        renderRoomList(model);
        break;
    case State::JOINING:
        renderStatus(Ui::Social::WAIT_HOST_DECISION, nowMs);
        break;
    case State::WAITING_HOST_DECISION:
        drawCentered(28, Ui::Social::INCOMING_REQUEST,
                     PixelRenderer::rgb(255, 216, 72));
        drawCentered(62, cursor == 0 ? Ui::Social::ACCEPT : Ui::Social::DECLINE,
                     PixelRenderer::rgb(241, 242, 232));
        drawCentered(112, Ui::Common::INPUT_HINT,
                     PixelRenderer::rgb(160, 164, 174));
        break;
    case State::SYNCING:
    case State::WAITING_ACCEPT:
        renderStatus(Ui::Social::SYNCING, nowMs);
        break;
    case State::ACTIVE:
        drawCentered(26, Ui::Social::VISITING_TITLE,
                     PixelRenderer::rgb(255, 216, 72));
        drawCentered(58, model.localIsHost ? Ui::Social::VISITING_HOST_HINT
                                          : Ui::Social::VISITING_GUEST_HINT,
                     PixelRenderer::rgb(241, 242, 232));
        drawCentered(112, Ui::Social::RECALL_HINT,
                     PixelRenderer::rgb(160, 164, 174));
        break;
    case State::ENDING:
        renderStatus(Ui::Social::SYNCING, nowMs);
        break;
    case State::FAILED:
    case State::ENDED:
        drawCentered(42, model.error ? model.error : Ui::Social::VISIT_ENDED,
                     PixelRenderer::rgb(255, 216, 72));
        drawCentered(112, Ui::Social::BACK_HINT,
                     PixelRenderer::rgb(160, 164, 174));
        break;
    }
}

void SocialScene::renderMenu() {
    auto& canvas = PixelRenderer::canvas();
    for (int index = 0; index < 3; ++index) {
        int top = 20 + index * 22;
        uint16_t color = index == cursor ? PixelRenderer::rgb(255, 216, 72)
                                         : PixelRenderer::rgb(241, 242, 232);
        if (index == cursor) canvas.fillRect(8, top + 3, 4, 14, color);
        PixelRenderer::text(22, top + 2, Ui::Social::MENU_ITEMS[index], color, 1);
    }
}

void SocialScene::renderRoomList(
    const Communication::VisitSessionService::ViewModel& model) {
    const char* title = Ui::Social::SELECT_ROOM;
    if (model.error && std::strcmp(model.error, "JOIN DECLINED") == 0) {
        title = Ui::Social::JOIN_REJECTED;
    } else if (model.error && std::strcmp(model.error, "JOIN TIMEOUT") == 0) {
        title = Ui::Social::JOIN_TIMED_OUT;
    }
    drawCentered(20, title,
                 PixelRenderer::rgb(255, 216, 72));
    for (uint8_t index = 0; index <= model.roomCount; ++index) {
        int top = 40 + index * 20;
        char room[24];
        const char* label = Ui::BACK;
        if (index < model.roomCount) {
            std::snprintf(room, sizeof(room), Ui::Social::ROOM_ROW_FMT,
                          static_cast<unsigned>(model.rooms[index].roomId));
            label = room;
        }
        uint16_t color = index == roomCursor ? PixelRenderer::rgb(255, 216, 72)
                                             : PixelRenderer::rgb(241, 242, 232);
        if (index == roomCursor) {
            PixelRenderer::canvas().fillRect(40, top + 1, 4, 14, color);
        }
        PixelRenderer::text(56, top, label, color, 1);
    }
}

void SocialScene::renderStatus(const char* title, uint32_t nowMs,
                               const char* detail) {
    drawCentered(34, title, PixelRenderer::rgb(241, 242, 232));
    if (detail) {
        drawCentered(62, detail, PixelRenderer::rgb(160, 164, 174));
    } else {
        char dots[4] = {};
        int count = static_cast<int>((nowMs - stateStartedMs) /
                                     DOTS_INTERVAL_MS) % 3 + 1;
        for (int index = 0; index < count; ++index) dots[index] = '.';
        drawCentered(62, dots, PixelRenderer::rgb(160, 164, 174));
    }
    drawCentered(112, Ui::Social::CANCEL_HINT,
                 PixelRenderer::rgb(120, 124, 134));
}
