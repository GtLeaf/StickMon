#include <cassert>
#include <cstring>
#include <cstdio>
#include <limits>
#include <map>
#include <string>
#include <vector>
#include "ui/round_touch/TouchCalibrationSession.h"
#include "TouchCalibrationStore.h"
#include "Cst816TouchState.h"

using Session = Ui::TouchCalibrationSession;
using Transform = Platform::TouchTransform;
struct Store : Platform::IBlobStore {
    bool fail = false;
    std::map<std::string, std::vector<uint8_t>> blobs;
    bool initialize() override { return true; }
    std::string key(const char* ns, const char* k) { return std::string(ns)+"/"+k; }
    size_t blobSize(const char* ns, const char* k) override { return blobs[key(ns,k)].size(); }
    bool readBlob(const char* ns, const char* k, void* p, size_t n) override {
        const auto& b = blobs[key(ns,k)];
        if (b.size() != n) return false;
        std::memcpy(p, b.data(), n); return true;
    }
    bool writeBlob(const char* ns, const char* k, const void* p, size_t n) override {
        if (fail) return false;
        const auto* b = static_cast<const uint8_t*>(p);
        blobs[key(ns,k)] = {b, b+n}; return true;
    }
    bool removeBlob(const char*, const char*) override { assert(false); return false; }
    bool clearNamespace(const char*) override { assert(false); return false; }
};
Session::Result hold(Session& s, int x, int y, uint32_t time = 1000) {
    s.down(x+9, y-9, time);
    // Settling samples are discarded; a short stable tail supplies the median.
    for (int i = 0; i < 7; ++i) s.move(x+(i%3)-1, y-(i%3)+1);
    return s.up(time+400);
}
void checkIdentity(const Transform& t) {
    assert(t.x(17,341) == 17 && t.y(17,341) == 341);
}
int main() {
    Session s; s.start();
    s.down(118,56,0); assert(s.up(20)==Session::Result::HOLD && s.count==0);
    s.down(118,56,0); s.cancelContact(); assert(s.up(500)==Session::Result::NONE);
    s.down(118,56,0);
    for (int i=0;i<5;++i) s.move(90+i*10,56);
    assert(s.up(500)==Session::Result::HOLD && s.count==0);
    for (int i=0;i<8;++i) {
        auto p=Ui::TouchTest::TARGETS[i];
        // Known distorted panel with shear, translation and unequal axis scale.
        const int x=int(std::lround(1.12*(p.x-180)+180+0.04*(p.y-180)+5));
        const int y=int(std::lround(1.09*(p.y-180)+180-0.03*(p.x-180)-3));
        assert(hold(s,x,y)==Session::Result::NEXT);
    }
    assert(s.count==8 && s.rms<1 && s.maximum<1);
    assert(hold(s,220,220)==Session::Result::CENTER_FAILED && s.count==8);
    assert(hold(s,185,177)==Session::Result::READY && s.centerError<1);
    const auto fitted=s.transform;
    assert(std::hypot(fitted.x(185,177)-180, fitted.y(185,177)-180)<1);
    // Independent interior targets were not used to fit or validate the model.
    for (int x:{80,140,220,280}) for (int y:{90,160,240,270}) {
        const float rx=1.12f*(x-180)+180+0.04f*(y-180)+5;
        const float ry=1.09f*(y-180)+180-0.03f*(x-180)-3;
        assert(std::hypot(fitted.x(rx,ry)-x, fitted.y(rx,ry)-y)<1);
    }
    s.start(); for(int i=0;i<8;++i) hold(s,180,180);
    assert(s.count==0 && s.result==Session::Result::FIT_FAILED);
    s.start(); for(int i=0;i<8;++i) {
        auto p=Ui::TouchTest::TARGETS[i]; hold(s,p.x+(i==3?60:0),p.y);
    }
    assert(s.count==0 && s.result==Session::Result::FIT_FAILED);
    // The manual retest has inconsistent left-side samples. Never silently
    // turn it into an accepted profile just because a least-squares fit exists.
    const int measured[8][2]={{109,47},{248,34},{332,121},{344,249},
                              {236,328},{117,346},{74,248},{81,120}};
    s.start(); for(const auto& p:measured) hold(s,p[0],p[1]);
    assert(s.result==Session::Result::FIT_FAILED);
    std::printf("PASS stable median sampling; affine fit and independent center/interior checks; bad/manual samples rejected (RMS %.2f MAX %.2f)\n",s.rms,s.maximum);

    using Saved=Sdgoods::TouchCalibrationStore;
    Store store; store.blobs["stickmon/state"]={1,2,3,4};
    checkIdentity(Saved::load(store));
    bool restoredRecord = true;
    Saved::load(store, &restoredRecord); assert(!restoredRecord);
    assert(Saved::save(store,fitted));
    const auto stored=store.blobs;
    auto restored=Saved::load(store);
    Saved::load(store, &restoredRecord); assert(restoredRecord);
    for(int i=0;i<6;++i) assert(restored.values[i]==fitted.values[i]);
    store.fail=true; assert(!Saved::save(store,{})); assert(store.blobs==stored);
    store.fail=false;
    for(int byte=0;byte<int(Saved::SIZE);++byte) {
        store.blobs=stored; store.blobs["touchcal/affine_v1"][byte]^=1;
        checkIdentity(Saved::load(store));
        Saved::load(store, &restoredRecord); assert(!restoredRecord);
    }
    store.blobs=stored; store.blobs["touchcal/affine_v1"].pop_back();
    checkIdentity(Saved::load(store));
    auto invalid=fitted; invalid.values[0]=std::numeric_limits<float>::quiet_NaN();
    assert(!Saved::save(store,invalid));
    invalid=fitted; invalid.values[2]=200; assert(!Saved::save(store,invalid));
    assert(Saved::save(store,{}));checkIdentity(Saved::load(store));
    Saved::load(store, &restoredRecord); assert(restoredRecord);
    assert(store.blobs["stickmon/state"]==std::vector<uint8_t>({1,2,3,4}));
    std::puts("PASS persistent round trip; failed writes, all record-byte corruptions, truncated/invalid records and identity restore; game blob untouched");

    Sdgoods::Cst816TouchState driver;
    Platform::IInputDevice::TouchEvent e;
    using Type=Platform::IInputDevice::TouchType;
    const auto sample=[&](int x,int y,uint32_t now) {
        const int rx=359-y;
        const uint8_t bytes[]{1,uint8_t(rx>>8),uint8_t(rx),uint8_t(x>>8),uint8_t(x)};
        driver.sample(bytes,now);
    };
    const uint8_t up[5]{};
    driver.setTransform(fitted);
    sample(185,177,100); assert(driver.poll(e) && e.type==Type::DOWN && e.x==180 && e.y==180);
    driver.sample(up,200);assert(driver.poll(e) && e.type==Type::UP && e.x==180 && e.y==180);
    sample(185,177,300);assert(driver.poll(e));
    driver.sample(nullptr,320);driver.bypass(true);
    assert(driver.poll(e) && e.type==Type::CANCEL);
    sample(185,177,340);assert(!driver.poll(e));
    driver.sample(up,360);assert(!driver.poll(e));
    sample(185,177,400);assert(driver.poll(e) && e.type==Type::DOWN && e.x==185 && e.y==177);
    driver.sample(up,420);assert(driver.poll(e) && e.type==Type::UP && e.x==185 && e.y==177);
    driver.bypass(false); sample(185,177,500);assert(driver.poll(e) && e.x==180 && e.y==180);
    const uint8_t invalidRaw[]{1,1,104,0,0};
    driver.sample(invalidRaw,520);assert(driver.poll(e) && e.type==Type::CANCEL);
    sample(185,177,540);assert(!driver.poll(e));
    driver.sample(up,560);assert(!driver.poll(e));
    Transform translated; translated.values[2]=-30;driver.setTransform(translated);
    sample(100,180,600);assert(driver.poll(e) && e.type==Type::DOWN && e.x==70);
    sample(0,180,620);assert(driver.poll(e) && e.type==Type::CANCEL);
    driver.sample(up,640);assert(!driver.poll(e));
    sample(0,180,660);assert(!driver.poll(e)); // No saturated tap on x=0.
    driver.sample(up,680);assert(!driver.poll(e));
    std::puts("PASS actual sensor rotation followed by correction; uncorrected bypass, release continuity and invalid/mode-change contact cancellation");
}
