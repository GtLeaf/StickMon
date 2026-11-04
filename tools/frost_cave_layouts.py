"""Validated original-asset cave geometry, shared with the generated C++ table.

Only floor masks and compressed route controls are stored; the device assembles
native tiles and seeded scenery at runtime. Rebuild the table with this module
when changing recipes. The four families preserve the approved preview layouts.
"""
import argparse
from collections import deque
from functools import lru_cache
from pathlib import Path
import copy
import random
from enum import IntEnum

MAP_W, MAP_H = 16, 12
GRID = {(x, y) for x in range(MAP_W) for y in range(MAP_H)}
DIRS = ((1, 0), (-1, 0), (0, 1), (0, -1))
LAYOUTS_PER_FAMILY = 96
FAMILY_COUNT = 4
LAYOUT_COUNT = LAYOUTS_PER_FAMILY * FAMILY_COUNT


def append_line(points, x, y):
    while points[-1][0] != x:
        px, py = points[-1]
        points.append((px + (1 if x > px else -1), py))
    while points[-1][1] != y:
        px, py = points[-1]
        points.append((px, py + (1 if y > py else -1)))


def boundary_tile(floor, openings, x, y):
    outside = frozenset(name for dx, dy, name in (
        (0, -1, 'top'), (1, 0, 'right'), (0, 1, 'bottom'), (-1, 0, 'left'))
        if (x + dx, y + dy) not in floor)
    tiles = {frozenset(): 0,
        frozenset(('top',)): 4513, frozenset(('right',)): 4516,
        frozenset(('bottom',)): 4518, frozenset(('left',)): 4515,
        frozenset(('top', 'left')): 4512, frozenset(('top', 'right')): 4514,
        frozenset(('bottom', 'left')): 4517, frozenset(('bottom', 'right')): 4519}
    if outside not in tiles:
        raise ValueError(('unsupported native cave corner', x, y))
    return tiles[outside]


# These recipe helpers use the same direction values as ExploreMapGenerator.
class Edge(IntEnum):
    TOP = 0
    RIGHT = 1
    BOTTOM = 2
    LEFT = 3


FROST_WALL_TOP = 4513


def rect(x1,y1,x2,y2):
    return {(x,y) for x in range(max(0,x1),min(15,x2)+1) for y in range(max(0,y1),min(11,y2)+1)}

def line(points):
    out=[points[0]]
    for p in points[1:]: append_line(out,*p)
    return out

def components(cells):
    result=[]; cells=set(cells)
    while cells:
        found={cells.pop()}; todo=list(found)
        while todo:
            x,y=todo.pop()
            for dx,dy in DIRS:
                q=x+dx,y+dy
                if q in cells: cells.remove(q); found.add(q); todo.append(q)
        result.append(found)
    return result

