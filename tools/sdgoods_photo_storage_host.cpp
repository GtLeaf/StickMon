#include <cassert>
#include <cstdio>
#if __has_include("PhotoFiles.h")
#include "PhotoFiles.h"
#include "esp_littlefs.h"
#include "core/PhotoLibrary.h"
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

static std::string scenario,root;
static size_t readBytes=0;
static unsigned formats=0,mounts=0;
static bool mounted=false;
static bool failSync=false,failRename=false;
static esp_partition_t partition{8197};
struct UnusedBlobs : Platform::IBlobStore {
    unsigned calls=0;
    bool initialize()override{++calls;return false;}
    size_t blobSize(const char*,const char*)override{++calls;return 0;}
    bool readBlob(const char*,const char*,void*,size_t)override{++calls;return false;}
    bool writeBlob(const char*,const char*,const void*,size_t)override{++calls;return false;}
    bool removeBlob(const char*,const char*)override{++calls;return false;}
    bool clearNamespace(const char*)override{++calls;return false;}
};
struct UnusedMemory : Platform::IMemoryAllocator {
    unsigned calls=0;
    void* allocate(size_t,bool)override{++calls;return nullptr;}
    void release(void*)override{++calls;}
    size_t externalFree()const override{return 0;}
};
static std::string translate(const char* path){
    const std::string p(path);
    assert(p=="/userdata"||p.rfind("/userdata/",0)==0);
    return root+p.substr(9);
}
extern "C" {
int __real_mkdir(const char*,mode_t);
int __real_stat(const char*,struct stat*);
FILE* __real_fopen(const char*,const char*);
int __real_rename(const char*,const char*);
int __real_unlink(const char*);
int __real_fsync(int);
int __wrap_mkdir(const char*p,mode_t m){return __real_mkdir(translate(p).c_str(),m);}
int __wrap_stat(const char*p,struct stat*s){return __real_stat(translate(p).c_str(),s);}
FILE* __wrap_fopen(const char*p,const char*m){return __real_fopen(translate(p).c_str(),m);}
int __wrap_rename(const char*a,const char*b){
    if(failRename){errno=EIO;return -1;}
    return __real_rename(translate(a).c_str(),translate(b).c_str());
}
int __wrap_unlink(const char*p){return __real_unlink(translate(p).c_str());}
int __wrap_fsync(int fd){if(failSync){errno=EIO;return -1;}return __real_fsync(fd);}
}
const esp_partition_t* esp_partition_find_first(int type,int subtype,const char*label){
    assert(type==ESP_PARTITION_TYPE_DATA&&subtype==ESP_PARTITION_SUBTYPE_DATA_LITTLEFS);
    assert(std::strcmp(label,"userdata")==0);
    return scenario=="missing"?nullptr:&partition;
}
esp_err_t esp_partition_read(const esp_partition_t*p,size_t offset,void*out,size_t n){
    assert(p==&partition&&!mounted&&formats==0&&offset==readBytes&&offset+n<=partition.size);
    readBytes+=n;
    if(scenario=="readfail"&&offset>=4096)return ESP_FAIL;
    std::memset(out,255,n);
    if((scenario=="existing"||scenario=="corrupt")&&offset==0)static_cast<uint8_t*>(out)[0]=0;
    if(scenario=="tail"&&offset+n==partition.size)static_cast<uint8_t*>(out)[n-1]=0;
    return ESP_OK;
}
esp_err_t esp_littlefs_format(const char*label){
    assert(std::strcmp(label,"userdata")==0&&!mounted&&mounts==0&&readBytes==partition.size);
    assert(scenario!="existing"&&scenario!="corrupt"&&scenario!="tail"&&scenario!="readfail");
    ++formats;return scenario=="formatfail"?ESP_FAIL:ESP_OK;
}
esp_err_t esp_vfs_littlefs_register(const esp_vfs_littlefs_conf_t*c){
    assert(!c->format_if_mount_failed&&!c->read_only&&!c->dont_mount&&!c->grow_on_mount);
    assert(std::strcmp(c->base_path,"/userdata")==0&&std::strcmp(c->partition_label,"userdata")==0);
    ++mounts;
    if(scenario=="corrupt"||scenario=="tail"||scenario=="readfail"||scenario=="mountfail")return ESP_FAIL;
    mounted=true;return ESP_OK;
}
esp_err_t esp_littlefs_info(const char*label,size_t*total,size_t*used){
    assert(mounted&&std::strcmp(label,"userdata")==0);
    *total=8192;*used=scenario=="badinfo"?8193:scenario=="full"?8192:4096;
    return scenario=="infofail"?ESP_FAIL:ESP_OK;
}
int main(int argc,char**argv){
    assert(argc==3);scenario=argv[1];root=argv[2];
    Sdgoods::PhotoFiles files;assert(files.availableBytes()==0);
    const bool expected=scenario=="blank"||scenario=="existing"||scenario=="infofail"||scenario=="badinfo"||scenario=="full";
    assert(files.begin()==expected);
    if(!expected){
        assert(files.availableBytes()==0);
        assert(formats==unsigned(scenario=="formatfail"||scenario=="mountfail"));
        assert(mounts==unsigned(scenario!="missing"&&scenario!="formatfail"));
    }else{
        assert(formats==unsigned(scenario!="existing")&&mounts==1);
        assert(files.begin()&&mounts==1);
        assert(files.availableBytes()==(scenario=="infofail"||scenario=="badinfo"||scenario=="full"?0:4096));
        const char bytes[]="photo bytes";char readback[sizeof(bytes)]{};
        assert(files.writeNew("slot0.photo",bytes,sizeof(bytes)));
        assert(!files.writeNew("slot0.photo","replace",7));
        assert(files.read("slot0.photo",0,readback,sizeof(readback))&&std::memcmp(bytes,readback,sizeof(bytes))==0);
        if(scenario=="infofail"||scenario=="badinfo"||scenario=="full"){
            UnusedBlobs blobs;UnusedMemory memory;
            Photos::Library library(&files,&blobs,&memory);
            std::vector<uint16_t> draft(Photos::PIXELS,0x6ac3);
            const auto before=draft;
            const auto want=scenario=="full"?Photos::Result::FULL:Photos::Result::UNAVAILABLE;
            assert(library.save(draft.data(),0,7)==want&&"Space query failure must be UNAVAILABLE, genuine zero free must be FULL");
            assert(draft==before&&blobs.calls==0&&memory.calls==0&&library.count()==1);
            assert(files.read("slot0.photo",0,readback,sizeof(readback))&&std::memcmp(bytes,readback,sizeof(bytes))==0);
            assert(!files.exists("slot1.photo")&&!std::filesystem::exists(root+"/photos/slot1.photo.tmp"));
            std::printf("sdgoods library save: %s returns %s; draft and existing file preserved\n",scenario.c_str(),scenario=="full"?"FULL":"UNAVAILABLE");
        }
        assert(!files.writeNew("../outside",bytes,sizeof(bytes)));
        assert(!files.read("slot0.photo",sizeof(bytes),readback,1));
        assert(!std::filesystem::exists(root+"/photos/slot0.photo.tmp"));
        // A failed durable flush or publication leaves old photos byte-exact
        // and removes the incomplete temp, allowing a later save to retry.
        failSync=true;
        assert(!files.writeNew("slot1.photo",bytes,sizeof(bytes))&&!files.exists("slot1.photo"));
        assert(!std::filesystem::exists(root+"/photos/slot1.photo.tmp"));
        failSync=false;failRename=true;
        assert(!files.writeNew("slot1.photo",bytes,sizeof(bytes))&&!files.exists("slot1.photo"));
        assert(!std::filesystem::exists(root+"/photos/slot1.photo.tmp"));
        failRename=false;
        assert(files.read("slot0.photo",0,readback,sizeof(readback))&&std::memcmp(bytes,readback,sizeof(bytes))==0);
        assert(files.writeNew("slot1.photo",bytes,sizeof(bytes))&&files.remove("slot1.photo"));
        assert(files.remove("slot0.photo")&&!files.exists("slot0.photo"));
    }
    std::printf("sdgoods photo storage: %s OK (read=%zu format=%u mount=%u)\n",scenario.c_str(),readBytes,formats,mounts);
}
#else
int main(){assert(false&&"SDGOODS PhotoFiles adapter is missing");}
#endif
