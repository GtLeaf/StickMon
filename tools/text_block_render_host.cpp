#include "presentation/TextBlock.h"
#include "ui/common/MarketTutorUi.h"
#include "ui/common/MarketShopUi.h"
#include "ui/common/MarketDialogueUi.h"
#include "game/MarketMapData.h"
#include "market_dialogue_fixture.h"
#include "core/FontResource.h"
#include "game/MonsterFactory.h"
#include "platform/desktop/DesktopPlatform.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

extern "C" unsigned uzlib_adler32(const void*,unsigned,unsigned seed){return seed;}
extern "C" unsigned uzlib_crc32(const void*,unsigned,unsigned seed){return seed;}
using Ui::TextBlock;
static const uint16_t BG=0x0821,INK=0xffff;
static std::vector<uint16_t> pixels(Canvas565& c) {
    std::vector<uint16_t> result;
    for(int y=0;y<c.height();++y)for(int x=0;x<c.width();++x)result.push_back(c.readPixel(x,y));
    return result;
}
static void save(Canvas565& c,const char* out,const char* name) {
    if(!out)return;
    auto* f=std::fopen((std::string(out)+"/"+name+".ppm").c_str(),"wb");assert(f);
    std::fprintf(f,"P6\n%d %d\n255\n",c.width(),c.height());
    for(int y=0;y<c.height();++y)for(int x=0;x<c.width();++x){const auto p=c.readPixel(x,y);
        const unsigned char v[]={uint8_t(((p>>11)&31)*255/31),uint8_t(((p>>5)&63)*255/63),uint8_t((p&31)*255/31)};std::fwrite(v,1,3,f);}
    std::fclose(f);
}
static void componentChecks(Canvas565& c,std::vector<uint16_t>& storage) {
    // Explicit font selection must match the established raster, regardless of
    // ambient nativeText, and keep its state unchanged.
    for(auto font:{BitmapText::Font::PX16,BitmapText::Font::PX32}) {
        c.setNativeText(font==BitmapText::Font::PX32);c.fillSprite(BG);
        PixelRenderer::text(c,12,10,"甲A♀♂中",INK);const auto reference=pixels(c);
        c.setNativeText(font!=BitmapText::Font::PX32);const bool native=c.nativeText();c.fillSprite(BG);
        PixelRenderer::bitmapText(c,12,10,"甲A♀♂中",INK,font);
        assert(pixels(c)==reference&&c.nativeText()==native);
    }
    TextBlock b;TextBlock::Options o;o.width=128;o.font=BitmapText::Font::PX32;
    b.setOptions(o);b.setText("甲乙丙丁，学习技能。\n第二段！");
    for(auto align:{TextBlock::Align::LEFT,TextBlock::Align::CENTER,TextBlock::Align::RIGHT}) {
        o.align=align;b.setOptions(o);c.clearClipRect();c.fillSprite(BG);b.draw(c,17,9,INK);
        const auto full=pixels(c);
        c.fillSprite(BG);
        for(int y=0;y<448;y+=7){c.setClipRect(0,y,368,std::min(7,448-y));b.draw(c,17,9,INK);assert(c.clipTop()==y);}
        c.clearClipRect();assert(pixels(c)==full);
    }
    const auto normal=pixels(c);
    c.attach({storage.data(),368,448,true});c.fillSprite(BG);b.draw(c,17,9,INK);assert(pixels(c)==normal);
    c.attach({storage.data(),368,448,false});
    TextBlock tiny;TextBlock::Options tinyOptions;tinyOptions.width=32;tinyOptions.maxHeight=15;
    tiny.setOptions(tinyOptions);tiny.setText("甲");c.fillSprite(BG);tiny.draw(c,10,10,INK);
    bool partialInk=false;
    for(int y=10;y<25;++y)for(int x=10;x<42;++x)partialInk|=c.readPixel(x,y)!=BG;
    assert(partialInk&&c.readPixel(10,25)==BG);
    o.width=128;o.maxHeight=66;b.setOptions(o);b.setText("甲乙丙丁戊己庚辛壬癸甲乙丙丁戊己");
    c.fillSprite(BG);b.draw(c,10,10,INK,68);const auto scrolled=pixels(c);
    assert(c.readPixel(10,9)==BG&&c.readPixel(10,76)==BG);
    c.fillSprite(BG);b.draw(c,10,10,INK);assert(pixels(c)!=scrolled);
    // Logical 32px text on a 2x Canvas is a nearest 2x version of the 1x draw.
    c.clearClipRect();c.fillSprite(BG);PixelRenderer::bitmapText(c,0,0,"甲A",INK,BitmapText::Font::PX32);
    const auto small=pixels(c);
    c.setCoordinateScale(2);c.fillSprite(BG);PixelRenderer::bitmapText(c,0,0,"甲A",INK,BitmapText::Font::PX32);
    c.setCoordinateScale(1);
    for(int y=0;y<64;++y)for(int x=0;x<96;++x)assert(c.readPixel(x,y)==small[(y/2)*368+x/2]);
    std::puts("text-block-render: real font parity, alignment, dirty bands, byte order, scroll clip and scaling OK");
}
static void revealChecks(Canvas565& c,std::vector<uint16_t>& storage) {
    TextBlock block;TextBlock::Options o;o.width=320;o.font=BitmapText::Font::PX32;
    o.align=TextBlock::Align::CENTER;o.lineGap=8;
    block.setOptions(o);block.setText("甲乙丙丁戊己庚辛壬癸，下一行。\n第三行！");
    const auto m=block.measure();assert(m.lineCount>=3);
    const auto first=block.line(0);assert(first.width==288);
    TextBlock::DrawRange range;range.firstLine=0;range.lineCount=2;range.visibleCharacters=1;
    c.clearClipRect();c.fillSprite(BG);block.draw(c,10,20,INK,range);
    const auto revealed=pixels(c);
    c.fillSprite(BG);PixelRenderer::bitmapText(c,10+(320-first.width)/2,20,"甲",INK,o.font);
    assert(pixels(c)==revealed&&!block.needsLayout());
    range.visibleCharacters=10;c.fillSprite(BG);block.draw(c,10,20,INK,range);const auto full=pixels(c);
    c.fillSprite(BG);for(int y=0;y<448;y+=5){c.setClipRect(0,y,368,std::min(5,448-y));block.draw(c,10,20,INK,range);}
    c.clearClipRect();assert(pixels(c)==full);
    c.attach({storage.data(),368,448,true});c.fillSprite(BG);block.draw(c,10,20,INK,range);assert(pixels(c)==full);
    c.attach({storage.data(),368,448,false});
    range.firstLine=2;range.lineCount=1;range.visibleCharacters=1;
    c.fillSprite(BG);block.draw(c,10,20,INK,range);const auto window=pixels(c);
    const auto& third=block.line(2);const char* begin=block.text()+third.begin;const char* end=begin;
    BitmapText::decode(end,block.text()+third.end);char one[5]{};std::memcpy(one,begin,size_t(end-begin));
    c.fillSprite(BG);PixelRenderer::bitmapText(c,10+(320-third.width)/2,20,one,INK,o.font);assert(pixels(c)==window);
    range.visibleCharacters=0;c.fillSprite(BG);block.draw(c,10,20,INK,range);
    for(auto pixel:pixels(c))assert(pixel==BG);
    range.firstLine=100;range.visibleCharacters=-1;block.draw(c,10,20,INK,range);
    for(auto pixel:pixels(c))assert(pixel==BG);
    assert(block.measure().lineCount==m.lineCount&&block.line(0).width==first.width);
    std::puts("text-reveal: immutable full-line alignment, UTF-8 budget, line window, dirty bands and byte order OK");
}
static void npcDialogueChecks(Canvas565& c,std::vector<uint16_t>& original,const char* out) {
    for(const bool round:{false,true}) {
        const int w=round?360:368,h=round?360:448;
        std::vector<uint16_t> frame(w*h);c.attach({frame.data(),uint16_t(w),uint16_t(h),false});
        c.setNativeText(!round);
        Market::Session session;assert(session.reset(MarketMapData::get(),38,25,133,0));session.enableManual();
        assert(marketStandNear(session,2));MarketDialogue dialogue;assert(dialogue.load());
        const Ui::MarketView::Viewport view{w,h,1,round,true};const auto box=Ui::MarketDialogueUi::layout(view);
        assert(dialogue.begin(session,2,box.textWidth,box.glyph));uint32_t now=0;
        for(int i=0;i<40&&!dialogue.visible();++i){now+=50;session.update(now);dialogue.update(session,now,true);}
        assert(dialogue.visible()&&!dialogue.complete());
        const auto fixed=std::string(dialogue.textLayout().text());
        const int first=dialogue.pageFirstLine();const auto fixedLine=dialogue.textLayout().line(first);
        int index=0;
        while(true) {
            c.clearClipRect();c.fillSprite(BG);Ui::MarketDialogueUi::render(dialogue,view);const auto full=pixels(c);
            char name[48];std::snprintf(name,sizeof(name),"npc-%d-%03d",box.glyph,index++);save(c,out,name);
            c.fillSprite(BG);for(int y=0;y<h;y+=7){c.setClipRect(0,y,w,std::min(7,h-y));Ui::MarketDialogueUi::render(dialogue,view);}
            c.clearClipRect();assert(pixels(c)==full&&c.nativeText()==!round);
            c.attach({frame.data(),uint16_t(w),uint16_t(h),true});c.fillSprite(BG);
            Ui::MarketDialogueUi::render(dialogue,view);assert(pixels(c)==full);
            c.attach({frame.data(),uint16_t(w),uint16_t(h),false});
            assert(fixed==dialogue.textLayout().text()&&first==dialogue.pageFirstLine());
            assert(fixedLine.begin==dialogue.textLayout().line(first).begin&&fixedLine.end==dialogue.textLayout().line(first).end);
            assert(!dialogue.textLayout().needsLayout());
            if(dialogue.complete())break;
            assert(index<100);now+=40;session.update(now);dialogue.update(session,now,true);
        }
        dialogue.advance(session);
        if(dialogue.active()) {
            assert(dialogue.revealedCharacters()==0);
            c.fillSprite(BG);Ui::MarketDialogueUi::render(dialogue,view);
            for(int y=box.textY;y<box.textY+box.glyph+box.lineHeight;++y)
                for(int x=box.textX;x<box.textX+box.textWidth;++x)
                    assert(c.readPixel(x,y)==PixelRenderer::rgb(248,248,224));
        }
        c.attach({original.data(),368,448,false});c.setNativeText(true);
    }
    std::puts("npc-typewriter-render: fixed 16/32px pages, all reveal frames, blank next page, dirty bands and byte order OK");
}
static void confirmationChecks(Canvas565& c,const char* out) {
    using UI=Ui::MarketTutorUi;using Type=Platform::IInputDevice::TouchType;
    Game::GameState s{};s.teamCount=1;s.coins=27400;s.marketTutorGifted=1;s.bag.heartScale=4;
    s.team[0]=Game::MonsterFactory::create(1,35);s.team[0].move2Id=73;s.team[0].move3Id=22;
    Game::MarketTutor::ensurePeriod(s);s.marketTutor.move=402;s.marketTutor.drawnSpecies=1;
    UI ui;uint32_t time=1000;
    auto tap=[&](int x,int y){ui.touch({Type::DOWN,int16_t(x),int16_t(y),time},s);
        const auto r=ui.touch({Type::UP,int16_t(x),int16_t(y),time+60},s);time+=200;return r;};
    auto settle=[&](){for(int i=0;i<6;++i)ui.update(100);};
    ui.open(s);settle();tap(184,380);settle();tap(184,244);tap(184,328);tap(184,294);
    assert(ui.page()==UI::Page::CONFIRM);
    c.clearClipRect();c.fillSprite(BG);ui.render(s);save(c,out,"learning-long");const auto actual=pixels(c);
    TextBlock expected;TextBlock::Options o;o.width=320;o.maxHeight=96;o.font=BitmapText::Font::PX32;o.align=TextBlock::Align::CENTER;
    expected.setOptions(o);expected.setText("妙蛙种子忘记寄生种子，学会种子炸弹。");
    assert(expected.measure().lineCount==2&&!expected.measure().overflow);
    c.fillSprite(PixelRenderer::rgb(248,248,224));
    expected.draw(c,24,270+(96-expected.measure().height)/2,PixelRenderer::rgb(40,56,80));
    for(int y=270;y<366;++y)for(int x=24;x<344;++x)assert(actual[y*368+x]==c.readPixel(x,y));
    c.fillSprite(BG);ui.render(s);const auto full=pixels(c);
    c.fillSprite(BG);for(int y=0;y<448;++y){c.setClipRect(0,y,368,1);ui.render(s);}
    c.clearClipRect();assert(pixels(c)==full);
    // A swipe in the body must never issue a payment even if UP hits the footer.
    ui.touch({Type::DOWN,184,300,time},s);ui.touch({Type::MOVE,184,420,time+40},s);
    assert(ui.touch({Type::UP,184,420,time+60},s).kind==UI::Request::NONE);time+=200;
    assert(tap(184,414).kind==UI::Request::TEACH);
    // Reopen the public flow for recall; the body keeps 32px while the scale
    // consumption remains outside the scroll viewport.
    ui.open(s);settle();tap(184,380);settle();tap(184,310);tap(184,328);
    Game::MoveId recallable[Game::MoveManagementService::MAX_RECALLABLE_MOVE_COUNT];
    const auto count=Game::MoveManagementService::collectRecallable(*findSpecies(1),s.team[0],recallable,
        Game::MoveManagementService::MAX_RECALLABLE_MOVE_COUNT);
    assert(count>0);tap(184,244);tap(184,294);assert(ui.page()==UI::Page::CONFIRM);
    char copy[192];std::snprintf(copy,sizeof(copy),"妙蛙种子忘记寄生种子，想起%s。",findMove(recallable[0])->name);
    expected.setText(copy);o.maxHeight=76;o.align=TextBlock::Align::LEFT;expected.setOptions(o);
    c.fillSprite(BG);ui.render(s);save(c,out,"recall-long");const auto recalled=pixels(c);
    c.fillSprite(PixelRenderer::rgb(248,248,224));expected.draw(c,24,270,PixelRenderer::rgb(40,56,80));
    for(int y=270;y<346;++y)for(int x=24;x<344;++x)assert(recalled[y*368+x]==c.readPixel(x,y));
    c.fillSprite(BG);ui.render(s);const auto recallFull=pixels(c);
    c.fillSprite(BG);for(int y=0;y<448;y+=9){c.setClipRect(0,y,368,std::min(9,448-y));ui.render(s);}
    c.clearClipRect();assert(pixels(c)==recallFull);
    assert(tap(184,414).kind==UI::Request::RECALL);
    std::puts("text-block-confirm: natural 32px long copy, dirty-row reconstruction, drag isolation and payment OK");
}
static void marketTextChecks(Canvas565& c,const char* out) {
    using Type=Platform::IInputDevice::TouchType;
    const auto paper=PixelRenderer::rgb(248,248,224),ink=PixelRenderer::rgb(40,56,80);
    TextBlock expected;TextBlock::Options o;o.width=320;o.font=BitmapText::Font::PX32;
    Game::GameState s{};s.teamCount=1;s.coins=30000;s.marketTutorGifted=1;s.bag.heartScale=1;
    s.team[0]=Game::MonsterFactory::create(133,35);s.team[0].move2Id=129;s.team[0].move3Id=44;
    Game::MarketTutor::ensurePeriod(s);s.marketTutor.move=247;s.marketTutor.drawnSpecies=133;
    Ui::MarketTutorUi tutor;tutor.open(s,"好好好好好好好，来学习吧。");
    for(int i=0;i<4;++i)tutor.update(100);
    c.clearClipRect();c.fillSprite(BG);tutor.render(s);const auto greeting=pixels(c);
    o.maxHeight=80;o.lineGap=8;expected.setOptions(o);expected.setText("耿鬼：好好好好好好好，来学习吧。");
    assert(!expected.measure().overflow);
    c.fillSprite(paper);expected.draw(c,24,342,ink);
    for(int y=342;y<422;++y)for(int x=24;x<344;++x)assert(greeting[y*368+x]==c.readPixel(x,y));
    c.fillSprite(BG);tutor.render(s);save(c,out,"gengar-greeting");
    uint32_t clock=1000;
    auto tap=[&](auto& ui,int x,int y){ui.touch({Type::DOWN,int16_t(x),int16_t(y),clock},s);
        const auto r=ui.touch({Type::UP,int16_t(x),int16_t(y),clock+60},s);clock+=200;return r;};
    tap(tutor,184,380);for(int i=0;i<6;++i)tutor.update(100);
    tap(tutor,184,244);tap(tutor,184,328);tap(tutor,184,294);
    assert(tap(tutor,184,414).kind==Ui::MarketTutorUi::Request::TEACH);
    assert(Game::MarketTutor::teach(s,0,1)==Game::MarketTutor::Result::OK);tutor.committed(true,s);
    for(int i=0;i<30;++i)tutor.update(250);
    assert(tutor.page()==Ui::MarketTutorUi::Page::RESULT);
    c.fillSprite(BG);tutor.render(s);save(c,out,"gengar-result");const auto result=pixels(c);
    o.maxHeight=68;o.lineGap=2;o.align=TextBlock::Align::CENTER;expected.setOptions(o);
    expected.setText("伊布学会了暗影球！");assert(!expected.measure().overflow);
    c.fillSprite(paper);expected.draw(c,24,295,ink); // established single-row result baseline
    for(int y=278;y<346;++y)for(int x=24;x<344;++x)assert(result[y*368+x]==c.readPixel(x,y));
    Game::MarketShop::ensurePeriod(s);s.marketSeason.items[0]=uint8_t(Game::ItemId::MAX_REVIVE);s.marketSeason.stock[0]=1;
    Ui::MarketShopUi shop;shop.open(s);for(int i=0;i<4;++i)shop.update(100);
    assert(shop.page()==Ui::MarketShopUi::Page::GREET);
    c.fillSprite(BG);shop.render(s);save(c,out,"kecleon-greeting");const auto shopGreet=pixels(c);
    o.maxHeight=80;o.lineGap=8;o.align=TextBlock::Align::LEFT;expected.setOptions(o);expected.setText(Ui::MarketShopText::GREET);
    assert(!expected.measure().overflow);c.fillSprite(paper);expected.draw(c,24,342,ink);
    for(int y=342;y<422;++y)for(int x=24;x<344;++x)assert(shopGreet[y*368+x]==c.readPixel(x,y));
    tap(shop,184,380);for(int i=0;i<6;++i)shop.update(100);tap(shop,184,280);tap(shop,184,244);
    assert(shop.page()==Ui::MarketShopUi::Page::CONFIRM);
    c.fillSprite(BG);shop.render(s);save(c,out,"kecleon-description");const auto description=pixels(c);
    o.maxHeight=66;o.lineGap=2;expected.setOptions(o);expected.setText(Game::ShopService::shortDescription(Game::ItemId::MAX_REVIVE));
    assert(!expected.measure().overflow);c.fillSprite(paper);expected.draw(c,24,282,ink);
    for(int y=282;y<348;++y)for(int x=24;x<344;++x)assert(description[y*368+x]==c.readPixel(x,y));
    assert(tap(shop,272,414).kind==Ui::MarketShopUi::Request::BUY);shop.committed(true,Game::ItemId::MAX_REVIVE,1);
    c.fillSprite(BG);shop.render(s);save(c,out,"kecleon-result");const auto bought=pixels(c);
    o.maxHeight=32;o.align=TextBlock::Align::CENTER;expected.setOptions(o);expected.setText("获得活力块×1");
    assert(!expected.measure().overflow);c.fillSprite(paper);expected.draw(c,24,282,ink);
    for(int y=282;y<314;++y)for(int x=24;x<344;++x)assert(bought[y*368+x]==c.readPixel(x,y));
    c.fillSprite(BG);shop.render(s);const auto full=pixels(c);
    c.fillSprite(BG);for(int y=0;y<448;y+=5){c.setClipRect(0,y,368,std::min(5,448-y));shop.render(s);}
    c.clearClipRect();assert(pixels(c)==full);
    tap(shop,184,414);
    shop.touch({Type::DOWN,184,410,clock},s);shop.touch({Type::MOVE,184,58,clock+100},s);
    shop.touch({Type::UP,184,58,clock+300},s);clock+=500;
    tap(shop,184,404);assert(shop.page()==Ui::MarketShopUi::Page::CONFIRM);
    assert(tap(shop,272,414).kind==Ui::MarketShopUi::Request::BUY);
    shop.committed(true,Game::ItemId::CANDY,2);
    auto advance=[&](int ms){while(ms>0){const int step=std::min(ms,100);shop.update(step);ms-=step;}};
    advance(600);assert(shop.page()==Ui::MarketShopUi::Page::BOX_WAIT);tap(shop,184,316);advance(1000);
    assert(shop.page()==Ui::MarketShopUi::Page::BOX_OPEN);
    c.fillSprite(BG);shop.render(s);save(c,out,"kecleon-box-fade");const auto faded=pixels(c);
    c.fillSprite(BG);for(int y=0;y<448;y+=7){c.setClipRect(0,y,368,std::min(7,448-y));shop.render(s);}
    c.clearClipRect();assert(pixels(c)==faded);
    advance(250);assert(shop.page()==Ui::MarketShopUi::Page::RESULT);
    c.fillSprite(BG);shop.render(s);save(c,out,"kecleon-box-result");const auto boxResult=pixels(c);
    bool changed=false;for(int y=310;y<342;++y)for(int x=24;x<344;++x)changed|=faded[y*368+x]!=boxResult[y*368+x];
    assert(changed);
    expected.setText("开出神奇糖果×2");expected.setOptions(o);assert(!expected.measure().overflow);
    c.fillSprite(paper);expected.draw(c,24,310,ink);
    for(int y=310;y<342;++y)for(int x=24;x<344;++x)assert(boxResult[y*368+x]==c.readPixel(x,y));
    std::puts("market-text-blocks: greeting punctuation, item bodies, purchase/result alignment and dirty bands OK");
}
int main(int argc,char** argv) {
    assert(argc==2||argc==3);DesktopPlatform d(argv[1]);Platform::bind(d.serviceBundle());assert(d.begin());
    assert(Platform::resources().mount()&&GameAssets::begin()&&FontResource::ins().begin());
    std::vector<uint16_t> storage(368*448);auto& c=PixelRenderer::canvas();c.attach({storage.data(),368,448,false});
    c.setCoordinateScale(1);c.setLayoutScale(1);c.setAssetScale(1);c.setNativeText(true);
    componentChecks(c,storage);confirmationChecks(c,argc==3?argv[2]:nullptr);
    revealChecks(c,storage);
    npcDialogueChecks(c,storage,argc==3?argv[2]:nullptr);
    marketTextChecks(c,argc==3?argv[2]:nullptr);
}
