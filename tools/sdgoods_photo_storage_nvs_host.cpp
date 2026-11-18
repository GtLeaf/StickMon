#include "platform/api/PlatformServices.h"
#include "esp_partition.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
using nvs_handle_t=unsigned;
enum nvs_open_mode_t {NVS_READONLY,NVS_READWRITE};
static esp_err_t openResult=ESP_OK,getResult=ESP_OK;
static unsigned opens=0,closes=0,writes=0;
static std::string openedNamespace;
static nvs_open_mode_t openedMode=NVS_READWRITE;
static size_t blobLength=16;
static esp_err_t nvs_open(const char*name,nvs_open_mode_t mode,nvs_handle_t*handle){
    ++opens;openedNamespace=name;openedMode=mode;*handle=42;return openResult;
}
static esp_err_t nvs_get_blob(nvs_handle_t handle,const char*key,void*output,size_t*length){
    assert(handle==42&&std::strcmp(key,"sequence")==0);
    if(output)std::memset(output,0,*length);
    *length=blobLength;return getResult;
}
static void nvs_close(nvs_handle_t handle){assert(handle==42);++closes;}
static esp_err_t nvs_set_blob(nvs_handle_t,const char*,const void*,size_t){++writes;return ESP_OK;}
static esp_err_t nvs_commit(nvs_handle_t){return ESP_OK;}
static esp_err_t nvs_erase_key(nvs_handle_t,const char*){++writes;return ESP_OK;}
static esp_err_t nvs_erase_all(nvs_handle_t){++writes;return ESP_OK;}
static const char* esp_err_to_name(esp_err_t){return "mock error";}
namespace Platform {void errorf(const char*,...) {}}
namespace Sdgoods {
class SdgoodsPlatform : public Platform::IBlobStore {
public:
    bool initialize()override{return true;}
    size_t blobSize(const char*,const char*)override;
    bool readBlob(const char*,const char*,void*,size_t)override;
    bool writeBlob(const char*,const char*,const void*,size_t)override;
    bool removeBlob(const char*,const char*)override;
    bool clearNamespace(const char*)override;
    void enableSaveWrites(bool value){saveWritesEnabled_=value;}
    // The harness emits the actual new override only once it exists.
    #include "sdgoods-photo-nvs-declaration.inc"
private:
    bool saveWritesEnabled_=true;
};
namespace {
#include "sdgoods-photo-nvs-open.inc"
}
#include "sdgoods-photo-nvs-methods.inc"
}
int main(){
    using Status=Platform::BlobLookup;
    Sdgoods::SdgoodsPlatform store;size_t length=777;
    assert(store.lookupBlob("photos","sequence",length)==Status::FOUND&&length==16);
    assert(openedNamespace=="sg_photos"&&openedMode==NVS_READONLY&&closes==1);
    openResult=ESP_ERR_NVS_NOT_FOUND;length=777;
    assert(store.lookupBlob("photos","sequence",length)==Status::MISSING&&length==0&&closes==1);
    openResult=ESP_FAIL;length=777;
    assert(store.lookupBlob("photos","sequence",length)==Status::FAILED&&length==0&&closes==1);
    openResult=ESP_OK;getResult=ESP_ERR_NVS_NOT_FOUND;length=777;
    assert(store.lookupBlob("photos","sequence",length)==Status::MISSING&&length==0&&closes==2);
    getResult=ESP_FAIL;length=777;
    assert(store.lookupBlob("photos","sequence",length)==Status::FAILED&&length==0&&closes==3);
    getResult=ESP_OK;blobLength=0;
    assert(store.lookupBlob("photos","sequence",length)==Status::FOUND&&length==0);
    const unsigned before=opens;
    assert(store.lookupBlob(nullptr,"sequence",length)==Status::FAILED&&length==0);
    assert(store.lookupBlob("photos",nullptr,length)==Status::FAILED&&length==0);
    assert(store.lookupBlob("","sequence",length)==Status::FAILED&&length==0);
    assert(store.lookupBlob("thirteenchars","sequence",length)==Status::FAILED&&length==0);
    assert(opens==before);
    store.enableSaveWrites(false);const char data[]="counter";
    assert(!store.writeBlob("photos","sequence",data,sizeof(data)));
    assert(!store.removeBlob("photos","sequence")&&!store.clearNamespace("photos"));
    assert(writes==0&&opens==before);
    blobLength=16;
    assert(store.lookupBlob("photos","sequence",length)==Status::FOUND&&length==16);
    std::puts("sdgoods NVS: sg_ isolation, authoritative missing/failure/zero length, read-only lookup and write gate OK");
}
