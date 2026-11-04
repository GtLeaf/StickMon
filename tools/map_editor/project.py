"""Editable source, validation and deterministic runtime export."""
import copy
import io
import json
import re
import subprocess
import zipfile
from itertools import takewhile

from PIL import Image
from catalog import EDGES, LOCAL, ROOT, native_binary, png, runtime
import lake_autotile as lake
import npcs
import map_editor_rotations as rotations
from map_editor_assets import GROUND_DECORATIONS

FLAGS = ['hasCoast','hasForest','hasCreek','hasCliff','hasWaterfall']
BOOL_PATH = ['cellCentered','fallsToNextLevel','terminal','interiorExit']


def integer(value, low, high):
    return type(value) is int and low <= value <= high


def point(value, size=(16,12)):
    return isinstance(value,list) and len(value)==2 and all(integer(v,0,size[i]-1) for i,v in enumerate(value))


def endpoint(value, size=(16,12)):
    return isinstance(value,dict) and point(value.get('point'),size) and value.get('edge') in EDGES


def check_project(p, catalog=None):
    def require(ok, message):
        if not ok: raise ValueError(message)
    require(isinstance(p,dict) and p.get('schemaVersion') == 1, '不支持的工程版本；需要 schemaVersion=1')
    require(isinstance(p.get('name'),str) and 0 < len(p['name']) <= 120, '工程名称无效')
    require(isinstance(p.get('sources'),dict) and isinstance(p.get('catalogVersion'),str), '缺少素材来源记录')
    for key,source in p['sources'].items():
        require(isinstance(source,dict) and isinstance(source.get('path'),str) and
                isinstance(source.get('sha256'),str), '素材来源记录无效')
    require(isinstance(p.get('prefabs'),list) and len(p['prefabs']) <= 200, '组合库无效或超过 200 项')
    ids=set()
    for pref in p['prefabs']:
        require(isinstance(pref,dict), '组合格式错误')
        require(isinstance(pref.get('id'),str) and pref['id'] not in ids, '组合 ID 缺失或重复')
        ids.add(pref['id'])
        require(isinstance(pref.get('name'),str) and len(pref['name']) <= 120, '组合名称无效')
        require(pref.get('anchorMode','shared') in ('shared','runtime'), '不支持的组合深度模式')
        require(integer(pref.get('width'),1,16) and integer(pref.get('height'),1,12), '组合尺寸无效')
        anchor=pref.get('anchor')
        require(isinstance(anchor,list) and len(anchor)==2 and all(type(v) in (int,float) and 0<=v<=16 for v in anchor), '落地锚点无效')
        require(isinstance(pref.get('footprint'),list) and len(pref['footprint'])<=192, '占地无效')
        for cell in pref['footprint']:
            require(point(cell) and cell[0]<pref['width'] and cell[1]<pref['height'], '占地超出组合')
        require(isinstance(pref.get('cells'),list) and 0<len(pref['cells'])<=576, '组合图块无效')
        positions=set()
        for cell in pref['cells']:
            require(isinstance(cell,dict) and integer(cell.get('x'),0,pref['width']-1) and integer(cell.get('y'),0,pref['height']-1)
                    and integer(cell.get('layer'),0,2) and isinstance(cell.get('asset'),str), '组合图块坐标或来源无效')
            slot=(cell['x'],cell['y'],cell['layer'])
            require(slot not in positions,'组合内部同层图块重复')
            positions.add(slot)
    m=p.get('map')
    require(isinstance(m,dict) and isinstance(m.get('size'),list) and len(m['size'])==2 and integer(m['size'][0],16,64) and integer(m['size'][1],12,64), '宽度须为 16～64 格，高度须为 12～64 格，且必须是整数')
    width,height=m['size']; cells=width*height
    map_point=lambda v:point(v,m['size'])
    map_endpoint=lambda v:endpoint(v,m['size'])
    require(isinstance(m.get('id'),str), '地图缺少稳定 ID')
    require(isinstance(m.get('layers'),list) and len(m['layers'])==3, '地图需要三个图层')
    for layer in m['layers']:
        require(isinstance(layer,list) and len(layer)==cells and all(v is None or isinstance(v,str) and len(v)<=100 for v in layer), '图层长度必须与地图尺寸一致')
    lake_ground = m.get('lakeGround',{})
    require(isinstance(lake_ground,dict) and len(lake_ground)<=cells, '湖泊原地面记录无效')
    for cell,key in lake_ground.items():
        require(isinstance(cell,str) and cell.isdecimal() and str(int(cell))==cell and 0<=int(cell)<cells
                and isinstance(key,str) and 0<len(key)<=100, '湖泊原地面记录格式无效')
    require(isinstance(m.get('objects'),list) and len(m['objects'])<=300, '物件列表无效或超过 300 个')
    object_ids=set()
    for o in m['objects']:
        require(isinstance(o,dict) and isinstance(o.get('id'),str) and o['id'] not in object_ids and o.get('prefab') in ids, '物件 ID 或组合引用无效')
        object_ids.add(o['id'])
        require(integer(o.get('x'),0,width-1) and integer(o.get('y'),0,height-1), '物件坐标无效')
    require(map_endpoint(m.get('entry')) and map_point(m.get('junction')), '入口或分岔坐标无效')
    require(integer(m.get('seed'),0,0xffffffff) and integer(m.get('areaIndex'),0,5), '种子或地区无效')
    require(type(m.get('valleyJourney')) is bool and integer(m.get('valleyNode'),0,13), '河谷上下文无效')
    require(all(type(m.get(k)) is bool for k in FLAGS), '地图环境标志无效')
    require(isinstance(m.get('paths'),list) and len(m['paths'])<=16, '路线列表无效')
    for path in m['paths']:
        require(isinstance(path,dict), '路线无效')
        require(path.get('kind') in (None,'roadNetwork'), '路线类型无效')
        road_route=path.get('kind')=='roadNetwork'
        valid_point=(lambda v:isinstance(v,list) and len(v)==2 and integer(v[0],0,width) and integer(v[1],0,height)) if road_route else map_point
        require(isinstance(path.get('points'),list) and len(path['points'])<=(4*(width+1)*(height+1)+1 if road_route else cells) and all(valid_point(v) for v in path['points']), '路线点无效')
        if road_route:
            require(len(path['points'])>=3 and isinstance(path.get('roadSource'),str) and len(path['roadSource'])<=2*1024*1024, '道路路线来源无效')
            require(map_endpoint(path.get('roadEntry')), '道路路线入口记录无效')
            require(isinstance(m.get('roadNetwork'),dict), '道路路线缺少路网')
        require(map_endpoint(path.get('exit')) and all(type(path.get(k)) is bool for k in BOOL_PATH), '路线出口或行为无效')
        require(integer(path.get('nextValleyNode'),0,255) and integer(path.get('nextValleyEntrance'),0,255), '转场目标无效')
    network=m.get('roadNetwork')
    if network is not None:
        require(isinstance(network,dict) and network.get('version')==1, '路网版本无效')
        require(isinstance(network.get('source'),str) and len(network['source'])<=2*1024*1024, '路网来源无效')
        nodes=network.get('nodes'); edges=network.get('edges')
        require(isinstance(nodes,list) and 0<len(nodes)<=(width+1)*(height+1) and all(isinstance(v,list) and len(v)==2 and
                integer(v[0],0,width) and integer(v[1],0,height) for v in nodes), '路网节点无效')
        require(len(set(map(tuple,nodes)))==len(nodes), '路网节点重复')
        require(isinstance(edges,list) and len(edges)<=2*(width+1)*(height+1) and all(isinstance(v,list) and len(v)==2 and
                integer(v[0],0,len(nodes)-1) and integer(v[1],0,len(nodes)-1) and v[0]!=v[1] for v in edges), '路网连接无效')
        require(len({tuple(sorted(v)) for v in edges})==len(edges), '路网连接重复')
        require(all(abs(nodes[a][0]-nodes[b][0])+abs(nodes[a][1]-nodes[b][1])==1 for a,b in edges), '路网连接必须相邻')
        require(integer(network.get('components'),1,len(nodes)) and integer(network.get('junctions'),0,len(nodes)), '路网统计无效')
        require(isinstance(network.get('uncovered'),list) and len(network['uncovered'])<=cells and all(map_point(v) for v in network['uncovered']), '未接入道路坐标无效')
    npcs.check(m)
    migrate_crate_stacks(p)
    if catalog is not None:
        migrate_source_samples(p,catalog)
        migrate_ground_decorations(p,catalog)
    return p


