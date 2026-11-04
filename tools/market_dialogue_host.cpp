#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
#include "core/MarketDialogue.h"
#include "platform/desktop/DesktopPlatform.h"
#include "game/MarketDialogue.h"
#include "game/MarketSession.h"

static Market::Data town() {
    Market::Data d;d.version=2;d.residentCount=2;d.familyCount=1;d.families[0]={25,172};
    d.entry=Market::node(10,Market::HEIGHT-1);d.outward=0;d.intervalMin=d.intervalMax=1;d.stayMin=300;d.stayMax=400;
    for(int y=0;y<Market::HEIGHT;++y)for(int x=0;x<Market::WIDTH;++x){d.roads.set(Market::node(x,y));d.pedestrian.set(Market::node(x,y));}
    d.stopCount=4;d.stops[0]=Market::node(10,12);d.stops[1]=Market::node(12,12);
    d.stops[2]=Market::node(10,16);d.stops[3]=Market::node(12,16);
    for(int i=0;i<2;++i) {
        auto& r=d.residents[i];r.species=i?115:25;r.navigation=d.roads;
        r.activity.spawn=uint16_t(2+i);r.activity.behavior=AuthoredNpc::Behavior::IDLE;
    }
    return d;
}
static void place(Market::Session& s,int i,int x,int y) {
    auto& a=const_cast<Market::Actor&>(s.actor(i));a.from=a.to=Market::node(x,y);a.stage=2;
    a.goal=Market::NONE;a.progress=0;a.motionMs=0;a.direction=0;
}
static void catalog(const std::vector<uint8_t>& bytes) {
    Market::Dialogue::Catalog c;assert(c.load(bytes.data(),bytes.size()));assert(c.count()==28);
    assert(std::strcmp(c.name(352),"变隐龙")==0);assert(!c.name(65535));
    // Full coverage grows with the roster, not with all possible pairs.
    for(int context=1;context<=3;++context)for(uint16_t species=1;species<=386;++species) {
        uint32_t random=species;uint16_t recent[6]{};int previous=-1;
        for(int n=0;n<12;++n) {
            const int script=c.choose(Market::Dialogue::Context(context),species,false,random,recent,6);
            assert(script>=0&&script!=previous);previous=script;
            for(int line=0;line<c.lines(script);++line) {
                assert(c.text(script,line)&&*c.text(script,line));
                assert(c.role(script,line)!=Market::Dialogue::PARTNER);
                if(context!=3)assert(c.role(script,line)!=Market::Dialogue::OTHER);
            }
            for(int j=5;j>0;--j)recent[j]=recent[j-1];recent[0]=c.id(script);
        }
    }
    for(size_t n=0;n<bytes.size();n+=11){Market::Dialogue::Catalog bad;assert(!bad.load(bytes.data(),n));}
    for(size_t n=0;n<bytes.size();n+=17){auto corrupt=bytes;corrupt[n]^=0x80;Market::Dialogue::Catalog bad;assert(!bad.load(corrupt.data(),corrupt.size()));}
    // Reach structural/UTF-8 validation with an otherwise valid checksum.
    const auto checksum=[](std::vector<uint8_t>& b) {
        uint32_t crc=~0U;
        for(size_t i=20;i<b.size();++i){crc^=b[i];for(int bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320U&uint32_t(-int32_t(crc&1)));}
        crc^=~0U;for(int i=0;i<4;++i)b[16+i]=uint8_t(crc>>(i*8));
    };
    const size_t scripts=20+(bytes[6]|unsigned(bytes[7])<<8)*8;
    for(size_t offset:{size_t(20),scripts+3,scripts+12,bytes.size()-2}) {
        auto corrupt=bytes;corrupt[offset]=offset==20?0:0xff;checksum(corrupt);
        Market::Dialogue::Catalog bad;assert(!bad.load(corrupt.data(),corrupt.size()));
    }
    assert(!c.load(nullptr,0)&&c.count()==0);
}

