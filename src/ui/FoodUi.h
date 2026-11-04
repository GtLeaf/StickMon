#pragma once

#include "ui/UiProfile.h"
#if STICKMON_UI_ROUND_TOUCH
#include "ui/round_touch/FoodUi.h"
#elif STICKMON_UI_COMPACT_TOUCH
#include "ui/compact_touch/FoodUi.h"
#else
#include "ui/stick/FoodUi.h"
#endif
