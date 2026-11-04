#pragma once

#include "core/Scene.h"
#include "ui/common/MenuPageHost.h"

namespace Ui {

class ComputerHost : public virtual MenuPageHost {
public:
    virtual void openComputerSocial() = 0;
    virtual void openComputerContacts() = 0;
};

class ComputerUiBase : public Scene {
public:
    explicit ComputerUiBase(ComputerHost& host) : host(host) {}
protected:
    ComputerHost& host;
    void activateComputer(uint8_t index);
};

}  // namespace Ui
