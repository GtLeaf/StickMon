#include "ui/ExplorePostcardLayout.h"
#include <cassert>
#include <cstdint>
int main(){
    AmoledV1::PostcardPoolClock c;
    assert(!c.tick(100,true));assert(!c.tick(3099,true));assert(c.tick(3100,true));assert(!c.tick(3101,true));
    c.manual(3200);assert(!c.tick(8199,true));assert(c.tick(8200,true));
    assert(!c.tick(9000,false));assert(!c.tick(19000,true));assert(!c.tick(21999,true));assert(c.tick(22000,true));
    c.pause();assert(!c.tick(30000,true));assert(!c.tick(32999,true));assert(c.tick(33000,true));
    c.pause();assert(!c.tick(UINT32_MAX-1000,true));assert(!c.tick(1998,true));assert(c.tick(1999,true));
}
