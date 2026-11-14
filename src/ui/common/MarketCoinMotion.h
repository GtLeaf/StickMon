#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include "game/MarketTransaction.h"

namespace Ui {
// Bounded presentation state, independent from the authoritative balance.
class MarketCoinMotion {
public:
    static constexpr uint16_t ROLL_MS=450, WIDTH_MS=120, TAG_MS=900;
    struct TagFrame { int64_t delta=0; uint16_t elapsed=TAG_MS; bool visible=false; };
    struct Frame {
        float wheels[10]{};
        float widthDigits=1;
        uint8_t slots=1;
        uint32_t balance=0;
        bool rolling=false;
        TagFrame tags[2]{};
    };
    void reset(uint32_t balance) {
        balance_=balance;initialized_=true;elapsed_=ROLL_MS+WIDTH_MS;
        widthFrom_=widthPeak_=float(digits(balance));
        uint32_t n=balance;
        for(int i=0;i<10;++i){from_[i]=to_[i]=float(n%10);n/=10;}
        for(auto& tag:tags_)tag=TagFrame{};
    }
    bool committed(Game::MarketTransaction::CoinChange change) {
        if(change.before==change.after)return false;
        if(!initialized_||balance_!=change.before)reset(change.before);
        const auto current=frame();
        const bool income=change.after>change.before;
        const float oldProgress=ease(std::min(float(elapsed_)/ROLL_MS,1.0f));
        uint32_t n=change.after,old=balance_;
        for(int i=0;i<10;++i){
            const float remaining=to_[i]-(from_[i]+(to_[i]-from_[i])*oldProgress);
            from_[i]=current.wheels[i];to_[i]=float(n%10);
            if(old%10==n%10)to_[i]=from_[i]+remaining;
            else {
                if(income&&to_[i]<from_[i]-.0001f)to_[i]+=10;
                if(!income&&to_[i]>from_[i]+.0001f)to_[i]-=10;
            }
            n/=10;old/=10;
        }
        widthFrom_=current.widthDigits;
        widthPeak_=std::max({widthFrom_,float(digits(change.before)),float(digits(change.after))});
        balance_=change.after;elapsed_=0;
        // Two stable lanes prevent overlapping amounts and unbounded queues.
        int slot=0;
        if(tags_[0].elapsed<TAG_MS)
            slot=tags_[1].elapsed>=TAG_MS?1:(tags_[0].elapsed>=tags_[1].elapsed?0:1);
        tags_[slot]={change.delta(),0,true};
        return true;
    }
    bool update(uint32_t deltaMs) {
        if(!deltaMs||!busy())return false;
        elapsed_=uint16_t(std::min<uint64_t>(ROLL_MS+WIDTH_MS,uint64_t(elapsed_)+deltaMs));
        for(auto& tag:tags_){
            tag.elapsed=uint16_t(std::min<uint64_t>(TAG_MS,uint64_t(tag.elapsed)+deltaMs));
            tag.visible=tag.delta!=0&&tag.elapsed<TAG_MS;
        }
        return true;
    }
    Frame frame() const {
        Frame f;f.balance=balance_;f.rolling=initialized_&&elapsed_<ROLL_MS;
        const float q=ease(std::min(float(elapsed_)/ROLL_MS,1.0f));
        for(int i=0;i<10;++i){
            float value=from_[i]+(to_[i]-from_[i])*q;
            value=std::fmod(value,10.0f);if(value<0)value+=10;
            f.wheels[i]=value;
        }
        if(elapsed_<ROLL_MS)
            f.widthDigits=widthFrom_+(widthPeak_-widthFrom_)*smooth(std::min(float(elapsed_)/WIDTH_MS,1.0f));
        else {
            const float shrink=smooth(std::min(float(elapsed_-ROLL_MS)/WIDTH_MS,1.0f));
            f.widthDigits=widthPeak_+(float(digits(balance_))-widthPeak_)*shrink;
        }
        f.slots=uint8_t(std::max(1,std::min(10,int(std::ceil(widthPeak_)))));
        for(int i=0;i<2;++i)f.tags[i]=tags_[i];
        return f;
    }
private:
    static int digits(uint32_t n) { int count=1;while(n>=10){n/=10;++count;}return count; }
    static float ease(float q) { const float rest=1-q;return 1-rest*rest*rest; }
    static float smooth(float q) { return q*q*(3-2*q); }
    bool busy() const {
        return initialized_&&(elapsed_<ROLL_MS+WIDTH_MS||tags_[0].visible||tags_[1].visible);
    }
    uint32_t balance_=0;
    uint16_t elapsed_=ROLL_MS+WIDTH_MS;
    bool initialized_=false;
    float from_[10]{},to_[10]{};
    float widthFrom_=1,widthPeak_=1;
    TagFrame tags_[2]{};
};
} // namespace Ui
