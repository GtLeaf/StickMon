#include <cassert>
#include <cstdio>
#if __has_include("core/PhotoLibrary.h")
#include "core/PhotoLibrary.h"
#include "platform/common/PosixUserFiles.h"
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include <vector>
struct Memory:Platform::IMemoryAllocator {
    bool fail=false;size_t live=0;
    void* allocate(size_t n,bool)override{if(fail)return nullptr;void* p=std::malloc(n);if(p)++live;return p;}
    void release(void* p)override{if(p){assert(live);--live;std::free(p);}}
    size_t externalFree()const override{return 8*1024*1024;}
};
struct Blobs:Platform::IBlobStore {
    std::map<std::string,std::vector<uint8_t>> data;bool fail=false,failSizeOnce=false;
    bool initialize()override{return true;}
    std::string key(const char*n,const char*k){return std::string(n)+"/"+k;}
    size_t blobSize(const char*n,const char*k)override{if(failSizeOnce){failSizeOnce=false;return 0;}auto i=data.find(key(n,k));return i==data.end()?0:i->second.size();}
    Platform::BlobLookup lookupBlob(const char*n,const char*k,size_t& length)override{
        length=0;if(failSizeOnce){failSizeOnce=false;return Platform::BlobLookup::FAILED;}
        auto i=data.find(key(n,k));if(i==data.end())return Platform::BlobLookup::MISSING;
        length=i->second.size();return Platform::BlobLookup::FOUND;
    }
    bool readBlob(const char*n,const char*k,void*out,size_t len)override{auto& b=data[key(n,k)];if(b.size()!=len)return false;std::memcpy(out,b.data(),len);return true;}
    bool writeBlob(const char*n,const char*k,const void*p,size_t len)override{if(fail)return false;const auto*b=static_cast<const uint8_t*>(p);data[key(n,k)]={b,b+len};return true;}
    bool removeBlob(const char*n,const char*k)override{data.erase(key(n,k));return true;}
    bool clearNamespace(const char*)override{return false;}
};
struct Files:Platform::PosixUserFiles {
    using PosixUserFiles::PosixUserFiles;bool failWrite=false,failDelete=false,failBegin=false,failBeginAfterWrite=false,failBeginAfterDelete=false;size_t available=SIZE_MAX;
    bool begin()override{return !failBegin&&PosixUserFiles::begin();}
    size_t availableBytes()override{return available;}
    bool writeNew(const char*n,const void*p,size_t len,Platform::FileYield yield=nullptr)override{bool result=!failWrite&&PosixUserFiles::writeNew(n,p,len,yield);if(result&&failBeginAfterWrite)failBegin=true;return result;}
    bool remove(const char*n)override{bool result=!failDelete&&PosixUserFiles::remove(n);if(result&&failBeginAfterDelete)failBegin=true;return result;}
};
static unsigned yields=0;
int main(int argc,char**argv){
    assert(argc==2);Files files(argv[1]);Blobs blobs;Memory memory;
    using namespace Photos;
    assert(CAPACITY==5&&"Default photo capacity must be five");
    Library library(&files,&blobs,&memory);assert(library.refresh()==Result::OK&&library.count()==0);
    size_t legacySize=9;assert(blobs.Platform::IBlobStore::lookupBlob("photos","sequence",legacySize)==Platform::BlobLookup::FAILED&&legacySize==0);
    std::vector<uint16_t> pixels(PIXELS),loaded(PIXELS),thumb(THUMB_PIXELS);
    for(size_t i=0;i<pixels.size();++i)pixels[i]=uint16_t(i*17u+7);
    assert(library.save(pixels.data(),0,12,[](){++yields;})==Result::OK&&library.count()==1&&yields>1);
    const Entry first=library.entry(0);assert(first.id&&first.day==12&&first.area==0);
    assert(library.loadImage(first,loaded.data())==Result::OK&&loaded==pixels);
    assert(library.loadThumbnail(first,thumb.data())==Result::OK&&thumb[0]==pixels[2*192+2]);
    Library reboot(&files,&blobs,&memory);assert(reboot.refresh()==Result::OK&&reboot.count()==1&&reboot.entry(0).id==first.id);
    files.failWrite=true;assert(library.save(pixels.data(),1,13)==Result::FAILED&&library.count()==1);files.failWrite=false;
    blobs.fail=true;assert(library.save(pixels.data(),1,13)==Result::FAILED&&library.count()==1);blobs.fail=false;
    memory.fail=true;assert(library.save(pixels.data(),1,13)==Result::NO_MEMORY);memory.fail=false;
    files.available=1;assert(library.save(pixels.data(),1,13)==Result::FULL);files.available=SIZE_MAX;
    for(unsigned i=1;i<CAPACITY;++i)assert(library.save(pixels.data(),uint8_t(i%6),uint32_t(12+i))==Result::OK);
    assert(library.count()==5&&library.save(pixels.data(),0,30)==Result::FULL);
    const auto newest=library.entry(0);assert(newest.id>first.id);
    files.failDelete=true;assert(library.erase(first)==Result::FAILED&&library.count()==5);files.failDelete=false;
    assert(library.erase(first)==Result::OK&&library.count()==4);
    assert(library.save(pixels.data(),2,31)==Result::OK&&library.entry(0).id>newest.id);
    assert(library.loadImage(first,loaded.data())==Result::NOT_FOUND);
    // Payload damage is never shown as pixels; malformed headers remain occupied.
    const auto current=library.entry(0);char name[24];Library::slotName(current.slot,name,sizeof(name));
    auto path=std::filesystem::path(argv[1])/name;FILE*f=std::fopen(path.c_str(),"r+b");assert(f);std::fseek(f,Library::HEADER_BYTES,SEEK_SET);std::fputc(0x55,f);std::fclose(f);
    assert(library.loadImage(current,loaded.data())==Result::CORRUPT);
    assert(library.loadThumbnail(current,thumb.data())==Result::OK);
    f=std::fopen(path.c_str(),"r+b");assert(f);std::fputc(0,f);std::fclose(f);assert(library.refresh()==Result::OK&&library.count()==5);
    bool broken=false;Entry bad;for(unsigned i=0;i<library.count();++i)if(!library.entry(i).valid){bad=library.entry(i);broken=true;}assert(broken);
    assert(library.erase(bad)==Result::OK&&library.count()==4);
    while(library.count())assert(library.erase(library.entry(0))==Result::OK);
    const auto sequenceBefore=blobs.data["photos/sequence"];
    blobs.failSizeOnce=true;assert(library.save(pixels.data(),0,40)==Result::UNAVAILABLE&&"Counter read failure after emptying album must not restart photo IDs");assert(blobs.data["photos/sequence"]==sequenceBefore);
    assert(library.save(pixels.data(),0,40)==Result::OK&&library.entry(0).id>current.id);
    assert(files.writeNew("notes.bin","keep",4));
    assert(files.writeNew("slot7.photo.tmp","unfinished",10));
    assert(library.refresh()==Result::OK&&library.count()==1&&files.size("notes.bin")==4);
    files.failBeginAfterWrite=true;assert(library.save(pixels.data(),0,41)==Result::OK&&library.count()==2&&"Published photo must report success even if a later refresh would fail");files.failBegin=files.failBeginAfterWrite=false;
    files.failBeginAfterDelete=true;assert(library.erase(library.entry(0))==Result::OK&&library.count()==1&&"Completed deletion must report success even if a later scan would fail");files.failBegin=files.failBeginAfterDelete=false;
    blobs.data["photos/sequence"]={1,2};assert(library.save(pixels.data(),0,40)==Result::CORRUPT&&library.count()==1);
    assert(!files.writeNew("../outside",pixels.data(),2)&&!files.remove("../outside"));
    Library::slotName(library.entry(0).slot,name,sizeof(name));assert(!files.writeNew(name,pixels.data(),2));
    assert(memory.live==0);std::puts("photo library: real files, stable IDs, five/full/delete/reboot, CRC damage, failed writes/counter/memory/quota, no overwrite/path escape OK");
}
#else
int main(){assert(false&&"Exploration photo file storage is not implemented");}
#endif
