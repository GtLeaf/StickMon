#include "core/ContentStore.h"
#include "platform/desktop/DesktopPlatform.h"
#include <cassert>
#include <cstdlib>
int main(int argc,char** argv) {
    assert(argc==3);DesktopPlatform platform(argv[1]);Platform::bind(platform.serviceBundle());
    assert(platform.begin());assert(ResourceFS::ins().begin());
    ContentStore store;ContentData::bind(&store);
    ContentData::Table<uint16_t,49> values{10};
    const bool wanted=std::atoi(argv[2]);assert(values.ready()==wanted);
    if(wanted) for(unsigned i=0;i<49;++i) assert(values[i]==i);
}
