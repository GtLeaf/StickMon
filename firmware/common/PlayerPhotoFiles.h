#pragma once
#include "platform/esp32/PlayerVolume.h"
#include "platform/common/PosixUserFiles.h"
namespace Stickmon {
class PlayerPhotoFiles final:public Platform::PosixUserFiles {
public:
    PlayerPhotoFiles():PosixUserFiles("/brain/photos"){}
    bool begin()override{
        if(!mountPlayerVolume())return false;
        if(PosixUserFiles::begin())return true;
        ESP_LOGE("PhotoStorage", "Cannot open /brain/photos as a directory");
        return false;
    }
    size_t availableBytes()override{
        size_t available=0;
        queryAvailableBytes(available);
        return available;
    }
    bool queryAvailableBytes(size_t& available)override{
        uint64_t total=0, freeBytes=0;
        available=0;
        const auto result=esp_vfs_fat_info("/brain",&total,&freeBytes);
        if(result==ESP_OK){
            available=static_cast<size_t>(std::min<uint64_t>(freeBytes,SIZE_MAX));
            return true;
        }
        ESP_LOGE("PhotoStorage", "Cannot read brainfs free space: 0x%x", unsigned(result));
        return false;
    }
};
}
