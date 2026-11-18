#include "core/PhotoLibrary.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>

namespace Photos {
namespace {
uint16_t u16(const uint8_t*p){return uint16_t(p[0]|uint16_t(p[1])<<8);}
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint64_t u64(const uint8_t*p){return u32(p)|(uint64_t(u32(p+4))<<32);}
void put16(uint8_t*p,uint16_t v){p[0]=uint8_t(v);p[1]=uint8_t(v>>8);}
void put32(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=uint8_t(v>>(8*i));}
void put64(uint8_t*p,uint64_t v){put32(p,uint32_t(v));put32(p+4,uint32_t(v>>32));}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;while(n--){c^=*p++;for(int i=0;i<8;++i)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;}
constexpr uint32_t MAGIC=0x48504d53,SEQUENCE_MAGIC=0x51534d53;
}
void Library::slotName(unsigned slot,char*out,size_t n){std::snprintf(out,n,"slot%u.photo",slot);}
bool Library::resolve(){
    if(Platform::bound()){
        if(!files_)files_=Platform::services().userFiles;
        if(!blobs_)blobs_=&Platform::blobs();
        if(!memory_)memory_=&Platform::memory();
    }
    return files_&&blobs_&&memory_&&files_->begin();
}
bool Library::header(unsigned slot,Entry&e,uint8_t*h){
    e={};e.slot=uint8_t(slot);char name[24];slotName(slot,name,sizeof(name));
    if(files_->size(name)!=FILE_BYTES||!files_->read(name,0,h,HEADER_BYTES))return false;
    if(u32(h)!=MAGIC||u16(h+4)!=1||u16(h+6)!=HEADER_BYTES||!u64(h+8)||h[20]>=6||
       h[21]||h[22]||h[23]||u16(h+24)!=WIDTH||u16(h+26)!=WIDTH||u16(h+28)!=THUMB_WIDTH||u16(h+30)!=THUMB_WIDTH||crc(h,40)!=u32(h+40))return false;
    e.id=u64(h+8);e.day=u32(h+16);e.area=h[20];e.valid=true;return true;
}
Result Library::refresh(){
    count_=0;
    if(!resolve())return Result::UNAVAILABLE;
    for(unsigned slot=0;slot<CAPACITY;++slot){
        char name[24];slotName(slot,name,sizeof(name));if(!files_->exists(name))continue;
        uint8_t h[HEADER_BYTES];Entry e;header(slot,e,h);entries_[count_++]=e;
    }
    std::sort(entries_,entries_+count_,[](const Entry&a,const Entry&b){return a.valid!=b.valid?a.valid:a.id>b.id;});
    return Result::OK;
}
Result Library::save(const uint16_t*pixels,uint8_t area,uint32_t day,Platform::FileYield yield){
    if(!pixels||area>=6)return Result::FAILED;
    auto result=refresh();if(result!=Result::OK)return result;
    size_t available=0;
    if(!files_->queryAvailableBytes(available))return Result::UNAVAILABLE;
    if(count_>=CAPACITY||available<FILE_BYTES+4096)return Result::FULL;
    uint64_t sequence=0,maxId=0;
    for(unsigned i=0;i<count_;++i)maxId=std::max(maxId,entries_[i].id);
    uint8_t serial[16]{};size_t serialSize=0;
    const auto lookup=blobs_->lookupBlob("photos","sequence",serialSize);
    if(lookup==Platform::BlobLookup::FAILED)return Result::UNAVAILABLE;
    if(lookup==Platform::BlobLookup::FOUND){
        if(serialSize!=sizeof(serial)||!blobs_->readBlob("photos","sequence",serial,sizeof(serial))||u32(serial)!=SEQUENCE_MAGIC||crc(serial,12)!=u32(serial+12))return Result::CORRUPT;
        sequence=u64(serial+4);
    }else if(count_)return Result::CORRUPT;
    sequence=std::max(sequence,maxId);if(sequence==std::numeric_limits<uint64_t>::max())return Result::FULL;
    auto* bytes=static_cast<uint8_t*>(memory_->allocate(FILE_BYTES,true));if(!bytes)return Result::NO_MEMORY;
    std::memset(bytes,0,HEADER_BYTES);put32(bytes,MAGIC);put16(bytes+4,1);put16(bytes+6,HEADER_BYTES);
    put64(bytes+8,++sequence);put32(bytes+16,day);bytes[20]=area;
    put16(bytes+24,WIDTH);put16(bytes+26,WIDTH);put16(bytes+28,THUMB_WIDTH);put16(bytes+30,THUMB_WIDTH);
    for(size_t i=0;i<PIXELS;++i)put16(bytes+HEADER_BYTES+i*2,pixels[i]);
    auto* thumb=bytes+HEADER_BYTES+PIXELS*2;
    for(unsigned y=0;y<THUMB_WIDTH;++y)for(unsigned x=0;x<THUMB_WIDTH;++x)put16(thumb+(y*THUMB_WIDTH+x)*2,pixels[(y*4+2)*WIDTH+x*4+2]);
    put32(bytes+32,crc(bytes+HEADER_BYTES,PIXELS*2));put32(bytes+36,crc(thumb,THUMB_PIXELS*2));put32(bytes+40,crc(bytes,40));
    put32(serial,SEQUENCE_MAGIC);put64(serial+4,sequence);put32(serial+12,crc(serial,12));
    bool saved=blobs_->writeBlob("photos","sequence",serial,sizeof(serial));
    unsigned publishedSlot=CAPACITY;
    if(saved){
        for(unsigned slot=0;slot<CAPACITY;++slot){char name[24];slotName(slot,name,sizeof(name));if(!files_->exists(name)){saved=files_->writeNew(name,bytes,FILE_BYTES,yield);if(saved)publishedSlot=slot;break;}}
    }
    memory_->release(bytes);if(!saved||publishedSlot==CAPACITY)return Result::FAILED;
    // A durable publish is the commit boundary; a later scan must not turn it
    // into a failure that invites the player to save the same photograph again.
    entries_[count_++]={sequence,day,area,uint8_t(publishedSlot),true};
    std::sort(entries_,entries_+count_,[](const Entry&a,const Entry&b){return a.valid!=b.valid?a.valid:a.id>b.id;});
    return Result::OK;
}
Result Library::load(const Entry&e,uint16_t*out,bool thumbnail){
    if(!out||e.slot>=CAPACITY||!resolve())return Result::UNAVAILABLE;
    char name[24];slotName(e.slot,name,sizeof(name));if(!files_->exists(name))return Result::NOT_FOUND;
    uint8_t h[HEADER_BYTES];Entry actual;
    if(!header(e.slot,actual,h))return Result::CORRUPT;
    if(actual.id!=e.id)return Result::NOT_FOUND;
    const size_t n=thumbnail?THUMB_PIXELS:PIXELS,offset=HEADER_BYTES+(thumbnail?PIXELS*2:0);
    // Output owns exactly n uint16_t; decode its little-endian bytes in place.
    auto* raw=reinterpret_cast<uint8_t*>(out);
    if(!files_->read(name,offset,raw,n*2))return Result::FAILED;
    if(crc(raw,n*2)!=u32(h+(thumbnail?36:32)))return Result::CORRUPT;
    for(size_t i=0;i<n;++i)out[i]=u16(raw+i*2);
    return Result::OK;
}
Result Library::loadImage(const Entry&e,uint16_t*out){return load(e,out,false);}
Result Library::loadThumbnail(const Entry&e,uint16_t*out){return load(e,out,true);}
Result Library::erase(const Entry&e){
    if(e.slot>=CAPACITY||!resolve())return Result::UNAVAILABLE;
    char name[24];slotName(e.slot,name,sizeof(name));if(!files_->exists(name))return Result::NOT_FOUND;
    uint8_t h[HEADER_BYTES];Entry actual;header(e.slot,actual,h);
    if(actual.valid!=e.valid||(e.valid&&actual.id!=e.id))return Result::NOT_FOUND;
    if(!files_->remove(name))return Result::FAILED;
    // Removal is the durable boundary, just as publication is for save().
    for(unsigned i=0;i<count_;++i)if(entries_[i].slot==e.slot){
        for(unsigned j=i+1;j<count_;++j)entries_[j-1]=entries_[j];
        --count_;break;
    }
    return Result::OK;
}
}
