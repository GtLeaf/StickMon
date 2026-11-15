// Capture current runtime maps through the production tile renderer.
#include "assets/GameAssets.h"
#include "game/ExploreMapGenerator.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/Canvas565.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

using namespace ExploreMapGenerator;

static void capture(const Map& map, const std::string& path) {
    constexpr int width=WIDTH*26, height=HEIGHT*26;
    std::vector<uint16_t> pixels(width*height);
    Canvas565 canvas;
    canvas.attach({pixels.data(),width,height,false});
    canvas.fillSprite(0);
    for (int layer=0;layer<LAYER_COUNT;++layer) {
        for (int cell=0;cell<CELL_COUNT;++cell) {
            const auto tile=map.layers[layer][cell];
            if (!tile) continue;
            assert(GameAssets::drawExploreTileTo(canvas,tile,cell%WIDTH*26,cell/WIDTH*26,0,1));
        }
    }
    auto* file=std::fopen(path.c_str(),"wb");assert(file);
    std::fprintf(file,"P6\n%d %d\n255\n",width,height);
    for (auto pixel:pixels) {
        const unsigned char rgb[]={uint8_t(((pixel>>11)&31)*255/31),
            uint8_t(((pixel>>5)&63)*255/63),uint8_t((pixel&31)*255/31)};
        assert(std::fwrite(rgb,1,3,file)==3);
    }
    assert(std::fclose(file)==0);
}

int main(int argc,char** argv) {
    assert(argc==3);
    DesktopPlatform desktop(argv[1]);Platform::bind(desktop.serviceBundle());
    assert(desktop.begin() && Platform::resources().mount() && GameAssets::begin());
    std::printf("algorithm %u\n",unsigned(ALGORITHM_VERSION));
    for (uint8_t stage=0;stage<7;++stage) {
        Map map;assert(generate(38,Edge::BOTTOM,MIST_FOREST_PATH_AREA,map,{stage,7,false}));
        const auto name="mist-stage-"+std::to_string(stage);
        capture(map,std::string(argv[2])+"/"+name+".ppm");
        std::printf("%s %08x\n",name.c_str(),fingerprint(map));
    }
    for (uint8_t node=0;node<14;++node) {
        const uint8_t entry=node==13?2:0;
        Map map;assert(generateValleyRegion(262,{node,entry},false,map));
        const auto name="valley-node-"+std::to_string(node);
        capture(map,std::string(argv[2])+"/"+name+".ppm");
        std::printf("%s %08x\n",name.c_str(),fingerprint(map));
    }
}
