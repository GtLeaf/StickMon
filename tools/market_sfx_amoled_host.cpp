#include "AmoledApp.h"
#include "core/AudioManager.h"
#include "core/FontResource.h"
#include "game/MonsterFactory.h"
#include "platform/desktop/DesktopPlatform.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

class RecordingAudio final : public Platform::IAudioDevice {
public:
    explicit RecordingAudio(Platform::IAudioDevice& audio) : audio_(audio) {}
    void setVolume(uint8_t v) override { audio_.setVolume(v); }
    uint8_t volume() const override { return audio_.volume(); }
    bool playPcmU8(const uint8_t* d,size_t n,uint32_t r) override { return audio_.playPcmU8(d,n,r); }
    bool playPcmU8Channel(const uint8_t* d,size_t n,uint32_t r,uint8_t c,bool replace) override {
        if(c==AudioManager::SFX_CHANNEL) {
            uint64_t hash=1469598103934665603ULL;
            for(size_t i=0;i<n;++i) hash=(hash^d[i])*1099511628211ULL;
            sounds.push_back(hash);
        }
        return audio_.playPcmU8Channel(d,n,r,c,replace);
    }
    void setChannelVolume(uint8_t c,uint8_t v) override { audio_.setChannelVolume(c,v); }
    bool playing() const override { return audio_.playing(); }
    uint8_t queuedPcm(uint8_t c) const override { return audio_.queuedPcm(c); }
    void stop() override { audio_.stop(); }
    void stopChannel(uint8_t c) override { audio_.stopChannel(c); }
    bool beginMicrophone() override { return false; }
    void endMicrophone() override {}
    bool recordMicrophone(int16_t*,size_t,uint32_t) override { return false; }
    bool microphoneRecording() const override { return false; }
    bool microphoneActive() const override { return false; }
    std::vector<uint64_t> sounds;
private:
    Platform::IAudioDevice& audio_;
};
class MusicCounter final : public Platform::IResourceStore {
public:
    explicit MusicCounter(Platform::IResourceStore& r) : resource_(r) {}
    bool mount() override { return resource_.mount(); }
    size_t totalBytes() const override { return resource_.totalBytes(); }
    size_t usedBytes() const override { return resource_.usedBytes(); }
    Platform::ResourceFile open(const char* path) override {
        if(path && std::strstr(path,"bgm_market.smonaudio")) ++opens;
        return resource_.open(path);
    }
    unsigned opens=0;
private:
    Platform::IResourceStore& resource_;
};
namespace AmoledV1 {
struct AmoledAppTestAccess {
    static void run(AmoledApp& app,RecordingAudio& audio,MusicCounter& music) {
        using TouchEvent = Platform::IInputDevice::TouchEvent;
        using TouchEventType = Platform::IInputDevice::TouchType;
        auto& manager=AudioManager::ins();
        audio.setVolume(60);
        std::map<SfxCue,uint64_t> hashes;
        for(auto cue:{SfxCue::UI_CONFIRM,SfxCue::UI_CANCEL,SfxCue::BUY_ITEM,SfxCue::ITEM_GET,SfxCue::MOVE_LEARNT}) {
            assert(manager.playSfx(cue)); hashes[cue]=audio.sounds.back();
        }
        assert(hashes[SfxCue::BUY_ITEM]!=hashes[SfxCue::ITEM_GET]);
        audio.sounds.clear();
        const char* phase="shop";
        auto expect=[&](size_t before,SfxCue cue) {
            if(audio.sounds.size()!=before+1) std::fprintf(stderr,"%s: expected one sound after %zu, got %zu\n",phase,before,audio.sounds.size());
            assert(audio.sounds.size()==before+1);
            assert(audio.sounds.back()==hashes.at(cue));
        };
        auto quiet=[&](size_t before) {
            if(audio.sounds.size()!=before) std::fprintf(stderr,"%s: expected silence after %zu, got %zu\n",phase,before,audio.sounds.size());
            assert(audio.sounds.size()==before);
        };
        auto& s=app.gameState;
        auto reset=[&]() {
            app.closeMarketTutor(); app.saveWritesBlocked=false;
            s={}; s.oobeDone=true; s.teamCount=2; s.coins=20000;
            s.marketTutorGifted=1;
            s.team[0]=Game::MonsterFactory::create(25,20);
            s.team[1]=Game::MonsterFactory::create(133,20);
            s.team[0].ivPacked=1234; s.team[0].metAt=100; s.team[0].affection=100;
            s.team[1].ivPacked=5678; s.team[1].metAt=200; s.team[1].affection=100;
            s.team[0].move2Id=98; s.team[0].moveProficiency[1]=10;
            s.team[0].bond=30; s.team[1].bond=30;
            s.team[1].move2Id=129; s.team[1].moveProficiency[1]=10;
            for(auto& count:s.explorePoolRerollCounts) count=1;
            app.syncHomeActors(0);
        };
        reset(); uint32_t t=0,stamp=1000;
        assert(app.startMarket(t));
        auto advance=[&](int n) { while(n--) { t+=100; app.update(t); } };
        advance(40); assert(manager.playingMusic()==MusicTrack::MARKET);
        const auto opens=music.opens; assert(opens>0);
        auto tap=[&](auto handler,int x,int y) {
            TouchEvent e{};e.x=x;e.y=y;e.timestampMs=stamp;e.type=TouchEventType::DOWN;
            const auto before=audio.sounds.size(); (app.*handler)(e); quiet(before);
            e.type=TouchEventType::UP;e.timestampMs+=60;(app.*handler)(e);stamp+=200;
        };
        const auto shop=&AmoledApp::handleMarketShopTouch;
        const auto tutor=&AmoledApp::handleMarketTutorTouch;
        const auto care=&AmoledApp::handleMarketCareTouch;
        auto openShop=[&]() { app.openMarketShop(t);advance(4);tap(shop,184,380);advance(5); };
        auto openTutor=[&]() {
            s.marketTutor={};s.marketTutor.periodDay=Game::MarketTutor::periodDay(s);
            s.marketTutor.move=247;s.marketTutor.drawnSpecies=133;
            app.openMarketTutor(t);advance(4);tap(tutor,184,380);advance(5);
        };
        auto openCare=[&]() { app.openMarketCare(t);advance(4);tap(care,184,380);advance(5); };
        openShop();
        auto n=audio.sounds.size();tap(shop,184,252);expect(n,SfxCue::UI_CONFIRM);
        app.saveWritesBlocked=true;n=audio.sounds.size();tap(shop,272,414);expect(n,SfxCue::UI_CANCEL);
        assert(app.marketShopUi_.page()==Ui::MarketShopUi::Page::CONFIRM);
        app.saveWritesBlocked=false;s.coins=0;n=audio.sounds.size();tap(shop,272,414);expect(n,SfxCue::UI_CANCEL);
        s.coins=20000;
        const auto bag=s.bag;
        const auto item=Game::MarketShop::item(s,0);
        assert(Game::ItemInventory::add(s,item,uint8_t(99-Game::ItemInventory::count(s,item))));
        n=audio.sounds.size();tap(shop,272,414);expect(n,SfxCue::UI_CANCEL);
        s.bag=bag;n=audio.sounds.size();tap(shop,272,414);expect(n,SfxCue::BUY_ITEM);
        advance(8);quiet(n+1); // result updates never replay the reward
        tap(shop,184,414);
        n=audio.sounds.size();tap(shop,0,447);quiet(n); // blank area
        TouchEvent canceled{}; canceled.x=184; canceled.y=252; canceled.timestampMs=stamp;
        canceled.type=TouchEventType::DOWN;app.handleMarketShopTouch(canceled);
        canceled.type=TouchEventType::CANCEL;app.handleMarketShopTouch(canceled);
        canceled.type=TouchEventType::UP;app.handleMarketShopTouch(canceled);stamp+=200;quiet(n);
        n=audio.sounds.size();
        TouchEvent drag{};drag.x=184;drag.y=400;drag.timestampMs=stamp;drag.type=TouchEventType::DOWN;
        app.handleMarketShopTouch(drag);drag.type=TouchEventType::MOVE;drag.y=48;app.handleMarketShopTouch(drag);
        drag.type=TouchEventType::UP;drag.timestampMs+=100;app.handleMarketShopTouch(drag);stamp+=200;quiet(n);
        assert(app.marketShopUi_.scrollOffset()==352);
        tap(shop,184,404);n=audio.sounds.size();tap(shop,272,414);expect(n,SfxCue::ITEM_GET);
        advance(5);quiet(n+1);
        phase="gift";
        // Welcome is audible only for a persisted gift, including a later retry.
        reset();s.marketTutorGifted=0;s.bag.heartScale=0;app.saveWritesBlocked=true;
        n=audio.sounds.size();app.openMarketTutor(t);quiet(n);assert(!s.marketTutorGifted);
        app.closeMarketTutor();app.saveWritesBlocked=false;app.openMarketTutor(t);expect(n,SfxCue::ITEM_GET);
        app.closeMarketTutor();n=audio.sounds.size();app.openMarketTutor(t);quiet(n);
        app.closeMarketTutor();s.marketTutorGifted=0;s.bag.heartScale=99;app.openMarketTutor(t);quiet(n);
        phase="teach-natural";
        // Teaching: no sound on payment, pause while locked, one sound at natural completion.
        reset();openTutor();tap(tutor,184,250);tap(tutor,270,320);tap(tutor,184,294);
        n=audio.sounds.size();tap(tutor,272,414);quiet(n);
        assert(app.marketTutorUi_.page()==Ui::MarketTutorUi::Page::TEACHING);
        app.displayLocked=true;advance(70);quiet(n);
        assert(app.marketTutorUi_.page()==Ui::MarketTutorUi::Page::TEACHING);
        app.displayLocked=false;advance(57);expect(n,SfxCue::MOVE_LEARNT);advance(8);quiet(n+1);
        phase="teach-skip";
        // Skipping teaching finishes once; taps before the 400 ms threshold are quiet.
        reset();openTutor();tap(tutor,184,250);tap(tutor,270,320);tap(tutor,184,294);
        n=audio.sounds.size();tap(tutor,272,414);tap(tutor,184,280);quiet(n);advance(5);
        tap(tutor,184,280);expect(n,SfxCue::MOVE_LEARNT);advance(60);quiet(n+1);
        phase="recall";
        // Recall is immediate and uses the learned cue too.
        reset();s.bag.heartScale=1;openTutor();tap(tutor,184,318);tap(tutor,98,320);
        assert(app.marketTutorUi_.page()==Ui::MarketTutorUi::Page::RECALL_LIST);
        tap(tutor,184,244);tap(tutor,184,294);n=audio.sounds.size();tap(tutor,272,414);expect(n,SfxCue::MOVE_LEARNT);
        phase="train";
        for(bool skip:{false,true}) {
            reset();openTutor();tap(tutor,184,386);tap(tutor,98,320);
            n=audio.sounds.size();tap(tutor,272,414);quiet(n);
            assert(app.marketTutorUi_.page()==Ui::MarketTutorUi::Page::TRAINING);
            if(skip) {advance(5);tap(tutor,184,280);} else advance(49);
            expect(n,SfxCue::UI_CONFIRM);advance(10);quiet(n+1);
        }
        phase="care";
        for(int mode=0;mode<2;++mode) for(bool skip:{false,true}) {
            phase=mode ? "tea" : "massage";
            reset();openCare();tap(care,184,250+68*mode);tap(care,184,244);
            assert(app.marketCareUi_.page()==Ui::MarketCareUi::Page::CONFIRM);
            n=audio.sounds.size();tap(care,272,414);quiet(n);
            assert(app.marketCareUi_.page()==Ui::MarketCareUi::Page::SHOW);
            if(skip) {advance(4);tap(care,184,280);} else advance(17);
            expect(n,SfxCue::UI_CONFIRM);advance(20);quiet(n+1);
        }
        phase="save-failure";
        reset();openCare();tap(care,184,250);tap(care,184,244);
        app.saveWritesBlocked=true;n=audio.sounds.size();tap(care,272,414);expect(n,SfxCue::UI_CANCEL);
        reset();openTutor();tap(tutor,184,386);tap(tutor,98,320);
        app.saveWritesBlocked=true;n=audio.sounds.size();tap(tutor,272,414);expect(n,SfxCue::UI_CANCEL);
        reset();openShop();tap(shop,184,252);audio.setVolume(0);
        n=audio.sounds.size();tap(shop,272,414);advance(8);quiet(n);
        assert(music.opens==opens); // every transaction preserves the market stream
        assert(manager.requestedMusic()==MusicTrack::MARKET);
        std::puts("market-sfx: actual PCM cues, atomic errors/gifts, natural/skip completion, lock pause, quiet drags, mute and BGM continuity OK");
    }
};
}
int main(int argc,char** argv) {
    assert(argc==2);DesktopPlatform desktop(argv[1]);auto& base=desktop.serviceBundle();
    RecordingAudio audio(base.audio);MusicCounter resources(base.resources);
    Platform::Services services{base.lifecycle,base.clock,base.logger,base.input,base.display,audio,base.imu,base.power,base.blobs,resources,base.memory,base.peers};
    Platform::bind(services);assert(desktop.begin());assert(Platform::resources().mount());
    assert(GameAssets::begin());assert(FontResource::ins().begin());
    std::vector<uint16_t> pixels(368*448);auto& c=PixelRenderer::canvas();c.attach({pixels.data(),368,448,false});
    c.setCoordinateScale(1);c.setLayoutScale(1);c.setAssetScale(1);c.setNativeText(true);
    AmoledV1::AmoledApp app;app.begin(0);AmoledV1::AmoledAppTestAccess::run(app,audio,resources);
}
