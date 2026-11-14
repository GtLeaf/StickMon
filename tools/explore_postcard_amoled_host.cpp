#include "AmoledApp.h"
#include "ui/UiCommon.h"
#include "ui/ExplorePostcardLayout.h"
#if STICKMON_EXPLORE_POSTCARD
#include "ui/ExplorePostcardArt.h"
#endif
#include "game/ExploreDestinations.h"
#include "core/FontResource.h"
#include "platform/desktop/DesktopPlatform.h"
#include "game/MonsterFactory.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
extern "C" unsigned uzlib_adler32(const void*,unsigned,unsigned p){return p;}
extern "C" unsigned uzlib_crc32(const void*,unsigned,unsigned p){return p;}
static std::string output;
static void shot(const char* name){auto* f=std::fopen((output+"/"+name+".ppm").c_str(),"wb");assert(f);std::fprintf(f,"P6\n368 448\n255\n");for(int y=0;y<448;y++)for(int x=0;x<368;x++){auto p=PixelRenderer::canvas().readPixel(x,y);unsigned char rgb[]={uint8_t(((p>>11)&31)*255/31),uint8_t(((p>>5)&63)*255/63),uint8_t((p&31)*255/31)};std::fwrite(rgb,1,3,f);}std::fclose(f);}
namespace AmoledV1 {
struct AmoledAppTestAccess {
static void run(AmoledApp& app){
    auto& s=app.gameState;s={};s.oobeDone=true;s.teamCount=1;s.team[0]=Game::MonsterFactory::create(25,20);s.team[0].satiety=100;s.settings.idleTimeoutIndex=4;
    app.sceneFlow.enter(AppSceneFlow::Scene::EXPLORE_AREAS);uint32_t t=1000;
    uint8_t grass=0;for(uint8_t i=0;i<ExploreDestinations::visibleCount(s);i++)if(ExploreDestinations::visibleId(i,s)==0)grass=i;
    app.selectExploreDestination(grass,t);
    auto update=[&](){for(int i=0;i<100;i++){t+=50;app.update(t);}};
    auto render=[&](const char* name){app.forceFullRender();app.render(PixelRenderer::canvas());shot(name);};
    auto tap=[&](int x,int y){TouchEvent e{};e.x=x;e.y=y;e.type=TouchEventType::DOWN;e.timestampMs=++t;app.handleTouch(e);e.type=TouchEventType::UP;e.timestampMs=t+=60;app.handleTouch(e);};
    update();render("01-grass");
#if STICKMON_EXPLORE_POSTCARD
    assert(exploreStartAt(300,412)&&exploreSelectionBackAt(30,25));
    // Exact shared header pixels, including the Back target and title anchor.
    auto& c=PixelRenderer::canvas();std::vector<uint16_t> header(368*76);for(int y=0;y<76;y++)for(int x=0;x<368;x++)header[y*368+x]=c.readPixel(x,y);
    UiCommon::drawPageHeader(c,"探险");for(int y=0;y<76;y++)for(int x=0;x<368;x++)if(c.readPixel(x,y))assert(header[y*368+x]==c.readPixel(x,y));
    // Production full versus dirty-row composition must be identical.
    ExploreViewModel m;PixelCache565 cache;m.previewPool=app.explorePreviewPool;
    for(int i=0;i<ExplorePool::POOL_CAP;i++){m.previewFrames[i]=app.explorePreviewFrames[i];m.previewHidden[i]=app.explorePreviewHidden[i];}
    m.visibleAreaCount=6;m.unlockedArea=3;m.selectedArea=3;
    renderExploreScreen(c,m,cache);std::vector<uint16_t> full(368*448);for(int y=0;y<448;y++)for(int x=0;x<368;x++)full[y*368+x]=c.readPixel(x,y);
    c.fillRect(0,0,368,448,0);for(int y=0;y<448;y+=37)renderExploreScreen(c,m,cache,y,std::min(448,y+37));
    for(int y=0;y<448;y++)for(int x=0;x<368;x++)assert(full[y*368+x]==c.readPixel(x,y));
    for(int id=0;id<7;id++){m.destinationMenu=true;m.visibleAreaCount=7;m.selectedArea=id;m.marketSelected=id==6;for(int j=0;j<7;j++){m.destinationIds[j]=j;m.destinationUnlocked[j]=j<4||j==6;}renderExploreScreen(c,m,cache);char name[30];std::snprintf(name,sizeof(name),"area-%d",id);shot(name);}
    m.destinationMenu=true;m.visibleAreaCount=7;m.selectedArea=6;m.marketSelected=true;
    m.destinationIds[6]=6;m.destinationUnlocked[6]=false;m.marketDaysUntilOpen=3;
    renderExploreScreen(c,m,cache);shot("market-closed");
    assert(c.readPixel(184,430)==PixelRenderer::rgb(39,53,59));
    // Tap right arrow selects a real visible destination; left returns.
    tap(330,155);assert(app.selectedExploreListIndex()==grass+1);update();render("02-next");
    tap(30,155);assert(app.selectedExploreListIndex()==grass);update();
    // Page boundaries use actual native widths; gestures cannot select maps
    // or trigger departure while captured by the lower scene region.
    const auto before=s;const auto pool=app.explorePreviewPool;
    const auto savedOffset=app.explorePostcardPoolOffset_;
    const PokemonSprites::SpriteFrame* oldFrames[ExplorePool::POOL_CAP];
    for(int i=0;i<ExplorePool::POOL_CAP;i++)oldFrames[i]=app.explorePreviewFrames[i];
    PokemonSprites::SpriteFrame fixture[3]{};int widths[]={168,138,96};
    app.explorePreviewPool.count=3;
    for(int i=0;i<3;i++){fixture[i].width=widths[i];fixture[i].height=96;app.explorePreviewFrames[i]=&fixture[i];}
    app.explorePostcardPoolOffset_=0;
    tap(184,300);assert(app.explorePostcardPoolOffset_==2);
    auto send=[&](TouchEventType type,int x,int y){TouchEvent e{};e.x=x;e.y=y;e.type=type;e.timestampMs=t+=40;app.handleTouch(e);};
    auto swipe=[&](int x1,int y1,int x2,int y2){send(TouchEventType::DOWN,x1,y1);send(TouchEventType::MOVE,x2,y2);send(TouchEventType::UP,x2,y2);};
    swipe(280,300,80,300);assert(app.explorePostcardPoolOffset_==0&&app.selectedExploreListIndex()==grass);
    swipe(80,300,280,300);assert(app.explorePostcardPoolOffset_==2&&app.selectedExploreListIndex()==grass);
    swipe(184,300,184,412);assert(app.explorePostcardPoolOffset_==2&&!app.pendingExpedition);
    send(TouchEventType::DOWN,280,300);send(TouchEventType::MOVE,80,300);app.cancelTouch();send(TouchEventType::UP,80,300);
    assert(app.explorePostcardPoolOffset_==2&&!app.pendingExpedition);
    tap(184,300);assert(app.explorePostcardPoolOffset_==0);
    app.explorePostcardClock_.pause();app.markRendered();
    app.update(t+=100);assert(app.explorePostcardPoolOffset_==0);
    app.update(t+=2999);assert(app.explorePostcardPoolOffset_==0);
    app.update(++t);assert(app.explorePostcardPoolOffset_==2&&app.renderRowBegin()==218&&app.renderRowEnd()==372);
    tap(184,300);assert(app.explorePostcardPoolOffset_==0);app.markRendered();
    app.update(t+=4999);assert(app.explorePostcardPoolOffset_==0);
    app.update(++t);assert(app.explorePostcardPoolOffset_==2);
    app.setDisplayLocked(true);app.update(t+=10000);assert(app.explorePostcardPoolOffset_==2);
    app.setDisplayLocked(false);app.update(++t);assert(app.explorePostcardPoolOffset_==2);
    app.update(t+=3000);assert(app.explorePostcardPoolOffset_==0);
    app.explorePreviewPool=pool;app.explorePostcardPoolOffset_=savedOffset;
    for(int i=0;i<ExplorePool::POOL_CAP;i++)app.explorePreviewFrames[i]=oldFrames[i];
    assert(s.coins==before.coins&&s.gameMinutesTotal==before.gameMinutesTotal);
    // A predominantly vertical drag must not select the next destination.
    swipe(280,100,200,350);assert(app.selectedExploreListIndex()==grass);
    // Upper scene swipe selects destination and includes the locked footer.
    swipe(300,180,100,180);assert(app.selectedExploreListIndex()==grass+1);
    app.selectExploreDestination(grass,t);update();
    app.markRendered();for(int i=0;i<5;i++){t+=33;app.update(t);assert(!app.needsRender());}
    tap(30,30);assert(app.sceneFlow.current()==AppSceneFlow::Scene::MAIN_MENU);
    app.sceneFlow.enter(AppSceneFlow::Scene::EXPLORE_AREAS);app.selectExploreDestination(grass,t);update();
    tap(300,412);assert(app.pendingExpedition&&app.sceneFlow.current()==AppSceneFlow::Scene::HOME);
    app.cancelExploreDeparture();
    // Native foreground pixels must match direct FRONT drawing, including
    // the two tallest species and an undiscovered silhouette.
    const uint16_t largeIds[]={6,130,75};
    PokemonSprites::setDynamicSceneSpecies(largeIds,3);PokemonSprites::preloadDynamicSpecies(largeIds,3);
    auto rawPixel=[&](int area,int x,int y){const auto& art=ExplorePostcardArt::scenes[area];int row=y/2,col=x/2,current=0;
        for(int j=art.rows[row];j<art.rows[row+1];j+=2){current+=art.runs[j];if(col<current)return art.palette[art.runs[j+1]];}assert(false);return uint16_t(0);};
    m={};m.visibleAreaCount=6;m.unlockedArea=5;m.selectedArea=5;
    for(int pair=0;pair<2;pair++){
        m.previewPool.count=pair?1:2;
        for(int i=0;i<m.previewPool.count;i++){int id=pair?75:largeIds[i];m.previewPool.entries[i].speciesId=id;m.previewFrames[i]=PokemonSprites::findSpeciesSprite(id,PokemonSprites::SpriteKind::FRONT);assert(m.previewFrames[i]);m.previewHidden[i]=pair;}
        renderExploreScreen(c,m,cache);shot(pair?"native-wide-hidden":"native-tall");
        std::vector<uint16_t> rendered(368*448);for(int y=0;y<448;y++)for(int x=0;x<368;x++)rendered[y*368+x]=c.readPixel(x,y);
        const int total=pair?168:268;int x=(368-total)/2;
        c.fillRect(0,0,368,448,0xF81F);
        for(int i=0;i<m.previewPool.count;i++){
            auto* frame=m.previewFrames[i];int top=350-frame->height;
            if(pair)PokemonSprites::drawFrameSilhouette(frame,x,top,PixelRenderer::rgb(126,145,145));
            else PokemonSprites::drawFrame(frame,x,top);
            int inkPixels=0;for(int y=top;y<350;y++)for(int col=x;col<x+frame->width;col++)if(c.readPixel(col,y)!=0xF81F){assert(rendered[y*368+col]==c.readPixel(col,y));++inkPixels;}
            assert(inkPixels>100);x+=frame->width+12;
        }
    }
    // An old Lv label or unmasked side seam changes these actual pixels.
    for(int area : {0,3,5}){
        m={};m.visibleAreaCount=6;m.unlockedArea=5;m.selectedArea=area;renderExploreScreen(c,m,cache);
        for(int y=134;y<150;y++)for(int x=28;x<100;x++)assert(c.readPixel(x,y)==rawPixel(area,x,y));
        for(int y=230;y<372;y++)for(int x : {0,367})assert(c.readPixel(x,y)!=rawPixel(area,x,y));
    }

    // Native byte ordering and single-row dirty updates must preserve every pixel.
    m.previewPool.count=2;for(int i=0;i<2;i++){m.previewFrames[i]=PokemonSprites::findSpeciesSprite(largeIds[i],PokemonSprites::SpriteKind::FRONT);m.previewHidden[i]=false;}
    renderExploreScreen(c,m,cache);std::vector<uint16_t> expected(368*448);
    for(int y=0;y<448;y++)for(int x=0;x<368;x++)expected[y*368+x]=c.readPixel(x,y);
    auto* original=c.rawPixels();const bool originalOrder=c.byteSwapped();std::vector<uint16_t> swapped(368*448);
    c.attach({swapped.data(),368,448,true});renderExploreScreen(c,m,cache);
    for(int y=0;y<448;y++)for(int x=0;x<368;x++)assert(expected[y*368+x]==c.readPixel(x,y));
    c.fillRect(0,0,368,448,0);for(int y=0;y<448;y++)renderExploreScreen(c,m,cache,y,y+1);
    for(int y=0;y<448;y++)for(int x=0;x<368;x++)assert(expected[y*368+x]==c.readPixel(x,y));
    c.attach({original,368,448,originalOrder});
#else
    assert(exploreStartAt(98,412)&&exploreSelectionBackAt(270,412));
    assert(!exploreStartAt(300,412)&&!exploreSelectionBackAt(30,25));
    tap(270,412);assert(app.sceneFlow.current()==AppSceneFlow::Scene::MAIN_MENU);
#endif
    std::puts("explore-postcard: shared header, real navigation/Back, dirty rows and reversible legacy geometry OK");
}};
}
int main(int argc,char**argv){assert(argc==3);output=argv[2];DesktopPlatform d(argv[1]);Platform::bind(d.serviceBundle());d.begin();assert(Platform::resources().mount()&&GameAssets::begin());FontResource::ins().begin();std::vector<uint16_t> pixels(368*448);auto& c=PixelRenderer::canvas();c.attach({pixels.data(),368,448,false});c.setCoordinateScale(1);c.setLayoutScale(1);c.setAssetScale(1);c.setNativeText(true);AmoledV1::AmoledApp app;app.begin(0);AmoledV1::AmoledAppTestAccess::run(app);}
