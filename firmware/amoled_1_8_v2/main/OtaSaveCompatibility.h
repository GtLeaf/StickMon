#pragma once

#include <cstdint>

namespace AmoledV2::Ota {

// The signed target declares the save formats it can read. Do not downgrade
// the writer format, and never infer forward compatibility from build alone.
inline bool canMigrateSave(uint32_t currentSchema, uint32_t currentVersion,
                           uint32_t targetSchema, uint32_t targetVersion,
                           uint32_t minSchema, uint32_t minVersion) {
    return minSchema > 0 && minVersion > 0 &&
           targetSchema <= UINT16_MAX && targetVersion <= UINT16_MAX &&
           minSchema <= currentSchema && currentSchema <= targetSchema &&
           minVersion <= currentVersion && currentVersion <= targetVersion;
}

} // namespace AmoledV2::Ota
