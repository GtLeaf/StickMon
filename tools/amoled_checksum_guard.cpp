// Some native toolchains retain unused uzlib wrapper references. Resource packs
// use raw deflate; abort if a host probe unexpectedly enters a checksum wrapper.
#include <cstdio>
#include <cstdlib>
extern "C" unsigned uzlib_adler32(const void*,unsigned,unsigned){
    std::fputs("unexpected uzlib checksum path in AMOLED host\n",stderr);
    std::abort();
}
extern "C" unsigned uzlib_crc32(const void*,unsigned,unsigned){
    std::fputs("unexpected uzlib checksum path in AMOLED host\n",stderr);
    std::abort();
}
