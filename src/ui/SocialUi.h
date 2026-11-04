#pragma once

#include "ui/UiProfile.h"
#if STICKMON_UI_ROUND_TOUCH
#include "ui/round_touch/SocialUi.h"
#elif STICKMON_UI_COMPACT_TOUCH
#include "ui/compact_touch/SocialUi.h"
#else
#include "ui/stick/SocialUi.h"
#endif
