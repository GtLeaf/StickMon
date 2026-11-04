#include "market_wire.h"
#include "game/MarketSession.h"
#include <iostream>
#include <iomanip>

int main() {
    Market::Data data;unsigned ticks;uint32_t seed;
    if(!(std::cin>>ticks>>seed)||ticks>1200||!readMarket(std::cin,data))return 2;
    Market::Session session;
    if(!session.reset(data,seed,0,0,0))return 2;
    std::cout<<std::fixed<<std::setprecision(4)<<'[';
    for(unsigned t=0;t<=ticks;++t) {
        if(t)std::cout<<',';
        std::cout<<'[';
        // Preserve editor actor indexing: residents first, then visitor pair.
        for(int i=0;i<data.residentCount+data.maxGroups*2;++i) {
            if(i)std::cout<<',';
            const int index=i<data.residentCount?i:i+2;
            const auto p=session.pose(index);
            std::cout<<'['<<p.x/26<<','<<p.y/26<<','<<unsigned(p.direction)<<','<<((t/2)%4)<<','<<p.moving<<','<<p.visible<<','<<p.species<<','<<p.speaking<<','<<p.lift<<']';
        }
        std::cout<<']';session.update((t+1)*100);
    }
    std::cout<<']';
}
