#include "ui/common/ComputerUiBase.h"

namespace Ui {

void ComputerUiBase::activateComputer(uint8_t index) {
    if (index == 0) host.openComputerSocial();
    else if (index == 1) host.openComputerContacts();
}

}  // namespace Ui
