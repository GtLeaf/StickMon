#pragma once

#include "ui/UiProfile.h"
#if STICKMON_UI_ROUND_TOUCH
#include "ui/round_touch/HomeUi.h"
#elif STICKMON_UI_COMPACT_TOUCH
#include "ui/compact_touch/HomeUi.h"
#else
#include "ui/stick/HomeUi.h"
#endif
