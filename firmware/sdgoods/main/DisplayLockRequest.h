#pragma once
#include <cstdint>
namespace Sdgoods {
// UI-thread request state; scene transitions may defer completing a lock.
class DisplayLockRequest {
public:
    void request(uint32_t now){if(!pending_){pending_=true;retryAt_=now;}}
    void toggle(uint32_t now){if(pending_)cancel();else request(now);}
    void cancel(){pending_=false;}
    bool pending() const{return pending_;}
    bool ready(uint32_t now) const{return pending_&&int32_t(now-retryAt_)>=0;}
private:
    bool pending_=false;
    uint32_t retryAt_=0;
};
}
