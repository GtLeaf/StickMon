// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "ui/common/MarketListScroll.h"
#include "ui/common/SocialUiBase.h"
namespace Ui {
class SocialUi : public SocialUiBase {
  public:
    explicit SocialUi(SocialHost &host) : SocialUiBase(host) {}
    SocialLayout layout() const override {
        return {360, 360};
    }
    bool touchNavigation() const override {
        return false;
    }
    void enter(const Communication::VisitSessionService::ViewModel &model, uint32_t nowMs) override;
    void stateChanged(SocialState state, uint32_t nowMs) override;
    void update(const Communication::VisitSessionService::ViewModel &model, uint32_t nowMs,
                RenderDemand &demand) override;
    uint32_t visualFrameIntervalMs(uint32_t) const override;
    void render() override;
    bool onTouch(const Platform::IInputDevice::TouchEvent &event) override;
    void cancelTouch() override {
        clearPress();
        motion_.cancel();
    }

  private:
    void clearPress() {
        tap_.cancel();
        context_ = -1;
        pressed_ = -1;
    }
    TouchTap tap_;
    MarketListScroll motion_;
    int context_ = -1, pressed_ = -1;
    int16_t downX_ = 0, downY_ = 0;
    int offset_ = 0, displayedOffset_ = 0;
    bool displayed_ = false;
    uint32_t lastUpdateMs_ = 0;
    Communication::VisitSessionService::ViewModel downModel_{};
    Communication::VisitSessionService::ViewModel displayedModel_{};
};
} // namespace Ui
