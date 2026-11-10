// SPDX-License-Identifier: Apache-2.0
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "platform/desktop/DesktopPlatform.h"
#include "game/MonsterFactory.h"
#include "core/AudioManager.h"
#include "presentation/FrameDamage.h"
#include "presentation/PixelRenderer.h"
#include "ui/common/MarketView.h"
#define private public
#include "core/GameEngine.h"
#include "scenes/ExploreScene.h"
#undef private

class Display final : public Platform::IDisplayDevice {
public:
    explicit Display(Platform::IDisplayDevice& base) : base(base), panel(360*360) {}
    Platform::FrameBuffer565 frameBuffer() override { return base.frameBuffer(); }
    void present() override { ++full; copy(0,0,360,360); }
    void presentRegion(int x,int y,int w,int h) override {
        assert(x==0 && y==106 && w==360 && h==140);
        ++partial; copy(x,y,w,h);
    }
    void copy(int x,int y,int w,int h) {
        const auto fb=frameBuffer();
        for(int row=y;row<y+h;++row)
            std::copy_n(fb.pixels+row*360+x,w,panel.data()+row*360+x);
        assert(std::equal(panel.begin(),panel.end(),fb.pixels));
        bytes+=w*h*2;
    }
    void setBrightness(uint8_t v) override { base.setBrightness(v); }
    uint8_t brightness() const override { return base.brightness(); }
    void sleep() override { base.sleep(); }
    Platform::IDisplayDevice& base;
    std::vector<uint16_t> panel;
    unsigned full=0,partial=0,bytes=0;
};
class Memory final : public Platform::IMemoryAllocator {
public:
    explicit Memory(Platform::IMemoryAllocator& base):base(base) {}
    void* allocate(size_t n,bool ext) override {
        if(n==360*140*2 && fail) return nullptr;
        return base.allocate(n,ext);
    }
    void release(void* p) override { base.release(p); }
    size_t externalFree() const override { return base.externalFree(); }
    Platform::IMemoryAllocator& base;
    bool fail=false;
};
int main(int argc,char** argv) {
    assert(argc==2);
    DesktopPlatform desktop(argv[1]);
    auto& d=desktop.serviceBundle();Display display(d.display);Memory memory(d.memory);
    Platform::Services services{d.lifecycle,d.clock,d.logger,d.input,display,d.audio,d.imu,
        d.power,d.blobs,d.resources,memory,d.peers};
    Platform::bind(services);
    auto& engine=GameEngine::ins();assert(engine.begin());
    auto& state=engine.gameState();state={};state.oobeDone=true;state.teamCount=1;
    state.settings.idleTimeoutIndex=4;state.team[0]=Game::MonsterFactory::create(25,10);
    PokemonSprites::setDynamicLoadingEnabled(true);
    engine.requestScene(SceneID::EXPLORE,false);
    auto& scene=*static_cast<ExploreScene*>(engine.currentScene.get());
    const auto tick=[&](unsigned ms) {
        const auto end=desktop.millis()+ms;
        while(desktop.millis()<end) {desktop.advanceMs(1);engine.run();}
    };
    // Select the same persistent route with or without the Debug-only market.
    assert(scene.areaPreviewPool.count>1);
    scene.ui.state.areaCursor=ExploreDestinations::visibleIndex(0,state);scene.loadAreaPreview();
    for (unsigned waited=0; !scene.areaPreviewReady && waited<3000; waited+=20) {
        for (auto sprite : scene.areaPreviewFrames) assert(!sprite);
        tick(20);
    }
    tick(6000);
    assert(scene.areaPreviewReady && !scene.areaPreviewPaused && scene.areaPreviewPool.count>1);
    using Preview = Ui::RoundExplore::PreviewLayout;
    const auto loaded = scene.exploreRouteFrame();
    std::printf("current grass pool members=%u width=%d static_width=%d\n", loaded.areaPreviewCount,
        Preview::stripWidth(loaded.areaPreviewFrames, loaded.areaPreviewCount), Preview::STATIC_WIDTH);
    // Pool membership rotates with the seed. Use two actual loaded members
    // to exercise a fitting group independently of today's active pool size.
    scene.areaPreviewPool.count = 2;
    const auto grass = scene.exploreRouteFrame();
    assert(!scene.areaPreviewScrolls());
    // A settled multi-member grass preview must park across complete cycles.
    assert(!scene.areaPreviewLoadPending);
    engine.invalidateScene();tick(100);
    const auto grassFrames = display.full + display.partial;
    tick(6000);
    assert(display.full + display.partial == grassFrames);
    auto result = scene.update(desktop.millis(), .033f);
    assert(!result.redraw && result.nextUpdateDelayMs == SceneUpdateResult::NO_UPDATE);
    // Boundary is measured from actual sprite widths, including the gap.
    PokemonSprites::SpriteFrame boundary = *grass.areaPreviewFrames[0];
    boundary.width = 150;
    const PokemonSprites::SpriteFrame* pair[]{&boundary, &boundary};
    assert(Preview::stripWidth(pair, 2) == 312 && !Preview::scrolls(pair, 2));
    boundary.width = 151;
    assert(Preview::scrolls(pair, 2));
    // Compare actual partial draws to a fresh full render, including toast,
    // buttons, rare marker, missing sprites, single/empty and byte swapping.
    auto frame=scene.exploreRouteFrame();
    const auto fb=desktop.frameBuffer();
    std::vector<uint16_t> actual(360*360),expected(360*360);
    for(bool swapped : {false,true}) {
        Ui::RoundExplore::Selection selection,reference;
        for(int i=0;i<90;++i) {
            frame.nowMs=frame.areaPreviewStartedAt+2200+i*33;
            frame.toast=i>=20 && i<40 ? "Test" : nullptr;
            frame.areaPreviewHasRare=i>=45;
            selection.pressed=reference.pressed=i>=50 && i<55 ? Ui::RoundExplore::NEXT : -1;
            if(i==12) {
                frame.areaPreviewCount=Ui::ExploreRouteFrame::PREVIEW_CAP;
                for(int slot=2;slot<frame.areaPreviewCount;++slot)
                    frame.areaPreviewFrames[slot]=frame.areaPreviewFrames[slot%2];
            }
            if(i==60)frame.areaPreviewFrames[0]=nullptr;
            if(i==70)frame.areaPreviewCount=1;
            if(i==80)frame.areaPreviewCount=0;
            PixelRenderer::bind({expected.data(),360,360,swapped});
            FrameDamage::allowed=false;reference.render(frame);
            PixelRenderer::bind({actual.data(),360,360,swapped});
            FrameDamage::begin(i!=10 && i!=11);selection.render(frame);
            if(i>0 && i<10)assert(FrameDamage::partial);
            assert(actual==expected);
        }
        frame=scene.exploreRouteFrame();
    }
    PixelRenderer::bind(fb);FrameDamage::allowed=false;
    memory.fail=true;
    {
        Ui::RoundExplore::Selection fallback;
        FrameDamage::begin(true);fallback.render(frame);
        assert(!fallback.previewBackground && !FrameDamage::partial);
        const auto first=std::vector<uint16_t>(fb.pixels,fb.pixels+360*360);
        FrameDamage::begin(true);fallback.render(frame);
        assert(std::equal(first.begin(),first.end(),fb.pixels));
    }
    memory.fail=false;
    // Force a wider-than-page strip to preserve the real-engine motion checks.
    scene.areaPreviewPool.count = Ui::ExploreRouteFrame::PREVIEW_CAP;
    for (int i=0; i<scene.areaPreviewPool.count; ++i)
        scene.areaPreviewFrames[i] = grass.areaPreviewFrames[i % grass.areaPreviewCount];
    assert(scene.areaPreviewScrolls());
    // Exercise the real engine, rather than only the scene's requested delay.
    engine.fullFrameRequired=true;engine.invalidateScene();
    scene.areaPreviewStartedAt=desktop.millis()-2301;
    scene.areaPreviewPaused=false;scene.areaPreviewLoadPending=false;
    scene.ui.state.areaPreviewVisualCycle=0;
    engine.scheduleSceneUpdate(desktop.millis());
    const auto initialFull=display.full,initialPartial=display.partial;
    unsigned coreTicks=0,lastCore=engine.lastUpdateMs;
    const auto end=desktop.millis()+500;
    while(desktop.millis()<end) {
        desktop.advanceMs(1);engine.run();
        if(lastCore!=engine.lastUpdateMs) {lastCore=engine.lastUpdateMs;++coreTicks;}
    }
    const auto frames=display.full+display.partial-initialFull-initialPartial;
    std::printf("movement frames=%u partial=%u core_ticks=%u\n",frames,display.partial-initialPartial,coreTicks);
    assert(frames>=14 && frames<=16 && display.partial-initialPartial>=12);
    assert(coreTicks<=8);
    tick(100);
    const auto heldFrames=display.full+display.partial;
    tick(500);
    assert(display.full+display.partial==heldFrames);
    // No decoder runs during motion; the load is rescheduled to the hold.
    scene.areaPreviewLoadPending=true;scene.areaPreviewNextLoadAt=desktop.millis();
    scene.areaPreviewStartedAt=desktop.millis()-2500;
    const auto reloads=PokemonSprites::cacheStats().reloadCount;
    assert(!scene.updateAreaPreviewLoading(desktop.millis()));
    assert(PokemonSprites::cacheStats().reloadCount==reloads);
    assert(scene.areaPreviewNextLoadAt==desktop.millis()+300);
    scene.areaPreviewLoadPending=false;
    // Single-member and idle views park; wake resumes from a stable hold.
    scene.areaPreviewPool.count=1;
    scene.ui.state.areaPreviewVisualCycle=0;
    result=scene.update(desktop.millis(),.033f);
    assert(!result.redraw && result.nextUpdateDelayMs==SceneUpdateResult::NO_UPDATE);
    scene.areaPreviewPool.count=Ui::ExploreRouteFrame::PREVIEW_CAP;
    engine.idleActive=true;
    result=scene.update(desktop.millis(),.033f);
    assert(scene.areaPreviewPaused);
    tick(1000);const auto idleFrames=display.full+display.partial;tick(1000);
    assert(display.full+display.partial==idleFrames);
    engine.wakeFromIdle();tick(100);
    assert(!scene.areaPreviewPaused);
    assert((desktop.millis()-scene.areaPreviewStartedAt)%2800<2300);
    const auto beforeFade=display.full;
    assert(engine.fadeToScene(SceneID::MAIN,120));tick(1200);
    assert(engine.currentId==SceneID::MAIN && display.full>beforeFade);
    std::puts("PASS static grass multi-member parking, measured width boundary, partial/full pixel parity, damage transfer, allocation fallback, 30 FPS overflow cadence, unchanged core cadence, loading exclusion, single-member parking and idle wake");
}
