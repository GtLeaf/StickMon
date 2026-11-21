#pragma once
#include "platform/api/PlatformServices.h"

// Native room tests use the same 8 MiB external-memory capacity as the touch devices.
class HomePower final:public Platform::IPowerDevice {
public:
    explicit HomePower(Platform::IPowerDevice& b):base(b){}
    int batteryLevel()override{return base.batteryLevel();}
    Platform::WakeReason wakeReason()const override{return base.wakeReason();}
    uint32_t hardwareRandom()override{return base.hardwareRandom();}
    size_t externalMemorySize()const override{return 8*1024*1024;}
    size_t externalMemoryFree()const override{return 8*1024*1024;}
    void restart()override{base.restart();}
    void enterDeepSleep(uint64_t ms,bool ext)override{base.enterDeepSleep(ms,ext);}
private:Platform::IPowerDevice& base;
};
