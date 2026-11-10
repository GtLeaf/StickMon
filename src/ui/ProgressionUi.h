#pragma once
#include "ui/UiProfile.h"
#if STICKMON_UI_ROUND_TOUCH
#include "ui/round_touch/ProgressionUi.h"
#elif STICKMON_UI_COMPACT_TOUCH
#include "ui/compact_touch/ProgressionUi.h"
#else
#include "ui/stick/ProgressionUi.h"
#endif
