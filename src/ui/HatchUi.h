#pragma once

#include "ui/UiProfile.h"
#if STICKMON_UI_ROUND_TOUCH
#include "ui/round_touch/HatchUi.h"
#elif STICKMON_UI_COMPACT_TOUCH
#include "ui/compact_touch/HatchUi.h"
#else
#include "ui/stick/HatchUi.h"
#endif
