#pragma once
#include "esp_partition.h"
struct esp_vfs_littlefs_conf_t {
    const char* base_path;
    const char* partition_label;
    const esp_partition_t* partition;
    uint8_t format_if_mount_failed:1;
    uint8_t read_only:1;
    uint8_t dont_mount:1;
    uint8_t grow_on_mount:1;
};
esp_err_t esp_vfs_littlefs_register(const esp_vfs_littlefs_conf_t*);
esp_err_t esp_littlefs_format(const char*);
esp_err_t esp_littlefs_info(const char*,size_t*,size_t*);
