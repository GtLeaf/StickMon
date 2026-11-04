// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "ui/common/SocialUiBase.h"

namespace Ui {
class SocialUi : public SocialUiBase {
public:
    explicit SocialUi(SocialHost& host) : SocialUiBase(host) {}
    SocialLayout layout() const override { return {240, 240}; }
    void render() override;
    bool touchNavigation() const override { return true; }
    bool onTouch(const Platform::IInputDevice::TouchEvent& event) override;
    void cancelTouch() override { tap.cancel(); context = -1; }
private:
    TouchTap tap;
    int context = -1;
};
}  // namespace Ui
