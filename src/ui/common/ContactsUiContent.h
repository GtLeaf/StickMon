#pragma once

#include "ui/common/ContactsUiBase.h"
#include "presentation/TouchUi.h"

namespace Ui {

struct ContactsLayout {
    int listTop = 6;
    int contentHeight = TouchUi::CONTENT_H;
    int popupOffsetY = 0;
};

void renderContactsContent(ContactsUiBase& page, float scroll,
                           const ContactsLayout& layout = {});

}  // namespace Ui
