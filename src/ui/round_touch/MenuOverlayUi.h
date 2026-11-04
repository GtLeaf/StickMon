// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ui/common/MenuOverlayUiBase.h"
#include "ui/PageProfile.h"
#include "presentation/TouchUi.h"
#include "ui/SceneSurface.h"

namespace Ui {
class MenuOverlayUi : public MenuOverlayUiBase {
public:
    MenuOverlayLayout layout() const override { return {SceneSurface::width(), SceneSurface::height(), TouchUi::CONTENT_H}; }
    const char* noticeText(const char* text) const;
    void renderTutorial(const MenuOverlayFrame& frame) const;
};
}  // namespace Ui
