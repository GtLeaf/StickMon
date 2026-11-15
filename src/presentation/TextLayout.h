#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include "presentation/BitmapTextMetrics.h"

namespace Ui {
// Pure bounded UTF-8 layout. Owns normalized text and cached line records;
// independent of Canvas, font resources, playback and rendering.
class TextLayout {
public:
    static constexpr int TEXT_CAPACITY=512,LINE_CAPACITY=32;
    enum class Align : uint8_t { LEFT,CENTER,RIGHT };
    enum class Overflow : uint8_t { CLIP,ELLIPSIS };
    struct Options {
        int width=0,maxHeight=0;
        BitmapText::Font font=BitmapText::Font::PX16;
        uint16_t lineGap=2,maxLines=0;
        Align align=Align::LEFT;
        Overflow overflow=Overflow::CLIP;
        bool wrap=true;
        bool operator==(const Options& b) const {
            return width==b.width&&maxHeight==b.maxHeight&&font==b.font&&lineGap==b.lineGap&&
                maxLines==b.maxLines&&align==b.align&&overflow==b.overflow&&wrap==b.wrap;
        }
    };
    struct Line { uint16_t begin=0,end=0;int width=0;uint8_t dots=0; };
    struct Measurement {
        int lineCount=0,height=0,visibleLines=0,visibleHeight=0;
        bool overflow=false,truncated=false,forcedBreak=false;
    };
    bool setText(const char* value) {
        char normalized[TEXT_CAPACITY];int used=0;bool truncated=false;
        const char* p=value?value:"";const char* end=p+std::strlen(p);
        while(p<end) {
            uint32_t cp=BitmapText::decode(p,end);
            if(cp=='\r'){if(p<end&&*p=='\n')++p;cp='\n';}
            if(cp=='\t')cp=' ';
            char bytes[4];const int n=BitmapText::encode(cp,bytes);
            if(used+n>=TEXT_CAPACITY){truncated=true;break;}
            std::memcpy(normalized+used,bytes,n);used+=n;
        }
        normalized[used]=0;
        if(length_==used&&truncated_==truncated&&std::memcmp(text_,normalized,used)==0)return false;
        std::memcpy(text_,normalized,used+1);length_=used;truncated_=truncated;dirty_=true;return true;
    }
    bool setOptions(Options options) {
        options.width=std::clamp(options.width,0,32767);
        options.maxHeight=std::clamp(options.maxHeight,0,32767);
        if(options_==options)return false;
        options_=options;dirty_=true;return true;
    }
    const char* text() const { return text_; }
    const Options& options() const { return options_; }
    bool needsLayout() const { return dirty_; }
    const Measurement& measure() const { if(dirty_)layout();return measurement_; }
    const Line& line(int index) const { measure();return lines_[index]; }


private:
    char text_[TEXT_CAPACITY]{};
    int length_=0;bool truncated_=false;
    Options options_{};
    mutable bool dirty_=true;
    mutable Line lines_[LINE_CAPACITY]{};
    mutable Measurement measurement_{};
    static bool word(uint32_t cp) {
        return (cp>='a'&&cp<='z')||(cp>='A'&&cp<='Z')||(cp>='0'&&cp<='9')||cp=='_';
    }
    static bool opening(uint32_t cp) {
        switch(cp){case '(':case '[':case '{':case 0xff08:case 0x3010:case 0x300a:
            case 0x3008:case 0x300c:case 0x300e:case 0x201c:case 0x2018:return true;default:return false;}
    }
    static bool closing(uint32_t cp) {
        switch(cp){case ',':case '.':case '!':case '?':case ':':case ';':case '%':
            case ')':case ']':case '}':case 0xff0c:case 0x3002:case 0x3001:case 0xff01:
            case 0xff1f:case 0xff1a:case 0xff1b:case 0xff09:case 0x3011:case 0x300b:
            case 0x3009:case 0x300d:case 0x300f:case 0x201d:case 0x2019:case 0x2026:return true;
            default:return false;}
    }
    uint32_t at(int offset) const {
        if(offset>=length_)return 0;
        const char* p=text_+offset;return BitmapText::decode(p,text_+length_);
    }
    int width(int begin,int end) const {
        int sum=0;const char* p=text_+begin;
        while(p<text_+end)sum+=BitmapText::advance(BitmapText::decode(p,text_+end),options_.font);
        return sum;
    }
    void emit(int begin,int end) const {
        while(end>begin&&text_[end-1]==' ')--end;
        const int w=width(begin,end),i=measurement_.lineCount++;
        if(i<LINE_CAPACITY)lines_[i]={uint16_t(begin),uint16_t(end),w,0};
        if(w>options_.width)measurement_.overflow=true;
    }
    void layout() const {
        dirty_=false;measurement_={};measurement_.truncated=truncated_;
        measurement_.overflow=truncated_;
        if(!length_)return;
        if(options_.width<=0){measurement_.overflow=true;return;}
        int start=0;
        while(start<=length_) {
            if(start==length_){if(text_[length_-1]=='\n')emit(start,start);break;}
            int cursor=start,used=0,best=-1;
            bool wrapped=false;
            while(cursor<length_&&text_[cursor]!='\n') {
                const char* p=text_+cursor;const uint32_t cp=BitmapText::decode(p,text_+length_);
                const int next=int(p-text_),adv=BitmapText::advance(cp,options_.font);
                if(options_.wrap&&used+adv>options_.width) {
                    int stop=best>start?best:cursor;
                    if(stop==start)stop=next;
                    if(best<=start)measurement_.forcedBreak=true;
                    emit(start,stop);start=stop;
                    while(start<length_&&text_[start]==' ')++start;
                    if(start<length_&&text_[start]=='\n')++start;
                    wrapped=true;break;
                }
                used+=adv;cursor=next;
                const uint32_t nextCp=at(cursor);
                if(!opening(cp)&&!closing(nextCp)&&!(word(cp)&&word(nextCp)))best=cursor;
            }
            if(wrapped)continue;
            emit(start,cursor);
            if(cursor==length_)break;
            start=cursor+1;
        }
        const int fontHeight=BitmapText::height(options_.font),stride=fontHeight+options_.lineGap;
        measurement_.height=measurement_.lineCount?measurement_.lineCount*stride-options_.lineGap:0;
        int visible=std::min(measurement_.lineCount,LINE_CAPACITY);
        if(options_.maxLines)visible=std::min(visible,int(options_.maxLines));
        if(options_.maxHeight)visible=std::min(visible,(options_.maxHeight+options_.lineGap)/stride);
        measurement_.visibleLines=visible;
        measurement_.visibleHeight=visible?visible*stride-options_.lineGap:0;
        measurement_.overflow|=visible<measurement_.lineCount;
        if(measurement_.overflow&&visible&&options_.overflow==Overflow::ELLIPSIS) {
            auto& last=lines_[visible-1];
            last.dots=uint8_t(std::min(3,options_.width/BitmapText::advance('.',options_.font)));
            const int available=options_.width-last.dots*BitmapText::advance('.',options_.font);
            int end=last.begin,used=0,safeEnd=end,safeWidth=0;const char* p=text_+end;
            while(p<text_+last.end) {
                const uint32_t cp=BitmapText::decode(p,text_+last.end);
                if(used+BitmapText::advance(cp,options_.font)>available)break;
                used+=BitmapText::advance(cp,options_.font);end=int(p-text_);
                if(!opening(cp)&&cp!=' '){safeEnd=end;safeWidth=used;}
            }
            last.end=uint16_t(safeEnd);last.width=safeWidth+last.dots*BitmapText::advance('.',options_.font);
        }
    }
};
}
