"""Approved forest progression layouts and connected original-art vegetation patches."""
from collections import deque
from functools import lru_cache
from pathlib import Path
import random
from PIL import Image
from asset_paths import essentials_dir

ROOT = Path(__file__).resolve().parents[1]
W, H, T = 16, 12, 32
ROUND = [(868, 1, 0)] + [(tile, x, y + 1) for y, row in enumerate(
    ((875, 876, 877), (883, 884, 885), (891, 892, 893))) for x, tile in enumerate(row)]
WIDE = [(tile, x, y) for y, row in enumerate(
    ((947, 948, 949), (955, 956, 957), (979, 980, 981), (987, 988, 989)))
    for x, tile in enumerate(row)]
PINE = [(854, 0, 0), (862, 0, 1), (870, 0, 2)]
PARTS = dict(round=ROUND, wide=WIDE, pine=PINE)
TITLES = ['林缘 S 弯', '夹林曲径', '混交错位双路', '环抱草甸', '深林折线', '树冠横廊', '深林三路']
NOTES = ['圆冠树 · 开阔入口与支路', '圆冠树 · 林道收拢、保留分岔',
         '圆冠为主 · 右侧宽冠树开始成片', '左右混交 · 林中留一处草甸',
         '宽冠为主 · 绕树进入深林', '宽冠树 · 直路穿过树冠下方',
         '宽冠树 · 分岔前保留可见空地']
LANES = [7, 5, 9, 7, 10, 5, 8, 7]
EXTRA_ART = {
    'red_flowers': ('Autotiles/Flowers1.png', (0, 0, 32, 32)),
    'white_flowers': ('Autotiles/Flowers2.png', (0, 0, 32, 32)),
    'sapling': ('Characters/Object tree 1.png', (0, 0, 32, 32)),
    'oran': ('Characters/berrytree_ORANBERRY.png', (0, 192, 32, 256)),
}


@lru_cache(None)
def extra_art(name):
    source, box = EXTRA_ART[name]
    return Image.open(essentials_dir() / 'Graphics' / source).convert('RGBA').crop(box)


def rect(x0, y0, x1, y1):
    return {(x, y) for y in range(y0, y1 + 1) for x in range(x0, x1 + 1)}


def neighbors(point):
    x, y = point
    return {(x-1,y), (x+1,y), (x,y-1), (x,y+1)}


def halo(cells):
    return cells | {q for p in cells for q in neighbors(p)}


def grow_patch(rng, available, preferred, minimum, maximum, height=1):
    """Grow connected plants with compact, uneven outlines and full footprints."""
    anchors = {p for p in available
               if rect(p[0],p[1],p[0],p[1]+height-1) <= available}
    jitter = {p:rng.random() for p in sorted(anchors)}
    seeds = sorted(anchors, key=lambda p:(p not in preferred, -jitter[p], p))
    best = None
    for start in seeds[:36]:
        plants, cells = {start}, rect(start[0],start[1],start[0],start[1]+height-1)
        target = rng.randint(minimum,maximum)
        while len(plants) < target:
            candidates = set()
            for x,y in plants:
                for dx,dy in ((-1,-1),(-1,0),(-1,1),(1,-1),(1,0),(1,1),(0,-height),(0,height)):
                    candidates.add((x+dx,y+dy))
            options = []
            for p in sorted(candidates & anchors - plants):
                foot = rect(p[0],p[1],p[0],p[1]+height-1)
                if foot & cells or not (halo(cells) & foot):
                    continue
                combined = cells | foot
                width = max(x for x,y in combined)-min(x for x,y in combined)+1
                depth = max(y for x,y in combined)-min(y for x,y in combined)+1
                if width > 5 or depth > 4:
                    continue
                contacts = sum(len(neighbors(q) & cells) for q in foot)
                score = contacts*2 + (p in preferred)*1.5 - abs(p[0]-start[0])*.3
                score += rng.random()*3 - (width*depth-len(combined))*.2
                options.append((score,p,foot))
            if not options:
                break
            _,p,foot = max(options,key=lambda item:item[:2])
            plants.add(p)
            cells |= foot
        if len(plants) < minimum:
            continue
        # Clip a corner off larger filled rectangles to avoid garden-bed shapes.
        if height == 1 and len(plants) > minimum:
            xs, ys = {x for x,y in cells}, {y for x,y in cells}
            if len(xs)>1 and len(ys)>1 and len(cells) == len(xs)*len(ys):
                corner = rng.choice(sorted({(x,y) for x in (min(xs),max(xs))
                                           for y in (min(ys),max(ys))}))
                plants.remove(corner)
                cells.remove(corner)
        width = len({x for x,y in cells})
        depth = len({y for x,y in cells})
        score = len(plants)*3 + min(width,depth)*2 + len(cells & preferred)
        # Prefer a shaped patch over a thin line; retain narrow edge patches
        # when the available forest floor cannot fit a wider one.
        if best is None or score > best[0]:
            best = score, plants, cells
        if len(plants) == target and width >= 2 and depth >= 2:
            break
    return (best[1],best[2]) if best else (set(),set())


