#pragma once

#include <algorithm>
#include <cstdio>
#include <cstring>
#include "core/ResourcePack.h"
#include "game/MarketDialogue.h"
#include "game/MarketSession.h"
#include "game/Species.h"

// Per-visit dialogue state and optional resource ownership. Stick never enables
// this controller; the market navigation random stream and save schema stay out.
class MarketDialogue {
    Market::Dialogue::Catalog catalog_;
    uint8_t* resource_=nullptr;
    uint16_t recent_[6]{};
    uint32_t random_=0x761d39b1,lastMs_=0,visualMs_=0,environmentMs_=0;
    uint16_t openingMs_=0,printMs_=0;
    int script_=-1,line_=0,width_=208,glyph_=16;
    size_t cursor_=0,pageEnd_=0,revealed_=0;
    char full_[256]{},page_[256]{};
    bool active_=false,visible_=false,clockValid_=false;
    static size_t next(const char* text,size_t i) {
        const auto c=uint8_t(text[i]);return i+(c<0x80?1:c<0xe0?2:c<0xf0?3:4);
    }
    static bool punctuation(const char* p) {
        static constexpr const char* marks[]={"，","。","！","？","、","：","；","…"};
        for(auto mark:marks)if(!std::strncmp(p,mark,std::strlen(mark)))return true;
        return false;
    }
    void release() {if(resource_){Platform::memory().release(resource_);resource_=nullptr;}catalog_={};}
    void makePage() {
        size_t out=0,pos=cursor_;
        for(int row=0;row<2&&full_[pos];++row) {
            const size_t start=pos;size_t previous=pos;int width=0;
            while(full_[pos]) {
                const int advance=uint8_t(full_[pos])<0x80?glyph_/2:glyph_;
                if(width+advance>width_)break;
                previous=pos;width+=advance;pos=next(full_,pos);
            }
            // Keep closing punctuation with its preceding character.
            if(full_[pos]&&punctuation(full_+pos)&&previous>start)pos=previous;
            const size_t count=pos-start;
            std::memcpy(page_+out,full_+start,count);out+=count;
            if(row==0&&full_[pos])page_[out++]='\n';
        }
        page_[out]=0;pageEnd_=pos;revealed_=0;printMs_=0;
    }
    void loadLine(Market::Session& s) {
        const auto role=script_>=0?catalog_.role(script_,line_):
            (line_==1?Market::Dialogue::SELF:Market::Dialogue::TARGET);
        const int actor=s.conversationActor(role);
        const uint16_t species=actor>=0?s.actor(actor).species:0;
        const char* name=catalog_.name(species);
        if(!name){const auto* found=findSpecies(species);name=found?found->name:"精灵";}
        static constexpr const char* fallback[]={"你好，今天也来逛市集吗？","这里真热闹。","慢慢逛吧。"};
        const char* text=script_>=0?catalog_.text(script_,line_):fallback[line_];
        std::snprintf(full_,sizeof(full_),"%s：%s",name,text);
        cursor_=0;makePage();s.conversationSpeaker(actor);
    }
public:
    ~MarketDialogue(){release();}
    MarketDialogue()=default;
    MarketDialogue(const MarketDialogue&)=delete;
    MarketDialogue& operator=(const MarketDialogue&)=delete;
    void reset() {
        release();std::fill_n(recent_,6,0);
        active_=visible_=clockValid_=false;visualMs_=environmentMs_=0;random_=0x761d39b1;
    }
    bool load() {
        release();char path[128];
        std::snprintf(path,sizeof(path),"%s/text/market.smondialogue",ResourcePack::ins().root());
        auto file=Platform::resources().open(path);
        if(!file||file.size()<20||file.size()>Market::Dialogue::Catalog::MAX_BYTES)return false;
        const auto size=file.size();
        resource_=static_cast<uint8_t*>(Platform::memory().allocate(size,true));
        if(!resource_)return false;
        if(file.read(resource_,size)!=size||!catalog_.load(resource_,size)){release();return false;}
        return true;
    }
    bool begin(Market::Session& s,int target,int width,int glyph) {
        if(active_||!s.beginConversation(target))return false;
        width_=std::max(64,width);glyph_=glyph==32?32:16;
        const auto context=target<s.data().residentCount?Market::Dialogue::Context::RESIDENT:
            s.conversationActor(2)>=0?Market::Dialogue::Context::PAIR:Market::Dialogue::Context::SOLO;
        random_^=(s.timeMs()+uint32_t(target+1)*0x9e3779b9U);if(!random_)random_=1;
        script_=catalog_.choose(context,s.actor(target).species,s.conversationActor(3)>=0,random_,recent_,6);
        if(script_>=0){for(int i=5;i>0;--i)recent_[i]=recent_[i-1];recent_[0]=catalog_.id(script_);}
        active_=true;visible_=false;openingMs_=printMs_=0;line_=0;full_[0]=page_[0]=0;
        clockValid_=false;return true;
    }
    void suspendClock(){clockValid_=false;}
    bool update(Market::Session& s,uint32_t now,bool running) {
        uint32_t delta=clockValid_?now-lastMs_:0;lastMs_=now;clockValid_=true;
        if(delta>250||!running)delta=0;
        if(!active_)return false;
        if(!s.conversing()){active_=visible_=false;return true;}
        visualMs_+=delta;environmentMs_+=delta;
        if(s.conversationReady()) {
            openingMs_=uint16_t(std::min<uint32_t>(150,openingMs_+delta));
            if(openingMs_>=150&&!visible_){visible_=true;loadLine(s);}
        }
        if(visible_&&page_[revealed_]) {
            printMs_=uint16_t(printMs_+delta);
            while(printMs_>=40&&page_[revealed_]) {
                printMs_-=40;
                if(page_[revealed_]=='\n')++revealed_;
                if(page_[revealed_])revealed_=next(page_,revealed_);
            }
        }
        return delta!=0;
    }
    void advance(Market::Session& s) {
        if(!visible_)return;
        if(!complete()){revealed_=std::strlen(page_);return;}
        if(full_[pageEnd_]){cursor_=pageEnd_;makePage();return;}
        const int count=script_>=0?catalog_.lines(script_):3;
        if(++line_<count){loadLine(s);return;}
        s.endConversation();active_=visible_=false;
    }
    bool active() const {return active_;}
    bool visible() const {return visible_;}
    bool complete() const {return visible_&&!page_[revealed_];}
    uint32_t visualMs() const {return visualMs_;}
    uint32_t environmentMs() const {return environmentMs_;}
    uint16_t openingMs() const {return openingMs_;}
    uint16_t scriptId() const {return catalog_.id(script_);}
    size_t visibleText(char* text,size_t capacity) const {
        if(!capacity)return 0;
        size_t n=std::min(revealed_,capacity-1);
        while(n<revealed_&&n&&(uint8_t(page_[n])&0xc0)==0x80)--n;
        std::memcpy(text,page_,n);text[n]=0;return n;
    }
};
