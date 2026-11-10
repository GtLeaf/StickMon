// SPDX-License-Identifier: Apache-2.0
#include "core/ResourceUpdate.h"
#include "core/ResourceBankFormat.h"
#include "../firmware/common/RecoveryText.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
using namespace ResourceUpdate;
struct Flash final:Backend {
    int calls=0,failAt=0;bool selected=false,bound=false,currentIntact=true;
    std::vector<std::string> events;
    bool op(const char* event) {events.emplace_back(event);return ++calls!=failAt;}
    bool currentValid() override {return op("current");}
    bool prepare(const Binding&) override {return op("prepare");}
    bool writeResource(uint32_t,const void*,size_t) override {return op("resource");}
    bool verifyResource(const Binding&) override {return op("verify-resource");}
    bool writeApp(uint32_t,const void*,size_t) override {return op("app");}
    bool verifyApp(const Binding&) override {return op("verify-app");}
    bool commitBinding(const Binding&) override {bound=op("binding");return bound;}
    bool selectBoot(uint8_t slot) override {assert(bound && slot==1);selected=op("select");return selected;}
    void cancel() override {events.emplace_back("cancel");}
};
Binding release() {Binding b;b.slot=1;b.sequence=42;b.appBytes=4099;return b;}
bool install(Flash& f) {
    Transaction t(f);auto b=release();if(!t.begin(b))return false;
    uint8_t bytes[4096]{};
    for(uint32_t at=0;at<BANK_SIZE;at+=4096) if(!t.chunk(at,bytes,sizeof(bytes)))return false;
    assert(t.state()==State::APP);
    if(!t.chunk(0,bytes,4096) || !t.chunk(4096,bytes,3))return false;
    assert(t.state()==State::READY && !f.selected);
    return t.commit();
}
int main() {
    size_t pixels=0;
    RecoveryText::draw(0,0,"STORAGE ERROR - Use USB installer",0xffff,[&](int x,int y,uint16_t){assert(x>=0 && y>=0);++pixels;});
    assert(pixels>100);
    uint32_t count=0;
    uint8_t header[16]={'S','M','F','I',1,0,196,0,1,0,0,0,0,0,0,0};
    assert(ResourceBankFormat::header(header,16,count) && count==1);
    header[12]=1;assert(!ResourceBankFormat::header(header,16,count));header[12]=0;
    for(unsigned i=0;i<16;++i)assert(!ResourceBankFormat::header(header,i,count));
    uint8_t record[196]{};ResourceBankFormat::Entry e;
    for(const auto* path:{"/absolute","../outside","a/../b","a\\b","","a b"}) {
        std::memset(record,0,sizeof(record));std::strcpy(reinterpret_cast<char*>(record),path);
        assert(!ResourceBankFormat::entry(record,sizeof(record),e));
    }
    std::memset(record,0,sizeof(record));std::strcpy(reinterpret_cast<char*>(record),"packs/dev/sprites/001.smonsp");
    assert(ResourceBankFormat::entry(record,sizeof(record),e));
    std::memset(record,'a',160);assert(!ResourceBankFormat::entry(record,sizeof(record),e));
    Binding b=release(),decoded;for(unsigned i=0;i<32;++i)b.appHash[i]=uint8_t(i);
    uint8_t raw[BINDING_BYTES];encodeBinding(b,raw);assert(decodeBinding(raw,sizeof(raw),decoded));
    assert(decoded.slot==1 && decoded.sequence==42 && decoded.appHash[31]==31);
    for(unsigned i=0;i<sizeof(raw);++i) {raw[i]^=1;assert(!decodeBinding(raw,sizeof(raw),decoded));raw[i]^=1;}
    for(unsigned n=0;n<sizeof(raw);++n)assert(!decodeBinding(raw,n,decoded));
    Flash good;assert(install(good));const auto calls=good.calls;
    assert(good.events.back()=="select");
    // Fail every flash/NVS/OTA operation, including every individual data chunk.
    for(int fault=1;fault<=calls;++fault) {
        Flash f;f.failAt=fault;assert(!install(f));assert(!f.selected && f.currentIntact);
        if(!f.bound) for(const auto& e:f.events)assert(e!="select");
    }
    uint8_t data[4097]{};
    for(unsigned mode=0;mode<5;++mode) {
        Flash f;Transaction t(f);assert(t.begin(release()));
        bool ok=mode==0?t.chunk(1,data,1):mode==1?t.chunk(0,data,4097):mode==2?t.chunk(0,nullptr,1):mode==3?t.chunk(0,data,0):t.commit();
        assert(!ok && !f.selected);
    }
    std::printf("PASS binding CRC/short records, %d interrupted update points, ordering and invalid chunks\n",calls);
}