def stroke(controls):
    cells = set()
    for (x, y), (xx, yy) in zip(controls, controls[1:]):
        assert x == xx or y == yy
        cells |= rect(min(x, xx), min(y, yy), max(x, xx) + 1, max(y, yy) + 1)
    return cells & rect(0, 0, 15, 11)


def layout(index, entry_edge=2, entry_lane=None, variant=0):
    start,end=LANES[index:index+2]
    if entry_lane is not None:start=entry_lane
    if entry_edge==0:start,end=end,start
    branches=[]
    if index==0:
        controls=[(start,11),(start,9),(11,9),(11,5),(2,5),(2,1),(end,1),(end,0)]
    elif index==1:
        controls=[(start,11),(start,8),(2,8),(2,3),(end,3),(end,0)]
    elif index==2:
        controls=[(start,11),(start,8),(4,8),(4,3),(end,3),(end,0)]
        branches=[[(start,8),(12,8),(12,1),(end,1)]]
    elif index==3:
        controls=[(start,11),(start,8),(2,8),(2,2),(end,2),(end,0)]
    elif index==4:
        controls=[(start,11),(start,9),(13,9),(13,4),(8,4),(8,6),(2,6),(2,1),(end,1),(end,0)]
    elif index==5:
        controls=[(start,11),(start,9),(12,9),(12,4),(end,4),(end,0)]
    else:
        controls=[(start,11),(start,8),(8,8),(8,4),(end,4),(end,0)]
        branches=[[(start,8),(2,8),(2,2),(end,2)],[(8,7),(13,7),(13,1),(end,1)]]
    controls=[p for i,p in enumerate(controls) if i==0 or p!=controls[i-1]]
    if entry_edge==0:
        controls.reverse()
    elif entry_edge in (1,3):
        def project(point):
            x,y=point
            xx=(min(y,10)*14+5)//10
            if entry_edge==3:xx=14-xx
            return xx,(x*10+7)//14
        controls=[project(p) for p in controls]
        branches=[[project(p) for p in branch] for branch in branches]
        controls[0]=(15 if entry_edge==1 else 0,controls[0][1])
        controls[-1]=(0 if entry_edge==1 else 15,controls[-1][1])
    roads=stroke(controls)
    for branch in branches:roads|=stroke(branch)
    turns={p for route in [controls,*branches] for p in route[1:-1]}
    turns.update(p for branch in branches for p in (branch[0],branch[-1]))
    protected=set()
    for x,y in turns:protected|=rect(x-1,y,x+2,y+2)&roads
    if entry_edge%2==0:protected|={(x,y) for x,y in roads if y<2 or y>9}
    else:protected|={(x,y) for x,y in roads if x<2 or x>13}
    clear=set()
    if index==3:
        # Reserve a small visible glade beside the route, with no tree roots.
        center=(6,5)
        if entry_edge%2: center=(8,5)
        clear=rect(center[0]-2,center[1]-1,center[0]+2,center[1]+1)-roads
    return roads,protected,clear,controls,branches


