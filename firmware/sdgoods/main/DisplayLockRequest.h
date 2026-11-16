#pragma once
#include <cstdint>
namespace Sdgoods {
// UI-thread request state; a failed final snapshot must not spin on flash.
class DisplayLockRequest {
public:
    void request(uint32_t now){if(!pending_){pending_=true;retryAt_=now;}}
    void toggle(uint32_t now){if(pending_)cancel();else request(now);}
    void cancel(){pending_=false;}
    bool pending() const{return pending_;}
    bool ready(uint32_t now) const{return pending_&&int32_t(now-retryAt_)>=0;}
    void retryLater(uint32_t now){retryAt_=now+1000;}
private:
    bool pending_=false;
    uint32_t retryAt_=0;
};
}
