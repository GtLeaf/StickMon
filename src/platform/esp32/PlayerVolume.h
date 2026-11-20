#pragma once
#include "platform/common/BlankStorageGuard.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_vfs_fat.h"
#include "wear_levelling.h"
#include <mutex>
#include <sys/stat.h>
namespace Stickmon {
template <bool DedicatedPhotos>
inline bool mountArchiveVolume() {
    constexpr const char* label = DedicatedPhotos ? "photos" : "brainfs";
    constexpr const char* path = DedicatedPhotos ? "/photo" : "/brain";
    static std::mutex mountMutex;
    const std::lock_guard<std::mutex> lock(mountMutex);
    static bool mounted=false;
    if(mounted)return true;
    const auto* partition=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_FAT,label);
    if(!partition){
        ESP_LOGE("PhotoStorage", "FAT partition %s is missing from the running partition table", label);
        return false;
    }
    // Check BEFORE mounting: wear levelling can initialize metadata on blank flash.
    const bool blank=fullyErased(partition->size,[&](size_t offset,void*out,size_t length){return esp_partition_read(partition,offset,out,length)==ESP_OK;});
    esp_vfs_fat_mount_config_t config{};
    config.max_files=12;config.allocation_unit_size=4096;
    config.format_if_mount_failed=blank;
    wl_handle_t handle=WL_INVALID_HANDLE;
    const auto result=esp_vfs_fat_spiflash_mount_rw_wl(path,label,&config,&handle);
    if (result == ESP_OK) {
        mounted = true;
        ESP_LOGI("PhotoStorage", "%s mounted at %s", label, path);
        return true;
    }
    // ESP-IDF 5.5.4 can leave the wear levelling handle allocated on failure.
    if(handle!=WL_INVALID_HANDLE)wl_unmount(handle);
    if(result==ESP_ERR_INVALID_STATE){
        struct stat s{};mounted=::stat(path,&s)==0&&S_ISDIR(s.st_mode);
    }
    if(!mounted)ESP_LOGE("PhotoStorage", "%s mount failed: 0x%x (blank=%d)", label, unsigned(result), int(blank));
    return mounted;
}

// Claw and legacy photo storage share this mount for the application lifetime.
inline bool mountPlayerVolume() { return mountArchiveVolume<false>(); }
inline bool mountPhotoVolume() { return mountArchiveVolume<true>(); }
}