def migrate_crate_stacks(p):
    fields=('width','height','anchor','footprint','cells')
    variants={v['id']:v for v in rotations.VARIANTS if '-stack' in v['id']}
    for pref in p['prefabs']:
        current=variants.get(pref['id'])
        if not current: continue
        legacy=copy.deepcopy(current)
        for cell in legacy['cells']: cell['layer']=1
        if pref.get('anchorMode','shared')=='shared' and all(pref.get(k)==legacy.get(k) for k in fields):
            pref['cells']=copy.deepcopy(current['cells'])


def migrate_source_samples(p,catalog):
    for pref in p['prefabs']:
        # Only upgrade the untouched geometry emitted by the old sampler.
        if not pref['id'].startswith('sample-') or pref.get('anchorMode')!='runtime' or pref['footprint'] or \
                pref['anchor']!=[pref['width']/2,pref['height']] or not pref['cells']: continue
        cells=pref['cells']
        if not all(c['layer']==0 and re.fullmatch(r'(outside|caves|interior):[0-9]+',c['asset']) and
                   c['asset'] in catalog.assets for c in cells): continue
        if not any(catalog.assets[c['asset']].get('hasTransparency') for c in cells): continue
        footprint=set()
        for c in cells:
            asset=catalog.assets[c['asset']]
            c['layer']=asset.get('requiredLayer',1)
            if c['layer'] and asset.get('depth',[0]*3)[c['layer']]==1 and asset.get('runtimeId') not in (391,4773,4747):
                footprint.add((c['x'],c['y']))
        pref['footprint']=[list(point) for point in sorted(footprint)]


