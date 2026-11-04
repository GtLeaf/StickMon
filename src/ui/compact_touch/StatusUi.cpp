#include "ui/compact_touch/StatusUi.h"
#include "ui/PageProfile.h"

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

namespace {
MenuGrid::Rect touchStatusBody() {
    return {0, TouchUi::SAFE_HEADER_H, Ui::Page::WIDTH, Ui::Page::HEIGHT - TouchUi::SAFE_HEADER_H};
}
}

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

StatusUi::~StatusUi() { stopStatusSlide(); }

void StatusUi::cancelTouch() {
    Scene::cancelTouch();
    gridTouch.cancel();
    statusSwipeActive = false;
    statusSwipeAxis = 0;
    statusSwipePreviewFailed = false;
    stopStatusSlide();
}

void StatusUi::stopStatusSlide() {
    if (statusSlideFrames) Platform::memory().release(statusSlideFrames);
    statusSlideFrames = nullptr;
    statusSlideDragging = false;
}

bool StatusUi::prepareStatusSlide(int direction) {
    auto& canvas = PixelRenderer::canvas();
    if (canvas.renderScale() != 1) return false;
    const auto body = touchStatusBody();
    const size_t count = size_t(body.width) * body.height;
    auto* frames = statusSlideFrames;
    const bool fresh = frames == nullptr;
    if (fresh) frames = static_cast<uint16_t*>(Platform::memory().allocate(count * 2 * sizeof(uint16_t), true));
    if (!frames) return false;
    // Detach while capturing so the renderer draws real page content.
    statusSlideFrames = nullptr;
    statusSlideCapturing = true;
    if (fresh) {
        renderStatusPage();
        std::memcpy(frames, canvas.rawPixels() + body.y * body.width, count * sizeof(uint16_t));
    }
    const auto sourcePage = statusPage;
    const auto sourceScroll = statusScroll;
    const auto sourceKey = statusScrollKey;
    statusSlideNextPage = (sourcePage + (direction < 0 ? STATUS_PAGE_COUNT - 1 : 1)) % STATUS_PAGE_COUNT;
    statusPage = statusSlideNextPage;
    statusScroll = 0;
    renderStatusPage();
    std::memcpy(frames + count, canvas.rawPixels() + body.y * body.width, count * sizeof(uint16_t));
    statusPage = sourcePage;
    statusScroll = sourceScroll;
    statusScrollKey = sourceKey;
    statusSlideCapturing = false;
    statusSlideFrames = frames;
    statusSlideDirection = direction;
    return true;
}

void StatusUi::changeStatusPage(int direction) {
    if (statusSlideFrames) return;
    cancelTouch();
    prepareStatusSlide(direction);
    statusPage = (statusPage + (direction < 0 ? STATUS_PAGE_COUNT - 1 : 1)) % STATUS_PAGE_COUNT;
    statusScroll = 0;
    statusScrollKey = -1;
    statusSlideStartedMs = Hal::ins().millis();
    statusSlidePixels = statusSlideFromPixels = 0;
    statusSlideToPixels = Ui::Page::WIDTH;
    statusSlideDurationMs = 220;
}

void StatusUi::trackStatusSwipe(int dx) {
    const int direction = dx == 0 ? statusSlideDirection : dx < 0 ? 1 : -1;
    if (!statusSwipePreviewFailed && (!statusSlideFrames || direction != statusSlideDirection))
        statusSwipePreviewFailed = !prepareStatusSlide(direction);
    statusSlideDirection = direction;
    statusSlideDragging = statusSlideFrames != nullptr;
    statusSlidePixels = std::min(std::abs(dx), int(Ui::Page::WIDTH));
}

void StatusUi::settleStatusSwipe(bool commit) {
    statusSwipeActive = false;
    statusSwipeAxis = 0;
    gridTouch.cancel();
    if (!statusSlideFrames) {
        if (commit) changeStatusPage(statusSlideDirection);
        return;
    }
    statusSlideDragging = false;
    if (commit) {
        statusPage = statusSlideNextPage;
        statusScroll = 0;
        statusScrollKey = -1;
    }
    statusSlideFromPixels = statusSlidePixels;
    statusSlideToPixels = commit ? Ui::Page::WIDTH : 0;
    const int remaining = std::abs(statusSlideToPixels - statusSlideFromPixels);
    statusSlideDurationMs = std::clamp(220 * remaining / int(Ui::Page::WIDTH), 80, 220);
    statusSlideStartedMs = Hal::ins().millis();
    if (remaining == 0) stopStatusSlide();
}

