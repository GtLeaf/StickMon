#include "wire.h"
#include "HomeScreen.h"
#include "core/FontResource.h"
#include "game/MonsterFactory.h"
#include "game/ExploreRouteGeometry.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/PixelRenderer.h"
#include "presentation/Canvas565.h"
#include "ui/RenderCaches.h"
#include "ui/ExploreMapRenderer.h"
#include "game/ExploreEditorTiles.h"
#include "game/ExploreLakeTiles.h"
#include "game/ExploreRotatedTiles.h"
#include "game/ExploreAreaCatalog.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cassert>

void verifyObjectOcclusion(Canvas565& canvas) {
    using namespace AmoledV1;
    ExploreMapGenerator::Map map{};
    ExploreRouteViewModel model{};model.map=&map;
    struct Check { uint16_t id; uint8_t depth; uint8_t frames; };
    std::vector<Check> checks;
    for (uint16_t n=0;n<ExploreEditorTiles::COUNT;++n)
        checks.push_back({static_cast<uint16_t>(ExploreEditorTiles::BASE+n),ExploreEditorTiles::TILES[n].depth,ExploreEditorTiles::TILES[n].frames});
    for (uint16_t n=0;n<ExploreRotatedTiles::COUNT;++n)
        checks.push_back({static_cast<uint16_t>(ExploreRotatedTiles::BASE+n),ExploreRotatedTiles::TILES[n].depth,1});
    for (const auto& check : checks) {
        map.layers[1][17]=check.id;
        const float ground=(1+check.depth)*26.0f;
        for (uint8_t frame=0;frame<check.frames;++frame) {
        model.mapFrame=frame;
        canvas.fillSprite(0x07ff);
        drawExploreRouteMapVegetationForActor(canvas,model,ground-.1f,52,52,104,104,0,448);
        int behind=0;
        for(int y=52;y<104;++y)for(int x=52;x<104;++x)behind+=canvas.readPixel(x,y)!=0x07ff;
        assert((behind>0)==(check.depth>0));
        canvas.fillSprite(0x07ff);
        drawExploreRouteMapVegetationForActor(canvas,model,ground+.1f,52,52,104,104,0,448);
        for(int y=52;y<104;++y)for(int x=52;x<104;++x)assert(canvas.readPixel(x,y)==0x07ff);
        }
    }
}

void verifyCachedObjectAnimation(Canvas565& canvas) {
    using namespace AmoledV1;
    std::vector<uint16_t> animated;
    for (uint16_t n=0;n<ExploreEditorTiles::COUNT;++n)
        if (ExploreEditorTiles::TILES[n].frames>1) animated.push_back(ExploreEditorTiles::BASE+n);
    for (uint16_t n=0;n<ExploreLakeTiles::COUNT;++n) animated.push_back(ExploreLakeTiles::BASE+n);
    for (uint16_t id : animated) {
        assert(GameAssets::isExploreTileAnimated(id));
        // Compare a warm production viewport cache against a clean composition.
        // The second case checks that a static foreground stays over the balloon.
        for (bool foreground : {false,true}) {
            ExploreMapGenerator::Map map{};
            std::fill_n(map.layers[0],ExploreMapGenerator::CELL_COUNT,385);
            map.layers[ExploreLakeTiles::contains(id) ? 0 : 1][3*16+4]=id;
            if (foreground) map.layers[2][3*16+4]=11022;
            ExploreRouteViewModel model{};model.map=&map;
            RenderCaches cache;
            std::vector<uint16_t> expectedPixels(52*52);
            Canvas565 expected;
            expected.attach({expectedPixels.data(),52,52,false});
            expected.setCoordinateScale(1);expected.setLayoutScale(1);expected.setAssetScale(1);
            for (uint8_t frame=0;frame<8;++frame) {
                model.mapFrame=frame;
                canvas.fillSprite(0x07ff);
                assert(drawExploreRouteMapLayer(canvas,model,cache.exploreWorld,0,448));
                expected.fillSprite(ExploreAreaCatalog::fieldColor(0));
                for (int z=0;z<3;++z) {
                    uint16_t tile=map.layers[z][3*16+4];
                    if(tile) assert(GameAssets::drawExploreTileTo(expected,tile,0,0,frame,2));
                }
                for(int y=0;y<52;++y)for(int x=0;x<52;++x)
                    assert(canvas.readPixel(208+x,156+y)==expected.readPixel(x,y));
            }
        }
    }
}