def make_spec(family,seed):
    rng=random.Random(seed)
    if family==0:
        low=rng.choice([7,8,9]); high=rng.choice([2,3]); x=rng.choice([3,4]); z=rng.choice([9,10,11])
        entry=((0,high),Edge.LEFT); exits=(((15,high),Edge.RIGHT),((z,11),Edge.BOTTOM)); junction=(z,low)
        stem=[(0,high),(x,high),(x,low),(z,low)]
        routes=[stem+[(z,high),(15,high)],stem+[(z,11)]]
        rooms=[(x,high,2,2),((x+z)//2,low,2,2),(13,high,2,2)]
        ice=[]
    elif family==1:
        y=rng.choice([5,6]); x=rng.choice([5,6,7]); top=rng.choice([2,3]); far=rng.choice([11,12]); low=rng.choice([8,9])
        entry=((0,y),Edge.LEFT); exits=(((far,0),Edge.TOP),((15,low),Edge.RIGHT)); junction=(x,y)
        routes=[[(0,y),(x,y),(x,top),(far,top),(far,0)],[(0,y),(x,y),(far,y),(far,low),(15,low)]]
        rooms=[(x,y,3,3),(far,top,2,2),(far,low,2,2)]
        ice=[]
    elif family==2:
        y=rng.choice([4,5,6]); start=rng.choice([4,5]); end=rng.choice([9,10]); j=rng.choice([12,13]); ey=rng.choice([8,9])
        entry=((0,ey),Edge.LEFT); exits=(((15,y),Edge.RIGHT),((j,11),Edge.BOTTOM)); junction=(j,y)
        stem=[(0,ey),(2,ey),(2,y),(j,y)]
        routes=[stem+[(15,y)],stem+[(j,11)]]
        rooms=[(2,ey,2,2),(2,y,2,2),(j,y,2,3)]
        ice=[(start,y-1,end,y+1)]
    else:
        y=rng.choice([7,8,9]); j=rng.choice([6,7,8]); left=rng.choice([3,4]); right=rng.choice([11,12]); h=rng.choice([2,3])
        entry=((j,11),Edge.BOTTOM); exits=(((left,1),Edge.TOP),((right,1),Edge.TOP)); junction=(j,y)
        stem=[(j,11),(j,y)]
        routes=[stem+[(left,y),(left,h),(left,1)],stem+[(right,y),(right,h),(right,1)]]
        rooms=[(j,y,2,2),(left,h+1,2,2),(right,h+1,2,2)]
        ice=[]
    expanded=[line(p) for p in routes]
    route=set().union(*map(set,expanded))
    floor=set().union(*(rect(x-1,y-1,x+1,y+1) for x,y in route))
    for x,y,rx,ry in rooms:
        # Room dimensions vary independently of the branch skeleton.
        floor |= rect(x-rx,y-ry,x+rx+(rng.randrange(2) if x<12 else 0),y+ry)
    for x1,y1,x2,y2 in ice: floor |= rect(x1-1,y1-1,x2+1,y2+1)
    # Closed solid islands are disallowed in this draft; attach all rock mass to an outer edge.
    for c in components(GRID-floor):
        if not any(x in (0,15) or y in (0,11) for x,y in c): floor |= c
    # Resolve concave one-cell notches using existing, unambiguous native corner modules.
    for _ in range(20):
        add=set()
        for x,y in floor:
            outside=[(x+dx,y+dy) for dx,dy in DIRS if (x+dx,y+dy) not in floor]
            if len(outside)>2 or (len(outside)==2 and outside[0][0]+outside[1][0]==2*x and outside[0][1]+outside[1][1]==2*y):
                add.update(p for p in outside if p in GRID)
            diagonal=[]
            for dx,dy in ((-1,-1),(1,-1),(-1,1),(1,1)):
                if (x+dx,y+dy) not in floor and (x+dx,y) in floor and (x,y+dy) in floor: diagonal.append((x+dx,y+dy))
            if len(diagonal)>1: add.update(p for p in diagonal if p in GRID)
        add-=floor
        if not add: break
        floor |= add
    spec=dict(rects=tuple((x,y,x,y) for x,y in sorted(floor)),entry=entry,exits=exits,junction=junction,routes=routes,ice_rects=ice)
    return spec,floor

"""Preview-only: closed cave boundary and complete native stair landings."""

def wall_mask(floor):
    walls={p for p in floor if boundary_tile(floor,set(),*p)}
    for x,y in floor-walls:
        for dx,dy in ((-1,-1),(1,-1),(-1,1),(1,1)):
            if (x+dx,y+dy) not in floor and (x+dx,y) in floor and (x,y+dy) in floor:
                walls.add((x,y));break
    return walls


def shortest(start,end,allowed):
    todo=deque([start]); prev={start:None}
    while todo:
        p=todo.popleft()
        if p==end:
            result=[]
            while p is not None: result.append(p);p=prev[p]
            return result[::-1]
        for dx,dy in DIRS:
            q=p[0]+dx,p[1]+dy
            if q in allowed and q not in prev: prev[q]=p;todo.append(q)
    raise ValueError(('unreachable stair landing',start,end))


def prep_portals(spec,floor,family):
    walls=wall_mask(floor)
    walk=floor-walls
    ice=set().union(*(rect(*r) for r in spec['ice_rects'])) if spec['ice_rects'] else set()
    original=[line(p) for p in spec['routes']]
    trimmed=[]
    for points in original:
        while points[0] not in walk: points.pop(0)
        while points[-1] not in walk: points.pop()
        assert set(points)<=walk
        trimmed.append(points)
    # Upward ladder's upper sprite must occupy a straight north wall, not a corner.
    # Its lower sprite occupies walkable floor, with one dry step in front.
    up_candidates=[]
    for x,y in sorted(walk-ice):
        if y<1 or (x,y+1) not in walk-ice: continue
        above=(x,y-1)
        if above not in walls: continue
        if boundary_tile(floor,set(),*above)!=FROST_WALL_TOP: continue
        if not {(x-1,y-1),(x+1,y-1)}<=walls: continue
        if not {(x-1,y),(x+1,y)}<=floor: continue
        up_candidates.append((x,y))
    # Keep the reference layout's downward-left / upward-right convention for D.
    # Other families choose the exit closest to a valid upright wall module.
    exit_choices=[]
    for index,points in enumerate(trimmed):
        if family==3 and index!=1: continue
        for p in up_candidates:
            if p==spec['junction'] or p==points[0]: continue
            cost=abs(p[0]-points[-1][0])+abs(p[1]-points[-1][1])
            exit_choices.append((cost,index,p))
    _,up_index,up_point=min(exit_choices)
    entry=trimmed[0][0]
    exits=[p[-1] for p in trimmed]
    exits[up_index]=up_point
    reserved={entry,*exits}
    assert len(reserved)==3
    # Preserve each branch's full original centerline to its chamber. Only append
    # the short connection from its old landing to the upright stair module.
    new=[]
    for index,points in enumerate(trimmed):
        if index==up_index:
            allowed=(walk-ice)-({entry,*exits}- {up_point})
            # Backtrack at most to the branch junction to remove terminal hooks.
            j=points.index(spec['junction'])
            options=[]
            for k in range(j,len(points)):
                if points[k] not in allowed: continue
                try: tail=shortest(points[k],up_point,allowed)
                except ValueError: continue
                if set(tail[1:]) & set(points[:k]): continue
                extra=len(tail)-1
                deviation=len(set(tail)-set(points))
                options.append((deviation*10 + extra + (len(points)-1-k),k,tail))
            assert options
            _,k,tail=min(options)
            points=points[:k]+tail
        new.append(points)
    spec=copy.deepcopy(spec)
    spec['entry']=(entry,Edge.LEFT)
    spec['exits']=tuple((p,Edge.TOP if i==up_index else Edge.BOTTOM) for i,p in enumerate(exits))
    spec['routes']=new
    spec['up_exit']=up_index
    return spec


def route_controls(points):
    controls = [points[0]]
    for i in range(1, len(points) - 1):
        a, b, c = points[i - 1:i + 2]
        if (b[0] - a[0], b[1] - a[1]) != (c[0] - b[0], c[1] - b[1]):
            controls.append(b)
    controls.append(points[-1])
    return tuple(controls)


@lru_cache(maxsize=LAYOUT_COUNT)
def layout(index):
    family, recipe_seed = index % FAMILY_COUNT, index // FAMILY_COUNT + 1
    spec, floor = make_spec(family, recipe_seed)
    spec = prep_portals(spec, floor, family)
    # Slot 1 is the upward ladder throughout existing session transition rules.
    routes = spec['routes']
    if spec['up_exit'] == 0:
        routes = routes[::-1]
    controls = tuple(route_controls(path) for path in routes)
    assert all(2 <= len(p) <= 8 for p in controls)
    assert all(len(path) <= 48 and len(path) == len(set(path)) for path in routes)
    assert routes[1][-2] == (routes[1][-1][0], routes[1][-1][1] + 1)
    assert len(set((routes[0][0], routes[0][-1], routes[1][-1]))) == 3
    return {
        'family': family, 'recipe_seed': recipe_seed,
        'floor_rows': tuple(sum(1 << x for x in range(16) if (x, y) in floor) for y in range(12)),
        'ice_rect': tuple(spec['ice_rects'][0]) if spec['ice_rects'] else (0, 0, 0, 0),
        'junction': tuple(spec['junction']), 'controls': controls,
    }


def layout_index(seed):
    return seed % LAYOUT_COUNT


def cpp_table():
    lines = ['// Generated by tools/frost_cave_layouts.py; geometry only, no raster assets.',
        '#pragma once', '', '#include "game/ExploreMapGenerator.h"', '',
        'namespace ExploreFrostLayouts {',
        'struct Layout {', '    uint16_t floorRows[12];',
        '    uint8_t iceRect[4];', '    ExploreMapGenerator::Point junction;',
        '    ExploreMapGenerator::Point controls[2][8];', '    uint8_t controlCounts[2];',
        '};', f'constexpr uint16_t COUNT = {LAYOUT_COUNT};',
        'constexpr Layout LAYOUTS[COUNT] = {']
    for i in range(LAYOUT_COUNT):
        spec = layout(i)
        fmt = lambda p: '{' + ', '.join(str(v) for v in p) + '}'
        controls = ['{' + ', '.join(fmt(p) for p in route) + '}' for route in spec['controls']]
        rows = '{' + ', '.join(f'0x{v:04x}' for v in spec['floor_rows']) + '}'
        lines.append('    {' + ', '.join((rows, fmt(spec['ice_rect']), fmt(spec['junction']),
            '{' + ', '.join(controls) + '}', fmt(tuple(map(len, spec['controls']))))) + '},')
    lines += ['};', '}  // namespace ExploreFrostLayouts', '']
    return '\n'.join(lines)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    target = Path(__file__).resolve().parents[1] / 'src/game/ExploreFrostLayouts.h'
    generated = cpp_table()
    if args.check:
        if target.read_text() != generated:
            raise SystemExit('Frost layout table is stale; rebuild tools/frost_cave_layouts.py')
    else:
        target.write_text(generated)
    print(f'{LAYOUT_COUNT} validated cave layouts; table {"checked" if args.check else "written"}')
