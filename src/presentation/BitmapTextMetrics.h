#pragma once
#include <cstdint>

namespace BitmapText {
enum class Font : uint8_t { PX16, PX32, PX20 };
inline int height(Font font) { return font==Font::PX32?32:font==Font::PX20?20:16; }
inline bool usesUnscii(uint32_t cp) { return cp<0x80||cp==0x2640||cp==0x2642; }
inline int advance(uint32_t cp,Font font) { return height(font)/(usesUnscii(cp)?2:1); }

// Bounded UTF-8 decoding. An invalid byte consumes one byte and becomes '?'.
inline uint32_t decode(const char*& p,const char* end) {
    if(p==end)return 0;
    const auto* s=reinterpret_cast<const uint8_t*>(p);
    if(s[0]<0x80){++p;return s[0];}
    int n=0;uint32_t cp=0,min=0;
    if(s[0]>=0xc2&&s[0]<=0xdf){n=2;cp=s[0]&31;min=0x80;}
    else if(s[0]>=0xe0&&s[0]<=0xef){n=3;cp=s[0]&15;min=0x800;}
    else if(s[0]>=0xf0&&s[0]<=0xf4){n=4;cp=s[0]&7;min=0x10000;}
    if(n&&end-p>=n) {
        bool valid=true;
        for(int i=1;i<n;++i){if((s[i]&0xc0)!=0x80){valid=false;break;}cp=(cp<<6)|(s[i]&63);}
        if(valid&&cp>=min&&cp<=0x10ffff&&(cp<0xd800||cp>0xdfff)){p+=n;return cp;}
    }
    ++p;return '?';
}
inline int encode(uint32_t cp,char* out) {
    if(cp<0x80){out[0]=char(cp);return 1;}
    if(cp<0x800){out[0]=char(0xc0|(cp>>6));out[1]=char(0x80|(cp&63));return 2;}
    if(cp<0x10000){out[0]=char(0xe0|(cp>>12));out[1]=char(0x80|((cp>>6)&63));out[2]=char(0x80|(cp&63));return 3;}
    out[0]=char(0xf0|(cp>>18));out[1]=char(0x80|((cp>>12)&63));out[2]=char(0x80|((cp>>6)&63));out[3]=char(0x80|(cp&63));return 4;
}
}
