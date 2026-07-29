#pragma once

#ifndef STICKMON_ENABLE_TRACE_LOGS
#define STICKMON_ENABLE_TRACE_LOGS 0
#endif

#ifndef STICKMON_ENABLE_RENDER_STATS
#define STICKMON_ENABLE_RENDER_STATS 0
#endif

#if STICKMON_ENABLE_TRACE_LOGS
#include <Arduino.h>
#define STICKMON_TRACEF(...) Serial.printf(__VA_ARGS__)
#else
#define STICKMON_TRACEF(...) do { } while (0)
#endif

#if STICKMON_ENABLE_RENDER_STATS
#include <Arduino.h>
#define STICKMON_RENDER_STATSF(...) Serial.printf(__VA_ARGS__)
#else
#define STICKMON_RENDER_STATSF(...) do { } while (0)
#endif
