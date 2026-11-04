#pragma once
#include "core/BuildConfig.h"
#if STICKMON_ENABLE_DEBUG_FEATURES
#include "ui/common/MenuPageHost.h"

namespace Ui {
enum class DebugAction : uint8_t {
    RECOVER, LEVEL_UP, SWITCH_SPECIES, ADD_COINS, SET_TIME, CYCLE_LIGHT,
    TOGGLE_TILT, TOGGLE_WALK_BOUNDS, PAIR_INTERACTION, RANDOM_BATTLE,
    TOGGLE_BATTLE_BOUNDS, CONTACT_PLAY, CONTACT_GIFT, CONTACT_EXPLORE
};
struct DebugFrame {
    uint16_t minutes = 0;
    const char* lightLabel = nullptr;
    bool tilt = false;
    bool walkBounds = false;
    bool battleBounds = false;
};
struct DebugLayout { int width; int height; int contentHeight; };
class DebugHost : public virtual MenuPageHost {
public:
    virtual DebugFrame debugFrame() const = 0;
    virtual bool activateDebug(DebugAction action, uint16_t value = 0) = 0;
};
}  // namespace Ui
#endif
