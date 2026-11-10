// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "core/ResourceUpdate.h"
namespace ResourceBankFormat {
constexpr size_t HEADER_BYTES=16,ENTRY_BYTES=196,PATH_BYTES=160;
inline bool safePath(const char* path) {
    if(!path || !*path || *path=='/' || std::strstr(path,"..") || std::strchr(path,'\\')) return false;
    for(const auto* p=path;*p;++p) if(uint8_t(*p)<33 || uint8_t(*p)>126) return false;
    return true;
}
inline bool header(const uint8_t* bytes,size_t length,uint32_t& count) {
    if(length!=HEADER_BYTES)return false;
    ContentData::Reader r{bytes,length};uint32_t magic=0,reserved=0;uint16_t schema=0,stride=0;
    r(magic,schema,stride,count,reserved);
    return r.good && magic==0x49464d53 && schema==1 && stride==ENTRY_BYTES && count && count<=4096 && !reserved;
}
struct Entry {char path[PATH_BYTES]{};uint32_t size=0;uint8_t hash[32]{};};
inline bool entry(const uint8_t* bytes,size_t length,Entry& out) {
    if(length!=ENTRY_BYTES)return false;
    ContentData::Reader r{bytes,length};r(out.path,out.size,out.hash);
    const auto* end=static_cast<const char*>(std::memchr(out.path,0,PATH_BYTES));
    if(!r.good || !end || !safePath(out.path) || out.size>ResourceUpdate::BANK_SIZE)return false;
    for(auto* p=end;p!=out.path+PATH_BYTES;++p) if(*p)return false;
    return true;
}
}
