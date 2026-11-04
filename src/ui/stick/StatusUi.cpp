#include "ui/stick/StatusUi.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "assets/GameAssets.h"
#include "core/MathUtil.h"
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TouchUi.h"
#include "ui/common/StatusUiContent.h"

namespace Ui {
using namespace StatusContent;

void StatusUi::reset() {
    cancelTouch();
    statusPage = 0;
    statusMonsterIndex = 0;
    statusFromStorage = false;
    statusScrollKey = -1;
    statusScroll = 0.0f;
    statusScrollLastMs = Hal::ins().millis();
}

void StatusUi::render() { renderStatusPage(); }

bool StatusUi::onButton(const ButtonEvent& event) {
    if (!host.pageExploreContext() && event.btn == 1 && event.action == BtnAction::LONG_PRESS)
        host.pageBackTutorial();
    if (event.action == BtnAction::LONG_PRESS && event.btn == 0) {
        host.pageHome();
        return true;
    }
    if (event.action == BtnAction::LONG_PRESS && event.btn == 1) {
        host.pageBack();
        return true;
    }
    if (event.action == BtnAction::PRESSED && event.btn == 1) {
        changeStatusPage(1);
        return true;
    }
    if (event.action == BtnAction::PRESSED && event.btn == 0) {
        host.pageBack();
        return true;
    }
    return false;
}

StatusUi::~StatusUi() = default;
void StatusUi::cancelTouch() { Scene::cancelTouch(); }
bool StatusUi::onTouch(const Platform::IInputDevice::TouchEvent& event) { return Scene::onTouch(event); }
int StatusUi::touchTarget(int16_t, int16_t) { return -1; }
bool StatusUi::onTap(int) { return false; }
void StatusUi::changeStatusPage(int direction) {
    cancelTouch();
    statusPage = (statusPage + (direction < 0 ? STATUS_PAGE_COUNT - 1 : 1)) % STATUS_PAGE_COUNT;
    statusScroll = 0;
    statusScrollKey = -1;
}
void StatusUi::renderStatusOverlay() { renderPageIndicator(statusPage, STATUS_PAGE_COUNT); }

SceneUpdateResult StatusUi::update(uint32_t, float) {
    RenderDemand demand;
    const auto& state = host.pageState();
    uint8_t count =
        statusFromStorage ? state.storageCount : state.teamCount;
    if (statusMonsterIndex < count) {
        const Game::MonsterRuntime& mon =
            statusFromStorage ? state.storage[statusMonsterIndex]
                              : state.team[statusMonsterIndex];
        int contentH = statusPageContentHeight(statusPage);
        if (statusPage == 2) {
            static constexpr int MOVE_DESC_W = Hal::DISPLAY_W - 36;
            contentH = 78;
            for (uint8_t slot = 0;
                 slot < SPECIAL_MOVE_SLOT_COUNT; ++slot) {
                const MoveInfo* move =
                    findMove(specialMoveIdForMonster(mon, slot));
                if (!move) {
                    contentH += 38;
                    continue;
                }
                int lines =
                    wrappedTextLineCount(move->description, MOVE_DESC_W);
                contentH += 74 + MathUtil::max(1, lines) * 16;
            }
            contentH = MathUtil::max(contentH, TouchUi::CONTENT_H);
        }
        int maxScroll = MathUtil::max(0, contentH - TouchUi::CONTENT_H);
        int key =
            (statusFromStorage ? 0x4000 : 0) |
            (static_cast<int>(statusMonsterIndex) << 8) | statusPage;
        demand.changed(updateStatusScroll(key, maxScroll));
        if (maxScroll > 0) demand.wakeIn(66);
    }
    return demand.result();
}

void StatusUi::renderStatusPage() {
    if (!host.pageState().oobeDone) {
        renderEggStatusPage();
        return;
    }

    auto& c = PixelRenderer::canvas();
    const auto& state = host.pageState();
    uint8_t statusCount = statusFromStorage ? state.storageCount : state.teamCount;
    if (statusCount == 0) {
        host.pageBack();
        statusMonsterIndex = 0;
        return;
    }
    if (statusMonsterIndex >= statusCount) statusMonsterIndex = 0;
    const Game::MonsterRuntime& activeMon = statusFromStorage
        ? state.storage[statusMonsterIndex]
        : state.team[statusMonsterIndex];
    const Species& mon = speciesFor(activeMon);
    char buf[64];
    if (statusPage >= STATUS_PAGE_COUNT) statusPage = 0;

    const auto body = MenuGrid::Rect{0, 0, Hal::DISPLAY_W, TouchUi::CONTENT_H};
    c.fillRect(body.x, body.y, body.width, body.height, PixelRenderer::rgb(10, 14, 20));
    const int scrollY = int(statusScroll);
    auto sx = [body](int x) { return body.x + x; };
    auto sy = [body, scrollY](int y) { return body.y + y - scrollY; };
    c.setClipRect(body.x, body.y, body.width, body.height);
    if (statusPage == 0) {
        drawStatusMonsterIcon(mon, sx(8), sy(14));

        int infoX = sx(88);
        snprintf(buf, sizeof(buf), Ui::Status::PROFILE_LINE_FMT,
                 mon.name, genderName(mon, activeMon), activeMon.level);
        PixelRenderer::text(infoX, sy(16), buf, PixelRenderer::rgb(241, 242, 232), 1);
        snprintf(buf, sizeof(buf), Ui::Status::TYPE_FMT, typeName(mon.type1), typeName(mon.type2));
        PixelRenderer::text(infoX, sy(38), buf, PixelRenderer::rgb(255, 216, 72), 1);

        PixelRenderer::text(infoX, sy(60), Ui::Status::ABILITY, PixelRenderer::rgb(67, 213, 224), 1);
        PixelRenderer::text(infoX + 42, sy(60), abilityName(abilityForSpecies(mon)), PixelRenderer::rgb(241, 242, 232), 1);
        int8_t likedFood = natureLikedFoodIndex(activeMon.nature);
        int8_t dislikedFood = natureDislikedFoodIndex(activeMon.nature);
        if (likedFood >= 0 && dislikedFood >= 0) {
            snprintf(buf, sizeof(buf), Ui::Status::NATURE_PREFERENCE_FMT,
                     natureName(activeMon.nature),
                     Ui::Status::FLAVOR_NAMES[likedFood],
                     Ui::Status::FLAVOR_NAMES[dislikedFood]);
        } else {
            snprintf(buf, sizeof(buf), Ui::Status::NATURE_FMT, natureName(activeMon.nature));
        }
        PixelRenderer::text(infoX, sy(82), buf, PixelRenderer::rgb(241, 242, 232), 1);

        c.drawFastHLine(sx(14), sy(101), 196, PixelRenderer::rgb(55, 63, 76));
        PixelRenderer::text(sx(16), sy(109), Ui::Status::SOURCE_INFO, PixelRenderer::rgb(67, 213, 224), 1);
        if (activeMon.origin == Game::Origin::BEFRIENDED &&
            activeMon.metArea < Ui::Explore::AREA_COUNT) {
            PixelRenderer::text(sx(60), sy(109), Ui::Status::SOURCE_AT, PixelRenderer::rgb(241, 242, 232), 1);
            const char* area = metAreaName(activeMon.metArea);
            PixelRenderer::text(sx(84), sy(109), area, PixelRenderer::rgb(255, 216, 72), 1);
            PixelRenderer::text(84 + textPixelWidth(area) + 6, sy(109),
                                Ui::Status::SOURCE_MET,
                                PixelRenderer::rgb(241, 242, 232), 1);
        } else if (activeMon.origin == Game::Origin::HATCHED) {
            PixelRenderer::text(sx(60), sy(109), Ui::Status::SOURCE_HATCHED,
                                PixelRenderer::rgb(255, 216, 72), 1);
        } else if (activeMon.origin == Game::Origin::STARTER) {
            PixelRenderer::text(sx(60), sy(109), Ui::Status::SOURCE_STARTER,
                                PixelRenderer::rgb(255, 216, 72), 1);
        } else {
            PixelRenderer::text(sx(60), sy(109), originName(activeMon.origin),
                                PixelRenderer::rgb(255, 216, 72), 1);
        }
    } else {
        paintDetails(statusPage, mon, activeMon, body, scrollY);
    }
    c.clearClipRect();

    renderStatusOverlay();
}

void StatusUi::renderEggStatusPage() {
    auto& c = PixelRenderer::canvas();

    statusPage = 0;
    const auto body = MenuGrid::Rect{0, 0, Hal::DISPLAY_W, TouchUi::CONTENT_H};
    auto sx = [body](int x) { return body.x + x; };
    auto sy = [body](int y) { return body.y + y; };
    c.fillRect(body.x, body.y, body.width, body.height, PixelRenderer::rgb(10, 14, 20));
    c.setClipRect(body.x, body.y, body.width, body.height);
    if (!GameAssets::drawCentered(GameAssets::Kind::EGG, sx(44), sy(63), 1.5f)) {
        c.fillEllipse(sx(44), sy(63), 16, 23, PixelRenderer::rgb(240, 232, 184));
    }
    PixelRenderer::text(sx(88), sy(38), Ui::Status::EGG_TITLE, PixelRenderer::rgb(241, 242, 232), 1);
    c.drawFastHLine(sx(84), sy(65), 122, PixelRenderer::rgb(55, 63, 76));
    PixelRenderer::text(sx(88), sy(78), Ui::Status::EGG_STATE, PixelRenderer::rgb(255, 216, 72), 1);
    c.clearClipRect();
}

void StatusUi::renderPageIndicator(uint8_t page, uint8_t count) {
    auto& c = PixelRenderer::canvas();
    const int dotW = 4;
    const int gap = 4;
    int totalH = count * 5 + (count - 1) * gap;
    const auto body = MenuGrid::Rect{0, 0, Hal::DISPLAY_W, TouchUi::CONTENT_H};
    int x = body.x + body.width - 10;
    int y = body.y + (body.height - totalH) / 2;
    for (uint8_t i = 0; i < count; ++i) {
        uint16_t color = i == page ? PixelRenderer::rgb(67, 213, 224) : PixelRenderer::rgb(92, 98, 110);
        c.fillRect(x, y + i * (5 + gap), dotW, 5, color);
    }
}

bool StatusUi::updateStatusScroll(int scrollKey, int maxScroll) {
    uint32_t now = Hal::ins().millis();
    float before = statusScroll;
    int previousKey = statusScrollKey;
    if (scrollKey != statusScrollKey) {
        statusScrollKey = scrollKey;
        statusScroll = 0.0f;
        statusScrollLastMs = now;
    }

    if (maxScroll <= 0) {
        statusScroll = 0.0f;
        statusScrollLastMs = now;
        return previousKey != statusScrollKey || before != statusScroll;
    }

    uint32_t dt = now - statusScrollLastMs;
    if (dt > 120) dt = 120;
    statusScrollLastMs = now;

    float ax = 0.0f;
    float ay = 0.0f;
    float az = 0.0f;
    if (!Hal::ins().readAccel(ax, ay, az)) {
        return previousKey != statusScrollKey || before != statusScroll;
    }

    static constexpr float SPEED = 72.0f;
    float input = 0.0f;
    if (statusPage == 2) {
        static constexpr float UP_THRESHOLD_DEG = 20.0f;
        static constexpr float DOWN_THRESHOLD_DEG = 45.0f;
        static constexpr float FULL_SPEED_RANGE_DEG = 15.0f;
        static constexpr float DEGREES_PER_RADIAN = 57.2957795f;

        // 0 degrees is screen-up and 90 degrees is upright. The absolute
        // pitch keeps the interaction independent of the sensor axis sign.
        float horizontalGravity = sqrtf(ax * ax + az * az);
        float angleDeg = atan2f(fabsf(ay), horizontalGravity) * DEGREES_PER_RADIAN;
        if (angleDeg < UP_THRESHOLD_DEG) {
            input = -(UP_THRESHOLD_DEG - angleDeg) / FULL_SPEED_RANGE_DEG;
        } else if (angleDeg > DOWN_THRESHOLD_DEG) {
            input = (angleDeg - DOWN_THRESHOLD_DEG) / FULL_SPEED_RANGE_DEG;
        }
    } else {
        static constexpr float DEADZONE = 0.18f;
        static constexpr float MAX_TILT = 0.75f;
        if (fabsf(ay) < DEADZONE) {
            return previousKey != statusScrollKey || before != statusScroll;
        }
        input = ay > 0.0f ? (ay - DEADZONE) : (ay + DEADZONE);
        input /= (MAX_TILT - DEADZONE);
    }
    if (input > 1.0f) input = 1.0f;
    if (input < -1.0f) input = -1.0f;
    if (input == 0.0f) {
        return previousKey != statusScrollKey || before != statusScroll;
    }

    statusScroll += input * SPEED * ((float)dt / 1000.0f);
    if (statusScroll < 0.0f) statusScroll = 0.0f;
    if (statusScroll > (float)maxScroll) statusScroll = (float)maxScroll;
    return previousKey != statusScrollKey || before != statusScroll;
}

}  // namespace Ui
