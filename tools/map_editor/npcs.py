"""Authored NPC schema, traversability and shared native simulation adapter."""
import hashlib
import json
import re
import subprocess
from pathlib import Path
from npc_sprites import sprite_catalog, sprite_path, sprite_png, sprite_ids
import population

ROOT = Path(__file__).resolve().parents[2]
LOCAL = ROOT / '.stick-local/artifacts/map-editor-npc-20261029'
BEHAVIORS = ['idle','wander','patrol']


def check(m):
    width,height=m.get('size',[16,12])
    def require(value,message):
        if not value: raise ValueError(message)
    def integer(v,lo,hi): return type(v) is int and lo<=v<=hi
    def point(v): return isinstance(v,list) and len(v)==2 and integer(v[0],0,width-1) and integer(v[1],0,height-1)
    def label(v): return isinstance(v,str) and 0<len(v)<=80
    zones=m.get('activityZones',[]); actors=m.get('npcs',[])
    require(isinstance(zones,list) and len(zones)<=16,'活动区域最多 16 个')
    ids=set()
    for z in zones:
        require(isinstance(z,dict) and label(z.get('id')) and z['id'] not in ids,'活动区域 ID 无效或重复')
        ids.add(z['id'])
        require(label(z.get('name')),'活动区域名称无效')
        require(isinstance(z.get('color'),str) and re.fullmatch(r'#[0-9a-fA-F]{6}',z['color']),'活动区域颜色无效')
        cells=z.get('cells');require(isinstance(cells,list) and len(cells)<=width*height and all(point(c) for c in cells),'活动区域格子无效')
        require(len(set(map(tuple,cells)))==len(cells),'活动区域包含重复格子')
        population.check_zone(z)
    require(isinstance(actors,list) and len(actors)<=16,'每张地图最多配置 16 个 NPC')
    require(len(population.enabled(m))<=8 and len(actors)+2*len(population.enabled(m))<=16,
            '固定 NPC 与随机来访组合合计最多 16 只，每个启用的随机区域预留 2 只')
    market=m.get('market',{})
    require(isinstance(market,dict),'市集设置无效')
    require(not market or bool(m.get('roadNetwork')),'市集接待配置需要已识别的道路网络')
    for key,lo,hi,default in [('maxGroups',1,3,1),('maxVisitors',1,6,2),('soloPercent',0,100,60),
                             ('firstMinMs',100,60000,8000),('firstMaxMs',100,60000,15000)]:
        require(integer(market.get(key,default),lo,hi),'市集设置 '+key+' 无效')
    require(all(market.get(k,default)%100==0 for k,default in [('firstMinMs',8000),('firstMaxMs',15000)]),'首组到来时间以 100 毫秒递增')
    require(market.get('maxVisitors',2)<=2*market.get('maxGroups',1),'游客上限超过组数容量')
    require(len(actors)+2+2*market.get('maxGroups',1)<=16 if market else True,'市集角色合计不能超过 16 只')
    require(market.get('firstMinMs',8000)<=market.get('firstMaxMs',15000),'首组到来时间无效')
    require(type(market.get('performances',False)) is bool,'市集演出开关无效')
    npc_ids=set(); sprites=sprite_ids()
    for n in actors:
        require(isinstance(n,dict) and label(n.get('id')) and n['id'] not in npc_ids,'NPC ID 无效或重复')
        npc_ids.add(n['id'])
        require(label(n.get('name')) and isinstance(n.get('sprite'),str) and n['sprite'] in sprites,'NPC 名称或精灵无效')
        require(n.get('spawn') is None or point(n['spawn']),'NPC 出生点无效')
        require(integer(n.get('direction'),0,3) and n.get('behavior') in BEHAVIORS,'NPC 朝向或行为无效')
        require(n.get('scope') in ('map','zones'),'NPC 活动范围类型无效')
        refs=n.get('zoneIds');require(isinstance(refs,list) and len(refs)<=16 and all(isinstance(r,str) and r in ids for r in refs),'NPC 引用了不存在的活动区域')
        require(len(set(refs))==len(refs),'NPC 活动区域重复')
        require(integer(n.get('stepMs'),200,2000) and n['stepMs']%100==0,'NPC 步速应为 200～2000 毫秒，步长 100')
        require(integer(n.get('waitMinMs'),100,60000) and integer(n.get('waitMaxMs'),n['waitMinMs'],60000)
                and n['waitMinMs']%100==0 and n['waitMaxMs']%100==0,'NPC 停留时间无效，步长 100 毫秒')
        reception=n.get('reception')
        if reception is not None:
            require(isinstance(reception,dict) and integer(reception.get('direction'),0,3),'接待朝向无效')
            directions=reception.get('lookDirections',[0,1,2,3])
            require(isinstance(directions,list) and bool(directions) and len(directions)<=4 and
                    all(integer(d,0,3) for d in directions) and len(set(directions))==len(directions),'观望方向无效')
            customers=reception.get('customerPoints',[])
            require(isinstance(customers,list) and len(customers)<=8 and all(point(v) for v in customers) and
                    len(set(map(tuple,customers)))==len(customers),'顾客点须为不重复的地图格子，最多 8 个')
        points=n.get('patrol');require(isinstance(points,list) and len(points)<=32 and all(point(v) for v in points),'NPC 巡逻点无效或超过 32 个')
    return m


