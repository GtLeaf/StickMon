#!/usr/bin/env python3
import os,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    binary=Path(directory)/'guard'
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-Isrc','tools/photo_storage_guard_host.cpp','-o',str(binary)],cwd=ROOT,check=True)
    subprocess.run([str(binary)],check=True)
    base=Path(directory)
    (base/'wear_levelling.h').write_text('''#pragma once
using wl_handle_t=int;constexpr int WL_INVALID_HANDLE=-1;
extern int liveHandles;
inline int wl_unmount(wl_handle_t handle){if(handle!=0||liveHandles!=1)return -1;--liveHandles;return 0;}
''')
    (base/'esp_log.h').write_text('#pragma once\n#define ESP_LOGE(...) ((void)0)\n')
    (base/'esp_partition.h').write_text('''#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
constexpr int ESP_OK=0,ESP_FAIL=-1,ESP_ERR_INVALID_STATE=2;
constexpr int ESP_PARTITION_TYPE_DATA=1,ESP_PARTITION_SUBTYPE_DATA_FAT=2;
struct esp_partition_t {size_t size;};
extern int scenario,mountCalls,formats;
inline const esp_partition_t* esp_partition_find_first(int,int,const char*){static esp_partition_t part{8197};return scenario==4?nullptr:&part;}
inline int esp_partition_read(const esp_partition_t*,size_t offset,void*out,size_t n){
 if(scenario==2)return ESP_FAIL;
 std::memset(out,255,n);
 if((scenario==1||scenario==3)&&offset==0)static_cast<uint8_t*>(out)[0]=0;
 if(scenario==6&&offset+n==8197)static_cast<uint8_t*>(out)[n-1]=0;
 return ESP_OK;
}
''')
    (base/'esp_vfs_fat.h').write_text('''#pragma once
#include "esp_partition.h"
#include "wear_levelling.h"
struct esp_vfs_fat_mount_config_t {bool format_if_mount_failed=false;int max_files=0;size_t allocation_unit_size=0;};
inline int esp_vfs_fat_spiflash_mount_rw_wl(const char*,const char*,const esp_vfs_fat_mount_config_t*c,wl_handle_t* handle){
 ++mountCalls;if(c->format_if_mount_failed)++formats;
 // ESP-IDF can hand back an allocated wear levelling handle even on FAT mount failure.
 *handle=WL_INVALID_HANDLE;if(liveHandles)return ESP_FAIL;
 ++liveHandles;*handle=0;
 return scenario==3||c->format_if_mount_failed?ESP_OK:ESP_FAIL;
}
''')
    binary=base/'volume'
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-Isrc','-I'+directory,'tools/photo_volume_host.cpp','-o',str(binary)],cwd=ROOT,check=True)
    for scenario in [0,1,2,3,4,6]:subprocess.run([str(binary),str(scenario)],check=True)
