#include "platform/esp32/PlayerVolume.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
int scenario=0,mountCalls=0,formats=0,liveHandles=0;
int main(int argc,char**argv){
    assert(argc==2);scenario=std::atoi(argv[1]);
    const bool result=Stickmon::mountPlayerVolume();
    if(scenario==0){assert(result&&formats==1&&liveHandles==1);assert(Stickmon::mountPlayerVolume()&&liveHandles==1);}
    else if(scenario==3){assert(result&&mountCalls==1&&formats==0);assert(Stickmon::mountPlayerVolume()&&mountCalls==1);}
    else if(scenario==4)assert(!result&&mountCalls==0&&formats==0);
    else {
        assert(!result&&mountCalls==1&&formats==0&&liveHandles==0);
        assert(!Stickmon::mountPlayerVolume()&&mountCalls==2&&formats==0&&liveHandles==0);
    }
    std::printf("photo volume scenario%d: format only proven blank; shared mount preserved OK\n",scenario);
}
