#include "core/GameClockService.h"

void GameClockService::start(uint32_t nowMs, uint32_t gameMinutes) {
    set(nowMs, gameMinutes);
}

uint32_t GameClockService::minutesAt(uint32_t nowMs, float speed) const {
    return uint32_t(millisecondsAt(nowMs,speed)/60000);
}
uint64_t GameClockService::millisecondsAt(uint32_t nowMs,float speed)const{
    return uint64_t(anchorMinutes_)*60000+fractionMs_+
        uint64_t(double(uint32_t(nowMs-anchorMs_))*double(speed));
}
void GameClockService::setMilliseconds(uint32_t nowMs,uint64_t gameMs){
    anchorMs_=nowMs;anchorMinutes_=uint32_t(gameMs/60000);fractionMs_=uint32_t(gameMs%60000);
}

bool GameClockService::sync(uint32_t nowMs, float speed,
                            uint32_t& gameMinutes) {
    uint32_t total = minutesAt(nowMs, speed);
    if (total == gameMinutes) return false;
    gameMinutes = total;
    return true;
}

void GameClockService::set(uint32_t nowMs, uint32_t gameMinutes) {
    anchorMs_ = nowMs;
    anchorMinutes_ = gameMinutes;
    fractionMs_=0;
}
