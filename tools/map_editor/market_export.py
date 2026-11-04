"""Compile authored town maps, preserving roads separately from explore paths."""
import argparse
import copy
import hashlib
import json
import io
import subprocess
import zipfile
from pathlib import Path

from catalog import Catalog, ROOT
from project import check_project, flatten, validate, road_source_matches
import npcs

WIDTH, HEIGHT = 33, 25


def support(x, y, width=16, height=12):
    xs = [x//2] if x % 2 else [x//2-1, x//2]
    ys = [y//2] if y % 2 else [y//2-1, y//2]
    return [b*width+a for b in ys for a in xs if 0 <= a < width and 0 <= b < height]


def mesh(cells, size=(16,12)):
    width,height=size
    return [all(cells[i] for i in support(x,y,width,height)) for y in range(height*2+1) for x in range(width*2+1)]


def packed(values):
    return [sum((1 << b) for b in range(8) if i+b < len(values) and values[i+b])
            for i in range(0, len(values), 8)]


def compile_market(project, catalog, preview=False):
    check_project(project)
    if not preview and (project["map"]["size"][0]*2+1)*(project["map"]["size"][1]*2+1)>1295:
        raise ValueError("大地图超出当前市集设备的导航栈预算（最多 1295 导航点）；仍可预览活动")
    width,height=project['map']['size']; nav_width=width*2+1; nav_height=height*2+1
    report = validate(project, catalog)
    errors=[v for v in report['issues'] if v['severity']=='error' and not (preview and v['code']=='source-change')]
    if errors:
        raise ValueError('；'.join(v['message'] for v in errors))
    m = project['map']; network = m.get('roadNetwork')
    if not network or network['components'] != 1 or network['uncovered']:
        raise ValueError('市集需要完整、连通的道路识别结果')
    # The editor stores exact source geometry. Reject stale graphs even when a
    # project has no saved traversal/playback route.
    if not road_source_matches(project,network['source']):
        raise ValueError('道路来源已变化，请重新识别道路')
    layers, _ = flatten(project)
    definitions, _, grid = npcs.definitions(project, catalog)
    roads = [bool(grid[i] and catalog.assets.get(key, {}).get('road')) for i, key in enumerate(layers[0])]
    navigation = mesh(roads,m['size'])
    for a, b in network['edges']:
        x, y = network['nodes'][a]; xx, yy = network['nodes'][b]
        for step in range(abs(xx-x)*2 + abs(yy-y)*2 + 1):
            nx, ny = x*2+(1 if xx>x else -1 if xx<x else 0)*step, y*2+(1 if yy>y else -1 if yy<y else 0)*step
            if not navigation[ny*nav_width+nx]:
                raise ValueError(f'道路中线 ({nx/2}, {ny/2}) 被地形或物件阻挡')
    edge = m['entry']['edge']; ex, ey = m['entry']['point']
    boundaries = [(x, y) for x, y in network['nodes'] if
                  dict(top=y == 0, right=x == width, bottom=y == height, left=x == 0)[edge]]
    if not boundaries:
        raise ValueError('道路未连接到所选地图边界入口')
    x, y = min(boundaries, key=lambda p: abs(p[0]-ex)+abs(p[1]-ey))
    entry = y*2*nav_width+x*2
    # Check reachability, including cell-center lanes used to pass oncoming NPCs.
    seen = {entry}; queue = [entry]
    while queue:
        i = queue.pop()
        for dx, dy in ((0,1),(-1,0),(0,-1),(1,0)):
            xx, yy = i%nav_width+dx, i//nav_width+dy; n = yy*nav_width+xx
            if 0 <= xx < nav_width and 0 <= yy < nav_height and navigation[n] and n not in seen:
                seen.add(n); queue.append(n)
    navigation = [v and i in seen for i, v in enumerate(navigation)]
    residents = []
    for n, d in zip(m.get('npcs', []), definitions):
        species = int(n['sprite'].removeprefix('pokemon-'))
        reception=n.get('reception')
        residents.append(dict(species=species, activity=d, navigation=packed(mesh(d['allowed'],m['size'])),
                              reception=reception['direction'] if reception else 255,
                              lookMask=sum(1<<v for v in reception.get('lookDirections',[0,1,2,3])) if reception else 15))
    settings=m.get('market',{})
    max_groups=settings.get('maxGroups',1)
    if len(residents)+2+2*max_groups>16 or len(residents)>12:
        raise ValueError('市集角色超出 16 只预算，请减少常驻或游客并发组数')
    rules = npcs.population.definitions(project, grid)
    if len(rules) > 1:
        raise ValueError('市集使用一个候选池，请合并来访规则；池内可配置多组并发')
    families = rules[0]['families'] if rules else []
    if families and all((2 if f['juvenile'] else 1)>settings.get('maxVisitors',2) for f in families):
        raise ValueError('游客总上限不足以容纳任何候选组')
    species={r['species'] for r in residents}|{f[k] for f in families for k in ('adult','juvenile') if f[k]}
    if len(species)>38:
        raise ValueError('市集精灵资源超过 38 种，请缩小候选池（另为玩家与同伴预留缓存）')
    zones = [c for z in m.get('activityZones', []) for c in z['cells'] if grid[c[1]*width+c[0]]]
    # Stop outside stalls on authored road cells. Never silently enlarge a
    # resident's scope or put three actors into a two-cell stall.
    stops = [((i//width)*2+1)*nav_width+(i%width)*2+1 for i, ok in enumerate(roads) if ok and
             abs((i%width)*2+1-x*2)+abs((i//width)*2+1-y*2) > 4 and
             any(abs(i%width-a)+abs(i//width-b) <= 2 for a,b in zones)]
    stops = [i for i in stops if navigation[i]][:48]
    if families and len(stops) < (2 if any(f['juvenile'] for f in families) else 1):
        raise ValueError('摊位附近缺少可达的道路停留位置')
    stalls=[]
    for i,(n,r) in enumerate(zip(m.get('npcs',[]),residents)):
        if r['reception']==255:continue
        spec=n['reception'];direction=r['reception']
        authored=spec.get('customerPoints',[])
        def point_node(c):return (c[1]*2+1)*nav_width+c[0]*2+1
        if authored:
            points=[point_node(c) for c in authored]
            if any(not navigation[c] or abs(c%nav_width-x*2)+abs(c//nav_width-y*2)<=4 for c in points):
                raise ValueError(n['name']+'：顾客点须在可达道路上并避开入口')
        else:
            points=[]
            scope=[((c%width)*2+1,(c//width)*2+1) for c,ok in enumerate(r['activity']['allowed']) if ok]
            for c in stops:
                px,py=c%nav_width,c//nav_width
                for ax,ay in scope:
                    dx,dy=px-ax,py-ay
                    front=(dy,-dx,-dy,dx)[direction]
                    side=abs(dx) if direction in (0,2) else abs(dy)
                    if 0<front<=6 and side<=2:
                        points.append(c);break
            spawn=point_node(n['spawn'])
            points.sort(key=lambda c:(abs(c%nav_width-spawn%nav_width)+abs(c//nav_width-spawn//nav_width),c))
            points=points[:8]
        if not points:raise ValueError(n['name']+'：接待侧缺少可达顾客点，请设置道路顾客点或调整接待朝向')
        stalls.append(dict(resident=i,points=points))
    result = dict(size=m["size"], version=2, sourceSha256=hashlib.sha256(json.dumps(project, sort_keys=True).encode()).hexdigest(),
                  layers=[[catalog.assets[key]['runtimeId'] if key else 0 for key in layer] for layer in layers],
                  entry=entry, outward=dict(bottom=0,left=1,top=2,right=3)[edge], roads=packed(navigation),
                  pedestrian=packed(mesh(grid,m['size'])),
                  residents=residents, stops=stops, families=families,stalls=stalls,
                  maxGroups=max_groups,maxVisitors=settings.get('maxVisitors',2),soloPercent=settings.get('soloPercent',60),
                  firstMin=settings.get('firstMinMs',0)//100,firstMax=settings.get('firstMaxMs',0)//100,
                  performances=settings.get('performances',False),
                  intervalMin=rules[0]['intervalMin'] if rules else 80,
                  intervalMax=rules[0]['intervalMax'] if rules else 150,
                  stayMin=rules[0]['stayMin'] if rules else 100,
                  stayMax=rules[0]['stayMax'] if rules else 200)
    return result


def header(data):
    array = lambda v: '{'+','.join(str(int(x)) for x in v)+'}'
    lines = ['// Generated by tools/map_editor/market_export.py. Edit the source project.', '#pragma once',
             '#include "game/MarketData.h"', 'namespace MarketMapData {',
             f'static_assert(Market::CELL_WIDTH=={data["size"][0]} && Market::CELL_HEIGHT=={data["size"][1]}, "Regenerate both market headers together");',
             f'inline constexpr const char* SOURCE_SHA256 = "{data["sourceSha256"]}";',
             'inline const Market::Data& get() {', '    static const Market::Data value = [] {', '        Market::Data d{};', f'        d.version={data["version"]};']
    for z, layer in enumerate(data['layers']):
        lines += [f'        const uint16_t layer{z}[] = '+array(layer)+';',
                  f'        for (int i=0;i<Market::CELLS;++i) d.map.layers[{z}][i]=layer{z}[i];']
    lines += [f'        d.entry={data["entry"]}; d.outward={data["outward"]};',
              '        d.roads=Market::Mask{'+array(data['roads'])+'};',
              '        d.pedestrian=Market::Mask{'+array(data['pedestrian'])+'};',
              f'        d.residentCount={len(data["residents"])};']
    for i, r in enumerate(data['residents']):
        d=r['activity']; prefix=f'd.residents[{i}]'
        lines += [f'        {prefix}.species={r["species"]};', f'        {prefix}.reception={r["reception"]}; {prefix}.lookMask={r["lookMask"]};', f'        {prefix}.navigation=Market::Mask{{'+array(r['navigation'])+'};',
                  f'        {prefix}.activity.spawn={d["spawn"]}; {prefix}.activity.direction={d["direction"]};',
                  f'        {prefix}.activity.behavior=AuthoredNpc::Behavior({d["behavior"]});',
                  f'        {prefix}.activity.stepTicks={d["stepTicks"]}; {prefix}.activity.waitMin={d["waitMin"]}; {prefix}.activity.waitMax={d["waitMax"]};',
                  f'        {prefix}.activity.seed={d["seed"]}U;',
                  f'        {prefix}.activity.patrolCount={len(d["patrol"])};']
        for j, cell in enumerate(d['patrol']): lines += [f'        {prefix}.activity.patrol[{j}]={cell};']
    lines += [f'        d.stopCount={len(data["stops"])};']
    for i, n in enumerate(data['stops']): lines += [f'        d.stops[{i}]={n};']
    lines += [f'        d.familyCount={len(data["families"])};']
    for i, f in enumerate(data['families']): lines += [f'        d.families[{i}]={{{f["adult"]},{f["juvenile"]}}};']
    for key in ('intervalMin','intervalMax','stayMin','stayMax'): lines += [f'        d.{key}={data[key]};']
    for key in ('maxGroups','maxVisitors','soloPercent','firstMin','firstMax','performances'):
        lines += [f'        d.{key}={int(data[key])};']
    lines += [f'        d.stallCount={len(data["stalls"])};']
    for i,stall in enumerate(data['stalls']):
        lines += [f'        d.stalls[{i}].resident={stall["resident"]}; d.stalls[{i}].pointCount={len(stall["points"])};']
        for j,n in enumerate(stall['points']):lines += [f'        d.stalls[{i}].points[{j}]={n};']
    return '\n'.join(lines+['        return d;', '    }();', '    return value;', '}', '} // namespace MarketMapData', ''])


def size_header(data):
    w,h=data["size"]
    return f'#pragma once\n// Generated by tools/map_editor/market_export.py.\nnamespace MarketMapSize {{ constexpr int WIDTH={w}, HEIGHT={h}; }}\n'


def wire(data):
    values=[data[k] for k in ('version','entry','outward')]
    values += [len(data['residents']),len(data['families']),len(data['stops'])]
    values += [data[k] for k in ('intervalMin','intervalMax','stayMin','stayMax')]
    values += data['roads']+data['pedestrian']+[i for layer in data['layers'] for i in layer]
    for r in data['residents']:
        d=r['activity']; values += [r['species']]+[d[k] for k in ('spawn','direction','behavior','stepTicks','waitMin','waitMax','seed')]
        values += [len(d['patrol']),*d['patrol'],*r['navigation']]
    for f in data['families']: values += [f['adult'],f['juvenile']]
    values+=data['stops']
    values += [int(data[k]) for k in ('maxGroups','maxVisitors','soloPercent','firstMin','firstMax','performances')]
    values += [len(data['stalls'])]
    for r in data['residents']:values += [r['reception'],r['lookMask']]
    for stall in data['stalls']:values += [stall['resident'],len(stall['points']),*stall['points']]
    return ' '.join(map(str,values))


def preview_activity(p,catalog,ticks,seed):
    from catalog import LOCAL
    data=compile_market(p,catalog,preview=True)
    width,height=p['map']['size']
    target=LOCAL/f'market-activity-host-{width}x{height}'
    deps=[ROOT/'tools/map_editor/market_activity_host.cpp',ROOT/'tools/map_editor/market_wire.h',
          ROOT/'src/game/MarketSession.h',ROOT/'src/game/MarketData.h',ROOT/'src/game/AuthoredNpc.h']
    if not target.exists() or any(f.stat().st_mtime>target.stat().st_mtime for f in deps):
        target.parent.mkdir(parents=True,exist_ok=True)
        subprocess.run(['c++','-std=c++17','-O2','-Wall','-Wextra','-Werror',f'-DSTICKMON_EDITOR_MAP_WIDTH={width}',f'-DSTICKMON_EDITOR_MAP_HEIGHT={height}','-I',str(ROOT/'src'),str(deps[0]),'-o',str(target)],check=True,capture_output=True,text=True)
    result=subprocess.run([str(target)],input=f'{ticks} {seed} '+wire(data),text=True,capture_output=True,check=True,timeout=15)
    actors=[dict(id=n['id'],name=n['name'],sprite=n['sprite']) for n in p['map'].get('npcs',[])]
    enabled=npcs.population.enabled(p['map'])
    if enabled:
        z=enabled[0]
        for g in range(data['maxGroups']):
            for member in ('adult','juvenile'):
                actors.append(dict(id=f'visitors:{z["id"]}:{g}:{member}',name=f'第 {g+1} 组 · '+('访客' if member=='adult' else '同行精灵'),visitor=True))
    issues,grid=npcs.diagnostics(p,catalog)
    return dict(stepMs=100,frames=json.loads(result.stdout),issues=issues,walkable=grid,
                ids=[a['id'] for a in actors],actors=actors,navigationVersion=2)


def package(p,catalog):
    from project import render_project
    from catalog import png
    data=compile_market(p,catalog);report=validate(p,catalog)
    report.update(navigationVersion=2,market=True)
    out=io.BytesIO();layers,_=flatten(p)
    keys=sorted({key for layer in layers for key in layer if key})
    species={r['species'] for r in data['residents']}
    species.update(f[k] for f in data['families'] for k in ('adult','juvenile') if f[k])
    with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
        for name,value in [('project.stickmap.json',p),('market-runtime.json',data),('validation.json',report),
                           ('resource-requirements.json',{key:catalog.assets[key] for key in keys})]:
            z.writestr(name,json.dumps(value,ensure_ascii=False,indent=2))
        z.writestr('MarketMapData.h',header(data))
        z.writestr('MarketMapSize.h',size_header(data))
        for name in ('MarketData.h','MarketSession.h','AuthoredNpc.h'):
            z.writestr(name,(ROOT/'src/game'/name).read_bytes())
        z.writestr('preview.png',png(render_project(p,catalog)))
        for sid in sorted(species):
            z.writestr(f'npc-packs/{sid:03d}.smonsp',(ROOT/f'data/packs/dev/sprites/{sid:03d}.smonsp').read_bytes())
        z.writestr('README.txt','市集导航 v2：由 Market::Session 加载 MarketMapData::get()，与普通探索路线独立。\n调整图层、入口或物件后重新识别道路、核对素材来源并导出；NPC 活动预览使用同一 C++ 核心。\n')
    return out.getvalue(),report


def render_native(p,catalog,output,walk=False,sequence=False):
    import tempfile
    from PIL import Image
    from catalog import LOCAL
    data=compile_market(p,catalog)
    output=Path(output);output.mkdir(parents=True,exist_ok=True)
    width,height=p['map']['size']
    target=LOCAL/f'market-render-{width}x{height}'
    deps=[ROOT/'tools/market_render_host.cpp',ROOT/'tools/map_editor/market_wire.h']
    deps+=list((ROOT/'src').rglob('*.h'))+list((ROOT/'firmware/amoled_1_8_v1/main').rglob('*.h'))
    deps+=list((ROOT/'src').rglob('*.cpp'))+list((ROOT/'firmware/amoled_1_8_v1/main').rglob('*.cpp'))
    if not target.exists() or any(f.stat().st_mtime>target.stat().st_mtime for f in deps):
        # Link the production renderer and resource services directly. The
        # installed application map must not constrain an editor preview size.
        from amoled_source import amoled_shared_sources
        sources=amoled_shared_sources(ROOT)+[ROOT/'src/hardware/Hal.cpp',ROOT/'src/platform/desktop/DesktopPlatform.cpp',deps[0]]
        target.parent.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='market-render-build-') as tmp:
            obj=Path(tmp)/'tinflate.o'
            subprocess.run(['cc','-O1','-c',str(ROOT/'src/third_party/uzlib/tinflate.c'),'-o',str(obj)],check=True)
            subprocess.run(['c++','-std=c++17','-O1','-ffunction-sections','-fdata-sections',
                            '-Wl,-dead_strip' if __import__('sys').platform=='darwin' else '-Wl,--gc-sections',
                            '-I'+str(ROOT/'src'),'-DSTICKMON_HAS_CLAW=0',
                            f'-DSTICKMON_MARKET_RENDER_WIDTH={width}',f'-DSTICKMON_MARKET_RENDER_HEIGHT={height}',
                            *[str(f) for f in sources if f.suffix=='.cpp'],str(obj),'-o',str(target)],check=True)
    files=[]
    with tempfile.TemporaryDirectory(prefix='market-',dir=output) as tmp:
        command=[str(target),str(ROOT/'data'),tmp,'--stdin']
        if walk:command.append('--walk')
        if sequence:command.append('--sequence')
        subprocess.run(command,input=wire(data),text=True,capture_output=True,check=True,timeout=180 if sequence else 60)
        if sequence:
            for name in ('events.tsv','render-metrics.tsv'):(output/name).write_text((Path(tmp)/name).read_text())
        for path in sorted(Path(tmp).glob('*.ppm')):
            dest=output/(path.stem+'.png')
            with Image.open(path) as im: im.save(dest)
            files.append(dest)
    return files


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('project', type=Path)
    parser.add_argument('--output', type=Path, default=ROOT/'src/game/MarketMapData.h')
    parser.add_argument('--install', action='store_true', help='Also install this project as origin_asset/maps/market/market.stickmap.json')
    parser.add_argument('--accept-source-update', action='store_true', help='After reviewing artwork, update source hashes in the specified source project')
    args=parser.parse_args(); p=json.loads(args.project.read_text()); catalog=Catalog()
    if args.accept_source_update:
        p=copy.deepcopy(p); p['catalogVersion']=catalog.version; p['sources']=catalog.sources
    data=compile_market(p,catalog)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.with_name("MarketMapSize.h").write_text(size_header(data))
    args.output.write_text(header(data))
    if args.install:
        source=ROOT/'origin_asset/maps/market/market.stickmap.json'
        source.parent.mkdir(parents=True,exist_ok=True)
        source.write_text(json.dumps(p,ensure_ascii=False,indent=2)+'\n')
    elif args.accept_source_update:
        args.project.write_text(json.dumps(p,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(dict(version=data['version'],residents=len(data['residents']),families=len(data['families']),stops=len(data['stops']),source=data['sourceSha256'])))


if __name__ == '__main__': main()
