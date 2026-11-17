#pragma once
#include "StandbyRuntime.h"
#include "StandbyBootFlow.h"
class ContentStore;
class GameEngine;
namespace Sdgoods {
class SdgoodsPlatform;
class RoomSession;
StandbyBoot resumeStandbyBoot(SdgoodsPlatform&,ContentStore&);
struct StandbyRetention {
    StandbyCheckpoint read() const;
    void store(const StandbyCheckpoint&);
    void clear();
};
using AdaptiveStandby=StandbyRuntime<SdgoodsPlatform,RoomSession,GameEngine,StandbyRetention>;
}