SceneUpdateResult StatusUi::update(uint32_t nowMs, float) {
    RenderDemand demand;
    if (statusSlideFrames && !statusSlideDragging) {
        const uint32_t elapsed = nowMs - statusSlideStartedMs;
        if (elapsed >= statusSlideDurationMs) {
            stopStatusSlide();
            demand.redraw();
        } else {
            const float t = float(elapsed) / statusSlideDurationMs;
            const float remaining = 1.0f - t;
            statusSlidePixels = statusSlideFromPixels + int(std::lround(
                (statusSlideToPixels - statusSlideFromPixels) * (1.0f - remaining * remaining * remaining)));
            demand.animate(true, 16);
        }
    }
    statusScroll = std::clamp(statusScroll, 0.0f,
        float(std::max(0, touchStatusContentHeight() - touchStatusBody().height)));
    return demand.result();
}


bool StatusUi::onTouch(const Platform::IInputDevice::TouchEvent& event) {
    using Type = Platform::IInputDevice::TouchType;
    if (event.type == Type::CANCEL) {
        const bool changed = statusSlideFrames || statusSwipeActive || gridTouch.active();
        cancelTouch();
        return changed;
    }
    if (statusSlideFrames && (!statusSlideDragging || event.type == Type::DOWN)) return false;
    const auto body = touchStatusBody();
    if (event.type == Type::DOWN) {
        statusSwipeActive = body.contains(event.x, event.y);
        statusSwipeAxis = 0;
        statusSwipePreviewFailed = false;
        statusSwipeX = event.x;
        statusSwipeY = event.y;
    }
    if (statusSwipeActive && (event.type == Type::MOVE || event.type == Type::UP)) {
        const int dx = event.x - statusSwipeX, dy = event.y - statusSwipeY;
        if (statusSwipeAxis == 0) {
            if (std::abs(dx) > 10 && std::abs(dx) > std::abs(dy) * 2 && host.pageState().oobeDone) {
                statusSwipeAxis = 1;
                gridTouch.cancel();
            } else if (std::abs(dy) > 10 && std::abs(dy) >= std::abs(dx)) statusSwipeAxis = 2;
            else if (event.type == Type::MOVE) return false;
        }
        if (statusSwipeAxis == 1) {
            trackStatusSwipe(dx);
            if (event.type == Type::UP) settleStatusSwipe(std::abs(dx) >= Ui::Page::WIDTH / 4);
            return true;
        }
        if (event.type == Type::UP) statusSwipeActive = false;
    }
    const float maximum = std::max(0, touchStatusContentHeight() - body.height);
    const auto result = gridTouch.update(event, touchTarget(event.x, event.y), statusScroll, maximum,
        body.contains(event.x, event.y));
    const bool activated = result.tap >= 0 && onTap(result.tap);
    return result.changed || activated;
}

int StatusUi::touchTarget(int16_t x, int16_t y) {
    if (x < 0 || x >= Ui::Page::WIDTH || y < 0 || y >= Ui::Page::HEIGHT) return -1;
    const auto header = TouchUi::safeHeaderBounds();
    if (y < header.y + header.height && !header.contains(x, y)) return -1;
    if (TouchUi::safeBack().contains(x, y)) return 230;
    if (!host.pageState().oobeDone) return -1;
    if (header.contains(x, y) && x >= 76) return 232;
    return -1;
}

bool StatusUi::onTap(int target) {
    if (target == 230) return onButton({1, BtnAction::LONG_PRESS});
    if (target == 231 || target == 232) {
        changeStatusPage(target == 231 ? -1 : 1);
        return true;
    }
    return false;
}


