#pragma once
#include "platform/api/PlatformServices.h"
#include <cstddef>
#include <cstdint>

namespace Photos {
#ifndef STICKMON_PHOTO_CAPACITY
#define STICKMON_PHOTO_CAPACITY 5
#endif
inline constexpr unsigned WIDTH=192,THUMB_WIDTH=48,CAPACITY=STICKMON_PHOTO_CAPACITY;
static_assert(CAPACITY>=1&&CAPACITY<=32,"Photo capacity must be1..32");
inline constexpr size_t PIXELS=WIDTH*WIDTH,THUMB_PIXELS=THUMB_WIDTH*THUMB_WIDTH;
enum class Result:uint8_t {OK,FULL,UNAVAILABLE,FAILED,CORRUPT,NOT_FOUND,NO_MEMORY};
struct Entry {uint64_t id=0;uint32_t day=0;uint8_t area=0,slot=0;bool valid=false;};
class PixelBuffer {
public:
    ~PixelBuffer(){reset();}
    PixelBuffer()=default;
    PixelBuffer(const PixelBuffer&)=delete;
    PixelBuffer& operator=(const PixelBuffer&)=delete;
    bool allocate(size_t count){
        if(pixels_&&count==count_)return true;
        reset();if(!Platform::bound())return false;
        allocator_=&Platform::memory();pixels_=static_cast<uint16_t*>(allocator_->allocate(count*2,true));
        if(pixels_)count_=count;
        return pixels_!=nullptr;
    }
    void reset(){if(pixels_)allocator_->release(pixels_);pixels_=nullptr;count_=0;allocator_=nullptr;}
    uint16_t* data(){return pixels_;}
    const uint16_t* data()const{return pixels_;}
private:
    uint16_t* pixels_=nullptr;size_t count_=0;Platform::IMemoryAllocator* allocator_=nullptr;
};
class Library {
public:
    static constexpr size_t HEADER_BYTES=44,FILE_BYTES=HEADER_BYTES+(PIXELS+THUMB_PIXELS)*2;
    Library(Platform::IUserFiles* files=nullptr,Platform::IBlobStore* blobs=nullptr,Platform::IMemoryAllocator* memory=nullptr):files_(files),blobs_(blobs),memory_(memory){}
    Result refresh();
    unsigned count()const{return count_;}
    const Entry& entry(unsigned index)const{return entries_[index<CAPACITY?index:0];}
    Result save(const uint16_t* pixels,uint8_t area,uint32_t day,Platform::FileYield yield=nullptr);
    Result loadImage(const Entry&,uint16_t* output);
    Result loadThumbnail(const Entry&,uint16_t* output);
    Result erase(const Entry&);
    Result readFrameBinding(uint64_t& id);
    Result writeFrameBinding(uint64_t id);
    static void slotName(unsigned slot,char* out,size_t length);
private:
    Platform::IUserFiles* files_;Platform::IBlobStore* blobs_;Platform::IMemoryAllocator* memory_;
    Entry entries_[CAPACITY]{};unsigned count_=0;
    Platform::IBlobStore* blobStore();
    bool resolve();
    bool header(unsigned slot,Entry& entry,uint8_t* bytes);
    Result load(const Entry&,uint16_t*,bool thumbnail);
};
}
