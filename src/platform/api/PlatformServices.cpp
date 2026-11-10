#include "platform/api/PlatformServices.h"
#include "core/BuildConfig.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {
Platform::Services* gServices = nullptr;
}

namespace Platform {

void bind(Services& value) {
    gServices = &value;
}

bool bound() {
    return gServices != nullptr;
}

Services& services() {
    return *gServices;
}

namespace {
void writeFormatted(const char* format, va_list arguments) {
    if (!gServices || !format) return;
    char buffer[384];
    int written = std::vsnprintf(buffer, sizeof(buffer), format, arguments);
    if (written <= 0) return;
    size_t length = static_cast<size_t>(written);
    if (length >= sizeof(buffer)) length = sizeof(buffer) - 1;
    gServices->logger.write(buffer, length);
}

void writeLine(const char* text) {
    if (!gServices || !text) return;
    size_t length = std::strlen(text);
    gServices->logger.write(text, length);
    static constexpr char newline = '\n';
    gServices->logger.write(&newline, 1);
}
}  // namespace

void logf(const char* format, ...) {
#if STICKMON_ENABLE_DEBUG_FEATURES || STICKMON_ENABLE_ROOM_DIAGNOSTICS
    va_list arguments;
    va_start(arguments, format);
    writeFormatted(format, arguments);
    va_end(arguments);
#else
    (void)format;
#endif
}

void logLine(const char* text) {
#if STICKMON_ENABLE_DEBUG_FEATURES || STICKMON_ENABLE_ROOM_DIAGNOSTICS
    writeLine(text);
#else
    (void)text;
#endif
}

void errorf(const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    writeFormatted(format, arguments);
    va_end(arguments);
}

void errorLine(const char* text) {
    writeLine(text);
}

}  // namespace Platform