def walkable(project,catalog):
    width,height=project['map']['size']
    from project import flatten, visible_footprint
    layers,owners=flatten(project); grid=[]
    for i,key in enumerate(layers[0]):
        a=catalog.assets.get(key,{})
        overlays=[catalog.assets.get(layers[z][i],{}) for z in (1,2)]
        bridge=any(o.get('bridge') for o in overlays)
        tid=a.get('runtimeId')
        known_ground = a.get('road') or tid in (*range(385,391),399,415,4758,*range(4500,4507),4511) or bool(a.get('provenance',{}).get('flags',0) & (2|64))
        ok=bool(a and not a.get('empty') and tid is not None and ((known_ground and not a.get('water')) or bridge))
        # Floor-level cliff faces and unknown tile geometry are not walkable.
        if a.get('depth',[0,0,0])[0]: ok=False
        grid.append(ok)
    owned=set()
    for obj in project['map']['objects']:
        pref=next(p for p in project['prefabs'] if p['id']==obj['prefab'])
        for c in pref['cells']:
            x,y=obj['x']+c['x'],obj['y']+c['y']
            if 0<=x<width and 0<=y<height: owned.add((c['layer'],y*width+x))
        for dx,dy in visible_footprint(obj,pref,owners,project['map']['size']):
            x,y=obj['x']+dx,obj['y']+dy
            if 0<=x<width and 0<=y<height:grid[y*width+x]=False
    for z in (1,2):
        for i,key in enumerate(layers[z]):
            if not key or (z,i) in owned: continue
            a=catalog.assets.get(key,{})
            passable = a.get('bridge') or a.get('runtimeId') in (391,4773,4747) or (a.get('behind',[False]*3)[z] and a.get('depth',[0,0,0])[z]!=1)
            if not a or a.get('runtimeId') is None or not passable:
                grid[i]=False
    return grid


def allowed(n,zones,grid,width=16):
    cells={y*width+x for z in zones if z['id'] in n['zoneIds'] for x,y in z['cells']}
    return [v and (n['scope']=='map' or i in cells) for i,v in enumerate(grid)]


def reachable(start,grid,width=16):
    height=len(grid)//width
    seen={start};queue=[start]
    while queue:
        i=queue.pop()
        for dx,dy in ((0,1),(-1,0),(0,-1),(1,0)):
            x,y=i%width+dx,i//width+dy;j=y*width+x
            if 0<=x<width and 0<=y<height and grid[j] and j not in seen:seen.add(j);queue.append(j)
    return seen


def diagnostics(p,catalog):
    check(p['map']);m=p['map'];width,height=m['size'];grid=walkable(p,catalog);zones=m.get('activityZones',[]);issues=[];occupied=set()
    def issue(code,message,point=None,severity='error'):
        item=dict(code=code,message=message,severity=severity)
        if point:item.update(x=point[0],y=point[1])
        issues.append(item)
    for z in zones:
        cells={y*width+x for x,y in z['cells'] if grid[y*width+x]}
        if not cells:issue('zone-empty',z['name']+'：区域没有可通行格子',severity='warning')
        else:
            remaining=set(cells);parts=[];mask=[i in cells for i in range(width*height)]
            while remaining:
                part=reachable(min(remaining),mask,width);remaining-=part;parts.append(sorted(part))
            if len(parts)>1:
                locations=['、'.join(f'({i%width}, {i//width})' for i in part[:3])+('…' if len(part)>3 else '') for part in parts]
                issue('zone-disconnected',z['name']+f'：范围分成 {len(parts)} 片（'+ ' / '.join(locations)+
                      '，坐标从 0 开始）。NPC 不能跨越区域外的格子，请擦除误选格子或补齐可通行范围',severity='warning')
    for n in m.get('npcs',[]):
        label=n['name']+'：';mask=allowed(n,zones,grid,width);start=n['spawn']
        if start is None:issue('npc-spawn',label+'请设置出生点');continue
        i=start[1]*width+start[0]
        if not mask[i]:issue('npc-spawn',label+'出生点在范围外或被障碍阻挡',start);continue
        if i in occupied:issue('npc-overlap',label+'出生点与其他 NPC 重叠',start)
        occupied.add(i);seen=reachable(i,mask,width)
        if n['behavior']=='wander' and len(seen)<2:issue('npc-trapped',label+'没有可以散步的相邻格子',start)
        if n['behavior']=='patrol':
            if len(set(map(tuple,n['patrol'])))<2:
                issue('npc-patrol',label+f'已选“沿点巡逻”，当前只有 {len(set(map(tuple,n["patrol"])))} 个不同的巡逻点；请用“绘制巡逻点”设置至少两个位置，或改为“随机散步”',start)
            for point in n['patrol']:
                if point[1]*width+point[0] not in seen:issue('npc-unreachable',label+'巡逻点不在可到达范围内',point)
        if n['scope']=='zones':
            for z in zones:
                if z['id'] in n['zoneIds'] and not any(y*width+x in seen for x,y in z['cells']):
                    issue('npc-zone-unreachable',label+'无法到达区域 '+z['name'],start)
    issues.extend(population.diagnostics(p,grid))
    return issues,grid