int StatusUi::renderTouchStatusProfile(bool draw) {
    const auto& state = host.pageState();
    const int count = statusFromStorage ? state.storageCount : state.teamCount;
    if (!count) return 0;
    const int index = statusMonsterIndex < count ? statusMonsterIndex : 0;
    const auto& activeMon = statusFromStorage ? state.storage[index] : state.team[index];
    const auto& mon = speciesFor(activeMon);
    const auto body = touchStatusBody();
    auto sx = [body](int x) { return body.x + x; };
    auto sy = [body, this](int y) { return body.y + y - int(statusScroll); };
    const auto white = PixelRenderer::rgb(241, 242, 232);
    const auto cyan = PixelRenderer::rgb(67, 213, 224);
    const auto yellow = PixelRenderer::rgb(255, 216, 72);
    if (draw) drawStatusMonsterIcon(mon, sx(8), sy(14));
    char line[128];
    int extra = 0;
    auto field = [&](int y, const char* text, uint16_t color, int x = 88) {
        const int lines = wrapText(text, body.width - x - 10, sx(x), sy(y + extra), color, draw);
        extra += std::max(0, lines - 1) * 16;
    };
    std::snprintf(line, sizeof(line), Ui::Status::PROFILE_LINE_FMT, mon.name, genderName(mon, activeMon), activeMon.level);
    field(16, line, white);
    std::snprintf(line, sizeof(line), Ui::Status::TYPE_FMT, typeName(mon.type1), typeName(mon.type2));
    field(38, line, yellow);
    if (draw) PixelRenderer::text(sx(88), sy(60 + extra), Ui::Status::ABILITY, cyan);
    field(60, abilityName(abilityForSpecies(mon)), white, 130);
    const int liked = natureLikedFoodIndex(activeMon.nature), disliked = natureDislikedFoodIndex(activeMon.nature);
    if (liked >= 0 && disliked >= 0)
        std::snprintf(line, sizeof(line), Ui::Status::NATURE_PREFERENCE_FMT, natureName(activeMon.nature),
            Ui::Status::FLAVOR_NAMES[liked], Ui::Status::FLAVOR_NAMES[disliked]);
    else std::snprintf(line, sizeof(line), Ui::Status::NATURE_FMT, natureName(activeMon.nature));
    field(82, line, white);
    if (draw) {
        PixelRenderer::canvas().drawFastHLine(sx(14), sy(101 + extra), 196, PixelRenderer::rgb(55, 63, 76));
        PixelRenderer::text(sx(16), sy(109 + extra), Ui::Status::SOURCE_INFO, cyan);
        if (activeMon.origin == Game::Origin::BEFRIENDED && activeMon.metArea < Ui::Explore::AREA_COUNT) {
            PixelRenderer::text(sx(60), sy(109 + extra), Ui::Status::SOURCE_AT, white);
            const char* area = metAreaName(activeMon.metArea);
            PixelRenderer::text(sx(84), sy(109 + extra), area, yellow);
            PixelRenderer::text(sx(84 + textPixelWidth(area) + 6), sy(109 + extra), Ui::Status::SOURCE_MET, white);
        } else {
            const char* source = activeMon.origin == Game::Origin::HATCHED ? Ui::Status::SOURCE_HATCHED : originName(activeMon.origin);
            PixelRenderer::text(sx(60), sy(109 + extra), source, yellow);
        }
    }
    return 129 + extra;
}

