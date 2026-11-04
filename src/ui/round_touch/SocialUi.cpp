// SPDX-License-Identifier: Apache-2.0
#include "ui/round_touch/SocialUi.h"

#include <cstring>
#include "core/UiStrings.h"
#include "presentation/PixelRenderer.h"
#include "ui/SceneInput.h"
#include "ui/common/SocialUiContent.h"
#include "ui/round_touch/RoundPage.h"

namespace Ui {
namespace {
MenuGrid::Rect menuRow(int i) { return {8, 50 + i * 50, 224, 44}; }
}
void SocialUi::render() {
    auto frame = host.socialFrame();
    frame.visuals = &state;
    if (frame.model.state == SocialState::IDLE) {
        RoundPage::background();
        for (int i = 0; i < 3; ++i) {
            const auto rect = menuRow(i);
            MenuGrid::card(PixelRenderer::canvas(), rect, false, state.cursor == i);
            MenuGrid::label(PixelRenderer::canvas(), 120, rect.y + 14, Ui::Social::MENU_ITEMS[i], 208, 0xffff);
        }
        RoundPage::header(Ui::SOCIAL, false);
    } else if (frame.model.state == SocialState::FAILED || frame.model.state == SocialState::ENDED) {
        RoundPage::background();
        RoundPage::header(Ui::SOCIAL, false);
        MenuGrid::card(PixelRenderer::canvas(), {8, 76, 224, 96}, false);
        const bool unavailable = frame.model.error && std::strcmp(frame.model.error, "LINK UNAVAILABLE") == 0;
        MenuGrid::label(PixelRenderer::canvas(), 120, 96,
            unavailable ? "无线暂不可用" : frame.model.error ? frame.model.error : Ui::Social::VISIT_ENDED, 208, 0xffe0);
        MenuGrid::label(PixelRenderer::canvas(), 120, 134, Ui::BACK, 208, 0xffff);
    } else {
        SocialUiContent::draw(frame, layout());
        // Link errors come from the shared service (including LINK UNAVAILABLE).
        // The navigation bar remains available in every state.
    }
}

bool SocialUi::onTouch(const Platform::IInputDevice::TouchEvent& event) {
    using Type = Platform::IInputDevice::TouchType;
    const auto model = host.socialFrame().model;
    const int key = int(model.state) * 16 + model.roomCount;
    if (event.type == Type::DOWN) { cancelTouch(); context = key; }
    else if (context != key) { cancelTouch(); return false; }
    int target = SceneInput::target(event.x, event.y, true);
    if (target == SceneInput::CONTENT) {
        target = -1;
        if (RoundPage::backHit(event.x, event.y) && (model.state == SocialState::IDLE ||
            model.state == SocialState::FAILED || model.state == SocialState::ENDED)) target = 230;
        else if (model.state == SocialState::IDLE) {
            for (int i = 0; i < 3; ++i) if (menuRow(i).contains(event.x, event.y)) target = i;
        } else if (model.state == SocialState::ROOM_LIST) {
            for (int i = 0; i <= model.roomCount; ++i)
                if (MenuGrid::Rect{40, 40 + i * 20, 192, 20}.contains(event.x, event.y)) target = 100 + i;
        }
    }
    const int activated = tap.update(event, target);
    if (activated < 0) return false;
    cancelTouch();
    ButtonEvent button;
    if (SceneInput::button(activated, button)) return onButton(button);
    if (activated == 230) return host.activateSocial(SocialAction::BACK);
    if (model.state == SocialState::IDLE && activated >= 0 && activated < 3) {
        state.cursor = static_cast<uint8_t>(activated);
        return onButton({0, BtnAction::PRESSED});
    }
    if (model.state == SocialState::ROOM_LIST && activated >= 100 && activated <= 100 + model.roomCount) {
        state.roomCursor = static_cast<uint8_t>(activated - 100);
        return onButton({0, BtnAction::PRESSED});
    }
    return false;
}
}  // namespace Ui
