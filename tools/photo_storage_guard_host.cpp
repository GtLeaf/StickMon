#include <cassert>
#include <cstdio>
#if __has_include("platform/common/BlankStorageGuard.h")
#include "platform/common/BlankStorageGuard.h"
#include <algorithm>
#include <cstring>
#include <vector>
int main(){
    std::vector<uint8_t> flash(10003,255);size_t readBytes=0;bool fail=false;
    auto read=[&](size_t offset,void*out,size_t n){if(fail)return false;assert(offset+n<=flash.size());std::memcpy(out,flash.data()+offset,n);readBytes+=n;return true;};
    assert(Stickmon::fullyErased(flash.size(),read)&&readBytes==flash.size());
    flash.back()=0;assert(!Stickmon::fullyErased(flash.size(),read));
    flash.back()=255;fail=true;assert(!Stickmon::fullyErased(flash.size(),read));
    assert(!Stickmon::fullyErased(0,read));
    std::puts("photo storage guard: whole partition blank proof, last-byte data and read failure prohibit formatting OK");
}
#else
int main(){assert(false&&"Photo storage lacks a non-destructive blank partition guard");}
#endif
