#pragma once
#include "ui/round_touch/ExploreSelectUi.h"

struct CardHost final:Ui::ExploreHost {
    Ui::ExploreRouteFrame frame;Ui::ExploreUiState state;int changes=0,departures=0,backs=0;
    CardHost(){frame.visibleAreaCount=std::min<int>(7,Ui::ExploreRouteFrame::AREA_CAP);for(int i=0;i<frame.visibleAreaCount;++i){frame.areaIds[i]=i;frame.areaUnlocked[i]=true;}frame.areaPreviewReady=true;}
    Ui::ExploreInputFrame exploreInputFrame()const override{Ui::ExploreInputFrame f;f.phase=Ui::ExplorePhase::SELECT;f.visibleAreaCount=frame.visibleAreaCount;return f;}
    Ui::ExploreRouteFrame exploreRouteFrame()const override{auto f=frame;f.areaCursor=state.areaCursor;f.marketPreview=f.areaIds[state.areaCursor]==ExploreDestinations::MARKET;return f;}
    Ui::ExploreBattleFrame exploreBattleFrame()const override{return {};}
    Ui::ExploreTutorialFrame exploreTutorialFrame()const override{return {};}
    bool activateExplore(Ui::ExploreAction a,uint8_t=0)override{if(a==Ui::ExploreAction::AREA_CHANGED)++changes;if(a==Ui::ExploreAction::DEPART)++departures;if(a==Ui::ExploreAction::AREA_BACK)++backs;return true;}
    bool handleExploreProgressionButton(const ButtonEvent&)override{return false;}
    void renderExploreProgression()override{}
};
struct CardDriver {
    CardHost host;Ui::RoundExplore::Selection ui;uint32_t now=1000;
    void event(Type type,int x,int y,int dt=40){now+=dt;ui.touch(host,host.state,{type,int16_t(x),int16_t(y),now});}
    void tap(int x,int y){event(Type::DOWN,x,y);event(Type::UP,x,y);}
    void swipe(int a,int b,int ya=200,int yb=200){event(Type::DOWN,a,ya);event(Type::MOVE,b,yb);event(Type::UP,b,yb);}
};
static void cardContract(Ui::RoundExplore::Selection& ui,const Ui::ExploreRouteFrame& f,const std::string& out,const char* name){
    auto& c=PixelRenderer::canvas();const auto old=Platform::display().frameBuffer();std::vector<uint16_t> pixels(360*360),expected(360*360);
    const bool visible=ui.previewVisible();const auto page=ui.previewPage();
    for(bool swapped:{false,true}){
        c.attach({pixels.data(),360,360,swapped});c.setCoordinateScale(1);c.setLayoutScale(1);c.setAssetScale(1);c.setNativeText(false);c.clearClipRect();c.fillSprite(0x31a5);ui.render(f);
        for(int y=0;y<360;++y)for(int x=0;x<360;++x){if(!swapped)expected[y*360+x]=c.readPixel(x,y);else assert(expected[y*360+x]==c.readPixel(x,y));}
        if(!swapped)capture(out,name);
        c.fillSprite(0x31a5);for(int y=0;y<360;y+=11){const int h=std::min(11,360-y);c.setClipRect(0,y,360,h);ui.render(f);assert(c.clipTop()==y&&c.clipBottom()==y+h);}
        c.clearClipRect();for(int y=0;y<360;++y)for(int x=0;x<360;++x)assert(c.readPixel(x,y)==expected[y*360+x]);
        assert(ui.previewVisible()==visible&&ui.previewPage()==page);
    }c.attach(old);c.setCoordinateScale(1);c.setLayoutScale(1);c.setAssetScale(1);
}
static void exploreCardTests(DesktopPlatform& desktop,GameEngine& engine,const std::string& out){
    using namespace Ui::RoundExplore;assert(CARD_BOUNDS.x+CARD_BOUNDS.width/2==180&&CARD_BOUNDS.y+CARD_BOUNDS.height/2==180);
    assert(PREVIOUS_BOUNDS.x+PREVIOUS_BOUNDS.width<CARD_BOUNDS.x&&NEXT_BOUNDS.x>CARD_BOUNDS.x+CARD_BOUNDS.width);
    for(const auto r:{CARD_BOUNDS,PREVIOUS_BOUNDS,NEXT_BOUNDS,DEPART_BOUNDS,TOGGLE_BOUNDS})for(const int x:{r.x,r.x+r.width-1})for(const int y:{r.y,r.y+r.height-1})assert((x-180)*(x-180)+(y-180)*(y-180)<180*180);
    CardDriver d;const char* names[]={Ui::Explore::GRASS_PATH,Ui::Explore::CREEK_SLOPE,Ui::Explore::TALL_GRASS_PARK,Ui::Explore::FROST_CRYSTAL_CAVE,Ui::Explore::MIST_FOREST_PATH,Ui::Explore::ANCIENT_WATERFALL_VALLEY,Ui::MarketText::NAME};
    for(int i=0;i<d.host.frame.visibleAreaCount;++i)d.host.frame.areaNames[i]=names[i];d.host.frame.areaIds[6]=ExploreDestinations::MARKET;d.host.frame.marketOpen=true;
    // All shipped FRONT canvases fit the native1x viewport. Group every sample
    // with real dimensions; do not silently shrink the larger species.
    int species = 0;
    for (size_t i = 0; i < speciesCount(); ++i) {
        const auto& entry = speciesTable()[i];
        if (entry.assetsPending) {
#if STICKMON_ENABLE_DEBUG_FEATURES
            const uint16_t active = engine.activeMonster().speciesId;
            assert(!engine.debugSetActiveSpecies(entry.id));
            assert(engine.activeMonster().speciesId == active);
#endif
            continue;
        }
        PokemonSprites::preloadDynamicSpecies(&entry.id, 1, 1);
        const auto* front = PokemonSprites::findSpeciesSprite(entry.id, PokemonSprites::SpriteKind::FRONT);
        if (!front || front->width > PET_BOUNDS.width || front->height > PET_BOUNDS.height)
            std::fprintf(stderr, "FRONT id%u %ux%u ready=%u\n",
                         entry.id, front ? front->width : 0, front ? front->height : 0, unsigned(front != nullptr));
        assert(front && front->width <= PET_BOUNDS.width && front->height <= PET_BOUNDS.height);
        ++species;
    }
    assert(species > 0);
    for(const uint16_t id:{113,115,352}){
        PokemonSprites::preloadDynamicSpecies(&id,1,1);
        const auto* f=PokemonSprites::findSpeciesSprite(id,PokemonSprites::SpriteKind::FRONT);
        assert(f&&f->width<=PET_BOUNDS.width&&f->height<=PET_BOUNDS.height);++species;
    }
    const uint16_t ids[]={25,133,130,143,6,3};d.host.frame.areaPreviewCount=6;PokemonSprites::setPinnedDynamicSpecies(ids,6);PokemonSprites::preloadDynamicSpecies(ids,6,6);
    for(int i=0;i<6;++i){d.host.frame.areaPreviewFrames[i]=PokemonSprites::findSpeciesSprite(ids[i],PokemonSprites::SpriteKind::FRONT);assert(d.host.frame.areaPreviewFrames[i]);}
    const auto pages=previewPages(d.host.exploreRouteFrame());assert(pages.count>1);
    for(int p=0;p<pages.count;++p){int width=0;for(int i=pages.starts[p];i<pages.ends[p];++i)width+=PreviewLayout::width(d.host.frame.areaPreviewFrames[i])+(i>pages.starts[p]?PreviewLayout::SPACING:0);assert(width<=PET_BOUNDS.width);}
    // Every scene uses the same centered geometry and existing real cover.
    for(int scene=0;scene<7;++scene){d.host.state.areaCursor=scene;char file[40];std::snprintf(file,sizeof(file),"scene-%d-hidden",scene);cardContract(d.ui,d.host.exploreRouteFrame(),out,file);}
    d.host.state.areaCursor=0;d.event(Type::UP,266,118);assert(!d.ui.previewVisible());d.tap(266,118);assert(d.ui.previewVisible());cardContract(d.ui,d.host.exploreRouteFrame(),out,"scene-0-pokemon");
    d.tap(328,180);assert(d.host.state.areaCursor==0&&d.host.changes==0);d.swipe(280,100);assert(d.ui.previewPage()==1&&d.host.state.areaCursor==0&&d.host.changes==0);cardContract(d.ui,d.host.exploreRouteFrame(),out,"scene-0-pokemon-page2");
    d.swipe(100,280);assert(d.ui.previewPage()==0);d.swipe(280,210,150,250);assert(d.ui.previewPage()==0&&d.host.state.areaCursor==0);
    d.event(Type::DOWN,266,118);d.ui.cancel();d.event(Type::UP,266,118);assert(d.ui.previewVisible());
    d.event(Type::DOWN,266,118);d.event(Type::MOVE,210,118);d.event(Type::UP,266,118);assert(d.ui.previewVisible());
    // A loading pool cannot reinterpret a creature swipe as a scene swipe.
    d.host.frame.areaPreviewReady=false;d.swipe(280,100);assert(d.host.state.areaCursor==0&&d.ui.previewVisible());cardContract(d.ui,d.host.exploreRouteFrame(),out,"scene-loading");d.host.frame.areaPreviewReady=true;
    d.tap(266,118);assert(!d.ui.previewVisible());d.tap(328,180);assert(d.host.state.areaCursor==1&&d.host.changes==1&&!d.ui.previewVisible());d.swipe(280,100);assert(d.host.state.areaCursor==2);
    // Locked scene keeps art and requirements, but both controls are inert.
    d.host.frame.areaUnlocked[2]=false;d.tap(266,118);d.tap(180,324);assert(!d.ui.previewVisible()&&d.host.departures==0);cardContract(d.ui,d.host.exploreRouteFrame(),out,"scene-locked");
    d.host.frame.areaUnlocked[2]=true;d.tap(266,118);d.host.frame.areaPreviewHidden[0]=true;cardContract(d.ui,d.host.exploreRouteFrame(),out,"scene-silhouette");
    // Rendering a hidden species changes only sprite pixels inside the viewport.
    auto& c=PixelRenderer::canvas();d.ui.render(d.host.exploreRouteFrame());std::vector<uint16_t> hidden(360*360);for(int y=0;y<360;++y)for(int x=0;x<360;++x)hidden[y*360+x]=c.readPixel(x,y);
    d.host.frame.areaPreviewHidden[0]=false;d.ui.render(d.host.exploreRouteFrame());int black=0;
    for(int y=0;y<360;++y)for(int x=0;x<360;++x)if(hidden[y*360+x]!=c.readPixel(x,y)){assert(PET_BOUNDS.contains(x,y)&&hidden[y*360+x]==0);++black;}assert(black>0);
    // Changed pools clamp the displayed page and remain safely navigable.
    d.swipe(280,100);d.host.frame.areaPreviewCount=1;cardContract(d.ui,d.host.exploreRouteFrame(),out,"scene-single-pokemon");d.swipe(280,100);assert(d.host.state.areaCursor==2);d.ui.release();assert(!d.ui.previewVisible()&&d.ui.previewPage()==0);
    d.host.state.areaCursor=6;d.host.frame.areaPreviewCount=4;d.tap(266,118);assert(d.ui.previewVisible());cardContract(d.ui,d.host.exploreRouteFrame(),out,"scene-market-generic-fixture");
    // Actual scene exposes IDs/readiness and no longer requests moving-strip33ms.
    engine.switchScene(SceneID::EXPLORE,false);auto& real=*static_cast<ExploreScene*>(engine.currentScene.get());
    auto f=real.exploreRouteFrame();for(int i=0;i<f.visibleAreaCount;++i)assert(f.areaIds[i]==ExploreDestinations::visibleId(i,engine.gameState()));
    real.ui.state.areaCursor=ExploreDestinations::visibleIndex(ExploreDestinations::MARKET,engine.gameState());
    real.activateExplore(Ui::ExploreAction::AREA_CHANGED,real.ui.state.areaCursor);
    for(int i=0;i<20&&!real.areaPreviewReady;++i){desktop.advanceMs(600);real.update(desktop.millis(),.6f);}
    f=real.exploreRouteFrame();assert(f.areaPreviewReady&&f.areaPreviewCount==MarketMapData::get().residentCount);
    for(int i=0;i<f.areaPreviewCount;++i)assert(f.areaPreviewFrames[i]&&real.areaPreviewSpeciesIds[i]==MarketMapData::get().residents[i].species);
    assert(real.ui.selection.tap(real,real.ui.state,TOGGLE_PREVIEW));
    cardContract(real.ui.selection,f,out,"scene-market-residents-real");
    // Physical navigation must reset preview before the cursor can wrap home.
    assert(real.ui.selection.previewVisible());
    assert(real.ui.onButton({1,BtnAction::PRESSED}));
    assert(!real.ui.selection.previewVisible()&&real.ui.selection.previewPage()==0);
    for(int i=1;i<f.visibleAreaCount;++i)assert(real.ui.onButton({1,BtnAction::PRESSED}));
    assert(!real.ui.selection.previewVisible());
    for(uint32_t t=0;t<10000;t+=33)assert(real.visualFrameIntervalMs(t)==66);
    real.onExit();std::printf("explore-cards:7covers/side arrows/center, modes/gestures/guards, native%dFRONTs/pages, hidden/loading/clip/swap/static cadence OK\n",species);(void)desktop;
}
