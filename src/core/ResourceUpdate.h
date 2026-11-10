// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "game/ContentData.h"

namespace ResourceUpdate {
// The 32 MiB SDGOODS target has two OTA application slots and one shared
// LittleFS resource partition.  Resources are installed together with the
// firmware; they are deliberately not duplicated per OTA slot.
#ifndef STICKMON_SDGOODS_FULL_STORAGE
#define STICKMON_SDGOODS_FULL_STORAGE 0
#endif
constexpr uint32_t APP_SIZE=0x300000;
constexpr uint32_t BANK_SIZE=STICKMON_SDGOODS_FULL_STORAGE ? 0x1800000 : 0x900000;
constexpr uint16_t ABI=1, SCHEMA=1;
constexpr size_t BINDING_BYTES=120;
inline uint32_t crc32(const uint8_t* p,size_t n) {
    uint32_t crc=~0U;
    while(n--) { crc^=*p++; for(unsigned i=0;i<8;++i) crc=(crc>>1)^(0xedb88320U & (0U-(crc&1))); }
    return ~crc;
}
struct Binding {
    uint32_t sequence=0, appBytes=0, resourceBytes=BANK_SIZE;
    uint8_t slot=0;
    uint8_t appHash[32]{}, resourceHash[32]{}, indexHash[32]{};
};
inline bool decodeBinding(const uint8_t* bytes,size_t size,Binding& out) {
    if(size!=BINDING_BYTES) return false;
    ContentData::Reader r{bytes,size};
    uint32_t magic=0,crc=0; uint16_t schema=0; uint8_t state=0;
    r(magic,schema,out.slot,state,out.sequence,out.appBytes,out.resourceBytes,
      out.appHash,out.resourceHash,out.indexHash,crc);
    return r.good && magic==0x42504d53 && schema==SCHEMA && out.slot<2 && state==1 &&
           out.sequence && out.appBytes && out.appBytes<=APP_SIZE &&
           out.resourceBytes==BANK_SIZE && crc==crc32(bytes,size-4);
}
inline void encodeBinding(const Binding& in,uint8_t (&bytes)[BINDING_BYTES]) {
    struct Buffer {uint8_t* bytes;size_t n=0;} b{bytes};
    ContentData::Writer w{&b,[](void* p,uint8_t v){auto& b=*static_cast<Buffer*>(p);b.bytes[b.n++]=v;}};
    Binding copy=in; uint32_t magic=0x42504d53; uint16_t schema=SCHEMA;uint8_t state=1;
    w(magic,schema,copy.slot,state,copy.sequence,copy.appBytes,copy.resourceBytes,copy.appHash,copy.resourceHash,copy.indexHash);
    uint32_t crc=crc32(bytes,BINDING_BYTES-4);w(crc);
}
// Device backends own flash/NVS/OTA. The commit order is shared and fault-testable.
class Backend {
public:
    virtual ~Backend()=default;
    virtual bool currentValid()=0;
    virtual bool prepare(const Binding&)=0;
    virtual bool writeResource(uint32_t,const void*,size_t)=0;
    virtual bool verifyResource(const Binding&)=0;
    virtual bool writeApp(uint32_t,const void*,size_t)=0;
    virtual bool verifyApp(const Binding&)=0;
    virtual bool commitBinding(const Binding&)=0;
    virtual bool selectBoot(uint8_t)=0;
    virtual void cancel()=0;
};
enum class State {IDLE,RESOURCE,APP,READY,COMMITTED,FAILED};
class Transaction {
    Backend& backend_; Binding binding_{};State state_=State::IDLE;uint32_t written_=0;
    bool fail() { backend_.cancel();state_=State::FAILED;return false; }
public:
    explicit Transaction(Backend& backend):backend_(backend) {}
    ~Transaction() { if(state_!=State::IDLE && state_!=State::COMMITTED) backend_.cancel(); }
    State state() const {return state_;}
    bool begin(const Binding& b) {
        if(state_!=State::IDLE || b.slot>1 || !b.sequence || !b.appBytes || b.appBytes>APP_SIZE || b.resourceBytes!=BANK_SIZE) return false;
        if(!backend_.currentValid() || !backend_.prepare(b)) return fail();
        binding_=b;written_=0;state_=State::RESOURCE;return true;
    }
    bool chunk(uint32_t offset,const void* data,size_t size) {
        const uint32_t total=state_==State::RESOURCE ? binding_.resourceBytes : binding_.appBytes;
        if((state_!=State::RESOURCE && state_!=State::APP) || !data || !size || size>4096 ||
           offset!=written_ || offset>total || size>total-offset) return fail();
        const bool ok=state_==State::RESOURCE ? backend_.writeResource(offset,data,size) : backend_.writeApp(offset,data,size);
        if(!ok) return fail();
        written_+=uint32_t(size);
        if(written_==total) {
            if(state_==State::RESOURCE) {
                if(!backend_.verifyResource(binding_)) return fail();
                state_=State::APP;written_=0;
            } else {
                if(!backend_.verifyApp(binding_)) return fail();
                state_=State::READY;
            }
        }
        return true;
    }
    bool commit() {
        if(state_!=State::READY) return false;
        if(!backend_.commitBinding(binding_) || !backend_.selectBoot(binding_.slot)) return fail();
        state_=State::COMMITTED;return true;
    }
};
} // namespace ResourceUpdate
