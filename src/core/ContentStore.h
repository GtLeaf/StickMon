// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "game/ContentData.h"
#include "core/ResourceFS.h"
#include "platform/api/PlatformServices.h"
#include <cstdio>
#include <new>

// Loaded once before entering the game. Views and strings live until shutdown.
class ContentStore final : public ContentData::Source {
    struct Entry { uint16_t id=0; uint8_t* raw=nullptr; void* data=nullptr;
                   size_t count=0,itemSize=0; };
    Entry entries_[96]{};
    bool failed_=false;
    size_t allocated_=0;
public:
    ~ContentStore() override {
        for(auto& e:entries_) { if(e.raw) Platform::memory().release(e.raw);
                               if(e.data) Platform::memory().release(e.data); }
    }
    bool healthy() const { return !failed_; }
    const void* table(uint16_t id,size_t itemSize,size_t maxCount,
                      ContentData::Decode decode,size_t& count) override {
        count=0;
        for(const auto& e:entries_) if(e.id==id) {
            if(e.itemSize!=itemSize || e.count>maxCount) { failed_=true; return nullptr; }
            count=e.count; return e.data;
        }
        char path[80]; std::snprintf(path,sizeof(path),"/packs/dev/catalog/%u.smondata",unsigned(id));
        auto file=ResourceFS::ins().open(path);
        const size_t size=file ? file.size() : 0;
        if(size<16 || size>1024*1024 || !itemSize || itemSize>128*1024) { failed_=true; return nullptr; }
        auto* raw=static_cast<uint8_t*>(Platform::memory().allocate(size,true));
        if(!raw) { failed_=true; return nullptr; }
        std::memset(raw,0,size);
        bool ok=file.read(raw,size)==size;
        ContentData::Reader r{raw,size};
        uint32_t magic=0, n=0, payload=0; uint16_t schema=0,key=0;
        r(magic,schema,key,n,payload);
        ok=ok && magic==0x44434d53 && schema==1 && key==id && n && n<=maxCount &&
           payload==size-16 && n<=1024*1024/itemSize && allocated_+size+n*itemSize<=2*1024*1024;
        void* data=ok ? Platform::memory().allocate(n*itemSize,true) : nullptr;
        if(data) std::memset(data,0,n*itemSize);
        ok=ok && data && decode(r,data,n);
        if(ok) for(auto& e:entries_) if(!e.id) {
            e={id,raw,data,n,itemSize}; allocated_+=size+n*itemSize; count=n; return data;
        }
        Platform::memory().release(raw);
        if(data) Platform::memory().release(data);
        failed_=true; return nullptr;
    }
};