struct DialogueMemory final : Platform::IMemoryAllocator {
    Platform::IMemoryAllocator& base;bool fail=false;int live=0;
    explicit DialogueMemory(Platform::IMemoryAllocator& allocator):base(allocator){}
    void* allocate(size_t bytes,bool external) override {
        assert(external);if(fail)return nullptr;auto* p=base.allocate(bytes,external);if(p)++live;return p;
    }
    void release(void* p) override {assert(p&&live>0);--live;base.release(p);}
    size_t externalFree() const override {return base.externalFree();}
};
struct DialogueFile final : Platform::IResourceFile {
    std::vector<uint8_t> bytes;size_t cursor=0;bool shortRead=false,open=true;
    DialogueFile(std::vector<uint8_t> data,bool shortened):bytes(std::move(data)),shortRead(shortened){}
    bool valid() const override {return open;}
    size_t size() const override {return bytes.size();}
    size_t position() const override {return cursor;}
    size_t read(void* out,size_t n) override {
        n=std::min(n,bytes.size()-cursor);if(shortRead&&n)--n;
        std::memcpy(out,bytes.data()+cursor,n);cursor+=n;return n;
    }
    bool seek(size_t pos) override {if(pos>bytes.size())return false;cursor=pos;return true;}
    void close() override {open=false;}
};
struct DialogueStore final : Platform::IResourceStore {
    Platform::IResourceStore& base;const std::vector<uint8_t>& bytes;int failure=0;
    DialogueStore(Platform::IResourceStore& store,const std::vector<uint8_t>& data):base(store),bytes(data){}
    bool mount() override {return base.mount();}
    size_t totalBytes() const override {return base.totalBytes();}
    size_t usedBytes() const override {return base.usedBytes();}
    Platform::ResourceFile open(const char* path) override {
        if(!std::strstr(path,"/text/market.smondialogue"))return base.open(path);
        if(failure==1)return {};
        auto data=bytes;if(failure==2)data.back()^=0x80;
        return Platform::ResourceFile(new DialogueFile(std::move(data),failure==3));
    }
};
static void controller(const std::vector<uint8_t>& bytes,const char* dataRoot) {
    DesktopPlatform desktop(dataRoot);auto base=desktop.serviceBundle();
    DialogueMemory memory(base.memory);DialogueStore store(base.resources,bytes);
    Platform::Services services{base.lifecycle,base.clock,base.logger,base.input,base.display,
        base.audio,base.imu,base.power,base.blobs,store,memory,base.peers};
    Platform::bind(services);assert(desktop.begin());assert(ResourcePack::ins().begin());
    Market::Dialogue::Catalog catalog;assert(catalog.load(bytes.data(),bytes.size()));
    auto data=town();Market::Session s;assert(s.reset(data,38,25,133,0));s.enableManual();
    uint32_t now=0;for(int i=0;i<70;++i){now+=100;s.update(now);}
    for(int failure=0;failure<5;++failure)for(int glyph:{16,32}) {
        MarketDialogue dialogue;store.failure=failure==4?0:failure;memory.fail=failure==4;
        assert(dialogue.load()==(failure==0));assert(memory.live==(failure==0?1:0));
        for(int iteration=0;iteration<12;++iteration) {
            place(s,0,8,8);place(s,2,7,8);place(s,3,6,8);
            assert(dialogue.begin(s,0,glyph==16?208:320,glyph));
            auto tick=[&](bool running=true) {now+=100;s.update(now,running);dialogue.update(s,now,running);};
            for(int n=0;n<5&&!dialogue.visible();++n)tick();assert(dialogue.visible());
            char before[256],after[256];dialogue.visibleText(before,sizeof(before));
            for(int n=0;n<10;++n)tick(false);dialogue.visibleText(after,sizeof(after));
            assert(std::strcmp(before,after)==0);
            int script=-1;for(int i=0;i<catalog.count();++i)if(catalog.id(i)==dialogue.scriptId())script=i;
            assert((script>=0)==(failure==0));
            const char* fallback[]={"你好，今天也来逛市集吗？","这里真热闹。","慢慢逛吧。"};
            const int count=script>=0?catalog.lines(script):3;
            for(int line=0;line<count;++line) {
                const auto role=script>=0?catalog.role(script,line):line==1?Market::Dialogue::SELF:Market::Dialogue::TARGET;
                const auto species=s.actor(s.conversationActor(role)).species;
                const char* name=catalog.name(species);if(!name)name=findSpecies(species)->name;
                const std::string expected=std::string(name)+"："+(script>=0?catalog.text(script,line):fallback[line]);
                std::string actual;
                for(int page=0;actual!=expected&&page<12;++page) {
                    dialogue.advance(s);assert(dialogue.complete());
                    const auto size=dialogue.visibleText(after,sizeof(after));assert(size==std::strlen(after));
                    for(size_t capacity=1;capacity<8;++capacity) {
                        char shortText[8];const auto n=dialogue.visibleText(shortText,capacity);
                        assert(n<capacity&&shortText[n]==0);
                        // Every truncated prefix ends before a complete UTF-8 character.
                        assert(n==size||(uint8_t(after[n])&0xc0)!=0x80);
                    }
                    int width=0,rows=1;
                    for(size_t i=0;i<size;++i) {
                        const auto c=uint8_t(after[i]);
                        if(c=='\n'){assert(width<=(glyph==16?208:320));width=0;++rows;}
                        else if((c&0xc0)!=0x80)width+=c<128?glyph/2:glyph;
                        if(c!='\n')actual+=after[i];
                    }
                    assert(rows<=2&&width<=(glyph==16?208:320));
                    assert(expected.compare(0,actual.size(),actual)==0);
                    dialogue.advance(s);
                }
                assert(actual==expected);
            }
            assert(!dialogue.active()&&!s.conversing());
        }
        dialogue.reset();assert(memory.live==0);
    }
    // Leaving while typing cancels the controller without retaining a lock.
    MarketDialogue dialogue;memory.fail=false;store.failure=0;assert(dialogue.load());
    place(s,0,8,8);place(s,2,7,8);assert(dialogue.begin(s,0,208,16));s.leave();
    dialogue.update(s,now,true);assert(!dialogue.active());dialogue.reset();assert(memory.live==0);
}
static void session() {
    auto d=town();Market::Session s;assert(s.reset(d,38,1,133,0));s.enableManual();
    uint32_t now=0;
    for(int i=0;i<70;++i){now+=100;s.update(now);}
    place(s,2,6,6);place(s,3,5,6); // keep the doorway clear for arriving visitors
    while(s.visitPhase()!=Market::VisitPhase::VISITING&&now<200000){now+=100;s.update(now);}
    assert(s.visitPhase()==Market::VisitPhase::VISITING);
    place(s,0,20,20);place(s,1,21,20);place(s,2,6,6);place(s,3,5,6);place(s,4,7,6);place(s,5,8,7);
    auto& p=const_cast<Market::Actor&>(s.actor(2));p.to=Market::node(6,7);p.motionMs=50;
    auto& target=const_cast<Market::Actor&>(s.actor(4));target.to=Market::node(8,6);target.progress=0;target.stepTicks=3;target.direction=3;
    auto reference=s;const auto first=s.playerPose();const auto outsider=s.pose(0);const auto time=s.timeMs();
    assert(s.nearbyNpc(4,true)==4&&s.beginConversation(4));
    assert(s.conversationActor(0)==2&&s.conversationActor(1)==4&&s.conversationActor(2)==5&&s.conversationActor(3)==3);
    assert(s.pose(2).x==first.x&&s.pose(2).y==first.y);assert(!s.beginConversation(0));
    s.setDirection(7);assert(!s.walkTo(d.stops[0]));
    now+=100;s.update(now);assert(!s.conversationReady());assert(s.playerPose().y>first.y);
    s.pause(true);const auto held=s.playerPose();now+=100;s.update(now);assert(s.playerPose().y==held.y);
    s.pause(false);now+=100;s.update(now);now+=100;s.update(now);assert(s.conversationReady());
    assert(s.timeMs()==time&&s.pose(0).x==outsider.x&&s.pose(0).y==outsider.y);
    assert(!s.playerPose().moving&&s.actor(4).from==s.actor(4).to);
    for(int role=0;role<4;++role) {
        const int i=s.conversationActor(role),to=s.conversationActor(role==0||role==3?1:0);
        const auto a=s.pose(i),b=s.pose(to);
        assert(a.direction==Market::directionForDelta(int(b.x-a.x),int(b.y-a.y)));
    }
    s.conversationSpeaker(5);assert(s.pose(5).speaking&&!s.pose(4).speaking);
    for(int i=0;i<20;++i){now+=100;s.update(now);assert(s.timeMs()==time);}
    s.endConversation();assert(!s.conversing());
    // Complete only the already-started edge in the reference. The remaining
    // route, visitor budgets and random stream must then run identically.
    for(int i:{2,4}){auto& a=const_cast<Market::Actor&>(reference.actor(i));a.from=a.to;a.progress=0;a.motionMs=0;}
    reference.setDirection(-1);reference.update(now,false);
    for(int n=0;n<300;++n) {
        now+=100;s.update(now);reference.update(now);
        assert(s.timeMs()==reference.timeMs()&&s.visitPhase()==reference.visitPhase());
        for(int i=0;i<s.count();++i) {
            const auto& a=s.actor(i);const auto& b=reference.actor(i);
            assert(a.from==b.from&&a.to==b.to&&a.goal==b.goal&&a.species==b.species&&a.wait==b.wait&&a.stage==b.stage);
        }
    }
    place(s,0,8,8);place(s,2,7,8);place(s,3,6,8);
    assert(s.beginConversation(0)&&s.conversationActor(2)==-1); // same species as guest, still a solo resident
    s.leave();assert(!s.conversing()&&s.exiting());
    Market::Session disabled;assert(disabled.reset(d,38,1,0,0));
    assert(!disabled.beginConversation(0));
}
int main(int argc,char** argv) {
    assert(argc==3);std::ifstream input(argv[1],std::ios::binary);
    const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(input),{}};
    catalog(bytes);session();controller(bytes,argv[2]);
    std::puts("market-dialogue: catalog CRC group settle facing pause resume RNG UTF-8 pagination resource failures OK");
}
