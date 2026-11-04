#pragma once

#include "ui/UiProfile.h"

#if STICKMON_UI_ROUND_TOUCH
#include "ui/round_touch/ShopUi.h"
#elif STICKMON_UI_COMPACT_TOUCH
#include "ui/compact_touch/ShopUi.h"
#else
#include "ui/stick/ShopUi.h"
#endif