def migrate_ground_decorations(p,catalog):
    for pref in p['prefabs']:
        blocking=set(); decorations=set()
        for c in pref['cells']:
            asset=catalog.assets.get(c['asset'],{})
            if not c['layer']: continue
            point=(c['x'],c['y'])
            source=asset.get('provenance',{})
            if c['layer']==1 and (source.get('sheet'),source.get('tile')) in GROUND_DECORATIONS:
                decorations.add(point)
            elif asset.get('depth',[0]*3)[c['layer']]==1 and asset.get('runtimeId') not in (391,4773,4747):
                blocking.add(point)
        # Upgrade the previous default only; keep deliberate custom footprints.
        if decorations and set(map(tuple,pref['footprint']))==blocking|decorations:
            pref['footprint']=[point for point in pref['footprint'] if tuple(point) in blocking]


def base(catalog, name='未命名地图'):
    return dict(schemaVersion=1,name=name,catalogVersion=catalog.version,sources=copy.deepcopy(catalog.sources),
                prefabs=copy.deepcopy(catalog.prefabs),map=dict(id='map-1',size=[16,12],layers=[['outside:385']*192,[None]*192,[None]*192],
                objects=[],entry=dict(point=[7,11],edge='bottom'),junction=[7,11],paths=[],seed=38,areaIndex=0,
                valleyJourney=False,valleyNode=0,**{k:False for k in FLAGS}),origin=dict(type='blank'))


def snapshot(catalog, args):
    seed=args.get('seed',38); area=args.get('area',4); edge=args.get('edge','bottom')
    level=args.get('level',0); count=args.get('count',6); node=args.get('node',0); entrance=args.get('entrance',0)
    if not integer(seed,0,0xffffffff) or not integer(area,0,5) or edge not in EDGES: raise ValueError('地区、种子或入口无效')
    if not integer(level,0,7) or not integer(count,1,8) or level>=count: raise ValueError('段号必须小于总段数，范围 1～8')
    if not integer(node,0,13) or not integer(entrance,0,3): raise ValueError('河谷节点或入口编号无效')
    r=runtime.generate_valley_region(seed,node,entrance) if area==5 else runtime.generate_map(seed,runtime.Edge(EDGES.index(edge)),area,
                                frost_level=level,frost_level_count=count)
    data=runtime.map_json(r)
    for i,path in enumerate(data['paths']): path['fallsToNextLevel']=r.paths[i].falls_to_next_level
    p=base(catalog, ['青草径','溪桥坡','高草园','霜晶窟','雾林径','古瀑谷'][area]+' · 快照')
    p['map'].update(data)
    p['map']['layers']=[[catalog.runtime_keys.get(t,f'runtime:{t}') if t else None for t in layer] for layer in r.layers]
    p['origin']=dict(type='snapshot',seed=seed,area=area,edge=edge,level=level,count=count,node=node,entrance=entrance,
                     algorithmVersion=runtime.ALGORITHM_VERSION,fingerprint=data['fingerprint'])
    group_snapshot_objects(p,catalog)
    return p


