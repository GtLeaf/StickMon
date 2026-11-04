#pragma once
#include <istream>
#include "game/MarketData.h"

inline bool readMarket(std::istream& in,Market::Data& d) {
    unsigned version,entry,outward,residents,families,stops,lo,hi,stayLo,stayHi;
    if(!(in>>version>>entry>>outward>>residents>>families>>stops>>lo>>hi>>stayLo>>stayHi)||
       (version!=1&&version!=2)||entry>=Market::NODES||outward>3||residents>12||families>64||stops>48||
       lo<1||hi<lo||hi>600||stayLo<1||stayHi<stayLo||stayHi>900)return false;
    d.version=version;d.entry=entry;d.outward=outward;d.residentCount=residents;
    d.familyCount=families;d.stopCount=stops;d.intervalMin=lo;d.intervalMax=hi;d.stayMin=stayLo;d.stayMax=stayHi;
    auto mask=[&](Market::Mask& m){for(auto& b:m.bytes){unsigned v;if(!(in>>v)||v>255)return false;b=v;}return true;};
    if(!mask(d.roads)||!mask(d.pedestrian))return false;
    for(auto& layer:d.map.layers)for(int i=0;i<Market::CELLS;++i){
        unsigned v;if(!(in>>v)||v>65535)return false;
        layer[i]=v;
    }
    for(unsigned i=0;i<residents;++i) {
        auto& r=d.residents[i];auto& a=r.activity;
        unsigned species,spawn,direction,behavior,step,waitLo,waitHi,count;uint32_t seed;
        if(!(in>>species>>spawn>>direction>>behavior>>step>>waitLo>>waitHi>>seed>>count)||
           !species||species>65535||spawn>=Market::CELLS||direction>3||behavior>2||step<2||step>20||
           waitLo<1||waitHi<waitLo||waitHi>600||count>32)return false;
        r.species=species;a.spawn=spawn;a.direction=direction;a.behavior=AuthoredNpc::Behavior(behavior);
        a.stepTicks=step;a.waitMin=waitLo;a.waitMax=waitHi;a.seed=seed;a.patrolCount=count;
        for(unsigned j=0;j<count;++j){unsigned v;if(!(in>>v)||v>=Market::CELLS)return false;a.patrol[j]=v;}
        if(!mask(r.navigation))return false;
    }
    for(unsigned i=0;i<families;++i){unsigned a,b;if(!(in>>a>>b)||!a||a==b||a>65535||b>65535)return false;d.families[i]={uint16_t(a),uint16_t(b)};}
    for(unsigned i=0;i<stops;++i){unsigned n;if(!(in>>n)||n>=Market::NODES)return false;d.stops[i]=n;}
    if(version==2) {
        unsigned groups,visitors,solo,firstLo,firstHi,performances,stalls;
        if(!(in>>groups>>visitors>>solo>>firstLo>>firstHi>>performances>>stalls)||
           groups<1||groups>3||visitors<1||visitors>groups*2||residents+2+groups*2>16||
           solo>100||firstHi<firstLo||firstHi>600||performances>1||stalls>12)return false;
        d.maxGroups=groups;d.maxVisitors=visitors;d.soloPercent=solo;d.firstMin=firstLo;d.firstMax=firstHi;
        d.performances=performances;d.stallCount=stalls;
        for(unsigned i=0;i<residents;++i) {
            unsigned direction,mask;if(!(in>>direction>>mask)||(direction>3&&direction!=255)||mask>15)return false;
            d.residents[i].reception=direction;d.residents[i].lookMask=mask;
        }
        for(unsigned i=0;i<stalls;++i) {
            unsigned resident,count;if(!(in>>resident>>count)||resident>=residents||count<1||count>8)return false;
            auto& s=d.stalls[i];s.resident=resident;s.pointCount=count;
            for(unsigned j=0;j<count;++j){unsigned n;if(!(in>>n)||n>=Market::NODES)return false;s.points[j]=n;}
        }
    }
    return true;
}
