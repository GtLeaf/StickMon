#pragma once

#include "core/Scene.h"
#include "ui/common/SocialUiModel.h"

namespace Ui {
enum class SocialAction : uint8_t {
    BACK, HOST, SEARCH, STOP, SELECT_ROOM, DECIDE, END_VISIT
};

class SocialHost {
public:
    virtual ~SocialHost() = default;
    virtual SocialFrame socialFrame() const = 0;
    virtual bool activateSocial(SocialAction action, uint8_t value = 0) = 0;
};
}  // namespace Ui
