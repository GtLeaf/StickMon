#include "ui/ExploreScreens.h"
#include <cassert>
int main(){
    assert(AmoledV1::exploreStartAt(300,410));
    assert(AmoledV1::exploreSelectionBackAt(30,25));
    assert(!AmoledV1::exploreSelectionBackAt(270,410));
    assert(AmoledV1::exploreAreaAt(322,358,0,7)==1);
    assert(AmoledV1::exploreAreaAt(46,358,1,7)==0);
    assert(AmoledV1::exploreAreaAt(330,200,0,7)==-1);
    assert(AmoledV1::exploreAreaAt(322,333,0,7)==-1);
    assert(AmoledV1::exploreAreaAt(322,334,0,7)==1);
    assert(AmoledV1::exploreAreaAt(322,381,0,7)==1);
    assert(AmoledV1::exploreAreaAt(322,382,0,7)==-1);
    assert(AmoledV1::exploreAreaAt(184,300,1,7)==-1);
    assert(AmoledV1::exploreAreaAt(343,358,0,7)==1);
    assert(AmoledV1::exploreAreaAt(24,358,1,7)==0);
    assert(AmoledV1::exploreAreaAt(3624,358,0,7)==-1);
}