def definitions(p,catalog):
    issues,grid=diagnostics(p,catalog)
    errors=[v['message'] for v in issues if v['severity']=='error']
    if errors:raise ValueError('；'.join(errors))
    m=p['map'];width,height=m['size'];result=[]
    for n in m.get('npcs',[]):
        seed=int.from_bytes(hashlib.sha256((str(m['seed'])+n['id']).encode()).digest()[:4],'little') or 1
        result.append(dict(allowed=allowed(n,m.get('activityZones',[]),grid,width),spawn=n['spawn'][1]*width+n['spawn'][0],
                           direction=n['direction'],behavior=BEHAVIORS.index(n['behavior']),stepTicks=n['stepMs']//100,
                           waitMin=n['waitMinMs']//100,waitMax=n['waitMaxMs']//100,seed=seed,
                           patrol=[y*width+x for x,y in n['patrol']]))
    return result,issues,grid


def binary(size=(16,12)):
    width,height=size
    target=LOCAL/f'npc-host-{width}x{height}';sources=[ROOT/'tools/map_editor/npc_host.cpp',ROOT/'src/game/AuthoredNpc.h',ROOT/'src/game/AuthoredNpcPopulation.h']
    if not target.exists() or any(p.stat().st_mtime>target.stat().st_mtime for p in sources):
        LOCAL.mkdir(parents=True,exist_ok=True)
        subprocess.run(['c++','-std=c++17','-O2','-Wall','-Wextra','-Werror',f'-DSTICKMON_EDITOR_MAP_WIDTH={width}',f'-DSTICKMON_EDITOR_MAP_HEIGHT={height}','-I',str(ROOT/'src'),str(sources[0]),'-o',str(target)],check=True,capture_output=True,text=True)
    return target


def preview(p,catalog,ticks=1200,seed=0):
    if p['map'].get('roadNetwork') and any(a.get('kind')=='roadNetwork' for a in p['map']['paths']):
        from market_export import preview_activity
        return preview_activity(p,catalog,ticks,seed)
    defs,issues,grid=definitions(p,catalog)
    wire=[len(defs),ticks]
    for d in defs:
        wire += [d[k] for k in ('spawn','direction','behavior','stepTicks','waitMin','waitMax','seed')]
        wire += [len(d['patrol']),*d['patrol'],*map(int,d['allowed'])]
    rules=population.definitions(p,grid)
    wire += population.wire(rules)+[seed]
    result=subprocess.run([str(binary(p['map']['size']))],input=' '.join(map(str,wire)),text=True,capture_output=True,check=True,timeout=10)
    actors=[dict(id=n['id'],name=n['name'],sprite=n['sprite']) for n in p['map'].get('npcs',[])]
    for r in rules:
        for member in ('adult','juvenile'):
            actors.append(dict(id='visitors:'+r['id']+':'+member,name=r['name']+' · '+('访客' if member=='adult' else '同行精灵'),visitor=True))
    return dict(stepMs=100,frames=json.loads(result.stdout),issues=issues,walkable=grid,
                ids=[n['id'] for n in actors],actors=actors)


def header(p,catalog):
    defs,_,_=definitions(p,catalog)
    lines=['#pragma once','#include "AuthoredNpc.h"','namespace AuthoredNpcs {',f'inline constexpr unsigned COUNT = {len(defs)};',
           'inline void load(AuthoredNpc::Definition* out) {']
    for i,d in enumerate(defs):
        lines.append(f'    out[{i}] = {{}};')
        for k in ('spawn','direction','stepTicks','waitMin','waitMax','seed'):
            lines.append(f'    out[{i}].{k} = {d[k]}U;')
        lines.append(f'    out[{i}].behavior = static_cast<AuthoredNpc::Behavior>({d["behavior"]});')
        lines.append(f'    out[{i}].patrolCount = {len(d["patrol"])};')
        for j,v in enumerate(d['patrol']):lines.append(f'    out[{i}].patrol[{j}] = {v};')
        for j,v in enumerate(d['allowed']):
            if v:lines.append(f'    out[{i}].allowed[{j}] = true;')
    return '\n'.join(lines+['}','} // namespace AuthoredNpcs',''])
