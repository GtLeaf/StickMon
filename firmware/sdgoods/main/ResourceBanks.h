// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "platform/api/PlatformServices.h"
#include "core/ResourceUpdate.h"
#include "esp_partition.h"

namespace Sdgoods {
bool storageLayoutHealthy();
int runningSlot();
const esp_partition_t* appPartition(unsigned slot);
const esp_partition_t* resourcePartition(unsigned slot);
bool loadBinding(unsigned slot,ResourceUpdate::Binding& out);
bool saveBinding(const ResourceUpdate::Binding& value);
bool hashPartition(const esp_partition_t*,size_t,uint8_t (&hash)[32]);
bool verifyResourceDirectory(const char* root,const uint8_t* expectedIndexHash=nullptr);
bool mountResourceBank(unsigned slot,const char* path);
void unmountResourceBank(unsigned slot);
class ResourceBanks final : public Platform::IResourceStore {
    int slot_=-1;
    size_t total_=0,used_=0;
public:
    bool mount() override;
    size_t totalBytes() const override {return total_;}
    size_t usedBytes() const override {return used_;}
    Platform::ResourceFile open(const char*) override;
};
} // namespace Sdgoods
