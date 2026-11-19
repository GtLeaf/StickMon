#pragma once
#include "platform/common/BlankStorageGuard.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_vfs_fat.h"
#include "wear_levelling.h"
#include <sys/stat.h>
namespace Stickmon {
// Shared lifetime with Claw. Neither owner unmounts the player archive.
inline bool mountPlayerVolume(){
    static bool mounted=false;
    if(mounted)return true;
    const auto* partition=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_FAT,"brainfs");
    if(!partition){
        ESP_LOGE("PhotoStorage", "FAT partition brainfs is missing from the running partition table");
        return false;
    }
    // Check BEFORE mounting: wear levelling can initialize metadata on blank flash.
    const bool blank=fullyErased(partition->size,[&](size_t offset,void*out,size_t length){return esp_partition_read(partition,offset,out,length)==ESP_OK;});
    esp_vfs_fat_mount_config_t config{};
    config.max_files=12;config.allocation_unit_size=4096;
    config.format_if_mount_failed=blank;
    wl_handle_t handle=WL_INVALID_HANDLE;
    const auto result=esp_vfs_fat_spiflash_mount_rw_wl("/brain","brainfs",&config,&handle);
    if(result==ESP_OK){mounted=true;return true;}
    // ESP-IDF 5.5.4 can leave the wear levelling handle allocated on failure.
    if(handle!=WL_INVALID_HANDLE)wl_unmount(handle);
    if(result==ESP_ERR_INVALID_STATE){
        struct stat s{};mounted=::stat("/brain",&s)==0&&S_ISDIR(s.st_mode);
    }
    if(!mounted)ESP_LOGE("PhotoStorage", "brainfs mount failed: 0x%x (blank=%d)", unsigned(result), int(blank));
    return mounted;
}
}
