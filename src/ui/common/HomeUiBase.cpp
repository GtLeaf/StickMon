#include "ui/common/HomeUiBase.h"

#include <algorithm>
#include "core/MathUtil.h"
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "presentation/PixelRenderer.h"

namespace Ui {

void HomeUiBase::enter() {
    cancelTouch();
    hungerAnimUntilMs = 0;
}

SceneUpdateResult HomeUiBase::update(uint32_t nowMs, float dtSeconds) {
    (void)dtSeconds;
#if STICKMON_ENABLE_DEBUG_FEATURES
    const bool combo =
        ButtonDispatcher::ins().isDown(Platform::InputButton::PRIMARY) &&
        ButtonDispatcher::ins().isDown(Platform::InputButton::SECONDARY);
    if (!combo) {
        comboStartMs = 0;
        comboSaved = false;
    } else if (comboStartMs == 0) {
        comboStartMs = nowMs;
    } else if (!comboSaved && nowMs - comboStartMs >= 800) {
        host.saveHome();
        toast = Ui::Common::SAVED;
        toastUntil = nowMs + 1200;
        comboSaved = true;
    }
#endif
    if (toast && static_cast<int32_t>(nowMs - toastUntil) >= 0) toast = nullptr;
    return SceneUpdateResult::animate(66);
}

void HomeUiBase::focusCamera(float x, float y, float focusY) {
    camera.x = RoomRenderer::clampCameraX(
        x - Hal::DISPLAY_W / (2.0f * RoomRenderer::PIXEL_SCALE));
    camera.y = RoomRenderer::clampCameraY(y - focusY);
}

uint8_t HomeUiBase::displayedHunger(uint8_t slot, uint8_t hunger, uint32_t nowMs) const {
    if (slot != 0 || static_cast<int32_t>(nowMs - hungerAnimUntilMs) >= 0 ||
        hungerAnimUntilMs <= hungerAnimStartedMs) return hunger;
    uint32_t elapsed = nowMs - hungerAnimStartedMs;
    const uint32_t duration = hungerAnimUntilMs - hungerAnimStartedMs;
    if (elapsed > duration) elapsed = duration;
    const int delta = static_cast<int>(hungerAnimTo) - static_cast<int>(hungerAnimFrom);
    return static_cast<uint8_t>(MathUtil::clamp(
        static_cast<int>(hungerAnimFrom) + static_cast<int>(static_cast<int64_t>(delta) * elapsed / duration),
        0, 100));
}

void HomeUiBase::animateHunger(uint8_t from, uint8_t to, uint32_t nowMs) {
    hungerAnimFrom = from;
    hungerAnimTo = to;
    hungerAnimStartedMs = nowMs;
    hungerAnimUntilMs = nowMs + 360;
}

void HomeUiBase::render() {
    const HomeFrame frame = host.homeFrame();
    const int16_t mainDepth = static_cast<int16_t>(frame.main.y - 78.0f);
    const int16_t visitorDepth = static_cast<int16_t>(frame.visitor.y - 78.0f);
    struct RenderItem { int16_t z; HomeLayer layer; bool roomLayer = false; };
    RenderItem items[] = {
        {0, HomeLayer::BACKGROUND},
        {10, HomeLayer::FLOOR, true},
        {18, HomeLayer::FOOD, true},
        {static_cast<int16_t>(20 + mainDepth), HomeLayer::MAIN_SHADOW, true},
        {static_cast<int16_t>(20 + visitorDepth), HomeLayer::VISITOR_SHADOW, true},
        {static_cast<int16_t>(30 + mainDepth), HomeLayer::MAIN_ACTOR, true},
        {static_cast<int16_t>(30 + visitorDepth), HomeLayer::VISITOR_ACTOR, true},
        {static_cast<int16_t>(40 + mainDepth), HomeLayer::HEARTS, true},
        {85, HomeLayer::LIGHT, true},
#if STICKMON_ENABLE_DEBUG_FEATURES
        {88, HomeLayer::WALK_BOUNDARY, true},
#endif
        {90, HomeLayer::HUD},
        {100, HomeLayer::TOAST},
        {105, HomeLayer::CONTACT},
        {107, HomeLayer::TUTORIAL},
        {110, HomeLayer::PROGRESSION},
    };
    std::sort(items, items + sizeof(items) / sizeof(items[0]),
        [](const RenderItem& a, const RenderItem& b) { return a.z < b.z; });
    auto& canvas = PixelRenderer::canvas();
    const auto view = RoomRenderer::viewport();
    for (const auto& item : items) {
        if (item.roomLayer) canvas.setClipRect(view.x, view.y, view.width, view.height);
        switch (item.layer) {
        case HomeLayer::HUD: drawHud(frame); break;
        case HomeLayer::TOAST: drawToast(); break;
        case HomeLayer::CONTACT: drawContact(frame); break;
        case HomeLayer::PROGRESSION: host.renderHomeProgression(); break;
        default: HomeUiContent::drawLayer(item.layer, frame, camera); break;
        }
        canvas.clearClipRect();
    }
    drawHomeMenu(frame);
}

void HomeUiBase::drawLayer(HomeLayer layer) {
    const HomeFrame frame = host.homeFrame();
    switch (layer) {
    case HomeLayer::HUD: drawHud(frame); break;
    case HomeLayer::TOAST: drawToast(); break;
    case HomeLayer::CONTACT: drawContact(frame); break;
    case HomeLayer::PROGRESSION: host.renderHomeProgression(); break;
    default: HomeUiContent::drawLayer(layer, frame, camera); break;
    }
}

bool HomeUiBase::onButton(const ButtonEvent& event) {
    if (host.homeOverlayActive()) return host.handleHomeOverlayButton(event);
    if (event.action != BtnAction::PRESSED && event.action != BtnAction::LONG_PRESS) return false;
    host.cancelHomePair();
    if (event.btn == 0 && event.action == BtnAction::PRESSED)
        return host.activateHome(HomeAction::PET);
    if (event.btn == 1 && event.action == BtnAction::PRESSED)
        return host.activateHome(HomeAction::FEED);
    if (event.btn == 0 && event.action == BtnAction::LONG_PRESS)
        return host.activateHome(HomeAction::MENU);
    return false;
}

bool HomeUiBase::onTap(int target) {
    if (target == 2) return onButton({0, BtnAction::LONG_PRESS});
    if (target == 0 || target == 1)
        return onButton({static_cast<uint8_t>(target), BtnAction::PRESSED});
    return false;
}

}  // namespace Ui
