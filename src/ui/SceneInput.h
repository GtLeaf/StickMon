#pragma once

#include "ui/UiProfile.h"
#if STICKMON_UI_ROUND_TOUCH
#include "ui/round_touch/SceneInput.h"
#elif STICKMON_UI_COMPACT_TOUCH
#include "ui/compact_touch/SceneInput.h"
#else
#include "ui/stick/SceneInput.h"
#endif
