#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "platform/desktop/DesktopPlatform.h"
#define private public
#include "core/GameEngine.h"
#include "scenes/MenuScene.h"
#include "ui/common/BagUi.h"
#include "ui/round_touch/ShopUi.h"
#undef private
#include "core/ContentStore.h"
#include "core/ContentValidation.h"
#include "core/FontResource.h"
#include "game/ItemInventory.h"
#include "game/MonsterFactory.h"
#include "presentation/TextBlock.h"

extern "C" unsigned int uzlib_crc32(const void*,unsigned int,unsigned int){assert(false);return 0;}
extern "C" unsigned int uzlib_adler32(const void*,unsigned int,unsigned int){assert(false);return 0;}
using Item=Game::ItemId;
using Type=Platform::IInputDevice::TouchType;
using Event=Platform::IInputDevice::TouchEvent;
struct ShopHost final:Ui::ShopHost {
    Game::GameState& state;int buys=0,sells=0,leaves=0;
    explicit ShopHost(Game::GameState& s):state(s){}
    const Game::GameState& shopState()const override{return state;}
    Game::ShopService::BuyResult buyShopItem(Features::ShopFlow::Item id)override{++buys;return Features::ShopFlow::buy(state,id);}
    Game::ShopService::SellResult sellShopItem(Features::ShopFlow::Item id)override{++sells;return Features::ShopFlow::sell(state,id);}
    void leaveShop()override{++leaves;}
};
static auto pixels(){auto& c=PixelRenderer::canvas();std::vector<uint16_t> v(360*360);for(int y=0;y<360;++y)for(int x=0;x<360;++x)v[y*360+x]=c.readPixel(x,y);return v;}
static void shot(const std::string& out,const char* name){
    auto* f=std::fopen((out+"/"+name+".ppm").c_str(),"wb");assert(f);std::fputs("P6\n360 360\n255\n",f);
    for(auto p:pixels()){const unsigned char rgb[]={uint8_t(((p>>11)&31)*255/31),uint8_t(((p>>5)&63)*255/63),uint8_t((p&31)*255/31)};std::fwrite(rgb,1,3,f);}assert(!std::fclose(f));
}
static Ui::TextBlock paragraph(Item id){
    Ui::TextBlock b;Ui::TextBlock::Options o;o.width=224;o.maxHeight=86;o.lineGap=4;o.font=BitmapText::Font::PX16;
    b.setOptions(o);b.setText(Game::ShopService::shortDescription(id));return b;
}
template<class Render>static void contract(Render render,Item id,uint16_t background){
    auto& c=PixelRenderer::canvas();std::vector<uint16_t> storage(360*360);auto old=Platform::display().frameBuffer();
    c.attach({storage.data(),360,360,false});c.setCoordinateScale(1);c.setLayoutScale(1);c.setAssetScale(1);c.setNativeText(false);c.clearClipRect();render();const auto full=pixels();
    auto body=paragraph(id);assert(!body.measure().overflow&&!body.measure().truncated);
    c.fillSprite(background);body.draw(c,68,200,PixelRenderer::rgb(126,175,175));
    for(int y=200;y<286;++y)for(int x=68;x<292;++x)assert(full[y*360+x]==c.readPixel(x,y)&&"description must match complete TextBlock paragraph");
    c.fillSprite(0xf81f);for(int y=0;y<360;y+=11){const int h=std::min(11,360-y);c.setClipRect(0,y,360,h);render();assert(c.clipTop()==y&&c.clipBottom()==y+h);}
    c.clearClipRect();const auto dirty=pixels();
    if(dirty!=full){for(int i=0;i<360*360;++i)if(dirty[i]!=full[i]){std::fprintf(stderr,"dirty mismatch item%d x%d y%d expected%04x actual%04x\n",int(id),i%360,i/360,full[i],dirty[i]);break;}}
    assert(dirty==full);
    c.attach({storage.data(),360,360,true});render();assert(pixels()==full);c.attach(old);c.setNativeText(false);
}
static int bagIndex(const Game::GameState& state,Item id){for(int i=0;i<Game::ItemInventory::homeBagItemCount(state);++i)if(Game::ItemInventory::homeBagItemAt(state,i)==id)return i;assert(false);return 0;}
int main(int argc,char** argv){
    assert(argc==4);DesktopPlatform desktop(argv[1]);Platform::bind(desktop.serviceBundle());assert(desktop.begin());
#if STICKMON_EXTERNAL_CONTENT
    assert(Platform::resources().mount());ContentStore catalog;ContentData::bind(&catalog);assert(validateContentCatalog()&&catalog.healthy());
#endif
    auto& engine=GameEngine::ins();assert(engine.begin());auto& state=engine.gameState();state.oobeDone=true;state.coins=27400;
    for(auto& clear:state.explorePoolRerollCounts)clear=1;
    for(int i=0;i<int(Item::COUNT);++i)Game::ItemInventory::add(state,Item(i),3);
    BagUi bag;BagUi::Context context;ShopHost host(state);Ui::ShopUi shop(host);shop.onEnter();
    auto& c=PixelRenderer::canvas();c.attach(desktop.frameBuffer());c.setCoordinateScale(1);c.setLayoutScale(1);c.setAssetScale(1);c.setNativeText(false);
    const bool captureOnly=std::string(argv[3])=="capture";
    state.coins=500000; // Match the original visual fixture, separately from rule checks.
    for(const auto id:{Item::MAX_REVIVE,Item::CANDY,Item::POTION}){
        const char* stem=id==Item::MAX_REVIVE?"revive":id==Item::CANDY?"candy":"potion";char file[48];
        bag.cursor=bagIndex(state,id);bag.detailOpen=true;bag.render(state,context);std::snprintf(file,sizeof(file),"bag-%s",stem);shot(argv[2],file);
        shop.viewMode=Ui::ShopUi::ViewMode::SELL;shop.cursor=0;while(shop.selectedItem()!=Ui::ShopUi::itemFromGameItemId(id)&&shop.cursor<shop.sellItemCount())++shop.cursor;
        if(shop.cursor<shop.sellItemCount()){shop.touchDetailOpen=true;shop.render();std::snprintf(file,sizeof(file),"shop-sell-%s",stem);shot(argv[2],file);}
    }
    if(captureOnly){engine.currentScene.reset();return 0;}
    state.coins=27400;
    for(int i=0;i<int(Item::COUNT);++i){
        const auto id=Item(i);assert(!paragraph(id).measure().overflow);
        Ui::TextLayout title;Ui::TextLayout::Options o;o.width=144*4/5;o.maxLines=2;
        title.setOptions(o);title.setText(Game::ShopService::shortName(id));assert(!title.measure().overflow);
    }
    // All item text is bounded, including details reached through battle bags.
    const auto original=state;
    int checked=0;
    for(int i=0;i<Game::ItemInventory::homeBagItemCount(state);++i){bag.cursor=i;bag.detailOpen=true;const auto id=Game::ItemInventory::homeBagItemAt(state,i);
        for(bool battle:{false,true}){context.battle=battle;contract([&]{bag.render(state,context);},id,0);++checked;}}
    for(bool sell:{false,true}){
        shop.viewMode=sell?Ui::ShopUi::ViewMode::SELL:Ui::ShopUi::ViewMode::DAILY;shop.activeCategory=Ui::ShopUi::CATEGORY_DAILY;
        const int count=shop.touchShopGrid().count();
        for(int i=0;i<count;++i){shop.touchSelect(i);shop.touchDetailOpen=true;const auto id=Ui::ShopUi::gameItemIdFor(shop.selectedItem());assert(id!=Item::COUNT);
            contract([&]{shop.render();},id,PixelRenderer::rgb(7,12,17));++checked;}
    }
    assert(!std::memcmp(&state,&original,sizeof(state)));
    // Native button targets stay inside the circle and preserve action meanings.
    for(const auto r:{Ui::ShopUi::actionBounds(),Ui::ShopUi::detailBackBounds()})for(int x:{r.x,r.x+r.width-1})for(int y:{r.y,r.y+r.height-1})assert((x-180)*(x-180)+(y-180)*(y-180)<180*180);
    uint32_t now=1000;const auto bagTap=[&](int x,int y){bag.onTouch({Type::DOWN,int16_t(x),int16_t(y),now+=30},state,context);return bag.onTouch({Type::UP,int16_t(x),int16_t(y),now+=30},state,context);};
    context.battle=false;bag.cursor=bagIndex(state,Item::NORMAL_FOOD);bag.detailOpen=true;assert(bag.target(130,312,state,context)==-1);assert(bagTap(230,312).changed&&!bag.detailOpen);
    context.battle=true;bag.detailOpen=true;assert(bagTap(130,312).kind==BagUi::Action::Kind::USE_DIRECT);
    bag.cursor=bagIndex(state,Item::POTION);assert(bagTap(130,312).kind==BagUi::Action::Kind::SELECT_TARGET);
    bag.onTouch({Type::DOWN,130,312,now+=30},state,context);bag.cancelTouch();assert(bag.onTouch({Type::UP,130,312,now+=30},state,context).kind==BagUi::Action::Kind::NONE);
    shop.onEnter();shop.touchSelect(0);shop.touchDetailOpen=true;const auto item=shop.gameItemIdFor(shop.selectedItem());const auto owned=Game::ItemInventory::count(state,item);const auto coins=state.coins;
    const auto shopTap=[&](int x,int y){shop.onTouch({Type::DOWN,int16_t(x),int16_t(y),now+=30});return shop.onTouch({Type::UP,int16_t(x),int16_t(y),now+=30});};
    shop.onTouch({Type::DOWN,130,312,now+=30});shop.cancelTouch();shop.onTouch({Type::UP,130,312,now+=30});assert(host.buys==0);
    shop.render();const auto controls=pixels();
    assert(shopTap(130,312)&&host.buys==1&&Game::ItemInventory::count(state,item)==owned+1&&state.coins==coins-Game::ShopService::buyPrice(item));
    shop.render();const auto result=pixels();
    for(const auto r:{Ui::ShopUi::actionBounds(),Ui::ShopUi::detailBackBounds()})
        for(int y=r.y;y<r.y+r.height;++y)for(int x=r.x;x<r.x+r.width;++x)assert(result[y*360+x]==controls[y*360+x]&&"purchase notice must not cover controls");
    shot(argv[2],"shop-buy-result");
    assert(shopTap(230,312)&&!shop.touchDetailOpen);
    shop.switchMode(true);shop.touchSelect(0);shop.touchDetailOpen=true;const auto sold=shop.gameItemIdFor(shop.selectedItem());const auto sellCount=Game::ItemInventory::count(state,sold);const auto balance=state.coins;
    const bool soldTap=shopTap(130,312);
    assert(soldTap&&host.sells==1&&Game::ItemInventory::count(state,sold)==sellCount-1&&state.coins==balance+Game::ShopService::sellPrice(sold));
    // Real scene consumes the detail action, opens target choice and applies once.
    engine.switchScene(SceneID::MENU,false);
    auto& menu=*static_cast<MenuScene*>(engine.currentScene.get());menu.pushView(MenuScene::ViewMode::BAG);
    state.teamCount=1;state.team[0]=Game::MonsterFactory::create(25,20);state.team[0].hpCur=1;
    menu.bagUi.cursor=bagIndex(state,Item::POTION);menu.bagUi.detailOpen=true;
    const auto potions=Game::ItemInventory::count(state,Item::POTION);
    menu.bagUi.onTouch({Type::DOWN,130,312,now+=30},state,menu.bagContext());
    const auto use=menu.bagUi.onTouch({Type::UP,130,312,now+=30},state,menu.bagContext());
    assert(menu.handleBagAction(use)&&menu.selectingTeamItem());
    menu.useTeamItemOn(0);menu.useTeamItemOn(0);
    assert(state.team[0].hpCur>1&&Game::ItemInventory::count(state,Item::POTION)==potions-1);
    desktop.advanceMs(800);menu.update(desktop.millis(),.8f);
    assert(menu.viewMode==MenuScene::ViewMode::BAG&&!menu.bagUi.detailOpen);
    std::printf("round-item-details: %d home/battle/buy/sell paragraphs, full/clip/swap/purity, readonly/feed/target/cancel, real buy/sell effects OK\n",checked);
    engine.currentScene.reset();
}
