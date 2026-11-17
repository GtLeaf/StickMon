#include <cassert>
#include <cstdio>
extern "C" unsigned int uzlib_crc32(const void*,unsigned int,unsigned int){assert(false);return 0;}
extern "C" unsigned int uzlib_adler32(const void*,unsigned int,unsigned int){assert(false);return 0;}
#if __has_include("ui/common/HomeLibraryUi.h")
#include "ui/common/HomeLibraryUi.h"
#include "game/HomeFurnitureService.h"
#define HAS_HOME_LIBRARY 1
#if STICKMON_UI_ROUND_TOUCH
#include "home_library_round_journey.h"
#endif
#endif

int main(int argc,char**argv){
#if HAS_HOME_LIBRARY
    Game::GameState state{};
    assert(!Game::HomeFurniture::owned(state,0));
    assert(Game::HomeFurniture::setPlaced(state,0,true)==Game::HomeFurniture::Result::NOT_OWNED);
    state.room.ownedFurniture=0x4001;state.room.placedFurniture=0x4000;
    assert(Game::HomeFurniture::commit(state,0,true,[]{return false;})==false);
    assert(state.room.ownedFurniture==0x4001&&state.room.placedFurniture==0x4000);
    assert(Game::HomeFurniture::commit(state,0,true,[]{return true;}));
    assert(state.room.placedFurniture==0x4001&&Game::HomeFurniture::visibleMask(state)==1);
    assert(Game::HomeFurniture::commit(state,0,false,[]{return true;}));
    assert(state.room.placedFurniture==0x4000&&state.room.ownedFurniture==0x4001);
    std::puts("home-library rules: owned placement, rollback, removal and unknown-bit preservation OK");
#if STICKMON_UI_ROUND_TOUCH
    homeRoundJourney(argc,argv);
#endif
#else
    assert(false&&"Home library placement UI has not been implemented");
#endif
}
