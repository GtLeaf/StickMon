#pragma once

#include "ui/UiProfile.h"
#if STICKMON_UI_ROUND_TOUCH
#include "ui/round_touch/MainMenuUi.h"
#elif STICKMON_UI_COMPACT_TOUCH
#include "ui/compact_touch/MainMenuUi.h"
#else
#include "ui/stick/MainMenuUi.h"
#endif
