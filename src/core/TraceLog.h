#pragma once

#include "core/BuildConfig.h"
#include "platform/api/PlatformServices.h"

#if STICKMON_ENABLE_TRACE_LOGS
#define STICKMON_TRACEF(...) Platform::logf(__VA_ARGS__)
#else
#define STICKMON_TRACEF(...) do { } while (0)
#endif

#if STICKMON_ENABLE_RENDER_STATS
#define STICKMON_RENDER_STATSF(...) Platform::logf(__VA_ARGS__)
#else
#define STICKMON_RENDER_STATSF(...) do { } while (0)
#endif
