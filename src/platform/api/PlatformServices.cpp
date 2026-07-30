#include "platform/api/PlatformServices.h"

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

}  // namespace Platform
