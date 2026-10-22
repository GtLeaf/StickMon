#include "firmware/amoled_1_8_v2/main/OtaSaveCompatibility.h"
#include <cassert>

int main() {
    using AmoledV2::Ota::canMigrateSave;
    // A migration-aware baseline can install a signed v5 target reading v4.
    assert(canMigrateSave(2, 4, 3, 5, 1, 1));
    assert(canMigrateSave(3, 5, 4, 6, 2, 4));
    assert(canMigrateSave(3, 5, 3, 5, 3, 5));
    // Legacy manifests default their minimums to their writer versions.
    assert(!canMigrateSave(2, 4, 3, 5, 3, 5));
    assert(!canMigrateSave(3, 5, 2, 4, 2, 4));
    // Both schema and state must fit: neither may be silently ignored.
    assert(!canMigrateSave(2, 4, 3, 5, 3, 1));
    assert(!canMigrateSave(2, 4, 3, 5, 1, 5));
    assert(!canMigrateSave(3, 5, 3, 4, 1, 1));
    assert(!canMigrateSave(3, 5, 2, 5, 1, 1));
    assert(!canMigrateSave(3, 5, 3, 5, 0, 1));
    assert(!canMigrateSave(3, 5, 3, 5, 1, 0));
    assert(!canMigrateSave(3, 5, 3, 5, 4, 1));
    assert(!canMigrateSave(3, 5, 65536, 5, 1, 1));
    assert(!canMigrateSave(3, 5, 3, 65536, 1, 1));
}
