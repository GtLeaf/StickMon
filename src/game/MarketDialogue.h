#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace Market::Dialogue {
enum class Context : uint8_t { ANY, RESIDENT, SOLO, PAIR };
enum Role : uint8_t { SELF, TARGET, OTHER, PARTNER };

// A non-owning, validated view of the versioned dialogue resource. No platform
// dependencies, allocations or simulation random numbers are used here.
class Catalog {
    const uint8_t* data_=nullptr;
    const uint8_t* strings_=nullptr;
    uint16_t profiles_=0,scripts_=0;
    uint32_t stringBytes_=0;
    static uint16_t u16(const uint8_t* p) {return uint16_t(p[0]|uint16_t(p[1])<<8);}
    static uint32_t u32(const uint8_t* p) {return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
    const uint8_t* record(int i) const {return data_+20+profiles_*8+i*32;}
    const char* string(uint32_t offset) const {
        if(offset>=stringBytes_)return nullptr;
        const auto* text=reinterpret_cast<const char*>(strings_+offset);
        const auto* end=static_cast<const char*>(std::memchr(text,0,stringBytes_-offset));
        return end&&end>text&&end-text<=192?text:nullptr;
    }
    static bool validUtf8(const char* text) {
        const auto* p=reinterpret_cast<const uint8_t*>(text);
        while(*p) {
            if(*p<0x80){if(*p<32||*p==127)return false;++p;continue;}
            const int n=*p>=0xc2&&*p<=0xdf?2:*p>=0xe0&&*p<=0xef?3:*p>=0xf0&&*p<=0xf4?4:0;
            if(!n)return false;
            for(int i=1;i<n;++i)if(!p[i]||(p[i]&0xc0)!=0x80)return false;
            if((n==3&&((p[0]==0xe0&&p[1]<0xa0)||(p[0]==0xed&&p[1]>=0xa0)))||
               (n==4&&((p[0]==0xf0&&p[1]<0x90)||(p[0]==0xf4&&p[1]>=0x90))))return false;
            p+=n;
        }
        return true;
    }
public:
    static constexpr size_t MAX_BYTES=20+128*8+64*32+16384;
    bool load(const void* bytes,size_t size) {
        *this={};
        if(!bytes||size<20||size>MAX_BYTES)return false;
        const auto* p=static_cast<const uint8_t*>(bytes);
        if(std::memcmp(p,"SMDL",4)||u16(p+4)!=1||u16(p+10))return false;
        const auto profiles=u16(p+6),scripts=u16(p+8);const auto strings=u32(p+12);
        if(profiles>128||!scripts||scripts>64||strings>16384||size!=20+profiles*8+scripts*32+strings)return false;
        uint32_t crc=~0U;
        for(size_t i=20;i<size;++i){crc^=p[i];for(int b=0;b<8;++b)crc=(crc>>1)^(0xedb88320U&uint32_t(-int32_t(crc&1)));}
        if((crc^~0U)!=u32(p+16))return false;
        data_=p;profiles_=profiles;scripts_=scripts;stringBytes_=strings;strings_=p+20+profiles*8+scripts*32;
        uint16_t previous=0;unsigned fallbacks=0;
        for(int i=0;i<profiles_;++i) {
            const auto* r=p+20+i*8;const auto* name=string(u32(r+4));
            if(u16(r)<=previous||u16(r+2)>63||!name||std::strlen(name)>48||!validUtf8(name)){*this={};return false;}
            previous=u16(r);
        }
        for(int i=0;i<scripts_;++i) {
            const auto* r=record(i);
            if(!u16(r)||r[2]>3||!r[3]||r[3]>4||u16(r+4)>63||!u16(r+8)||u16(r+8)>100||u16(r+10)){
                *this={};return false;
            }
            for(int j=0;j<i;++j)if(id(j)==u16(r)){*this={};return false;}
            if(!u16(r+4)&&!u16(r+6))fallbacks|=1U<<r[2];
            for(int j=0;j<r[3];++j) {
                const auto* text=string(u32(r+13+j*5));
                if(r[12+j*5]>3||(r[12+j*5]==OTHER&&r[2]!=3)||!text||!validUtf8(text)){*this={};return false;}
            }
        }
        if((fallbacks&14)!=14){*this={};return false;}
        return true;
    }
    int count() const {return scripts_;}
    uint16_t id(int i) const {return i>=0&&i<scripts_?u16(record(i)):0;}
    int lines(int i) const {return i>=0&&i<scripts_?record(i)[3]:0;}
    Role role(int i,int line) const {return Role(record(i)[12+line*5]);}
    const char* text(int i,int line) const {return string(u32(record(i)+13+line*5));}
    uint16_t traits(uint16_t species) const {
        for(int i=0;i<profiles_;++i)if(u16(data_+20+i*8)==species)return u16(data_+22+i*8);
        return 0;
    }
    const char* name(uint16_t species) const {
        for(int i=0;i<profiles_;++i)if(u16(data_+20+i*8)==species)return string(u32(data_+24+i*8));
        return nullptr;
    }
    int choose(Context context,uint16_t species,bool partner,uint32_t& random,const uint16_t* recent,int recentCount) const {
        int candidates[64],count=0;uint32_t total=0;
        for(int pass=0;pass<3&&!count;++pass)for(int i=0;i<scripts_;++i) {
            const auto* r=record(i);const auto filter=u16(r+6),tags=u16(r+4);
            if((r[2]&&r[2]!=uint8_t(context))||(filter&&filter!=species)||(tags&&!(traits(species)&tags)))continue;
            bool eligible=true;
            for(int j=0;j<lines(i);++j)if(role(i,j)==PARTNER&&!partner)eligible=false;
            if(!pass)for(int j=0;j<recentCount;++j)if(recent[j]==id(i))eligible=false;
            if(pass==1&&recentCount&&recent[0]==id(i))eligible=false;
            if(eligible){candidates[count++]=i;total+=u16(r+8);}
        }
        if(!count)return -1;
        random^=random<<13;random^=random>>17;random^=random<<5;
        auto pick=random%total;
        for(int j=0;j<count;++j){const auto weight=u16(record(candidates[j])+8);if(pick<weight)return candidates[j];pick-=weight;}
        return candidates[0];
    }
};
} // namespace Market::Dialogue
