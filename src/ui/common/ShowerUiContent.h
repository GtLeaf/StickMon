#pragma once

#include "ui/common/ShowerUiModel.h"

namespace Ui::ShowerUiContent {

using DrawControl = void (*)(const ShowerFrame&, const ShowerLayout&);
void draw(const ShowerFrame& frame, const ShowerLayout& layout,
          DrawControl menu = nullptr, DrawControl soapPicker = nullptr);

}  // namespace Ui::ShowerUiContent
