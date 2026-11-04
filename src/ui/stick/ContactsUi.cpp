#include "ui/stick/ContactsUi.h"

#include "core/MathUtil.h"
#include "core/UiMotion.h"
#include "hardware/Hal.h"
#include "presentation/TouchUi.h"
#include "ui/common/ContactsUiContent.h"

namespace Ui {

void ContactsUi::reset() {
    cancelTouch();
    resetState();
    storageScroll = 0.0f;
}

void ContactsUi::enter() {
    cancelTouch();
    enterState();
    storageScroll = 0.0f;
}

SceneUpdateResult ContactsUi::update(uint32_t, float) {
    RenderDemand demand;
    constexpr int VISIBLE_H = TouchUi::CONTENT_H - 12;
    constexpr int ROW_H = 24;
    const uint8_t rows = state().storageCount + 1;
    const int maximum = MathUtil::max(0, int(rows) * ROW_H - VISIBLE_H);
    const int target = storageCursor * ROW_H - VISIBLE_H / 2 + ROW_H / 2;
    const auto step = UiMotion::lerp(storageScroll, float(MathUtil::clamp(target, 0, maximum)), 0.25f, 0.5f);
    demand.changed(step.changed);
    demand.animate(step.active);
    return demand.result();
}

void ContactsUi::render() { renderContactsContent(*this, storageScroll); }
bool ContactsUi::onButton(const ButtonEvent& event) { return handleButton(event); }
void ContactsUi::cancelTouch() { Scene::cancelTouch(); }
bool ContactsUi::onTouch(const Platform::IInputDevice::TouchEvent& event) { return Scene::onTouch(event); }

int ContactsUi::touchTarget(int16_t x, int16_t y) {
    if (x < 0 || x >= Hal::DISPLAY_W || y < 0 || y >= TouchUi::CONTENT_H) return -1;
    if (storageReleaseConfirmOpen || storageInviteConfirmOpen) {
        const int top = storageInviteConfirmOpen ? 76 : 75;
        if (y < top || y >= top + 29 || x < 65 || x >= 205) return -1;
        return 200 + (x >= 139);
    }
    if (storageActionOpen) {
        const int top = 27, height = 21;
        const int count = actionCount();
        if (x < 142 || x >= 220 || y < top || y >= top + count * height) return -1;
        return 128 + (y - top) / height;
    }
    const int index = (y - 6 + int(storageScroll)) / 24;
    const int count = state().storageCount + 1;
    return y >= 6 && index >= 0 && index < count ? index : -1;
}

bool ContactsUi::onTap(int target) { return handleTap(target); }

}  // namespace Ui
