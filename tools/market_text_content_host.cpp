#include "presentation/TextBlock.h"
#include "game/MarketShopService.h"
#include "game/Species.h"
#include "core/UiStrings.h"
#include <cassert>
#include <cstdio>
#include <set>
#include <vector>

static void fits(const char* value,int height,int gap=2) {
    Ui::TextBlock block;Ui::TextBlock::Options options;
    options.width=320;options.maxHeight=height;options.lineGap=uint16_t(gap);
    options.font=BitmapText::Font::PX32;
    block.setText(value);block.setOptions(options);
    const auto& m=block.measure();
    if(m.overflow)std::fprintf(stderr,"Market text exceeds %dpx: %s (%d lines, %dpx)\n",height,value,m.lineCount,m.height);
    assert(!m.overflow);
}
int main() {
    Game::GameState state{};std::set<Game::ItemId> offered;
    for(uint32_t day=0;day<1200;++day) {
        state.gameMinutesTotal=day*1440;
        if(!Game::MarketShop::ensurePeriod(state))continue;
        for(uint8_t slot=0;slot<Game::MarketShop::BOX;++slot)offered.insert(Game::MarketShop::item(state,slot));
    }
    for(const auto& item:Game::MarketItemCatalog::ITEMS)assert(offered.count(item.id));
    char text[192];
    for(const auto id:offered) {
        assert(id<Game::ItemId::COUNT);fits(Game::ShopService::shortDescription(id),66);
        std::snprintf(text,sizeof(text),Ui::MarketShopText::GOT,Game::ShopService::shortName(id));fits(text,32);
    }
    for(uint8_t roll=0;roll<100;++roll) {
        const auto reward=Game::MarketShop::boxRewardForRoll(state,roll);
        for(unsigned amount:{unsigned(reward.amount),255U}) {
            std::snprintf(text,sizeof(text),Ui::MarketShopText::BOX_GOT,Game::ShopService::shortName(reward.item),amount);fits(text,32);
        }
    }
    fits(Ui::MarketShopText::BOX_LINE1,66);
    fits(Ui::MarketShopText::GREET,80,8);fits(Ui::MarketShopText::GREET_SOLD_OUT,80,8);
    for(const char* greeting:{Ui::MarketTutorText::WELCOME_GIFT,Ui::MarketTutorText::GREET_TAUGHT,Ui::MarketTutorText::GREET_CLOSED}) {
        std::snprintf(text,sizeof(text),"%s%s",Ui::MarketTutorText::SPEAKER,greeting);fits(text,80,8);
    }
    std::vector<const MoveInfo*> moves;
    for(uint32_t id=1;id<=65535;++id)if(const auto* move=findMove(Game::MoveId(id)))moves.push_back(move);
    assert(!moves.empty());
    for(const auto* move:moves) {
        char greeting[144];std::snprintf(greeting,sizeof(greeting),Ui::MarketTutorText::GREET_CLASS,move->name);
        std::snprintf(text,sizeof(text),"%s%s",Ui::MarketTutorText::SPEAKER,greeting);fits(text,80,8);
        for(uint8_t i=0;i<speciesCount();++i) {
            for(const char* pattern:{Ui::MarketTutorText::TEACH_DONE,Ui::MarketTutorText::RECALL_DONE,
                                    Ui::MarketTutorText::TRAIN_DONE,Ui::MarketTutorText::TRAIN_SKILLED,Ui::MarketTutorText::TRAIN_MASTERED}) {
                const int n=std::snprintf(text,sizeof(text),pattern,speciesTable()[i].name,move->name);
                assert(n>=0&&n<int(sizeof(text)));fits(text,68);
            }
        }
    }
    std::printf("market-text-content: %zu offered items, 100 reward rolls, %zu moves x %u species x 5 results and all greetings fit without clipping/shrinking\n",
                offered.size(),moves.size(),unsigned(speciesCount()));
}
