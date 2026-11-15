#include "presentation/TextBlock.h"
#include <cassert>
#include <cstdio>
#include <string>

using Ui::TextBlock;
static std::string line(const TextBlock& block, int index) {
    const auto& l=block.line(index);
    return std::string(block.text()+l.begin,block.text()+l.end);
}
int main() {
    TextBlock b; TextBlock::Options o; o.width=320;o.font=BitmapText::Font::PX32;
    b.setOptions(o);b.setText("妙蛙种子忘记寄生种子，学会种子炸弹。");
    auto m=b.measure();assert(m.lineCount==2&&m.height==66&&!m.overflow);
    assert(line(b,0)=="妙蛙种子忘记寄生种");
    assert(line(b,1)=="子，学会种子炸弹。");
    assert(!b.needsLayout()&&!b.setText(b.text()));
    assert(!b.setOptions(o)&&!b.needsLayout());
    o.width=128;b.setOptions(o);b.setText("甲乙丙丁，");
    assert(b.measure().lineCount==2&&line(b,0)=="甲乙丙"&&line(b,1)=="丁，");
    o.width=96;b.setOptions(o);b.setText("甲乙（丙丁");
    assert(b.measure().lineCount==2&&line(b,0)=="甲乙"&&line(b,1)=="（丙丁");
    o.font=BitmapText::Font::PX16;o.width=64;b.setOptions(o);b.setText("one two three");
    assert(b.measure().lineCount==2&&line(b,0)=="one two"&&line(b,1)=="three");
    o.width=16;b.setOptions(o);b.setText("abcdef");
    assert(b.measure().lineCount==3&&b.measure().forcedBreak&&line(b,2)=="ef");
    o.width=320;b.setOptions(o);b.setText("甲\r\n\n乙\r");
    assert(std::string(b.text())=="甲\n\n乙\n");
    assert(b.measure().lineCount==4&&line(b,1).empty()&&line(b,3).empty());
    o.width=24;b.setOptions(o);b.setText("abc \nX");
    assert(b.measure().lineCount==2&&line(b,0)=="abc"&&line(b,1)=="X");
    o.width=16;o.font=BitmapText::Font::PX32;b.setOptions(o);b.setText("甲\n");
    assert(b.measure().lineCount==2&&line(b,1).empty());
    o.width=320;o.font=BitmapText::Font::PX16;b.setOptions(o);
    b.setText("");assert(b.measure().lineCount==0&&b.measure().height==0);
    b.setText(nullptr);assert(!b.measure().overflow);
    const char bad[]={char(0xf0),char(0x9f),0};b.setText(bad);
    assert(std::string(b.text())=="??"&&b.measure().lineCount==1);
    b.setText("A♀♂中");assert(b.measure().lineCount==1&&b.line(0).width==40);
    o.font=BitmapText::Font::PX32;o.width=16;b.setOptions(o);b.setText("甲乙");
    assert(b.measure().lineCount==2&&b.measure().overflow&&b.measure().forcedBreak);
    o.width=0;b.setOptions(o);assert(b.measure().overflow&&b.measure().visibleLines==0);
    o.width=40;o.font=BitmapText::Font::PX16;o.maxLines=1;o.overflow=TextBlock::Overflow::ELLIPSIS;
    b.setOptions(o);b.setText("abcdefghijk");
    assert(b.measure().lineCount==3&&b.measure().visibleLines==1&&b.measure().overflow);
    assert(line(b,0)=="ab"&&b.line(0).dots==3&&b.line(0).width==40);
    o.width=96;b.setOptions(o);b.setText("甲（乙）丙丁戊");
    assert(b.measure().overflow&&line(b,0)=="甲（乙）");
    o.width=40;b.setOptions(o);b.setText("abcdefghijk");
    o.maxLines=0;o.maxHeight=15;b.setOptions(o);
    assert(b.measure().visibleLines==0&&b.measure().overflow);
    o.maxHeight=34;o.overflow=TextBlock::Overflow::CLIP;b.setOptions(o);
    assert(b.measure().visibleLines==2&&b.measure().visibleHeight==34);
    o.maxHeight=0;o.width=320;o.wrap=false;b.setOptions(o);
    b.setText(std::string(100,'a').c_str());
    assert(b.measure().lineCount==1&&b.measure().overflow);
    o.wrap=true;b.setOptions(o);std::string many;
    for(int i=0;i<80;++i)many+="甲\n";
    b.setText(many.c_str());assert(b.measure().lineCount==81&&b.measure().overflow&&b.measure().visibleLines==32);
    const std::string tooLong=std::string(510,'a')+"甲";
    b.setText(tooLong.c_str());assert(b.measure().truncated&&b.measure().overflow&&std::string(b.text())==std::string(510,'a'));
    b.setText("恢复");assert(!b.measure().truncated);
    char local[]="临时文字";b.setText(local);local[0]='x';assert(std::string(b.text())=="临时文字");
    // Mixed input/property stress: finite layout and retained byte boundaries
    // must survive arbitrary malformed bytes and narrow containers.
    uint32_t seed=42;
    for(int test=0;test<1500;++test) {
        std::string value;
        for(int i=0;i<180;++i){seed=seed*1664525u+1013904223u;value+=char(1+(seed>>24)%255);}
        o={};o.width=test%321;o.font=test%2?BitmapText::Font::PX16:BitmapText::Font::PX32;
        o.maxHeight=test%100;o.overflow=test%3?TextBlock::Overflow::CLIP:TextBlock::Overflow::ELLIPSIS;
        b.setOptions(o);b.setText(value.c_str());const auto result=b.measure();
        assert(result.lineCount<=TextBlock::TEXT_CAPACITY&&result.visibleLines<=TextBlock::LINE_CAPACITY);
        for(int i=0;i<result.visibleLines;++i) {
            const auto& l=b.line(i);assert(l.begin<=l.end&&l.end<std::strlen(b.text())+1);
            const char* p=b.text()+l.begin;while(p<b.text()+l.end){const char* prior=p;BitmapText::decode(p,b.text()+l.end);assert(p>prior);}
        }
    }
    std::puts("text-block: natural/CJK/word breaks, UTF-8, blank lines, metrics, cache, bounds and ellipsis OK");
}
