#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
namespace Stickmon {
template<class Read> bool fullyErased(size_t size,Read read){
    if(!size)return false;
    uint8_t bytes[1024];
    for(size_t offset=0;offset<size;){
        const auto length=std::min(sizeof(bytes),size-offset);
        if(!read(offset,bytes,length))return false;
        for(size_t i=0;i<length;++i)if(bytes[i]!=0xff)return false;
        offset+=length;
    }
    return true;
}
}
