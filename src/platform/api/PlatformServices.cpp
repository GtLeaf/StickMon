#include "platform/api/PlatformServices.h"

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

void logf(const char* format, ...) {
    if (!gServices || !format) return;
    char buffer[384];
    va_list arguments;
    va_start(arguments, format);
    int written = std::vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    if (written <= 0) return;
    size_t length = static_cast<size_t>(written);
    if (length >= sizeof(buffer)) length = sizeof(buffer) - 1;
    gServices->logger.write(buffer, length);
}

void logLine(const char* text) {
    if (!gServices || !text) return;
    size_t length = std::strlen(text);
    gServices->logger.write(text, length);
    static constexpr char newline = '\n';
    gServices->logger.write(&newline, 1);
}

}  // namespace Platform