def design(index, seed, entry_edge=2, entry_lane=None, variant=0):
    rng = random.Random(seed * 131 + index + variant * 7919)
    roads, protected, clearing, controls, branches = layout(index,entry_edge,entry_lane,variant)
    roots, canopy, objects = set(), set(), []
    def add(kind, left, top, force=False):
        width, height = (1, 3) if kind == 'pine' else (3, 4)
        parts = PARTS[kind]
        footprint = {(left + dx, top + dy) for _, dx, dy in parts}
        visible = footprint & rect(0, 0, 15, 11)
        if len(visible) < 2:
            return False
        foot = rect(left, top + height - 1, left + width - 1, top + height - 1)
        # Trunks and roots stay clear even when foliage crosses the path.
        solid = foot | rect(left + (width // 2), top + height - 2,
                            left + (width // 2), top + height - 1)
        if solid & roads or foot & roots or visible & (protected | clearing):
            return False
        if not force and any(o['left'] == left and abs(o['top'] - top) < 2 for o in objects):
            return False
        if len((canopy | visible) & roads) > max(3, int(len(roads) * (.12 if index < 2 else .18))):
            return False
        roots.update(foot)
        canopy.update(visible)
        objects.append(dict(kind=kind, left=left, top=top, height=height, parts=parts))
        return True

    def species(x, y):
        if index < 2:
            return 'round'
        if index == 2:
            return 'wide' if x >= 10 else 'round'
        if index == 3:
            return 'wide' if x >= 7 else 'round'
        if index == 4:
            return 'round' if x < 3 and y > 6 else 'wide'
        return 'wide'

    # Place readable landmarks first, then grow irregular groups toward edges.
    features = [[('round', 10, 1), ('round', 1, 0), ('round', 10, 6)],
                [('round', 6, 4), ('round', 0, 0), ('round', 11, 7)],
                [('round', 4, 4), ('wide', 11, 0), ('wide', 12, 5)],
                [('round', 0, 1), ('wide', 11, 0), ('wide', 12, 6)],
                [('wide', 7, 4), ('wide', 1, 7), ('wide', 11, 0)],
                [('wide', 11, 4), ('wide', 6, 2), ('wide', 10, 0)],
                [('wide', 3, 6), ('wide', 11, 7), ('wide', 10, 0)]][index]
    if index==2 and entry_edge%2==0:
        features.insert(0,('round',7,4))
    if index==6 and entry_edge%2==0:
        features[0:0]=[('wide',4,4),('wide',10,3)]
    for args in features:
        add(*args, force=True)
    # A continuous surrounding forest extends beyond the viewport.
    boundary = [(x, y) for y in (-3, -1, 1, 3, 5, 7, 9, 11)
                for x in (-2, 0, 13, 15)]
    boundary += [(x, y) for y in (-3, 9, 11) for x in (2, 5, 8, 11)]
    rng.shuffle(boundary)
    for x, y in boundary:
        add(species(x, y), x, y)
    choices = [(x, y) for y in range(-1, 9) for x in range(1, 13)]
    rng.shuffle(choices)
    target = (20, 23, 27, 24, 29, 31, 30)[index]
    for x, y in choices:
        if len(objects) >= target:
            break
        if any(abs(x - o['left']) < 2 and abs(y - o['top']) < 2 for o in objects):
            continue
        add(species(x, y), x, y)
    if index < 4:
        for x, y in choices:
            if len([o for o in objects if o['kind'] == 'pine']) >= 3:
                break
            if (x, y + 2) in canopy:
                continue
            add('pine', x, y)

    # Use coherent patches instead of independent random plant cells.
    free = rect(0, 0, 15, 11) - roads - roots
    visible = free - canopy
    edge = halo(roots) & free
    patches, occupied = [], set()

    def patch(kind, available, preferred, minimum, maximum, height=1):
        same_kind = {tuple(p) for group in patches if group['kind'] == kind for p in group['cells']}
        anchors,cells = grow_patch(rng,available-occupied-halo(same_kind),preferred,
                                  minimum,maximum,height)
        if cells:
            patches.append(dict(kind=kind,anchors=sorted(anchors),cells=sorted(cells)))
            occupied.update(cells)
        return anchors,cells

    flowers, grass, long_grass = set(), set(), set()
    if index in (0, 1, 3):
        _,flowers = patch('flowers',visible,clearing if index == 3 else halo(edge),3,6)
    for _ in range(1 if index < 4 else 2):
        anchors,_cells = patch('long_grass',visible-clearing,halo(edge),3,5,height=2)
        long_grass |= anchors
    for _ in range(2):
        _,cells = patch('grass',free-clearing,visible & halo(edge),4,9)
        grass |= cells
    bushes = set()
    sites = sorted(visible - occupied - clearing)
    rng.shuffle(sites)
    for p in sites:
        if any(abs(p[0]-x)+abs(p[1]-y) <= 1 for x,y in grass | roots):
            bushes.add(p)
        if len(bushes) >= 4:
            break
    # Each stage gets a small landmark group, never scattered objects on roads.
    decorations = []
    available = visible - occupied - bushes

    def decorate(kind, height, target, edge=False):
        candidates = [(x, y) for x, y in sorted(available)
                      if rect(x, y, x, y + height - 1) <= available]
        if edge:
            candidates = [(x, y) for x, y in candidates
                          if any(abs(x-rx) + abs(y-ry) <= 2 for rx, ry in roots)]
        candidates.sort(key=lambda p: (abs(p[0]-target[0])+abs(p[1]-target[1]), p))
        if not candidates:
            return
        x, y = candidates[0]
        available.difference_update(rect(x, y, x, y + height - 1))
        decorations.append(dict(kind=kind, left=x, top=y, height=height))

    if index in (0, 3):
        decorate('oran', 2, (11, 5) if index == 0 else (6, 2))
    for target in ((2, 5), (12, 8)):
        decorate('sapling', 1, target, edge=True)
    if index in (2, 4, 6):
        decorate('stone', 1, (12, 7), edge=True)
    result = dict(index=index, variant=variant, title=TITLES[index], seed=seed, roads=sorted(roads),
        protected=sorted(protected), clearing=sorted(clearing), controls=controls,
        branches=branches, objects=objects, grass=sorted(grass), flowers=sorted(flowers),
        bushes=sorted(bushes), long_grass=sorted(long_grass), plant_patches=patches,
        decorations=decorations, roots=sorted(roots),
        canopy_on_road=sorted(canopy & roads),
        south_lane=LANES[index], north_lane=LANES[index+1], entry_edge=entry_edge,
        entry_point=controls[0],exit_edge=(entry_edge+2)%4,exit_point=controls[-1])
    validate(result)
    return result


def validate(data):
    roads = set(map(tuple, data['roads']))
    seen, queue = set(), deque([next(iter(roads))])
    while queue:
        p = queue.popleft()
        if p in seen:
            continue
        seen.add(p)
        queue.extend(q for q in ((p[0]+1,p[1]),(p[0]-1,p[1]),(p[0],p[1]+1),(p[0],p[1]-1))
                     if q in roads and q not in seen)
    assert seen == roads
    assert not set(map(tuple, data['roots'])) & roads
    assert not set(map(tuple, data['canopy_on_road'])) & set(map(tuple, data['protected']))
    for key in ('flowers', 'grass', 'bushes'):
        assert not set(map(tuple, data[key])) & roads, key
    occupied = set()
    by_kind = {}
    for group in data['plant_patches']:
        cells = set(map(tuple,group['cells']))
        assert len(group['anchors']) >= (4 if group['kind'] == 'grass' else 3)
        assert len(group['anchors']) <= {'grass':9,'flowers':6,'long_grass':5}[group['kind']]
        assert not cells & (roads | set(map(tuple,data['roots'])) | occupied)
        assert not cells & halo(by_kind.get(group['kind'],set()))
        assert cells <= rect(0,0,15,11)
        reached, queue = set(), [next(iter(cells))]
        while queue:
            p = queue.pop()
            if p in reached:
                continue
            reached.add(p)
            queue.extend((neighbors(p)&cells)-reached)
        assert reached == cells
        if group['kind'] == 'long_grass':
            assert cells == {p for x,y in group['anchors'] for p in ((x,y),(x,y+1))}
        occupied |= cells
        by_kind.setdefault(group['kind'],set()).update(cells)
    for key in ('grass','flowers'):
        assert by_kind.get(key,set()) == set(map(tuple,data[key]))
    for obj in data['decorations']:
        assert not rect(obj['left'], obj['top'], obj['left'], obj['top']+obj['height']-1) & roads
    for key in ('entry','exit'):
        x,y = data[f'{key}_point']
        pair = (x+1,y) if data[f'{key}_edge']%2 == 0 else (x,y+1)
        assert {(x,y),pair} <= roads

