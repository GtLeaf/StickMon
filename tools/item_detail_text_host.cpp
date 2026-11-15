#include "ui/ItemShopScreens.h"
#include "ui/UiCommon.h"
#include "ui/UiMetrics.h"
#include "presentation/TextBlock.h"
#include "core/FontResource.h"
#include "game/ItemInventory.h"
#include "platform/desktop/DesktopPlatform.h"
#include "assets/GameAssets.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

using namespace AmoledV1;
static std::vector<uint16_t> pixels(Canvas565& c) {
    std::vector<uint16_t> out;out.reserve(368*448);
    for(int y=0;y<448;++y)for(int x=0;x<368;++x)out.push_back(c.readPixel(x,y));
    return out;
}
static void capture(Canvas565& c,const char* out,const char* name) {
    if(!out)return;
    auto* f=std::fopen((std::string(out)+"/"+name+".ppm").c_str(),"wb");assert(f);
    std::fprintf(f,"P6\n368 448\n255\n");
    for(auto p:pixels(c)){const unsigned char v[]={uint8_t(((p>>11)&31)*255/31),uint8_t(((p>>5)&63)*255/63),uint8_t((p&31)*255/31)};std::fwrite(v,1,3,f);}
    std::fclose(f);
}
template<class Model,class Render>static void check(Canvas565& c,std::vector<uint16_t>& storage,
                                                   const Model& model,Game::ItemId id,Render render) {
    c.clearClipRect();c.fillSprite(0xf81f);render(c,model,0,448);const auto full=pixels(c);
    Ui::TextBlock expected;Ui::TextBlock::Options options;
    options.width=320;options.maxHeight=100;options.font=BitmapText::Font::PX32;
    expected.setOptions(options);expected.setText(Game::ShopService::shortDescription(id));
    assert(!expected.measure().overflow);
    c.fillSprite(UiMetrics::PAGE_BACKGROUND);expected.draw(c,24,248,UiCommon::rgb(126,175,175));
    for(int y=248;y<348;++y)for(int x=24;x<344;++x)assert(full[y*368+x]==c.readPixel(x,y));
    // Reconstruct cropped rows through the renderer's real PageClip contract.
    c.fillSprite(0xf81f);
    for(int y=0;y<448;y+=7)render(c,model,uint16_t(y),uint16_t(std::min(448,y+7)));
    assert(pixels(c)==full);
    assert(c.clipLeft()==0&&c.clipTop()==0&&c.clipRight()==368&&c.clipBottom()==448);
    c.attach({storage.data(),368,448,true});c.fillSprite(0xf81f);render(c,model,0,448);
    assert(pixels(c)==full);c.attach({storage.data(),368,448,false});
}
int main(int argc,char** argv) {
    assert(argc==2||argc==3);const char* out=argc==3?argv[2]:nullptr;
    DesktopPlatform desktop(argv[1]);Platform::bind(desktop.serviceBundle());assert(desktop.begin());
    assert(Platform::resources().mount()&&GameAssets::begin()&&FontResource::ins().begin());
    std::vector<uint16_t> storage(368*448);auto& c=PixelRenderer::canvas();c.attach({storage.data(),368,448,false});
    c.setCoordinateScale(1);c.setLayoutScale(1);c.setAssetScale(1);c.setNativeText(true);
    Game::GameState state{};state.coins=27400;
    for(int i=0;i<int(Game::ItemId::COUNT);++i)Game::ItemInventory::add(state,Game::ItemId(i),3);
    ItemListViewModel bag;bag.state=&state;bag.confirmOpen=true;bag.itemCount=1;
    ShopViewModel shop;shop.state=&state;shop.coins=state.coins;shop.itemCount=1;shop.detailProgress=1;
    // Save representative full frames before any new wrap assertion, so RED
    // captures are also available for an exact outside-body comparison.
    for(const auto id:{Game::ItemId::MAX_REVIVE,Game::ItemId::CANDY}) {
        const char* stem=id==Game::ItemId::CANDY?"candy":"revive";char name[48];
        bag.pendingItem=id;c.fillSprite(0);renderItemListScreen(c,bag,0,448);
        std::snprintf(name,sizeof(name),"bag-%s",stem);capture(c,out,name);
        shop.detailItem=id;shop.mode=ShopViewModel::Mode::BUY;c.fillSprite(0);renderShopScreen(c,shop,0,448);
        std::snprintf(name,sizeof(name),"shop-buy-%s",stem);capture(c,out,name);
        shop.mode=ShopViewModel::Mode::SELL;c.fillSprite(0);renderShopScreen(c,shop,0,448);
        std::snprintf(name,sizeof(name),"shop-sell-%s",stem);capture(c,out,name);
    }
    for(int i=0;i<int(Game::ItemId::COUNT);++i) {
        const auto id=Game::ItemId(i);bag.pendingItem=id;shop.detailItem=id;
        for(bool battle:{false,true}){bag.battleMode=battle;check(c,storage,bag,id,renderItemListScreen);}
        for(auto mode:{ShopViewModel::Mode::BUY,ShopViewModel::Mode::SELL}){shop.mode=mode;check(c,storage,shop,id,renderShopScreen);}
    }
    bag.battleMode=false;bag.pendingItem=Game::ItemId::MAX_REVIVE;bag.toast=nullptr;
    c.fillSprite(0);renderItemListScreen(c,bag,0,448);UiCommon::drawToast(c,"使用了伤药");const auto toast=pixels(c);
    bag.toast="使用了伤药";c.fillSprite(0);renderItemListScreen(c,bag,0,448);assert(pixels(c)==toast);
    const auto again=pixels(c);c.fillSprite(0);
    for(int y=0;y<448;y+=9)renderItemListScreen(c,bag,uint16_t(y),uint16_t(std::min(448,y+9)));
    assert(pixels(c)==again);
    assert(itemConfirmChoiceAt(100,390)==0&&itemConfirmChoiceAt(260,390)==1);
    assert(!itemListBackAt(20,20,true));
    std::printf("item-detail-text: all%u items x home/battle/buy/sell fit; full/dirty/swapped drawing, toast layering and button geometry OK\n",unsigned(Game::ItemId::COUNT));
}
