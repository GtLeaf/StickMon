#pragma once

#include "ui/UiProfile.h"
#if STICKMON_UI_ROUND_TOUCH
#include "ui/round_touch/MenuOverlayUi.h"
#elif STICKMON_UI_COMPACT_TOUCH
#include "ui/compact_touch/MenuOverlayUi.h"
#else
#include "ui/stick/MenuOverlayUi.h"
#endif
