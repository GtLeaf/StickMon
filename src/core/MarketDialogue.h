#pragma once

#include <algorithm>
#include <cstdio>
#include <cstring>
#include "core/ResourcePack.h"
#include "presentation/TextLayout.h"
#include "game/MarketDialogue.h"
#include "game/MarketSession.h"
#include "game/Species.h"

// Shared two-line typewriter and pagination for market conversations.
class MarketDialogueText {
    char full_[256]{};
    Ui::TextLayout layout_;
    size_t chunkStart_=0,pageEnd_=0,revealed_=0,pageCharacters_=0;
    int pageFirstLine_=0,pageLineCount_=0;
    uint32_t visualMs_=0,printMs_=0;
    int cachedLines() const {return std::min(layout_.measure().lineCount,Ui::TextLayout::LINE_CAPACITY);}
    void makePage() {
        pageLineCount_=std::min(2,cachedLines()-pageFirstLine_);
        pageCharacters_=revealed_=0;printMs_=0;
        for(int row=0;row<pageLineCount_;++row) {
            const auto& line=layout_.line(pageFirstLine_+row);
            const char* p=layout_.text()+line.begin;const char* end=layout_.text()+line.end;
            while(p<end){BitmapText::decode(p,end);++pageCharacters_;}
        }
        const int nextLine=pageFirstLine_+pageLineCount_;
        if(nextLine<cachedLines())pageEnd_=chunkStart_+layout_.line(nextLine).begin;
        else if(pageLineCount_) {
            size_t consumed=layout_.line(nextLine-1).end;
            // Continue a bounded layout chunk after its consumed soft spaces
            // or one explicit separator; retain subsequent blank lines.
            const char* text=layout_.text();
            while(text[consumed]==' ')++consumed;
            if(text[consumed]=='\n')++consumed;
            pageEnd_=chunkStart_+consumed;
        } else pageEnd_=std::strlen(full_);
    }
public:
    void clear() {
        full_[0]=0;layout_.setText("");
        chunkStart_=pageEnd_=revealed_=pageCharacters_=0;
        pageFirstLine_=pageLineCount_=0;visualMs_=printMs_=0;
    }
    void start(const char* speaker,const char* text,int width,BitmapText::Font font) {
        std::snprintf(full_,sizeof(full_),"%s%s",speaker,text);
        chunkStart_=0;pageFirstLine_=0;visualMs_=0;
        Ui::TextLayout::Options options;options.width=width;options.lineGap=8;options.font=font;
        layout_.setOptions(options);layout_.setText(full_);
        std::strcpy(full_,layout_.text());
        makePage();
    }
    bool update(uint32_t delta) {
        const auto before=revealed_;
        const auto arrowPhase=visualMs_/160%4;
        visualMs_=(visualMs_+delta%640)%640;
        if(!complete()) {
            printMs_+=delta;
            while(printMs_>=40&&!complete()){printMs_-=40;++revealed_;}
        }
        return revealed_!=before||(complete()&&visualMs_/160%4!=arrowPhase);
    }
    // One tap fills the current page, advances to the next one, or reports
    // that the caller can advance the conversation.
    bool advance() {
        if(!complete()){revealed_=pageCharacters_;return false;}
        if(!full_[pageEnd_])return true;
        pageFirstLine_+=pageLineCount_;
        if(pageFirstLine_>=cachedLines()) {
            chunkStart_=pageEnd_;layout_.setText(full_+chunkStart_);pageFirstLine_=0;
        }
        makePage();return false;
    }
    bool complete() const {return revealed_>=pageCharacters_;}
    const Ui::TextLayout& textLayout() const {return layout_;}
    int pageFirstLine() const {return pageFirstLine_;}
    int pageLineCount() const {return pageLineCount_;}
    size_t revealedCharacters() const {return revealed_;}
    uint32_t visualMs() const {return visualMs_;}
    // Compatibility view of the rendered page prefix, never an input to layout.
    size_t visibleText(char* text,size_t capacity) const {
        if(!capacity)return 0;
        size_t out=0,remaining=revealed_;text[0]=0;
        for(int row=0;row<pageLineCount_&&remaining;++row) {
            if(row) {if(out+1>=capacity)break;text[out++]='\n';}
            const auto& line=layout_.line(pageFirstLine_+row);
            const char* p=layout_.text()+line.begin;const char* end=layout_.text()+line.end;
            while(p<end&&remaining) {
                const char* start=p;BitmapText::decode(p,end);
                const size_t bytes=size_t(p-start);
                if(out+bytes>=capacity){text[out]=0;return out;}
                std::memcpy(text+out,start,bytes);out+=bytes;--remaining;
            }
        }
        text[out]=0;return out;
    }
};