def group_snapshot_objects(p,catalog):
    """Recover connected tree pieces sharing native ground depth, losslessly."""
    groups={}
    for i,key in enumerate(p['map']['layers'][1]):
        a=catalog.assets.get(key,{})
        depth=a.get('depth',[0,0,0])[1]
        if a.get('forest') and depth>0:
            groups.setdefault(i//16+depth,set()).add((i%16,i//16))
    serial=0
    for ground,cells in sorted(groups.items()):
        remaining=set(cells)
        while remaining:
            stack=[min(remaining)];component=set()
            while stack:
                x,y=stack.pop()
                if (x,y) not in remaining: continue
                remaining.remove((x,y));component.add((x,y))
                stack.extend([(x-1,y),(x+1,y),(x,y-1),(x,y+1)])
            if len(component)<2: continue
            serial+=1
            left=min(x for x,y in component);top=min(y for x,y in component)
            width=max(x for x,y in component)-left+1;height=max(y for x,y in component)-top+1
            key=f'import-tree-{serial}'
            pref=dict(id=key,name=f'导入树木 {serial}',category='导入物件',width=width,height=height,
                      anchor=[width/2,ground-top],footprint=[[x-left,y-top] for x,y in sorted(component) if y+1==ground],
                      cells=[dict(x=x-left,y=y-top,layer=1,asset=p['map']['layers'][1][y*16+x]) for x,y in sorted(component)])
            p['prefabs'].append(pref)
            p['map']['objects'].append(dict(id=key,prefab=key,x=left,y=top))
            for x,y in component: p['map']['layers'][1][y*16+x]=None


def flatten(p):
    """Resolve each layer and cell by instance order: the last object wins."""
    width,height=p['map']['size']
    layers=copy.deepcopy(p['map']['layers'])
    prefabs={v['id']:v for v in p['prefabs']}
    owners={}
    for obj in p['map']['objects']:
        for c in prefabs[obj['prefab']]['cells']:
            x,y,z=obj['x']+c['x'],obj['y']+c['y'],c['layer']
            if x>=width or y>=height: continue
            slot=(z,y*width+x)
            owners[slot]=obj['id']
            layers[z][y*width+x]=c['asset']
    return layers,owners


def visible_footprint(obj,pref,owners,size=(16,12)):
    width,height=size
    if not any(owners.get((c['layer'],(obj['y']+c['y'])*width+obj['x']+c['x']))==obj['id']
               for c in pref['cells'] if obj['x']+c['x']<width and obj['y']+c['y']<height):return []
    result=[]
    for dx,dy in pref['footprint']:
        x,y=obj['x']+dx,obj['y']+dy
        if not 0<=x<width or not 0<=y<height:continue
        parts=[c for c in pref['cells'] if c['x']==dx and c['y']==dy]
        if parts:
            visible=any(owners.get((c['layer'],y*width+x))==obj['id'] for c in parts)
        else:
            visible=not any(owners.get((c['layer'],y*width+x)) not in (None,obj['id']) for c in pref['cells'])
        if visible:result.append((dx,dy))
    return result


def route_world(path):
    if path.get('kind')=='roadNetwork': return [[x*26,y*26] for x,y in path['points']]
    result=[]
    points=path['points']
    for i,(x,y) in enumerate(points):
        xx,yy=x*26+13,y*26+13
        if not path['cellCentered'] and len(points)>1:
            neighbors=points[max(0,i-1):i]+points[i+1:i+2]
            if any(a==x for a,b in neighbors): xx+=13
            if any(b==y for a,b in neighbors): yy+=13
        result.append([xx,yy])
    return result


def road_source_matches(p,source):
    try: data=json.loads(source)
    except (ValueError,TypeError): return False
    m=p['map']; expected=[m['layers'],m['objects'],p['prefabs']]
    return data==expected+[m['size']] or (m['size'] in ([16,12],[32,24]) and data==expected)


def validate(p,catalog):
    check_project(p,catalog)
    issues=[]
    def issue(code,message,x=None,y=None,obj=None,severity='error'):
        issues.append(dict(code=code,message=message,x=x,y=y,objectId=obj,severity=severity))
    m=p['map']; width,height=m['size']; cells=width*height; layers,owners=flatten(p)
    if m['size'] != [16,12]: issue('editor-size','大地图支持编辑、保存和预览；经典探索设备端导出暂限 16×12，市集另按导航预算校验',severity='warning')
    if not m['valleyJourney']:
        x,y=m['entry']['point']
        if not {'top':y==0,'bottom':y==height-1,'left':x==0,'right':x==width-1}[m['entry']['edge']]:
            issue('entry-edge','边界入口方向与位置不符',x,y)
    if p['catalogVersion']!=catalog.version: issue('catalog','素材库或运行时版本已变化，请核对后更新来源记录',severity='warning')
    for key,value in catalog.sources.items():
        if p['sources'].get(key,{}).get('sha256')!=value['sha256']: issue('source-change',f'{key} 原素材文件发生变化，需重新确认来源')
    unknown=set(); unsupported=set(); used=set()
    for z,layer in enumerate(layers):
        for i,key in enumerate(layer):
            x,y=i%width,i//width
            if key is None:
                if z==0: issue('ground','地面缺少底图',x,y)
                continue
            used.add(key); a=catalog.assets.get(key)
            if not a:
                if key not in unknown: issue('missing',f'找不到素材 {key}',x,y)
                unknown.add(key);continue
            if a['empty']: issue('placeholder','空白或红叉占位图块',x,y)
            if a.get('requiredLayer',z)!=z: issue('layer',f'此图块必须放在 L{a["requiredLayer"]} 层',x,y)
            if a.get('runtimeId') is None:
                if key not in unsupported: issue('unsupported',f'{a["label"]} 尚未接入游戏资源；可保存草稿',x,y)
                unsupported.add(key)
    lake_spec = lake.descriptor()
    automatic = set(lake_spec['byMask'].values())
    water = automatic | set(lake_spec['compatibleKeys'])
    for i,key in enumerate(layers[0]):
        if key not in automatic or m["layers"][0][i] not in automatic: continue
        expected = lake_spec['byMask'][str(lake.mask_at(layers[0],i%width,i//width,water,width,height))]
        if key != expected: issue('lake-edge','湖岸与相邻水域不匹配，请使用自动铺湖重新修岸',i%width,i//width)
        saved = m.get('lakeGround',{}).get(str(i))
        if saved not in catalog.assets or saved in water:
            issue('lake-ground','自动湖泊缺少有效的原地面记录',i%width,i//width)
    for i in m.get('lakeGround',{}):
        if m['layers'][0][int(i)] not in automatic:
            issue('lake-ground','湖泊原地面记录与水域不一致',int(i)%width,int(i)//width)
    prefabs={v['id']:v for v in p['prefabs']}
    route_cells=set()
    for path in m['paths']:
        if path.get('kind')=='roadNetwork': continue
        if path['cellCentered']: route_cells.update(map(tuple,path['points']))
        else:
            for i,(x,y) in enumerate(path['points']):
                route_cells.add((x,y))
                adj=path['points'][max(0,i-1):i]+path['points'][i+1:i+2]
                if any(a==x for a,b in adj): route_cells.add((x+1,y))
                if any(b==y for a,b in adj): route_cells.add((x,y+1))
    for o in m['objects']:
        pref=prefabs[o['prefab']]
        native_footprint=set()
        for c in pref['cells']:
            a=catalog.assets.get(c['asset'],{})
            if c['layer'] and a.get('depth',[0]*3)[c['layer']]==1 and a.get('runtimeId') not in (391,4773,4747):
                native_footprint.add((c['x'],c['y']))
        if all(catalog.assets.get(c['asset'],{}).get('runtimeId') is not None for c in pref['cells']) and native_footprint!=set(map(tuple,pref['footprint'])):
            issue('footprint-semantics','占地标注与游戏图块规则不同，需要恢复默认或扩展运行时规则',o['x'],o['y'],o['id'])
        if o['x']+pref['width']>width or o['y']+pref['height']>height: issue('bounds','完整物件超出地图',o['x'],o['y'],o['id'])
        for dx,dy in visible_footprint(o,pref,owners,m['size']):
            x,y=o['x']+dx,o['y']+dy
            if (x,y) in route_cells: issue('footprint','物件占地与行走道路重叠',x,y,o['id'])
        for c in pref['cells']:
            a=catalog.assets.get(c['asset'],{}); depth=a.get('depth',[0]*3)[c['layer']]
            if pref.get('anchorMode','shared')=='shared' and depth and abs(c['y']+depth-(.5 if a.get('ladder',[False]*3)[c['layer']] else 0)-pref['anchor'][1])>.01:
                issue('anchor','组合锚点与当前游戏遮挡深度不一致',o['x']+c['x'],o['y']+c['y'],o['id'])
    if not 1<=len(m['paths'])<=3: issue('path-count','游戏要求 1～3 条完整路线')
    for pi,path in enumerate(m['paths']):
        points=path['points']; label=f'路线 {pi+1}'
        if path.get('kind')=='roadNetwork':
            network=m['roadNetwork']
            try: fresh=path['roadEntry']==m['entry'] and road_source_matches(p,path['roadSource']) and json.loads(network['source'])==json.loads(path['roadSource'])
            except (ValueError,TypeError): fresh=False
            if not fresh: issue('road-route-stale',label+' 的道路已变化，请重新识别并存为路线')
            edges={tuple(sorted((tuple(network['nodes'][a]),tuple(network['nodes'][b])))) for a,b in network['edges']}
            used_edges={tuple(sorted((tuple(a),tuple(b)))) for a,b in zip(points,points[1:])}
            if points[0]!=points[-1] or used_edges!=edges or not edges:
                issue('road-route-links',label+' 必须沿路网连续走遍各支路并返回起点')
            issue('road-route-export',label+' 为道路播放路线；游戏导出使用独立市集路网，不使用这条遍历轨迹',severity='warning')
            continue
        if not 2<=len(points)<=48: issue('path-size',label+' 需要 2～48 个点')
        if not points: continue
        if points[0]!=m['entry']['point']: issue('path-entry',label+' 未从地图入口开始',*points[0])
        if points[-1]!=path['exit']['point']: issue('path-exit',label+' 末点与出口不一致',*points[-1])
        if m['junction'] not in points: issue('junction',label+' 未经过首分岔',*m['junction'])
        if len(set(map(tuple,points)))!=len(points): issue('loop',label+' 存在重复路线点')
        for a,b in zip(points,points[1:]):
            if abs(a[0]-b[0])+abs(a[1]-b[1])!=1: issue('continuity',label+' 必须逐格连续，不支持斜线或跳格',*b)
        if not path['interiorExit']:
            x,y=path['exit']['point']; edge=path['exit']['edge']
            if not {'top':y==0,'bottom':y==height-1,'left':x==0,'right':x==width-1}[edge]: issue('exit-edge',label+' 边界出口方向与位置不符',x,y)
        if path['interiorExit'] or path['nextValleyNode']!=255 or path['fallsToNextLevel']:
            issue('transition',label+' 包含跨图或楼层转场，当前单图导出不能验证目标',severity='error')
        if path['terminal'] and not m['valleyJourney']:
            issue('terminal','普通地图的探索结束由行程段号决定；terminal 仅适用于河谷',severity='warning')
    for x,y in sorted(route_cells):
        if not 0<=x<width or not 0<=y<height:
            issue('road-bounds','双格道路超出地图边界',min(x,width-1),min(y,height-1)); continue
        a=catalog.assets.get(layers[0][y*width+x],{})
        overlays=[catalog.assets.get(layers[z][y*width+x],{}) for z in (1,2)]
        if a.get('water') and not any(o.get('bridge') for o in overlays): issue('water','路线穿过水面且没有桥面',x,y)
        for z,o in enumerate(overlays,1):
            depth=o.get('depth',[0]*3)[z]
            if o and not o.get('bridge') and depth==1 and o.get('runtimeId') not in (391,4773,4747): issue('obstacle','路线穿过物件落地部位',x,y)
    for path in m['paths']:
        if path.get('kind')=='roadNetwork': continue
        if not path['cellCentered']:
            for x,y in path['points']:
                a=catalog.assets.get(layers[0][y*width+x],{})
                if not a.get('road') and not any(catalog.assets.get(layers[z][y*width+x],{}).get('bridge') for z in (1,2)):
                    issue('road','双格路线中心缺少游戏道路图块',x,y)
    classic_paths=[path for path in m['paths'] if path.get('kind')!='roadNetwork']
    if len(classic_paths)>1 and all(path['points'] for path in classic_paths):
        common=[]
        for values in zip(*(path['points'] for path in classic_paths)):
            if any(v!=values[0] for v in values): break
            common.append(values[0])
        if not common or common[-1]!=m['junction']: issue('first-junction','junction 必须是各完整路线公共前缀的最后一个点')
    for i,key in enumerate(layers[1]):
        if key=='outside:391' and (i+width>=cells or layers[0][i+width]!='outside:399'): issue('grass','长草缺少下半株',i%width,i//width)
    for i,key in enumerate(layers[0]):
        if key=='outside:399' and (i<width or layers[1][i-width]!='outside:391'): issue('grass','长草缺少上半株',i%width,i//width)
    # Curated editor artwork is only exportable as a complete assembly, even
    # after an instance has been explicitly exploded into raw tiles.
    for z,values in enumerate(layers):
        for i,key in enumerate(values):
            if catalog.assets.get(key,{}).get('requiredLayer') not in (1,2): continue
            x,y=i%width,i//width
            owner=owners.get((z,i))
            obj=next((o for o in m['objects'] if o['id']==owner),None)
            source={(c['layer'],obj['y']+c['y'],obj['x']+c['x']):c['asset']
                    for c in prefabs[obj['prefab']]['cells']} if obj else {}
            def assembly_part(c,ox,oy):
                xx,yy,zz=ox+c['x'],oy+c['y'],c['layer']
                if not 0<=xx<width or not 0<=yy<height:return False
                j=yy*width+xx
                if layers[zz][j]==c['asset']:return True
                # An intact source assembly may have parts covered by a later
                # object. Missing parts in an exploded/raw assembly still fail.
                return owners.get((zz,j)) not in (None,owner) and source.get((zz,yy,xx),m['layers'][zz][j])==c['asset']
            matched=False
            for pref in catalog.prefabs:
                for part in pref['cells']:
                    if part['asset']!=key or part['layer']!=z: continue
                    ox,oy=x-part['x'],y-part['y']
                    if all(assembly_part(c,ox,oy) for c in pref['cells']):
                        matched=True;break
                if matched: break
            if not matched: issue('assembly','物件缺少组成图块，请使用完整组合或补齐',x,y)
    if m['size']==[16,12] and not unknown and not unsupported and len(classic_paths)==len(m['paths']):
        r=to_runtime(p,catalog)
        for path in r.paths:
            if not runtime.route_crosses_ice_straight(r,path): issue('ice','冰面路线必须直穿，且分岔与停靠点位于普通地面')
    if m['valleyJourney']: issue('region','河谷快照可编辑；完整导航节点需要地图组适配后才能导出')
    npc_issues,_=npcs.diagnostics(p,catalog)
    issues.extend(npc_issues)
    return dict(issues=issues,errors=sum(v['severity']=='error' for v in issues),warnings=sum(v['severity']=='warning' for v in issues),
                usedAssets=len(used),newAssets=len(unsupported),pack=catalog.pack_stats,worldPaths=[route_world(path) for path in m['paths']])


def require_device_size(p):
    if p['map']['size'] != [16,12]:
        raise ValueError('大地图暂不支持设备端导出或正式绘制器，请保存工程或导出 PNG')


def to_runtime(p,catalog):
    require_device_size(p)
    m=p['map']; layers,_=flatten(p)
    if any(path.get('kind')=='roadNetwork' for path in m['paths']):
        raise ValueError('道路往返路线暂不支持游戏数据包或正式绘制器导出，请使用保存工程、下载工程 JSON 和编辑器播放')
    end=lambda e:runtime.Endpoint(tuple(e['point']),runtime.Edge(EDGES.index(e['edge'])))
    paths=[runtime.RoutePath(points=list(map(tuple,path['points'])),exit=end(path['exit']),
                falls_to_next_level=path['fallsToNextLevel'],cell_centered=path['cellCentered'],
                next_valley_node=path['nextValleyNode'],next_valley_entrance=path['nextValleyEntrance'],
                terminal=path['terminal'],interior_exit=path['interiorExit']) for path in m['paths']]
    return runtime.RuntimeMap(m['seed'],m['areaIndex'],end(m['entry']),tuple(m['junction']),paths,
           [[catalog.assets[key]['runtimeId'] if key else 0 for key in layer] for layer in layers],
           *(m[k] for k in FLAGS),m['valleyJourney'],m['valleyNode'])


def wire(r):
    end=lambda e:[*e.point,int(e.edge)]
    v=[r.seed,r.area_index,*end(r.entry),*r.junction,r.valley_journey,r.valley_node,r.has_coast,r.has_forest,r.has_creek,r.has_cliff,r.has_waterfall,len(r.paths)]
    for p in r.paths:
        v += [len(p.points),*end(p.exit),p.falls_to_next_level,p.cell_centered,p.next_valley_node,p.next_valley_entrance,p.terminal,p.interior_exit]
        v += [x for point in p.points for x in point]
    v += [x for layer in r.layers for x in layer]
    return ' '.join(str(int(x)) for x in v)


def header(r):
    lines=['#pragma once','#include "game/ExploreMapGenerator.h"','namespace AuthoredMap {','inline ExploreMapGenerator::Map load() {','    ExploreMapGenerator::Map m{};']
    def assign(name,value): lines.append(f'    m.{name} = {int(value)};')
    def end(name,e):
        lines.append(f'    m.{name} = {{{{{e.point[0]}, {e.point[1]}}}, static_cast<ExploreMapGenerator::Edge>({int(e.edge)})}};')
    assign('seed',r.seed);assign('areaIndex',r.area_index);end('entry',r.entry)
    lines.append(f'    m.junction = {{{r.junction[0]}, {r.junction[1]}}};')
    for field in ['hasCoast','hasForest','hasCreek','hasCliff','hasWaterfall','valleyJourney','valleyNode']:
        snake=re.sub(r'([A-Z])',lambda x:'_'+x[1].lower(),field)
        assign(field,getattr(r,snake))
    assign('pathCount',len(r.paths))
    for i,p in enumerate(r.paths):
        prefix=f'paths[{i}]'
        assign(prefix+'.pointCount',len(p.points));end(prefix+'.exit',p.exit)
        for field in ['fallsToNextLevel','cellCentered','nextValleyNode','nextValleyEntrance','terminal','interiorExit']:
            assign(prefix+'.'+field,getattr(p,re.sub(r'([A-Z])',lambda x:'_'+x[1].lower(),field)))
        for j,(x,y) in enumerate(p.points): lines.append(f'    m.{prefix}.points[{j}] = {{{x}, {y}}};')
    for z,layer in enumerate(r.layers):
        for i,tile in enumerate(layer):
            if tile: assign(f'layers[{z}][{i}]',tile)
    return '\n'.join(lines+['    return m;','}','}  // namespace AuthoredMap',''])


def render_project(p,catalog,size=32,frame=0):
    check_project(p,catalog)
    width,height=p['map']['size']
    layers,_=flatten(p)
    image=Image.new('RGBA',(width*32,height*32),(0,0,0,255))
    for layer in layers:
        for i,key in enumerate(layer):
            if key in catalog.assets: image.alpha_composite(catalog.tile(key,frame),(i%width*32,i//width*32))
    return image if size==32 else image.resize((width*size,height*size),Image.Resampling.NEAREST)


def pave(p,catalog):
    require_device_size(p)
    check_project(p,catalog)
    p=copy.deepcopy(p)
    m=p['map']
    end=lambda e:runtime.Endpoint(tuple(e['point']),runtime.Edge(EDGES.index(e['edge'])))
    paths=[runtime.RoutePath(list(map(tuple,v['points'])),end(v['exit'])) for v in m['paths'] if not v['cellCentered']]
    if not paths: raise ValueError('请选择或绘制双格道路路线')
    roads=runtime.build_road(paths)
    from types import SimpleNamespace
    context=SimpleNamespace(paths=paths,entry=end(m['entry']))
    for x,y in roads:
        if 0<=x<16 and 0<=y<12:
            tile=runtime.road_tile(context,roads,x,y)
            m['layers'][0][y*16+x]=catalog.runtime_keys[tile]
    spec=lake.descriptor(); automatic=set(spec['byMask'].values()); water=automatic|set(spec['compatibleKeys'])
    for i in list(m.get('lakeGround',{})):
        if m['layers'][0][int(i)] not in automatic: del m['lakeGround'][i]
    values=flatten(p)[0][0]
    for i,key in enumerate(m['layers'][0]):
        if key in automatic: m['layers'][0][i]=spec['byMask'][str(lake.mask_at(values,i%16,i//16,water))]
    return p


def compile_project(p,catalog):
    require_device_size(p)
    report=validate(p,catalog)
    if report['errors']: return None,report
    if any(path.get('kind')=='roadNetwork' for path in p['map']['paths']):
        from market_export import package
        return package(p,catalog)
    import generate_game_assets as ga
    ga.validate_kind_order();ga.validate_committed_pack_kind_ids()
    r=to_runtime(p,catalog)
    proc=subprocess.run([str(native_binary())],input=wire(r),text=True,capture_output=True,check=True)
    lines=proc.stdout.splitlines(); expected=f'{runtime.fingerprint(r):08x}'
    if lines[0]!=expected: raise ValueError('Python/C++ 地图指纹不一致，已阻止导出')
    native_points=[list(map(float,line.split())) for line in lines[1:]]
    expected_points=[v for path in report['worldPaths'] for v in path]
    if native_points!=expected_points: raise ValueError('Python/C++ 行走坐标不一致，已阻止导出')
    data=runtime.map_json(r)
    for i,path in enumerate(data['paths']): path['fallsToNextLevel']=r.paths[i].falls_to_next_level
    report.update(fingerprint=expected,nativeVerified=True)
    out=io.BytesIO()
    layers,_=flatten(p)
    keys=sorted({key for layer in layers for key in layer if key})
    requirements={key:catalog.assets[key] for key in keys}
    with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as archive:
        for name,value in [('project.stickmap.json',p),('runtime-map.json',data),('validation.json',report),('resource-requirements.json',requirements)]:
            archive.writestr(name,json.dumps(value,ensure_ascii=False,indent=2))
        archive.writestr('AuthoredMap.h',header(r))
        archive.writestr('preview.png',png(render_project(p,catalog)))
        if p['map'].get('activityZones') or p['map'].get('npcs'):
            activity=dict(version=2,zones=p['map'].get('activityZones',[]),npcs=p['map'].get('npcs',[]))
            archive.writestr('activity.json',json.dumps(activity,ensure_ascii=False,indent=2))
            archive.writestr('AuthoredNpcs.h',npcs.header(p,catalog))
            archive.writestr('AuthoredNpc.h',(ROOT/'src/game/AuthoredNpc.h').read_bytes())
            _,grid=npcs.diagnostics(p,catalog)
            rules=npcs.population.definitions(p,grid)
            archive.writestr('AuthoredPopulations.h',npcs.population.header(p,grid))
            archive.writestr('AuthoredNpcPopulation.h',(ROOT/'src/game/AuthoredNpcPopulation.h').read_bytes())
            activity_families={f['id']:f for r in rules for f in r['families']}
            archive.writestr('npc-families.json',json.dumps(list(activity_families.values()),ensure_ascii=False,indent=2))
            used={n['sprite'] for n in activity['npcs']}
            used.update(f'pokemon-{f[k]:03d}' for f in activity_families.values() for k in ('adult','juvenile') if f[k])
            sprites=[s for s in npcs.sprite_catalog(include_legacy=True) if s['id'] in used]
            archive.writestr('npc-sources.json',json.dumps(sprites,ensure_ascii=False,indent=2))
            for s in sprites:
                archive.writestr('npc-art/'+s['id']+'.png',npcs.sprite_png(s['id']))
                if not s.get('legacy'):
                    archive.writestr(f'npc-packs/{s["speciesId"]:03d}.smonsp',npcs.sprite_path(s['id']).read_bytes())
            archive.writestr('NPC-README.txt','区域与 NPC 配置见 activity.json，精灵来源、speciesId、帧矩形与脚底锚点见 npc-sources.json。\n精灵 PNG 图集在 npc-art；对应游戏原资源包在 npc-packs，来源 sha256 指向原包，atlasSha256 指向 PNG。旧版人物保留原 PNG 与来源哈希。\nAuthoredNpcs::load(definitions) 后调用 AuthoredNpc::Simulation::reset，每 100ms 调用 tick；pose 返回格坐标、朝向、行走帧。\n有随机来访时，同时调用 AuthoredPopulations::load(rules)，使用 AuthoredNpcPopulation::Simulation::reset(definitions, AuthoredNpcs::COUNT, rules, AuthoredPopulations::COUNT, runtimeSeed)。每 100ms tick；pose 返回 actor、visible、speciesId，隐藏时不绘制。固定 NPC 的 speciesId=0，仍按 activity.json 的 sprite 绘制。\n随机来访规则、全部候选组合和精灵资源均已导出；来访间隔从上一组完全离开后计时。修改 runtimeSeed 可重新随机，暂停不重置。通道错峰使用；停留计时结束后可能等待其他组通过通道。\n图集 actions.idle/walking 按下、左、上、右排列，使用每帧 width/height/groundOffsetY 对齐脚底；精灵比例为每格 26px。\n区域与地形已合成 allowed 网格；修改地形需重新导出。\n设备端仍需在自定义地图入口加载定义、加载精灵资源，并按脚底深度绘制精灵。现有地图场景不会自动加载此文件。\npreview.png 与正式绘制器预览目前只包含地图；NPC 的精灵行走效果请使用编辑器“预览活动”。\n')
        archive.writestr('README.txt','此包是待接入地图源数据，不会自动替换正式地图。\n在地图选择/生成入口显式调用 AuthoredMap::load()；资源依赖使用当前项目地图包。\nPython/C++ 指纹及路线坐标已检查；浏览器预览不是实机截图。\n新增素材、任意遮挡和跨图转场必须适配后导出；不可仅复制 PNG 进入游戏。\n')
    return out.getvalue(),report