bool writePpm(const std::string& name, const std::vector<uint16_t>& pixels, int w, int h) {
    FILE* file = std::fopen(name.c_str(), "wb");
    if (!file) return false;
    std::fprintf(file, "P6\n%d %d\n255\n", w, h);
    for (uint16_t c : pixels) {
        unsigned char rgb[] = {static_cast<unsigned char>(((c >> 11) & 31) * 255 / 31),
                               static_cast<unsigned char>(((c >> 5) & 63) * 255 / 63),
                               static_cast<unsigned char>((c & 31) * 255 / 31)};
        std::fwrite(rgb, 1, 3, file);
    }
    std::fclose(file);
    return true;
}
int main(int argc, char** argv) {
    if (argc != 3) return 2;
    ExploreMapGenerator::Map map{};
    if (!readEditorMap(map)) return 2;
    DesktopPlatform desktop(argv[1]);
    Platform::bind(desktop.serviceBundle());
    desktop.begin();
    if (!Platform::resources().mount() || !GameAssets::begin()) return 3;
    FontResource::ins().begin();
    auto& canvas = PixelRenderer::canvas();
    std::vector<uint16_t> full(416 * 312);
    canvas.attach({full.data(), 416, 312, false});
    canvas.setCoordinateScale(1);canvas.setLayoutScale(1);canvas.setAssetScale(1);
    bool animated=false;
    for (int z=0;z<3;++z)for(int i=0;i<192;++i)
        animated=animated || GameAssets::isExploreTileAnimated(map.layers[z][i]);
    for (uint8_t frame=0;frame<(animated?8:1);++frame) {
        canvas.fillSprite(0);
        for (int z = 0; z < 3; ++z) for (int i = 0; i < 192; ++i) {
            const auto tile = map.layers[z][i];
            if (tile && !GameAssets::drawExploreTileTo(canvas,tile,i % 16 * 26,i / 16 * 26,frame)) return 4;
        }
        if(frame==0 && !writePpm(std::string(argv[2])+"/native-map.ppm",full,416,312)) return 5;
        if(animated) {
            char file[64];std::snprintf(file,sizeof(file),"/native-animation-%02d.ppm",frame);
            if(!writePpm(std::string(argv[2])+file,full,416,312)) return 5;
        }
    }
    Game::GameState state{};
    state.oobeDone = true;state.teamCount = 1;
    state.team[0] = Game::MonsterFactory::create(25,5);
    std::vector<uint16_t> screen(368 * 448);
    canvas.attach({screen.data(),368,448,false});canvas.setNativeText(true);
    verifyObjectOcclusion(canvas);
    verifyCachedObjectAnimation(canvas);
    AmoledV1::RenderCaches caches;
    AmoledV1::ExploreRouteViewModel model{};
    model.state=&state;model.map=&map;model.area=map.areaIndex;model.speciesId=25;
    model.routePointCount=map.paths[0].pointCount;
    for (int i = 0; i < map.paths[0].pointCount; ++i) {
        auto position=ExploreRouteGeometry::pathPoint(map.paths[0],i);
        model.worldX=position.x;model.worldY=position.y;
        model.routeIndex=i;
        constexpr int viewHeight = AmoledV1::EXPLORE_ROUTE_MAP_BOTTOM - AmoledV1::EXPLORE_ROUTE_MAP_TOP;
        model.cameraX=std::clamp(int(std::lround(position.x))-92,0,416-184);
        model.cameraY=std::clamp(int(std::lround(position.y))-viewHeight/4,0,312-viewHeight/2);
        canvas.fillSprite(0);
        AmoledV1::renderExploreRouteScreen(canvas,model,caches.exploreWorld,0,448);
        char file[64];std::snprintf(file,sizeof(file),"/native-route-%02d.ppm",i);
        if (!writePpm(std::string(argv[2])+file,screen,368,448)) return 5;
    }
    return 0;
}