// Per-visit dialogue state and optional resource ownership. Stick never enables
// this controller; the market navigation random stream and save schema stay out.
class MarketDialogue {
    Market::Dialogue::Catalog catalog_;
    uint8_t* resource_=nullptr;
    uint16_t recent_[6]{};
    uint32_t random_=0x761d39b1,lastMs_=0,visualMs_=0,environmentMs_=0;
    uint16_t openingMs_=0;
    int script_=-1,line_=0,width_=208;
    BitmapText::Font font_=BitmapText::Font::PX16;
    MarketDialogueText text_;
    bool active_=false,visible_=false,clockValid_=false;
    void release() {if(resource_){Platform::memory().release(resource_);resource_=nullptr;}catalog_={};}
    void loadLine(Market::Session& s) {
        const auto role=script_>=0?catalog_.role(script_,line_):
            (line_==1?Market::Dialogue::SELF:Market::Dialogue::TARGET);
        const int actor=s.conversationActor(role);
        const uint16_t species=actor>=0?s.actor(actor).species:0;
        const char* name=catalog_.name(species);
        if(!name){const auto* found=findSpecies(species);name=found?found->name:"精灵";}
        static constexpr const char* fallback[]={"你好，今天也来逛市集吗？","这里真热闹。","慢慢逛吧。"};
        const char* text=script_>=0?catalog_.text(script_,line_):fallback[line_];
        char speaker[64];
        std::snprintf(speaker,sizeof(speaker),"%s：",name);
        text_.start(speaker,text,width_,font_);s.conversationSpeaker(actor);
    }
public:
    ~MarketDialogue(){release();}
    MarketDialogue()=default;
    MarketDialogue(const MarketDialogue&)=delete;
    MarketDialogue& operator=(const MarketDialogue&)=delete;
    void reset() {
        release();std::fill_n(recent_,6,0);
        active_=visible_=clockValid_=false;visualMs_=environmentMs_=0;random_=0x761d39b1;
        text_.clear();
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
        width_=std::max(64,width);
        font_=glyph==32?BitmapText::Font::PX32:glyph==20?BitmapText::Font::PX20:BitmapText::Font::PX16;
        const auto context=target<s.data().residentCount?Market::Dialogue::Context::RESIDENT:
            s.conversationActor(2)>=0?Market::Dialogue::Context::PAIR:Market::Dialogue::Context::SOLO;
        random_^=(s.timeMs()+uint32_t(target+1)*0x9e3779b9U);if(!random_)random_=1;
        script_=catalog_.choose(context,s.actor(target).species,s.conversationActor(3)>=0,random_,recent_,6);
        if(script_>=0){for(int i=5;i>0;--i)recent_[i]=recent_[i-1];recent_[0]=catalog_.id(script_);}
        active_=true;visible_=false;openingMs_=0;line_=0;text_.clear();
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
        if(visible_)text_.update(delta);
        return delta!=0;
    }
    void advance(Market::Session& s) {
        if(!visible_||!text_.advance())return;
        const int count=script_>=0?catalog_.lines(script_):3;
        if(++line_<count){loadLine(s);return;}
        s.endConversation();active_=visible_=false;
    }
    bool active() const {return active_;}
    bool visible() const {return visible_;}
    bool complete() const {return visible_&&text_.complete();}
    const MarketDialogueText& text() const {return text_;}
    const Ui::TextLayout& textLayout() const {return text_.textLayout();}
    int pageFirstLine() const {return text_.pageFirstLine();}
    int pageLineCount() const {return text_.pageLineCount();}
    size_t revealedCharacters() const {return text_.revealedCharacters();}
    uint32_t visualMs() const {return visualMs_;}
    uint32_t environmentMs() const {return environmentMs_;}
    uint16_t openingMs() const {return openingMs_;}
    uint16_t scriptId() const {return catalog_.id(script_);}
    size_t visibleText(char* text,size_t capacity) const {return text_.visibleText(text,capacity);}
};
