// SPDX-License-Identifier: Apache-2.0
#include "ui/round_touch/ExploreSelectUi.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "presentation/FrameDamage.h"
#include "assets/GameAssets.h"
#include "core/UiStrings.h"
#include "presentation/PixelRenderer.h"
#include "ui/round_touch/RoundPage.h"

namespace Ui::RoundExplore {
namespace {
constexpr MenuGrid::Rect PREVIEW{0, 106, 360, 140};
constexpr MenuGrid::Rect SWIPE_BOUNDS{48, 68, 264, 204};

void previewMember(const ExploreRouteFrame& frame, int index, int cx) {
    const auto* sprite = frame.areaPreviewFrames[index];
    if (!sprite || cx + sprite->width / 2 < PREVIEW.x ||
        cx - sprite->width / 2 >= PREVIEW.x + PREVIEW.width) return;
    const int x = cx - sprite->width / 2, y = 172 - sprite->height / 2;
    if (frame.areaPreviewHidden[index]) PokemonSprites::drawFrameSilhouette(sprite, x, y, 0);
    else PokemonSprites::drawFrame(sprite, x, y);
}

void preview(const ExploreRouteFrame& frame) {
    const int count = std::min<int>(frame.areaPreviewCount, ExploreRouteFrame::PREVIEW_CAP);
    if (!count) return;
    if (count == 1) {
        auto& c = PixelRenderer::canvas();
        c.setClipRect(PREVIEW.x, PREVIEW.y, PREVIEW.width, PREVIEW.height);
        previewMember(frame, 0, 180);
        c.clearClipRect();
        return;
    }
    const uint32_t elapsed = frame.nowMs - frame.areaPreviewStartedAt;
    const int current = (elapsed / ExploreRouteTiming::PREVIEW_CYCLE_MS + 1) % count;
    const int next = (current + 1) % count;
    const auto width = [&](int i) { return frame.areaPreviewFrames[i] ? int(frame.areaPreviewFrames[i]->width) : 80; };
    const uint32_t phase = elapsed % ExploreRouteTiming::PREVIEW_CYCLE_MS;
    float progress = phase <= ExploreRouteTiming::PREVIEW_HOLD_MS ? 0.0f
        : float(phase - ExploreRouteTiming::PREVIEW_HOLD_MS) / ExploreRouteTiming::PREVIEW_MOVE_MS;
    progress = progress * progress * (3.0f - 2.0f * progress);
    auto& c = PixelRenderer::canvas();
    c.setClipRect(PREVIEW.x, PREVIEW.y, PREVIEW.width, PREVIEW.height);
    // Move the whole repeating strip together so spacing is constant at the cycle seam.
    const int step = width(current) / 2 + 12 + width(next) / 2;
    const int anchor = 180 - int(std::lround(step * progress));
    previewMember(frame, current, anchor);
    int index = current, cx = anchor;
    while (cx - width(index) / 2 > PREVIEW.x) {
        const int previous = (index + count - 1) % count;
        cx -= width(index) / 2 + 12 + width(previous) / 2;
        previewMember(frame, previous, cx);
        index = previous;
    }
    index = current;
    cx = anchor;
    while (cx - width(index) / 2 + width(index) < PREVIEW.x + PREVIEW.width) {
        const int following = (index + 1) % count;
        cx += width(index) / 2 + 12 + width(following) / 2;
        previewMember(frame, following, cx);
        index = following;
    }
    c.clearClipRect();
}

void arrow(MenuGrid::Rect bounds, bool right, bool enabled, bool pressed) {
    auto& c = PixelRenderer::canvas();
    const int cx = bounds.x + bounds.width / 2, cy = bounds.y + bounds.height / 2;
    if (pressed) c.fillCircle(cx, cy, 19, PixelRenderer::rgb(38, 65, 65));
    const auto color = enabled ? PixelRenderer::rgb(226, 238, 233) : PixelRenderer::rgb(82, 88, 96);
    const int direction = right ? 1 : -1;
    for (int i = -1; i <= 1; ++i) {
        c.drawLine(cx - direction * 3 + i, cy - 7, cx + direction * 4 + i, cy, color);
        c.drawLine(cx + direction * 4 + i, cy, cx - direction * 3 + i, cy + 7, color);
    }
}
}  // namespace

void Selection::release() const {
    if (previewBackground) Platform::memory().release(previewBackground);
    previewBackground = nullptr;
    cachedTarget = nullptr;
    allocationAttempted = false;
    lastFrameSerial = 0;
    staticKey[0] = 0;
}

void Selection::render(const ExploreRouteFrame& frame) const {
    auto& c = PixelRenderer::canvas();
    const int count = std::min<int>(frame.visibleAreaCount, ExploreRouteFrame::AREA_CAP);
    const bool valid = frame.areaCursor < count;
    const bool unlocked = valid && frame.areaUnlocked[frame.areaCursor];
    char key[sizeof(staticKey)];
    std::snprintf(key, sizeof(key), "%u/%u/%d/%d/%d/%s/%s", frame.areaCursor,
        frame.visibleAreaCount, unlocked, frame.areaPreviewHasRare, pressed,
        valid && frame.areaNames[frame.areaCursor] ? frame.areaNames[frame.areaCursor] : "",
        frame.toast ? frame.toast : "");
    const bool native = c.physicalWidth() == 360 && c.physicalHeight() == 360 && c.renderScale() == 1;
    if (native && !allocationAttempted) {
        allocationAttempted = true;
        previewBackground = static_cast<uint16_t*>(Platform::memory().allocate(
            PREVIEW.width * PREVIEW.height * sizeof(uint16_t), true));
    }
    const auto drawToast = [&]() {
        if (!frame.toast) return;
        c.fillRoundRect(52, 210, 256, 28, 7, PixelRenderer::rgb(34, 39, 47));
        MenuGrid::label(c, 180, 216, frame.toast, 244, 0xffff);
    };
    const bool partial = native && unlocked && previewBackground && FrameDamage::allowed &&
        lastFrameSerial + 1 == FrameDamage::serial && cachedTarget == c.rawPixels() && cachedByteSwapped == c.byteSwapped() &&
        std::strcmp(key, staticKey) == 0;
    lastFrameSerial = FrameDamage::serial;
    if (partial) {
        c.pushImageNative(PREVIEW.x, PREVIEW.y, PREVIEW.width, PREVIEW.height, previewBackground);
        preview(frame);
        drawToast();
        FrameDamage::restrictTo(PREVIEW.x, PREVIEW.y, PREVIEW.width, PREVIEW.height);
        return;
    }
    std::memcpy(staticKey, key, sizeof(key));
    cachedTarget = c.rawPixels();
    cachedByteSwapped = c.byteSwapped();
    PixelRenderer::clear(PixelRenderer::rgb(11, 22, 27));
    c.setClipRect(0, 52, 360, 308);
    GameAssets::drawBattleBackground(GameAssets::Kind::EXPLORE_MENU_BACKGROUND);
    c.clearClipRect();
    PixelRenderer::fillRectAlpha(0, 52, 360, 308, 0, 48);
    if (native && previewBackground) {
        std::memcpy(previewBackground, c.rawPixels() + PREVIEW.y * 360,
                    PREVIEW.width * PREVIEW.height * sizeof(uint16_t));
    }
    if (unlocked) {
        preview(frame);
        if (frame.areaPreviewHasRare)
            MenuGrid::label(c, 180, 248, Ui::Explore::MASS_OUTBREAK, 200, 0xffff);
    } else if (valid) {
        MenuGrid::label(c, 180, 134, Ui::Explore::AREA_LOCKED, 224, PixelRenderer::rgb(226, 238, 233));
        MenuGrid::label(c, 180, 166, Ui::Explore::DEFEAT_PREVIOUS_BOSS, 240, PixelRenderer::rgb(156, 164, 176));
    }
    if (valid) MenuGrid::label(c, 180, 72, frame.areaNames[frame.areaCursor], 140, 0xffff);
    const auto button = DEPART_BOUNDS;
    c.fillRoundRect(button.x, button.y, button.width, button.height, 12,
        unlocked ? (pressed == DEPART ? PixelRenderer::rgb(42, 88, 70) : PixelRenderer::rgb(29, 65, 52))
                 : PixelRenderer::rgb(30, 38, 45));
    MenuGrid::label(c, 180, button.y + 12, Ui::Explore::DEPART, button.width - 20,
        unlocked ? 0xffff : PixelRenderer::rgb(126, 145, 145));
    arrow(PREVIOUS_BOUNDS, false, valid && frame.areaCursor > 0, pressed == PREVIOUS);
    arrow(NEXT_BOUNDS, true, valid && frame.areaCursor + 1 < count, pressed == NEXT);
    for (int i = 0; i < count; ++i)
        c.fillCircle(180 + (2 * i - count + 1) * 7, 339, i == frame.areaCursor ? 3 : 2,
            i == frame.areaCursor ? PixelRenderer::rgb(115, 226, 183) : PixelRenderer::rgb(126, 145, 145));
    drawToast();
    RoundPage::nativeHeader(Ui::EXPLORE, pressed == BACK);
}

int Selection::target(const ExploreInputFrame& frame, const ExploreUiState& state, int x, int y) const {
    if (frame.phase != ExplorePhase::SELECT) return -1;
    if (RoundPage::NATIVE_BACK.contains(x, y)) return BACK;
    const int count = std::min<int>(frame.visibleAreaCount, ExploreRouteFrame::AREA_CAP);
    if (PREVIOUS_BOUNDS.contains(x, y) && state.areaCursor > 0 && state.areaCursor < count) return PREVIOUS;
    if (NEXT_BOUNDS.contains(x, y) && state.areaCursor + 1 < count) return NEXT;
    if (DEPART_BOUNDS.contains(x, y) && state.areaCursor < count) return DEPART;
    return -1;
}

bool Selection::tap(ExploreHost& host, ExploreUiState& state, int target) {
    const auto input = host.exploreInputFrame();
    if (input.phase != ExplorePhase::SELECT) return false;
    if (target == BACK) return host.activateExplore(ExploreAction::AREA_BACK);
    const int count = std::min<int>(input.visibleAreaCount, ExploreRouteFrame::AREA_CAP);
    if (state.areaCursor >= count) return false;
    if (target == DEPART) {
        const auto frame = host.exploreRouteFrame();
        return frame.areaUnlocked[state.areaCursor] && host.activateExplore(ExploreAction::DEPART, state.areaCursor);
    }
    const int next = int(state.areaCursor) + (target == NEXT ? 1 : target == PREVIOUS ? -1 : 0);
    if (next < 0 || next >= count || next == state.areaCursor) return false;
    state.areaCursor = static_cast<uint8_t>(next);
    state.areaAnimCursor = next;
    return host.activateExplore(ExploreAction::AREA_CHANGED, state.areaCursor);
}

bool Selection::touch(ExploreHost& host, ExploreUiState& state,
                      const Platform::IInputDevice::TouchEvent& event) {
    using Type = Platform::IInputDevice::TouchType;
    const auto frame = host.exploreInputFrame();
    const bool wasPressed = pressed >= 0;
    if (event.type == Type::CANCEL || frame.phase != ExplorePhase::SELECT ||
        event.x < 0 || event.x >= 360 || event.y < 0 || event.y >= 360) {
        cancel();
        return wasPressed;
    }
    const int hit = target(frame, state, event.x, event.y);
    if (event.type == Type::DOWN) {
        cancel();
        active = true;
        startX = event.x;
        startY = event.y;
        swipe = SWIPE_BOUNDS.contains(startX, startY) && hit < 0;
        pressed = hit;
        tracker.update(event, hit);
        return pressed >= 0 || wasPressed;
    }
    if (!active) return false;
    const int dx = event.x - startX, dy = event.y - startY;
    if (pressed >= 0 && (dx * dx + dy * dy > 100 || pressed != hit)) pressed = -1;
    const int activated = tracker.update(event, hit);
    if (event.type != Type::UP) return wasPressed != (pressed >= 0);
    const bool changedArea = swipe && std::abs(dx) >= 40 && std::abs(dx) > std::abs(dy);
    cancel();
    // Clear all tracking before an action that can synchronously destroy the scene.
    if (changedArea) return tap(host, state, dx < 0 ? NEXT : PREVIOUS) || wasPressed;
    return (activated >= 0 && tap(host, state, activated)) || wasPressed;
}

void Selection::cancel() {
    tracker.cancel();
    pressed = -1;
    active = swipe = false;
}
}  // namespace Ui::RoundExplore
