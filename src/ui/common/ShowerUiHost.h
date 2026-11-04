#pragma once

#include "core/Scene.h"
#include "ui/common/ShowerUiModel.h"

namespace Ui {

enum class ShowerAction : uint8_t {
    EXIT_CHOICE, MENU, BEGIN_SOAP, REQUEST_EXIT, CHOOSE_SOAP, BEGIN_BRUSH, BEGIN_RINSE
};

class ShowerHost {
public:
    virtual ~ShowerHost() = default;
    virtual ShowerFrame showerFrame() const = 0;
    virtual int8_t nextShowerSoap(int8_t from, int8_t direction = 1) const = 0;
    virtual bool activateShower(ShowerAction action, uint8_t value = 0) = 0;
};

struct ShowerMotion {
    float dynX = 0, dynY = 0, shake = 0;
};

}  // namespace Ui
