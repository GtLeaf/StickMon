// SPDX-License-Identifier: Apache-2.0
#include "ResourceBanks.h"
#include "core/ResourceBankFormat.h"
#include "esp_littlefs.h"
#include "esp_ota_ops.h"
#include "esp_flash.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "mbedtls/sha256.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <vector>
#include <algorithm>

namespace Sdgoods {
namespace {
constexpr const char* RESOURCE_LABEL = "resources";
constexpr bool FULL_STORAGE = ResourceUpdate::BANK_SIZE == 0x1800000;
constexpr const char* LAYOUT = FULL_STORAGE ? "sdgoods-32m-single-v1" : "sdgoods-16m-single-v1";
class DiskFile final : public Platform::IResourceFile {
    FILE* file_;size_t size_;
public:
    DiskFile(FILE* file,size_t size):file_(file),size_(size) {}
    ~DiskFile() override {close();}
    bool valid() const override {return file_!=nullptr;}
    size_t size() const override {return size_;}
    size_t position() const override {return file_ ? size_t(std::ftell(file_)) : 0;}
    size_t read(void* data,size_t n) override {return file_ ? std::fread(data,1,n,file_) : 0;}
    bool seek(size_t offset) override {return file_ && offset<=size_ && !std::fseek(file_,long(offset),SEEK_SET);}
    void close() override {if(file_) std::fclose(file_);file_=nullptr;}
};
using ResourceBankFormat::safePath;
bool hashFile(const char* path,uint32_t expected,uint8_t (&out)[32]) {
    FILE* file=std::fopen(path,"rb");if(!file) return false;
    mbedtls_sha256_context sha;mbedtls_sha256_init(&sha);mbedtls_sha256_starts(&sha,0);
    uint8_t bytes[2048];size_t n,total=0;unsigned blocks=0;
    while((n=std::fread(bytes,1,sizeof(bytes),file))) {
        total+=n;if(total>expected) break;
        mbedtls_sha256_update(&sha,bytes,n);
        if(++blocks%16==0) vTaskDelay(1);
    }
    const bool ok=total==expected && !std::ferror(file);
    mbedtls_sha256_finish(&sha,out);mbedtls_sha256_free(&sha);std::fclose(file);return ok;
}
bool textEquals(cJSON* doc,const char* key,const char* value) {
    auto* field=cJSON_GetObjectItemCaseSensitive(doc,key);
    return cJSON_IsString(field) && !std::strcmp(field->valuestring,value);
}
bool numberEquals(cJSON* doc,const char* key,unsigned value) {
    auto* field=cJSON_GetObjectItemCaseSensitive(doc,key);
    return cJSON_IsNumber(field) && field->valuedouble==value;
}
}
int runningSlot() {
    const auto* p=esp_ota_get_running_partition();
    if(!p || (p->subtype!=ESP_PARTITION_SUBTYPE_APP_OTA_0 && p->subtype!=ESP_PARTITION_SUBTYPE_APP_OTA_1)) return -1;
    return p->subtype-ESP_PARTITION_SUBTYPE_APP_OTA_0;
}
const esp_partition_t* appPartition(unsigned slot) {
    return slot<2 ? esp_partition_find_first(ESP_PARTITION_TYPE_APP,
        esp_partition_subtype_t(ESP_PARTITION_SUBTYPE_APP_OTA_0+slot),nullptr) : nullptr;
}
const esp_partition_t* resourcePartition(unsigned slot) {
    // Resources are shared by both OTA application slots.  The slot argument
    // remains part of the binding API because bindings still identify the
    // application image that was validated against the resource manifest.
    return slot<2 ? esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_LITTLEFS,RESOURCE_LABEL) : nullptr;
}
bool storageLayoutHealthy() {
    uint32_t flashSize=0;
    if(esp_flash_get_size(esp_flash_default_chip,&flashSize)!=ESP_OK || flashSize!=0x2000000) return false;
    for(unsigned i=0;i<2;++i) {
        auto* app=appPartition(i);
        if(!app || app->address!=0x20000+i*ResourceUpdate::APP_SIZE || app->size!=ResourceUpdate::APP_SIZE) return false;
    }
    const auto* resources=resourcePartition(0);
    if(!resources || resources->address!=0x620000 || resources->size!=ResourceUpdate::BANK_SIZE) return false;
    struct Expected {const char* name;uint32_t address,size;};
    for(const auto& e:{Expected{"nvs",0x9000,0x6000},{"otadata",0xf000,0x2000},
        {"userdata",FULL_STORAGE ? 0x1e20000u : 0xf20000u,FULL_STORAGE ? 0x1c0000u : 0xc0000u},
        {"resmeta",FULL_STORAGE ? 0x1fe0000u : 0xfe0000u,0x10000},
        {"coredump",FULL_STORAGE ? 0x1ff0000u : 0xff0000u,0x10000}}) {
        const auto* p=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_ANY,e.name);
        if(!p || p->address!=e.address || p->size!=e.size) return false;
    }
    return true;
}
bool hashPartition(const esp_partition_t* partition,size_t size,uint8_t (&out)[32]) {
    if(!partition || size>partition->size) return false;
    mbedtls_sha256_context sha;mbedtls_sha256_init(&sha);mbedtls_sha256_starts(&sha,0);
    uint8_t bytes[2048];bool ok=true;unsigned blocks=0;
    for(size_t at=0;at<size;at+=sizeof(bytes)) {
        const size_t n=std::min(sizeof(bytes),size-at);
        if(esp_partition_read(partition,at,bytes,n)!=ESP_OK) {ok=false;break;}
        mbedtls_sha256_update(&sha,bytes,n);if(++blocks%16==0) vTaskDelay(1);
    }
    mbedtls_sha256_finish(&sha,out);mbedtls_sha256_free(&sha);return ok;
}
bool loadBinding(unsigned slot,ResourceUpdate::Binding& out) {
    if(slot>1 || nvs_flash_init_partition("resmeta")!=ESP_OK) return false;
    nvs_handle_t handle=0;
    if(nvs_open_from_partition("resmeta","pairs",NVS_READONLY,&handle)!=ESP_OK) return false;
    bool found=false;
    for(unsigned copy=0;copy<2;++copy) {
        char key[4];std::snprintf(key,sizeof(key),"%c%u",slot?'b':'a',copy);
        uint8_t bytes[ResourceUpdate::BINDING_BYTES];size_t size=sizeof(bytes);ResourceUpdate::Binding value;
        if(nvs_get_blob(handle,key,bytes,&size)==ESP_OK && ResourceUpdate::decodeBinding(bytes,size,value) &&
           value.slot==slot && (!found || value.sequence>out.sequence)) {out=value;found=true;}
    }
    nvs_close(handle);return found;
}
bool saveBinding(const ResourceUpdate::Binding& value) {
    ResourceUpdate::Binding old;
    if(value.slot>1 || !value.sequence || (loadBinding(value.slot,old) && value.sequence<=old.sequence)) return false;
    if(nvs_flash_init_partition("resmeta")!=ESP_OK) return false;
    nvs_handle_t handle=0;
    if(nvs_open_from_partition("resmeta","pairs",NVS_READWRITE,&handle)!=ESP_OK) return false;
    uint8_t bytes[ResourceUpdate::BINDING_BYTES];ResourceUpdate::encodeBinding(value,bytes);
    char key[4];std::snprintf(key,sizeof(key),"%c%u",value.slot?'b':'a',unsigned(value.sequence&1));
    bool ok=nvs_set_blob(handle,key,bytes,sizeof(bytes))==ESP_OK && nvs_commit(handle)==ESP_OK;
    nvs_close(handle);ResourceUpdate::Binding check;
    return ok && loadBinding(value.slot,check) && check.sequence==value.sequence && !std::memcmp(check.appHash,value.appHash,32);
}
bool mountResourceBank(unsigned slot,const char* path) {
    if(slot>1) return false;
    esp_vfs_littlefs_conf_t config{};config.base_path=path;config.partition_label=RESOURCE_LABEL;
    config.read_only=true;config.format_if_mount_failed=false;
    return esp_vfs_littlefs_register(&config)==ESP_OK;
}
void unmountResourceBank(unsigned slot) {if(slot<2) esp_vfs_littlefs_unregister(RESOURCE_LABEL);}
bool verifyResourceDirectory(const char* root,const uint8_t* expectedIndexHash) {
    std::string base(root), indexPath=base+"/files.idx";
    FILE* meta=std::fopen((base+"/bank-manifest.json").c_str(),"rb");if(!meta) return false;
    char json[2048]{};const size_t n=std::fread(json,1,sizeof(json)-1,meta);const bool end=std::fgetc(meta)==EOF;std::fclose(meta);
    cJSON* doc=end ? cJSON_ParseWithLength(json,n) : nullptr;
    if(!doc) return false;
    bool ok=textEquals(doc,"format","stickmon-resource-bank-v1") && textEquals(doc,"layout_id",LAYOUT) &&
        textEquals(doc,"target","sdgoods") && textEquals(doc,"root","/packs/dev") && numberEquals(doc,"schema",1) &&
        numberEquals(doc,"content_abi",ResourceUpdate::ABI) && numberEquals(doc,"bank_size",ResourceUpdate::BANK_SIZE);
    auto* countField=cJSON_GetObjectItemCaseSensitive(doc,"file_count");auto* indexField=cJSON_GetObjectItemCaseSensitive(doc,"index_sha256");
    const unsigned count=cJSON_IsNumber(countField) && countField->valuedouble>=1 &&
        countField->valuedouble<=4096 && countField->valuedouble==countField->valueint ? unsigned(countField->valueint) : 0;
    ok=ok && count>0 && count<=4096 && cJSON_IsString(indexField) && std::strlen(indexField->valuestring)==64;
    uint8_t hash[32]{};
    if(ok) ok=hashFile(indexPath.c_str(),16+196*count,hash);
    char hex[65]{};if(ok) for(unsigned i=0;i<32;++i) std::sprintf(hex+i*2,"%02x",hash[i]);
    ok=ok && !std::strcmp(hex,indexField && indexField->valuestring ? indexField->valuestring : "") &&
       (!expectedIndexHash || !std::memcmp(hash,expectedIndexHash,32));cJSON_Delete(doc);
    if(!ok) return false;
    FILE* index=std::fopen(indexPath.c_str(),"rb");if(!index) return false;
    uint8_t header[16]{};ok=std::fread(header,1,16,index)==16;
    uint32_t num=0;
    ok=ok && ResourceBankFormat::header(header,sizeof(header),num) && num==count;
    std::string previous;
    for(unsigned i=0;i<count && ok;++i) {
        uint8_t record[ResourceBankFormat::ENTRY_BYTES]{};
        ResourceBankFormat::Entry entry;
        ok=std::fread(record,1,sizeof(record),index)==sizeof(record) &&
           ResourceBankFormat::entry(record,sizeof(record),entry) && previous<entry.path;
        if(ok) {previous=entry.path;ok=hashFile((base+"/"+entry.path).c_str(),entry.size,hash) && !std::memcmp(hash,entry.hash,32);}
    }
    std::fclose(index);return ok;
}
bool ResourceBanks::mount() {
    if(slot_>=0) return true;
    const int slot=runningSlot();ResourceUpdate::Binding binding;
    uint8_t hash[32];
    if(!storageLayoutHealthy() || slot<0 || !loadBinding(unsigned(slot),binding) ||
       !hashPartition(appPartition(slot),binding.appBytes,hash) || std::memcmp(hash,binding.appHash,32) ||
       !mountResourceBank(slot,"/resources")) return false;
    bool ok=verifyResourceDirectory("/resources",binding.indexHash) &&
        esp_littlefs_info(RESOURCE_LABEL,&total_,&used_)==ESP_OK;
    if(!ok) {unmountResourceBank(slot);return false;}
    slot_=slot;Platform::logf("[SDGOODS] LittleFS shared total=%u used=%u ABI=%u\n",unsigned(total_),unsigned(used_),ResourceUpdate::ABI);
    return true;
}
Platform::ResourceFile ResourceBanks::open(const char* path) {
    if(slot_<0 || !path || *path!='/' || !safePath(path+1)) return {};
    const std::string full=std::string("/resources")+path;
    FILE* file=std::fopen(full.c_str(),"rb");if(!file) return {};
    if(std::fseek(file,0,SEEK_END)) {std::fclose(file);return {};}
    const long size=std::ftell(file);if(size<0 || std::fseek(file,0,SEEK_SET)) {std::fclose(file);return {};}
    auto* object=new(std::nothrow) DiskFile(file,size_t(size));
    if(!object) std::fclose(file);
    return Platform::ResourceFile(object);
}
} // namespace Sdgoods
