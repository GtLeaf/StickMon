#pragma once
#include "platform/esp32/PlayerVolume.h"
#include "platform/common/PosixUserFiles.h"
namespace Stickmon {
class PlayerPhotoFiles final:public Platform::PosixUserFiles {
public:
    PlayerPhotoFiles():PosixUserFiles("/brain/photos"){}
    bool begin()override{return mountPlayerVolume()&&PosixUserFiles::begin();}
    size_t availableBytes()override{
        size_t total=0,available=0;
        return esp_vfs_fat_info("/brain",&total,&available)==ESP_OK?available:0;
    }
};
}
