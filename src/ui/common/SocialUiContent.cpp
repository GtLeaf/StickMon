#include "ui/common/SocialUiContent.h"

#include <cstdio>
#include <cstring>
#include "core/UiStrings.h"
#include "presentation/PixelRenderer.h"

namespace Ui::SocialUiContent {
namespace {
using State = SocialState;
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

void drawCentered(const SocialLayout& layout, int top, const char* text, uint16_t color) {
    int left = (layout.width - textPixelWidth(text)) / 2;
    PixelRenderer::text(left < 0 ? 0 : left, top, text, color, 1);
}

void renderMenu(const SocialUiState& state);
void renderRoomList(const SocialFrame& frame, const SocialLayout& layout);
void renderStatus(const SocialFrame& frame, const SocialLayout& layout,
                  const char* title, const char* detail = nullptr);

void renderMenu(const SocialUiState& state) {
    auto& canvas = PixelRenderer::canvas();
    for (int index = 0; index < 3; ++index) {
        int top = 20 + index * 22;
        uint16_t color = index == state.cursor ? PixelRenderer::rgb(255, 216, 72)
                                         : PixelRenderer::rgb(241, 242, 232);
        if (index == state.cursor) canvas.fillRect(8, top + 3, 4, 14, color);
        PixelRenderer::text(22, top + 2, Ui::Social::MENU_ITEMS[index], color, 1);
    }
}

void renderRoomList(const SocialFrame& frame, const SocialLayout& layout) {
    const auto& model = frame.model;
    const auto& state = *frame.visuals;
    const char* title = Ui::Social::SELECT_ROOM;
    if (model.error && std::strcmp(model.error, "JOIN DECLINED") == 0) {
        title = Ui::Social::JOIN_REJECTED;
    } else if (model.error && std::strcmp(model.error, "JOIN TIMEOUT") == 0) {
        title = Ui::Social::JOIN_TIMED_OUT;
    }
    drawCentered(layout, 20, title,
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
        uint16_t color = index == state.roomCursor ? PixelRenderer::rgb(255, 216, 72)
                                             : PixelRenderer::rgb(241, 242, 232);
        if (index == state.roomCursor) {
            PixelRenderer::canvas().fillRect(40, top + 1, 4, 14, color);
        }
        PixelRenderer::text(56, top, label, color, 1);
    }
}

void renderStatus(const SocialFrame& frame, const SocialLayout& layout,
                  const char* title, const char* detail) {
    const auto& state = *frame.visuals;
    drawCentered(layout, 34, title, PixelRenderer::rgb(241, 242, 232));
    if (detail) {
        drawCentered(layout, 62, detail, PixelRenderer::rgb(160, 164, 174));
    } else {
        char dots[4] = {};
        int count = static_cast<int>((frame.nowMs - state.stateStartedMs) /
                                     SOCIAL_DOTS_INTERVAL_MS) % 3 + 1;
        for (int index = 0; index < count; ++index) dots[index] = '.';
        drawCentered(layout, 62, dots, PixelRenderer::rgb(160, 164, 174));
    }
    drawCentered(layout, 112, Ui::Social::CANCEL_HINT,
                 PixelRenderer::rgb(120, 124, 134));
}

}  // namespace

void draw(const SocialFrame& frame, const SocialLayout& layout) {
    auto& canvas = PixelRenderer::canvas();
    canvas.fillRect(0, 0, layout.width, layout.height,
                    PixelRenderer::rgb(7, 9, 14));
    const auto& model = frame.model;
    const auto& state = *frame.visuals;
    switch (model.state) {
    case State::IDLE:
        renderMenu(state);
        break;
    case State::HOSTING: {
        char room[24];
        std::snprintf(room, sizeof(room), Ui::Social::ROOM_ROW_FMT,
                      static_cast<unsigned>(model.roomId));
        renderStatus(frame, layout, Ui::Social::HOSTING_VISIT, room);
        break;
    }
    case State::SEARCHING:
        renderStatus(frame, layout, Ui::Social::SEARCHING_VISIT);
        break;
    case State::ROOM_LIST:
        renderRoomList(frame, layout);
        break;
    case State::JOINING:
        renderStatus(frame, layout, Ui::Social::WAIT_HOST_DECISION);
        break;
    case State::WAITING_HOST_DECISION:
        drawCentered(layout, 28, Ui::Social::INCOMING_REQUEST,
                     PixelRenderer::rgb(255, 216, 72));
        drawCentered(layout, 62, state.cursor == 0 ? Ui::Social::ACCEPT : Ui::Social::DECLINE,
                     PixelRenderer::rgb(241, 242, 232));
        drawCentered(layout, 112, Ui::Common::INPUT_HINT,
                     PixelRenderer::rgb(160, 164, 174));
        break;
    case State::SYNCING:
    case State::WAITING_ACCEPT:
        renderStatus(frame, layout, Ui::Social::SYNCING);
        break;
    case State::ACTIVE:
        drawCentered(layout, 26, Ui::Social::VISITING_TITLE,
                     PixelRenderer::rgb(255, 216, 72));
        drawCentered(layout, 58, model.localIsHost ? Ui::Social::VISITING_HOST_HINT
                                          : Ui::Social::VISITING_GUEST_HINT,
                     PixelRenderer::rgb(241, 242, 232));
        drawCentered(layout, 112, Ui::Social::RECALL_HINT,
                     PixelRenderer::rgb(160, 164, 174));
        break;
    case State::ENDING:
        renderStatus(frame, layout, Ui::Social::SYNCING);
        break;
    case State::FAILED:
    case State::ENDED:
        drawCentered(layout, 42, model.error ? model.error : Ui::Social::VISIT_ENDED,
                     PixelRenderer::rgb(255, 216, 72));
        drawCentered(layout, 112, Ui::Social::BACK_HINT,
                     PixelRenderer::rgb(160, 164, 174));
        break;
    }
}

}  // namespace Ui::SocialUiContent
