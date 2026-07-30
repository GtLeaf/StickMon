#include "core/ResourceService.h"

#include "core/FontResource.h"
#include "core/ResourceFS.h"
#include "core/ResourcePack.h"
#include "core/RoomResource.h"

bool ResourceService::begin() {
    if (initialized_) return filesystemReady_;
    initialized_ = true;
    filesystemReady_ = ResourceFS::ins().begin();
    packReady_ = filesystemReady_ && ResourcePack::ins().begin();
    FontResource::ins().begin();
    RoomResource::ins().begin();
    return filesystemReady_;
}
