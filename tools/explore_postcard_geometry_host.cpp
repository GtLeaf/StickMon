#include "ui/ExploreScreens.h"
#include <cassert>
int main(){
    assert(AmoledV1::exploreStartAt(300,410));
    assert(AmoledV1::exploreSelectionBackAt(30,25));
    assert(!AmoledV1::exploreSelectionBackAt(270,410));
    assert(AmoledV1::exploreAreaAt(330,155,0,7)==1);
    assert(AmoledV1::exploreAreaAt(30,155,1,7)==0);
    assert(AmoledV1::exploreAreaAt(330,200,0,7)==1);
    assert(AmoledV1::exploreAreaAt(184,300,1,7)==-1);
    assert(AmoledV1::exploreAreaAt(363,176,0,7)==1);
    assert(AmoledV1::exploreAreaAt(4,176,1,7)==0);
    assert(AmoledV1::exploreAreaAt(364,176,0,7)==-1);
}
