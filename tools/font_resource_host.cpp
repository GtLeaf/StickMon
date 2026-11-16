#include "core/FontResource.h"
#include "platform/desktop/DesktopPlatform.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

static uint32_t le32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24;
}
int main(int argc, char** argv) {
    assert(argc == 5);
    DesktopPlatform platform(argv[1]); Platform::bind(platform.serviceBundle());
    assert(platform.begin());
    auto& font=FontResource::ins();
    const std::string selected=argv[3];
    const bool wanted=std::string(argv[4])=="1";
    for (const char* name : {"zh16","ascii16-unscii","zh32","ascii32","round-zh16-a4","round-zh20-a4"}) {
        if (selected!="all" && selected!=name) continue;
        std::ifstream input(std::string(argv[2])+"/"+name+".smonfont",std::ios::binary);
        std::vector<uint8_t> raw{std::istreambuf_iterator<char>(input),{}};
        assert(raw.size()>=16);
        const unsigned count=raw[6] | unsigned(raw[7])<<8;
        const unsigned stride=raw[10];
        for (unsigned i=0;i<count;++i) {
            const auto* record=raw.data()+16+i*(4+stride);
            const auto cp=le32(record); const uint8_t* bits=nullptr;
            if (std::string(name).find("round-")==0) bits=font.findCoverageGlyph(cp,raw[9]);
            else {
                const auto face=std::string(name).find("ascii")==0 ? FontFace::UNSCII_ASCII : FontFace::SARASA_CJK;
                bits=raw[9]==32 ? font.findLargeGlyphBitmap(cp,face) : font.findGlyphBitmap(cp,face);
            }
            assert(bool(bits)==wanted);
            if (wanted) assert(std::memcmp(bits,record+4,stride)==0);
        }
    }
}
