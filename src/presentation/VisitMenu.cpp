#include "presentation/VisitMenu.h"
#include "assets/VisitIcons.h"
#include "core/UiStrings.h"
#include "presentation/PixelRenderer.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
namespace Ui::VisitMenu {
namespace {
constexpr uint16_t INK = 0xe77d, MINT = 0x7716, CORAL = 0xec72, AMBER = 0xf68d;
constexpr uint16_t PANEL = 0x1925, BORDER = 0x436c;
using Font = BitmapText::Font;
void label(Canvas565 &c, int cx, int y, const char *s, uint16_t color, Font font = Font::PX32) {
    int width = 0;
    const char *end = s + std::strlen(s);
    for (const char *p = s; p < end;)
        width += BitmapText::advance(BitmapText::decode(p, end), font);
    PixelRenderer::bitmapText(c, cx - width / 2, y, s, color, font);
}
void button(Canvas565 &c, MenuGrid::Rect r, const char *s, uint16_t color, bool down) {
    c.fillRoundRect(r.x, r.y, r.width, r.height, 8, down ? 0x2a89 : PANEL);
    c.drawRoundRect(r.x, r.y, r.width, r.height, 8, down ? MINT : BORDER);
    label(c, r.x + r.width / 2, r.y + (r.height - 32) / 2 + (down ? 1 : 0), s, color);
}
bool same(const char *a, const char *b) {
    return a && std::strcmp(a, b) == 0;
}
} // namespace
Error error(const char *code) {
    using namespace VisitText;
    if (same(code, "TEAM NOT SOLO"))
        return {UNAVAILABLE, SOLO, ADJUST, true};
    if (same(code, "NO PET"))
        return {UNAVAILABLE, NO_PET, Ui::BACK, false};
    if (same(code, "LINK UNAVAILABLE"))
        return {FAILED, RADIO, Ui::BACK, false};
    if (same(code, "SEARCH TIMEOUT"))
        return {NO_ROOM, nullptr, Ui::BACK, false};
    if (same(code, "HOST TIMEOUT"))
        return {NO_VISITOR, nullptr, Ui::BACK, false};
    if (same(code, "LINK LOST"))
        return {LOST, nullptr, Ui::BACK, false};
    if (same(code, "HOST TEAM BUSY") || same(code, "HOST NO PET") || same(code, "HOST FULL"))
        return {FAILED, HOST_BUSY, Ui::BACK, false};
    return {FAILED, TIMEOUT, Ui::BACK, false};
}
const char *title(const Model &m) {
    using namespace VisitText;
    switch (m.state) {
    case State::HOSTING:
        return WAITING;
    case State::SEARCHING:
    case State::ROOM_LIST:
        if (same(m.error, "JOIN DECLINED"))
            return VisitText::REJECTED;
        if (same(m.error, "JOIN TIMEOUT"))
            return VisitText::NO_CONFIRM;
        return VisitText::SEARCHING;
    case State::WAITING_HOST_DECISION:
        return REQUEST;
    case State::JOINING:
    case State::WAITING_ACCEPT:
        return JOINING;
    case State::SYNCING:
        return SYNCING;
    case State::ACTIVE:
        return ACTIVE;
    case State::ENDING:
        return ENDING;
    case State::FAILED:
        return error(m.error).title;
    case State::ENDED:
        return ENDED;
    default:
        return VisitText::TITLE;
    }
}
bool waiting(State s) {
    return s == State::HOSTING || s == State::SEARCHING || s == State::ROOM_LIST ||
           s == State::JOINING || s == State::SYNCING || s == State::WAITING_ACCEPT ||
           s == State::ENDING;
}
bool sameRooms(const Model &a, const Model &b) {
    if (a.roomCount != b.roomCount)
        return false;
    for (uint8_t i = 0; i < a.roomCount; ++i) {
        if (a.rooms[i].roomId != b.rooms[i].roomId ||
            std::memcmp(a.rooms[i].mac, b.rooms[i].mac, 6) != 0)
            return false;
    }
    return true;
}
bool sameTarget(const Model &before, const Model &after, int item) {
    if (before.state != after.state)
        return false;
    if (after.state == State::ROOM_LIST && item >= 0 && item < CANCEL) {
        return item < before.roomCount && item < after.roomCount &&
               before.rooms[item].roomId == after.rooms[item].roomId &&
               std::memcmp(before.rooms[item].mac, after.rooms[item].mac, 6) == 0;
    }
    if (after.state == State::WAITING_HOST_DECISION) {
        return before.requestSeq == after.requestSeq &&
               std::memcmp(before.peerMac, after.peerMac, 6) == 0;
    }
    return true;
}
MenuGrid::Rect homeButton(bool r, int i) {
    return r ? MenuGrid::Rect{44, 112 + i * 92, 272, 76}
             : MenuGrid::Rect{20, 112 + i * 112, 328, 88};
}
MenuGrid::Rect roomRow(bool r, int i) {
    return r ? MenuGrid::Rect{36, 102 + i * 72, 288, 62}
             : MenuGrid::Rect{16, 108 + i * 72, 336, 64};
}
MenuGrid::Rect decision(bool r, int i) {
    return r ? MenuGrid::Rect{64, 212 + i * 62, 232, 50}
             : MenuGrid::Rect{20, 248 + i * 92, 328, 72};
}
MenuGrid::Rect roomViewport() {
    return {36, 102, 288, 150};
}
int roomScrollMaximum(uint8_t count) {
    return std::max(0, int(count) * 72 - 10 - roomViewport().height);
}
MenuGrid::Rect cancelButton(bool r) {
    return r ? MenuGrid::Rect{76, 264, 208, 48} : MenuGrid::Rect{64, 396, 240, 44};
}
MenuGrid::Rect resultButton(bool r) {
    return r ? MenuGrid::Rect{64, 258, 232, 52} : MenuGrid::Rect{20, 340, 328, 72};
}
MenuGrid::Rect backButton(bool r) {
    return r ? MenuGrid::Rect{70, 34, 44, 44} : MenuGrid::Rect{0, 0, 80, 76};
}
void shortId(const uint8_t mac[6], char out[5]) {
    std::snprintf(out, 5, "%02X%02X", mac[4], mac[5]);
}
int itemAt(int x, int y, const Model &m, bool r, int scrollOffset) {
    if (r) {
        int dx = 2 * x - 359, dy = 2 * y - 359;
        if (dx * dx + dy * dy >= 360 * 360)
            return -1;
    }
    if (m.state == State::IDLE) {
        for (int i = 0; i < 2; ++i)
            if (homeButton(r, i).contains(x, y))
                return i;
    } else if (m.state == State::SEARCHING || m.state == State::ROOM_LIST) {
        if (!r || roomViewport().contains(x, y)) {
            for (int i = 0; i < m.roomCount; ++i) {
                auto row = roomRow(r, i);
                if (r)
                    row.y -= std::clamp(scrollOffset, 0, roomScrollMaximum(m.roomCount));
                if (row.contains(x, y))
                    return i;
            }
        }
        if (cancelButton(r).contains(x, y))
            return CANCEL;
    } else if (m.state == State::WAITING_HOST_DECISION) {
        for (int i = 0; i < 2; ++i)
            if (decision(r, i).contains(x, y))
                return i;
    } else if (m.state == State::FAILED || m.state == State::ENDED) {
        if (resultButton(r).contains(x, y))
            return 0;
    } else if (m.state == State::ACTIVE) {
        if (resultButton(r).contains(x, y))
            return 0;
    } else if (m.state != State::ENDING && cancelButton(r).contains(x, y))
        return CANCEL;
    return -1;
}
void render(Canvas565 &c, const Model &m, bool r, uint32_t now, int pressed, int scrollOffset) {
    using namespace VisitText;
    int cx = r ? 180 : 184;
    if (r) {
        auto b = backButton(true);
        c.fillCircle(b.x + 22, b.y + 22, 17, PANEL);
        c.drawCircle(b.x + 22, b.y + 22, 17, BORDER);
        for (int i = 0; i < 2; ++i) {
            c.drawLine(b.x + 25 + i, b.y + 14, b.x + 17 + i, b.y + 22, INK);
            c.drawLine(b.x + 17 + i, b.y + 22, b.x + 25 + i, b.y + 30, INK);
        }
        label(c, 210, 40, title(m), INK);
    }
    if (m.state == State::IDLE) {
        for (int i = 0; i < 2; ++i) {
            auto b = homeButton(r, i);
            button(c, b, "", i ? INK : MINT, pressed == i);
            if (i)
                VisitIcons::draw(c, VisitIcons::Kind::PAWS, b.x + 42, b.y + (b.height - 48) / 2);
            else
                VisitIcons::draw(c, VisitIcons::Kind::DOOR, b.x + 42, b.y + (b.height - 48) / 2);
            label(c, b.x + b.width / 2 + 20, b.y + (b.height - 32) / 2, i ? SEARCH : HOST,
                  i ? INK : MINT);
        }
    } else if (m.state == State::SEARCHING || m.state == State::ROOM_LIST) {
        const int left = c.clipLeft(), top = c.clipTop(), right = c.clipRight(),
                  bottom = c.clipBottom();
        const auto viewport = roomViewport();
        const int maximum = r ? roomScrollMaximum(m.roomCount) : 0;
        const int offset = std::clamp(scrollOffset, 0, maximum);
        if (r) {
            const int x = std::max(left, viewport.x), y = std::max(top, viewport.y);
            c.setClipRect(x, y, std::max(0, std::min(right, viewport.x + viewport.width) - x),
                          std::max(0, std::min(bottom, viewport.y + viewport.height) - y));
        }
        for (int i = 0; i < m.roomCount; ++i) {
            auto b = roomRow(r, i);
            b.y -= offset;
            button(c, b, "", INK, pressed == i);
            char value[24];
            std::snprintf(value, sizeof(value), Ui::Social::ROOM_ROW_FMT, m.rooms[i].roomId);
            PixelRenderer::bitmapText(c, b.x + 16, b.y + 4, value, INK, Font::PX32);
            char id[5];
            shortId(m.rooms[i].mac, id);
            PixelRenderer::bitmapText(c, b.x + 16, b.y + 40, id, INK, Font::PX16);
            label(c, b.x + b.width - 44, b.y + 14, Ui::Amoled::JOIN, MINT);
        }
        c.setClipRect(left, top, right - left, bottom - top);
        if (r && maximum > 0) {
            const int thumb =
                std::max(24, viewport.height * viewport.height / (viewport.height + maximum));
            const int y = viewport.y + offset * (viewport.height - thumb) / maximum;
            c.fillRoundRect(330, viewport.y, 4, viewport.height, 2, BORDER);
            c.fillRoundRect(330, y, 4, thumb, 2, MINT);
        }
        button(c, cancelButton(r), Ui::Social::RECALL_CANCEL, INK, pressed == CANCEL);
    } else if (m.state == State::WAITING_HOST_DECISION) {
        VisitIcons::draw(c, VisitIcons::Kind::VISITOR, cx, r ? 92 : 112);
        char id[5];
        shortId(m.peerMac, id);
        label(c, cx, r ? 160 : 192, id, INK);
        button(c, decision(r, 0), Ui::Amoled::ACCEPT, MINT, pressed == 0);
        button(c, decision(r, 1), Ui::Amoled::DECLINE, CORAL, pressed == 1);
    } else if (m.state == State::FAILED) {
        auto e = error(m.error);
        VisitIcons::draw(c, VisitIcons::Kind::WARNING, cx, r ? 100 : 120);
        if (e.detail)
            label(c, cx, r ? 188 : 228, e.detail, INK);
        button(c, resultButton(r), e.action, MINT, pressed == 0);
    } else if (m.state == State::ENDED) {
        button(c, resultButton(r), Ui::BACK, MINT, pressed == 0);
    } else if (m.state == State::ACTIVE) {
        if (m.remote.known) {
            char stats[64];
            std::snprintf(stats, sizeof(stats), "%s %u  %s %u", Ui::HUNGER, m.remote.satiety,
                          Ui::MOOD, m.remote.mood);
            label(c, cx, r ? 112 : 136, stats, INK, Font::PX16);
            if (m.remote.hpMax)
                std::snprintf(stats, sizeof(stats), "HP %u/%u", m.remote.hpCur, m.remote.hpMax);
            else
                std::snprintf(stats, sizeof(stats), "HP --");
            label(c, cx, r ? 140 : 164, stats, INK, Font::PX16);
        }
        char time[16];
        std::snprintf(time, sizeof(time), "%u:%02u", m.remainSec / 60, m.remainSec % 60);
        label(c, cx, r ? 184 : 216, time, MINT);
        button(c, resultButton(r), END, CORAL, pressed == 0);
    } else {
        if (m.state == State::HOSTING) {
            char value[24];
            std::snprintf(value, sizeof(value), Ui::Social::ROOM_ROW_FMT, m.roomId);
            label(c, cx, r ? 150 : 192, value, INK);
        }
        if (m.state != State::ENDING)
            button(c, cancelButton(r), Ui::Social::RECALL_CANCEL, INK, pressed == CANCEL);
    }
    if (waiting(m.state)) {
        for (int i = 0; i < 3; ++i)
            c.fillRect(cx - 24 + i * 20, r ? 84 : 86, 8, 8,
                       i == int(now / 500 % 3) ? MINT : BORDER);
    }
}
} // namespace Ui::VisitMenu
