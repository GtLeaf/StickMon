#pragma once

#include "ui/common/SocialUiBase.h"

namespace Ui {
class SocialUi : public SocialUiBase {
public:
    explicit SocialUi(SocialHost& host) : SocialUiBase(host) {}
    SocialLayout layout() const override;
    void render() override;
};
}  // namespace Ui