int StatusUi::touchStatusContentHeight() {
    const auto& state = host.pageState();
    const int count = statusFromStorage ? state.storageCount : state.teamCount;
    if (!state.oobeDone || !count) return 0;
    if (statusPage == 0) return renderTouchStatusProfile(false);
    if (statusPage != 2) return statusPageContentHeight(statusPage);
    const int index = statusMonsterIndex < count ? statusMonsterIndex : 0;
    const auto& mon = statusFromStorage ? state.storage[index] : state.team[index];
    int height = 78;
    for (uint8_t slot = 0; slot < SPECIAL_MOVE_SLOT_COUNT; ++slot) {
        const auto* move = findMove(specialMoveIdForMonster(mon, slot));
        height += move ? 74 + std::max(1, wrappedTextLineCount(move->description, TouchUi::pageBody().width - 36)) * 16 : 38;
    }
    return height;
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
    if (statusPage >= STATUS_PAGE_COUNT) statusPage = 0;

    const auto body = touchStatusBody();
    if (statusSlideFrames) {
        const size_t count = size_t(body.width) * body.height;
        const int distance = std::clamp(statusSlidePixels, 0, int(body.width));
        auto* pixels = c.rawPixels() + body.y * body.width;
        for (int y = 0; y < body.height; ++y) {
            const auto* oldRow = statusSlideFrames + y * body.width;
            const auto* newRow = statusSlideFrames + count + y * body.width;
            auto* row = pixels + y * body.width;
            if (statusSlideDirection > 0) {
                std::memcpy(row, oldRow + distance, (body.width - distance) * sizeof(uint16_t));
                std::memcpy(row + body.width - distance, newRow, distance * sizeof(uint16_t));
            } else {
                std::memcpy(row, newRow + body.width - distance, distance * sizeof(uint16_t));
                std::memcpy(row + distance, oldRow, (body.width - distance) * sizeof(uint16_t));
            }
        }
        renderStatusOverlay();
        return;
    }
    c.fillRect(0, 0, Ui::Page::WIDTH, Ui::Page::HEIGHT, 0);
    statusScroll = std::clamp(statusScroll, 0.0f, float(std::max(0, touchStatusContentHeight() - body.height)));
    c.fillRect(body.x, body.y, body.width, body.height, PixelRenderer::rgb(10, 14, 20));
    const int scrollY = int(statusScroll);
    c.setClipRect(body.x, body.y, body.width, body.height);
    if (statusPage == 0) {
        renderTouchStatusProfile(true);
    } else {
        paintDetails(statusPage, mon, activeMon, body, scrollY);
    }
    c.clearClipRect();

    renderStatusOverlay();
}

void StatusUi::renderStatusOverlay() {
    if (statusSlideCapturing) return;
    renderPageIndicator(statusPage, STATUS_PAGE_COUNT);
    char title[32];
    std::snprintf(title, sizeof(title), "%s %u/%u", Ui::STATUS, unsigned(statusPage + 1), unsigned(STATUS_PAGE_COUNT));
    TouchUi::safePageHeader(title, gridTouch.pressed() == 230);
}

void StatusUi::renderEggStatusPage() {
    auto& c = PixelRenderer::canvas();

    statusPage = 0;
    const auto body = touchStatusBody();
    auto sx = [body](int x) { return body.x + x; };
    auto sy = [body](int y) { return body.y + y; };
    c.fillRect(0, 0, Ui::Page::WIDTH, Ui::Page::HEIGHT, 0);
    c.fillRect(body.x, body.y, body.width, body.height, PixelRenderer::rgb(10, 14, 20));
    c.setClipRect(body.x, body.y, body.width, body.height);
    if (!GameAssets::drawCentered(GameAssets::Kind::EGG, sx(44), sy(63), 1.5f)) {
        c.fillEllipse(sx(44), sy(63), 16, 23, PixelRenderer::rgb(240, 232, 184));
    }
    PixelRenderer::text(sx(88), sy(38), Ui::Status::EGG_TITLE, PixelRenderer::rgb(241, 242, 232), 1);
    c.drawFastHLine(sx(84), sy(65), 122, PixelRenderer::rgb(55, 63, 76));
    PixelRenderer::text(sx(88), sy(78), Ui::Status::EGG_STATE, PixelRenderer::rgb(255, 216, 72), 1);
    c.clearClipRect();
    TouchUi::safePageHeader(Ui::Status::EGG_TITLE,
        gridTouch.pressed() == 230);
}

void StatusUi::renderPageIndicator(uint8_t page, uint8_t count) {
    auto& c = PixelRenderer::canvas();
    const int dotW = 4;
    const int gap = 4;
    const int totalW = count * dotW + (count - 1) * gap;
    const int left = (Ui::Page::WIDTH - totalW) / 2;
    const int top = Ui::Page::HEIGHT - 14;
    c.fillRoundRect(left - 8, top, totalW + 16, 14, 7, PixelRenderer::rgb(8, 10, 14));
    for (uint8_t i = 0; i < count; ++i) {
        const auto color = i == page ? PixelRenderer::rgb(67, 213, 224) : PixelRenderer::rgb(92, 98, 110);
        c.fillRect(left + i * (dotW + gap), top + 6, dotW, 4, color);
    }
}

}  // namespace Ui
