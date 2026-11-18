// SPDX-License-Identifier: Apache-2.0
#include "PhotoFiles.h"
#include "platform/common/BlankStorageGuard.h"
#include "esp_littlefs.h"
#include "esp_partition.h"

namespace Sdgoods {
bool PhotoFiles::begin() {
    if (!mounted_) {
        const auto* partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
            ESP_PARTITION_SUBTYPE_DATA_LITTLEFS, "userdata");
        if (!partition) return false;
        // Prove the ENTIRE partition is erased before any filesystem operation.
        // An unreadable or damaged archive can be mounted, but never formatted.
        const bool blank = Stickmon::fullyErased(partition->size,
            [&](size_t offset, void* output, size_t length) {
                return esp_partition_read(partition, offset, output, length) == ESP_OK;
            });
        if (blank && esp_littlefs_format("userdata") != ESP_OK) return false;
        esp_vfs_littlefs_conf_t config{};
        config.base_path = "/userdata";
        config.partition_label = "userdata";
        config.format_if_mount_failed = false;
        if (esp_vfs_littlefs_register(&config) != ESP_OK) return false;
        mounted_ = true;
    }
    return PosixUserFiles::begin();
}

size_t PhotoFiles::availableBytes() {
    size_t available = 0;
    queryAvailableBytes(available);
    return available;
}

bool PhotoFiles::queryAvailableBytes(size_t& available) {
    available = 0;
    size_t total = 0, used = 0;
    if (!mounted_ || esp_littlefs_info("userdata", &total, &used) != ESP_OK || used > total)
        return false;
    available = total - used;
    return true;
}
}
