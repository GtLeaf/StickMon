"""Per-zone visitor rules and portable population definitions."""
from functools import lru_cache
import hashlib
import re
from npc_sprites import ROOT, species


@lru_cache(None)
def families():
    source=(ROOT/'src/game/Species.cpp').read_text()
    links={int(s):int(t) for s,t in re.findall(r'^\s*\{(\d+), Ui::SpeciesName::[^\n]+?EV\([^)]*\), (\d+), EvolutionMethod::',source,re.M) if int(t)}
    parents={t:s for s,t in links.items()}
    # These branch predecessors are explicitly supported by Species.cpp.
    branch=source.split('bool isDirectEvolutionPredecessor',1)[1].split('return true;',2)[1]
    for sid in re.findall(r'case (\d+):',branch):parents[int(sid)]=133
    available={sid:name for sid,name in species().values()}
    result=[]
    for adult in sorted(parents):
        child=parents[adult];seen={adult}
        while child in parents and child not in seen:
            seen.add(child);child=parents[child]
        if adult in available and child in available and adult!=child and not {adult,child}&{151,380,381}:
            result.append(dict(id=f'{adult:03d}-{child:03d}',adult=adult,juvenile=child,
                               name=f'{available[adult]} ＋ {available[child]}'))
    # Standalone actors and the former stall residents may visit alone.
    # Keep the candidate catalog within the rule table's 64 entries.
    for sid,name in sorted(available.items()):
        if (sid not in links and sid not in parents or sid in {184,212}) and sid not in {151,380,381}:
            result.append(dict(id=f'solo-{sid:03d}',adult=sid,juvenile=0,name=name+'（单只）'))
    return result


def enabled(m):
    return [z for z in m.get('activityZones',[]) if z.get('visitors',{}).get('enabled')]


def check_zone(z):
    if 'visitors' not in z:return
    v=z['visitors'];valid={f['id'] for f in families()}
    def require(ok,message):
        if not ok:raise ValueError(z['name']+'：'+message)
    require(isinstance(v,dict) and type(v.get('enabled')) is bool,'随机来访开关无效')
    choices=v.get('families')
    require(isinstance(choices,list) and len(choices)<=64 and all(isinstance(f,str) and f in valid for f in choices),
            '候选须为已有资源的普通精灵单只或同链组合，不能包含神兽或人物')
    require(len(set(choices))==len(choices),'候选组合重复')
    require(not v['enabled'] or bool(choices),'请至少选择一组候选精灵')
    require(type(v.get('marketJourney',False)) is bool,'市集行程开关无效')
    for low,high in [('intervalMinMs','intervalMaxMs'),('stayMinMs','stayMaxMs')]:
        require(all(type(v.get(k)) is int and 1000<=v[k]<=(90000 if low=='stayMinMs' and z.get('visitors',{}).get('marketJourney') else 60000) and v[k]%100==0 for k in (low,high))
                and v[low]<=v[high],'来访间隔须为 1～60 秒，停留为 1～60 秒（市集行程可至 90 秒），最短不能大于最长')


def ports(m):
    width,height=m.get('size',[16,12])
    result=[]
    for e in [m['entry']]+[p['exit'] for p in m['paths']]:
        x,y=e['point'];edge=e['edge']
        if not ((edge=='top' and y==0) or (edge=='bottom' and y==height-1) or
                (edge=='left' and x==0) or (edge=='right' and x==width-1)):continue
        port=dict(cell=y*width+x,direction={'bottom':0,'left':1,'top':2,'right':3}[edge])
        if port not in result:result.append(port)
    return result


def definitions(p,grid):
    m=p['map'];width,height=m['size'];choices={f['id']:f for f in families()};result=[]
    # An authored stationary actor is a permanent obstacle, not a temporary wait.
    mask=list(grid)
    for n in m.get('npcs',[]):
        if n['behavior']=='idle' and n['spawn']:
            x,y=n['spawn'];mask[y*width+x]=False
    for z in enabled(m):
        v=z['visitors'];cells={y*width+x for x,y in z['cells']}
        result.append(dict(id=z['id'],name=z['name'],allowed=mask,
            zone=[i in cells and mask[i] for i in range(width*height)],
            ports=[port for port in ports(m) if mask[port['cell']]],
            families=[choices[f] for f in v['families']],
            seed=int.from_bytes(hashlib.sha256((str(m['seed'])+z['id']).encode()).digest()[:4],'little') or 1,
            intervalMin=v['intervalMinMs']//100,intervalMax=v['intervalMaxMs']//100,
            stayMin=v['stayMinMs']//100,stayMax=v['stayMaxMs']//100))
    return result


def diagnostics(p,grid):
    from npcs import reachable
    width=p['map']['size'][0]
    issues=[]
    for r in definitions(p,grid):
        def issue(code,message):issues.append(dict(code=code,message=r['name']+'：'+message,severity='error'))
        cells={i for i,v in enumerate(r['zone']) if v}
        minimum=2 if any(f['juvenile'] for f in r['families']) else 1
        if len(cells)<minimum:
            issue('visitors-zone',f'随机来访至少需要 {minimum} 个可通行格子，结伴候选须有两个相邻格子');continue
        if not cells<=reachable(min(cells),r['zone'],width):
            issue('visitors-disconnected','随机来访区域需连成一片，请擦除零散格子或补齐范围');continue
        if not r['ports']:
            issue('visitors-port','没有可通行的地图边缘入口，请检查已有入口／路线出口及障碍');continue
        seen=set()
        for port in r['ports']:seen|=reachable(port['cell'],r['allowed'],width)
        if not cells<=seen:issue('visitors-unreachable','活动区域无法从地图出入口到达，请检查围栏、水面和物件占地')
    return issues


def wire(rules):
    result=[len(rules)]
    for r in rules:
        result += [r[k] for k in ('intervalMin','intervalMax','stayMin','stayMax','seed')]
        result += [len(r['families'])]
        for f in r['families']:result += [f['adult'],f['juvenile']]
        result += [len(r['ports'])]
        for port in r['ports']:result += [port['cell'],port['direction']]
        result += [*map(int,r['allowed']),*map(int,r['zone'])]
    return result


def header(p,grid):
    rules=definitions(p,grid)
    lines=['#pragma once','#include "AuthoredNpcPopulation.h"','namespace AuthoredPopulations {',
           f'inline constexpr unsigned COUNT = {len(rules)};',
           'inline void load(AuthoredNpcPopulation::Rule* out) {']
    for i,r in enumerate(rules):
        lines.append(f'    out[{i}] = {{}};')
        for k in ('intervalMin','intervalMax','stayMin','stayMax','seed'):
            lines.append(f'    out[{i}].{k} = {r[k]}U;')
        lines.append(f'    out[{i}].familyCount = {len(r["families"])};')
        for j,f in enumerate(r['families']):lines.append(f'    out[{i}].families[{j}] = {{{f["adult"]}, {f["juvenile"]}}};')
        lines.append(f'    out[{i}].portCount = {len(r["ports"])};')
        for j,port in enumerate(r['ports']):lines.append(f'    out[{i}].ports[{j}] = {{{port["cell"]}, {port["direction"]}}};')
        for key in ('allowed','zone'):
            for j,v in enumerate(r[key]):
                if v:lines.append(f'    out[{i}].{key}[{j}] = true;')
    return '\n'.join(lines+['}','} // namespace AuthoredPopulations',''])
