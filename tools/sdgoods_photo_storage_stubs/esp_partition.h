#pragma once
#include <cstddef>
#include <cstdint>
using esp_err_t=int;
constexpr int ESP_OK=0,ESP_FAIL=-1,ESP_ERR_INVALID_STATE=2,ESP_ERR_NVS_NOT_FOUND=3,ESP_ERR_INVALID_ARG=4;
constexpr int ESP_PARTITION_TYPE_DATA=1,ESP_PARTITION_SUBTYPE_DATA_LITTLEFS=0x83;
struct esp_partition_t {size_t size;};
const esp_partition_t* esp_partition_find_first(int,int,const char*);
esp_err_t esp_partition_read(const esp_partition_t*,size_t,void*,size_t);
