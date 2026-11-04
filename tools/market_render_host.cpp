#ifndef STICKMON_MARKET_RENDER_WIDTH
#include "game/MarketMapData.h"
#endif
#include "map_editor/market_wire.h"
#include "ui/common/MarketView.h"
#include "ui/common/MarketWalkUi.h"
#include "core/FontResource.h"
#include "platform/desktop/DesktopPlatform.h"
#include <cstdio>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <chrono>

static bool save(const std::string& path,const std::vector<uint16_t>& pixels,int w,int h) {
    auto* f=std::fopen(path.c_str(),"wb");if(!f)return false;
    std::fprintf(f,"P6\n%d %d\n255\n",w,h);
    for(auto c:pixels){unsigned char rgb[]={uint8_t(((c>>11)&31)*255/31),uint8_t(((c>>5)&63)*255/63),uint8_t((c&31)*255/31)};std::fwrite(rgb,1,3,f);}
    return std::fclose(f)==0;
}
int main(int argc,char** argv) {
    if(argc<3||argc>6)return 2;
    bool walk=false,sequence=false;
    for(int i=3;i<argc;++i){const std::string flag=argv[i];if(flag=="--walk")walk=true;else if(flag=="--sequence")sequence=true;else if(flag!="--stdin")return 2;}
    DesktopPlatform desktop(argv[1]);Platform::bind(desktop.serviceBundle());desktop.begin();
    if(!Platform::resources().mount()||!GameAssets::begin())return 3;
    FontResource::ins().begin();
    Market::Data input;
    if(argc>=4&&!readMarket(std::cin,input))return 2;
    #ifdef STICKMON_MARKET_RENDER_WIDTH
    if(argc<4)return 2;
    const auto& data=input;
    #else
    const auto& data=argc>=4?input:MarketMapData::get();
    #endif
    if(!Ui::MarketView::preload(data,25,133))return 4;
    Market::Session session;session.reset(data,38,25,133,0);
    if(walk)session.enableManual();
    Ui::MarketWalkUi ui;
    const Ui::MarketView::Viewport views[]={{Market::CELL_WIDTH*26,Market::CELL_HEIGHT*26+(walk?0:24),1,false,walk},{240,135,1,false,walk},{360,360,1,true,walk},{368,448,2}};
    const char* names[]={"overview","stick","round","amoled"};
    long long renderUs[4]{},maxRenderUs[4]{};unsigned renderCount[4]{};
    std::ofstream events;
    if(sequence)events.open(std::string(argv[2])+"/events.tsv");
    for(uint32_t t=0;t<=(sequence?900U:1200U);++t) {
        session.update(t*100);
        if(t%250==0){session.walkSelected();session.nextTarget();}
        if(!sequence&&t!=200&&t!=400&&t!=600&&t!=800&&t!=1000&&t!=1200)continue;
        if(sequence)for(int a=0;a<session.count();++a){const auto p=session.pose(a);if(p.speaking||p.lift>0)events<<t<<'\t'<<a<<'\t'<<p.species<<'\t'<<p.x<<'\t'<<p.y<<'\t'<<p.speaking<<'\t'<<p.lift<<'\n';}
        for(int i=0;i<(walk?3:4);++i) {
            if(sequence&&i!=0&&i!=2)continue;
            const auto v=views[i];std::vector<uint16_t> pixels(v.width*v.height);
            auto& c=PixelRenderer::canvas();c.attach({pixels.data(),uint16_t(v.width),uint16_t(v.height),false});
            c.setCoordinateScale(1);c.setLayoutScale(1);c.setAssetScale(1);c.setNativeText(i==3);
            const auto renderStart=std::chrono::steady_clock::now();
            if(walk&&i<3) {
                ui.refresh(session);
                ui.render(session,v,12*60,i==2);
            } else Ui::MarketView::render(session,v,t*100,12*60);
            const auto duration=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-renderStart).count();
            renderUs[i]+=duration;maxRenderUs[i]=std::max(maxRenderUs[i],static_cast<long long>(duration));++renderCount[i];
            if(v.round)for(int y=0;y<v.height;++y)for(int x=0;x<v.width;++x) {
                const int dx=2*x-v.width+1,dy=2*y-v.height+1;
                if(dx*dx+dy*dy>=v.width*v.width)pixels[y*v.width+x]=0;
            }
            const auto path=std::string(argv[2])+"/"+names[i]+"-"+(sequence?std::string(4-std::to_string(t).size(),'0'):std::string())+std::to_string(t)+".ppm";
            if(!save(path,pixels,v.width,v.height))return 5;
        }
    }
    if(sequence) {
        std::ofstream metrics(std::string(argv[2])+"/render-metrics.tsv");
        for(int i=0;i<4;++i)if(renderCount[i])metrics<<names[i]<<'\t'<<double(renderUs[i])/renderCount[i]<<'\t'<<maxRenderUs[i]<<'\n';
        const auto& cache=PokemonSprites::cacheStats();
        metrics<<"sprite-cache\t"<<unsigned(cache.cachedSpecies)<<'\t'<<cache.decodedBytes<<'\t'<<cache.compressedBytes<<'\n';
    }
}
