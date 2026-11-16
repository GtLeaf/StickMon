#include "core/MapLayoutResources.h"
#include "game/ContentData.h"
#if STICKMON_EXTERNAL_MAP_LAYOUTS && !STICKMON_EXTERNAL_CONTENT
#include "core/ContentStore.h"
#include "core/MapLayoutValidation.h"
namespace {
// Like the font/icon caches, this belongs to the process, not static teardown:
// a host/browser platform allocator may already be gone during global dtors.
ContentStore* store=nullptr;
bool attempted=false;
bool ready=false;
}
bool beginMapLayoutResources() {
    if(attempted) return ready;
    attempted=true;
    if(!ResourceFS::ins().begin()) return false;
    store=new(std::nothrow) ContentStore;
    if(!store) return false;
    ContentData::bind(store);
    ready=MapLayoutValidation::validate() && store->healthy();
    return ready;
}
#else
bool beginMapLayoutResources() {return true;}
#endif
