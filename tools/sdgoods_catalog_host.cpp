// SPDX-License-Identifier: Apache-2.0
// Run the same catalog validation used before SDGOODS restores any save.
#include "core/ContentStore.h"
#include "core/ContentValidation.h"
#include "platform/desktop/DesktopPlatform.h"
#include <cstdio>

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    DesktopPlatform desktop(argv[1]);
    Platform::bind(desktop.serviceBundle());
    ContentStore catalog;
    ContentData::bind(&catalog);
    if (!validateContentCatalog() || !catalog.healthy()) return 1;
    std::puts("PASS SDGOODS startup content catalog");
    return 0;
}
