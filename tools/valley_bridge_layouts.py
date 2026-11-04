"""Seeded A/B/C bridge-cave design data; not wired into runtime generation.

Tiles 6000+ and 10000+ are design aliases for original Caves and Outside
tiles. Water aliases describe original Sea quarters, never painted pixels.
No graphics, local preview scripts, or random global state are needed here.
"""

import hashlib
import json


WIDTH, HEIGHT = 16, 12
RECIPES = (
    dict(name="东岸短桥", river=9, width=3, bridge=5, body=1,
         entrance=6, b_stair=7, c_stair=10, c_exit=10,
         b_river=2, b_width=3, c_river=3, c_width=3),
    dict(name="中峡高瀑", river=7, width=3, bridge=4, body=2,
         entrance=4, b_stair=10, c_stair=9, c_exit=12,
         b_river=2, b_width=4, c_river=3, c_width=3),
    dict(name="东峡宽桥", river=9, width=4, bridge=5, body=1,
         entrance=5, b_stair=11, c_stair=9, c_exit=11,
         b_river=3, b_width=4, c_river=3, c_width=3),
    dict(name="西峡绕岸", river=6, width=3, bridge=4, body=1,
         entrance=3, b_stair=8, c_stair=11, c_exit=12,
         b_river=2, b_width=3, c_river=3, c_width=4),
)
DIRECTIONS = ((0, -1), (1, 0), (0, 1), (-1, 0))


class Random:
    def __init__(self, seed):
        self.state = seed & 0xFFFFFFFF or 0xA341316C

    def next(self):
        x = self.state
        x ^= x << 13 & 0xFFFFFFFF
        x ^= x >> 17
        x ^= x << 5 & 0xFFFFFFFF
        self.state = x & 0xFFFFFFFF
        return self.state

    def choose(self, values):
        return values[self.next() % len(values)]

    def shuffle(self, values):
        for i in range(len(values) - 1, 0, -1):
            j = self.next() % (i + 1)
            values[i], values[j] = values[j], values[i]


def rect(left, top, right, bottom):
    return {(x, y) for y in range(top, bottom + 1)
            for x in range(left, right + 1)}


def route(controls):
    points = [controls[0]]
    for x, y in controls[1:]:
        px, py = points[-1]
        assert x == px or y == py
        while (px, py) != (x, y):
            px += (x > px) - (x < px)
            py += (y > py) - (y < py)
            points.append((px, py))
    return points


def flood(cells, start):
    if start not in cells:
        return set()
    seen, queue = {start}, [start]
    while queue:
        x, y = queue.pop()
        for dx, dy in DIRECTIONS:
            p = x + dx, y + dy
            if p in cells and p not in seen:
                seen.add(p)
                queue.append(p)
    return seen


def water_tile(context, x, y, quarters):
    n, e, s, w = [(x + dx, y + dy) in context for dx, dy in DIRECTIONS]
    nw, ne, sw, se = [(x + dx, y + dy) in context
                     for dx, dy in ((-1, -1), (1, -1), (-1, 1), (1, 1))]
    tl = ((26 if nw else 4, False) if n and w else (24, False) if n
          else (14, False) if w else (12, False))
    tr = ((27 if ne else 5, False) if n and e else (29, False) if n
          else (15, False) if e else (17, False))
    bl = ((32, False) if s and w and sw else (4, True) if s and w
          else (30, False) if s else (14, True) if w else (12, True))
    br = ((33, False) if s and e and se else (5, True) if s and e
          else (35, False) if s else (15, True) if e else (17, True))
    flags = [n, e, s, w, nw, ne, sw, se]
    tile = 8000 + sum(int(v) << i for i, v in enumerate(flags))
    quarters[tile] = [tl, tr, bl, br]
    return tile


class Room:
    def __init__(self, key):
        self.key = key
        self.layers = [[6610] * 192, [0] * 192, [0] * 192]
        self.walls, self.cliffs, self.water, self.stairs = set(), set(), set(), set()
        self.decor, self.portals, self.paths, self.falls = set(), [], [], []
        self.quarters, self.modules, self.patches = {}, [], []
        self.bridge = set()
        for x in range(16):
            self.set(x, 0, 513, 0)
            self.set(x, 0, 521)
            self.set(x, 1, 529)
            self.set(x, 11, 505)
            self.set(x, 11, 513, 0)
            self.walls.update(((x, 0), (x, 1), (x, 11)))
        for y in range(1, 12):
            self.set(0, y, 514)
            self.set(15, y, 512)
            self.walls.update(((0, y), (15, y)))
        for x, y, tile in ((0, 1, 510), (15, 1, 511),
                           (0, 11, 518), (15, 11, 519)):
            self.set(x, y, tile)
        self.set(0, 0, 529)
        self.set(15, 0, 529)

    def set(self, x, y, tile, layer=1, raw=False):
        assert 0 <= x < WIDTH and 0 <= y < HEIGHT
        self.layers[layer][y * WIDTH + x] = tile if raw else 6000 + tile

    def face(self, left, right, crest, body, stair=None):
        for y in range(crest, crest + body + 1):
            for x in range(left, right + 1):
                if x == stair:
                    self.set(x, y, 539)
                    self.stairs.add((x, y))
                else:
                    self.set(x, y, 521 if y == crest else 529)
                    self.cliffs.add((x, y))

    def waterfall(self, left, width, crest, body, rng):
        right, splash = left + width - 1, crest + body + 1
        upper = rect(left, 2 if self.key == 'A' else 3, right, crest - 1)
        pool = rect(left - 1, splash, right + 1, 10)
        # Only the far bottom corners vary. The full splash and receiver rows
        # always remain open and at least one tile wider than the waterfall.
        if splash + 1 < 10:
            for x in (left - 1, right + 1):
                if rng.next() % 2:
                    pool.discard((x, 10))
        for x, y in sorted(pool):
            self.set(x, y, water_tile(pool | rect(left, splash - 1, right, splash),
                                      x, y, self.quarters), raw=True)
        for x, y in sorted(upper):
            if y == min(p[1] for p in upper):
                tile = water_tile(upper | rect(left, crest, right, crest),
                                  x, y, self.quarters)
                self.set(x, y, tile, raw=True)
            else:
                self.set(x, y, 96, raw=True)
                if x in (left, right):
                    self.set(x, y, 11136 if x == left else 11138, 2, True)
        rows = [(283, 273, 285), (322, 308, 324)]
        if body > 1:
            rows += [(304, 288, 312)] * (body - 2) + [(328, 316, 326)]
        rows += [(336, 336, 336)]
        falling = rect(left, crest, right, splash)
        for dy, row in enumerate(rows):
            for x in range(left, right + 1):
                tile = row[0 if x == left else 2 if x == right else 1]
                self.set(x, crest + dy, tile, raw=True)
        self.set(left, crest, 11144, 2, True)
        self.set(right, crest, 11146, 2, True)
        self.water |= upper | falling | pool
        self.cliffs -= falling
        self.falls.append(dict(left=left, width=width, crest=crest,
                               body_height=body, splash=splash,
                               pool=sorted(pool)))

    def portal(self, point, edge):
        x, y = point
        assert point in self.walls
        module = []
        if edge == 'BOTTOM':
            for xx, tile in ((x - 1, 507), (x, 508), (x + 1, 509)):
                self.set(xx, y, tile)
                module.append((xx, y, tile, 1))
            self.set(x, y, 516, 2)
            module.append((x, y, 516, 2))
            dx, dy = 0, -1
        else:
            tile = 517 if edge == 'LEFT' else 515
            self.set(x, y, tile)
            module.append((x, y, tile, 1))
            dx, dy = (1, 0) if edge == 'LEFT' else (-1, 0)
        approach = [(x + k * dx, y + k * dy) for k in (1, 2)]
        assert not set(approach) & (self.walls | self.cliffs | self.water | self.stairs)
        self.portals.append(dict(edge=edge, visual_anchor=point,
                                 approach=approach, module=module))

    def path(self, level, name, controls):
        self.paths.append(dict(level=level, name=name, cells=route(controls)))

    def protected(self):
        cells = {p for path in self.paths for p in path['cells']}
        cells |= {p for portal in self.portals for p in portal['approach']}
        cells |= self.bridge | self.stairs
        # Keep rock contours away from feet and stair/bridge landing zones.
        return cells | {(x + dx, y + dy) for x, y in cells for dx, dy in DIRECTIONS}

    def decorate(self, seed):
        rng = Random(seed)
        protected = self.protected()
        free = rect(1, 2, 14, 10) - self.walls - self.cliffs - self.water
        free -= self.stairs | protected
        crater_candidates = [(x, y) for y in range(2, 10) for x in range(1, 14)
                             if rect(x, y, x + 1, y + 1) <= free]
        if crater_candidates and (self.key == 'A' or rng.next() % 3 == 0):
            x, y = rng.choose(crater_candidates)
            for dy, row in enumerate(((542, 543), (550, 551))):
                for dx, tile in enumerate(row):
                    self.set(x + dx, y + dy, tile, 2)
            footprint = rect(x, y, x + 1, y + 1)
            self.decor |= footprint
            free -= footprint
            self.modules.append(dict(kind='crater', origin=[x, y],
                                     tiles=[[542, 543], [550, 551]]))
        anchors = sorted(p for p in free if any(
            (p[0] + dx, p[1] + dy) in self.walls | self.cliffs | self.water
            for dx, dy in DIRECTIONS))
        rng.shuffle(anchors)
        centers = []
        for x, y in anchors:
            if len(centers) >= 3 or (x, y) not in free:
                continue
            if any(abs(x - xx) + abs(y - yy) < 4 for xx, yy in centers):
                continue
            centers.append((x, y))
            nearby = sorted(rect(x - 1, y - 1, x + 1, y + 1) & free)
            rng.shuffle(nearby)
            group = [(x, y)] + [p for p in nearby if p != (x, y)][:1 + rng.next() % 2]
            for xx, yy in group:
                self.set(xx, yy, rng.choose((557, 558, 559, 566)), 2)
            self.decor.update(group)
            free -= set(group)
            self.modules.append(dict(kind='rock_group', cells=group))
        # Patches may lie on paths: they are flat original ground, not obstacles.
        patch_free = rect(2, 2, 13, 10) - self.walls - self.cliffs - self.water
        patch_free -= self.stairs | self.decor | self.bridge
        for _ in range(1 if self.key == 'A' else 2):
            candidates = [(x, y, w) for w in (2, 3) for y in range(2, 10)
                          for x in range(2, 14 - w + 1)
                          if rect(x, y, x + w - 1, y + 1) <= patch_free]
            if not candidates:
                break
            x, y, width = rng.choose(candidates)
            for dy, row in enumerate(((657, 658, 659), (673, 674, 675))):
                for dx in range(width):
                    self.set(x + dx, y + dy, row[0 if dx == 0 else 2 if dx == width - 1 else 1], 0)
            footprint = rect(x, y, x + width - 1, y + 1)
            patch_free -= footprint | {(xx + dx, yy + dy) for xx, yy in footprint for dx, dy in DIRECTIONS}
            self.patches.append(dict(origin=[x, y], width=width, height=2))

    def data(self):
        return dict(layers=self.layers, walls=sorted(self.walls),
                    cliffs=sorted(self.cliffs), water=sorted(self.water),
                    stairs=sorted(self.stairs), decor=sorted(self.decor),
                    portals=self.portals, paths=self.paths, falls=self.falls,
                    bridge=sorted(self.bridge), water_quarters=self.quarters,
                    modules=self.modules, patches=self.patches)


def family_index(seed):
    seed &= 0xFFFFFFFF
    return (seed ^ (seed >> 16)) % len(RECIPES)


def generate(seed, family=None):
    seed &= 0xFFFFFFFF
    index = family_index(seed) if family is None else family
    assert 0 <= index < len(RECIPES)
    spec = RECIPES[index]
    high, low = spec['bridge'], 9
    crest, body = high + 1, spec['body']
    bottom = crest + body + 1
    rooms = {key: Room(key) for key in 'ABC'}
    a, b, c = [rooms[key] for key in 'ABC']
    left, right = spec['river'], spec['river'] + spec['width'] - 1
    a.face(1, left - 1, crest, body)
    a.face(right + 1, 14, crest, body)
    for y in range(2, high):
        for x, tile in ((left - 1, 590), (right + 1, 588)):
            a.set(x, y, tile)
            a.cliffs.add((x, y))
    a.waterfall(left, spec['width'], crest, body, Random(seed ^ 0xA001))
    a.bridge = rect(left - 1, high, right + 1, high)
    entrance = spec['entrance']
    for point, edge in (((entrance, 11), 'BOTTOM'), ((0, low), 'LEFT'),
                        ((0, high), 'LEFT'), ((15, high), 'RIGHT')):
        a.portal(point, edge)
    a.path(0, 'entrance-to-B', [(entrance, 11), (entrance, low), (0, low)])
    a.path(1, 'B-to-C-over-bridge', [(0, high), (15, high)])

    for room, prefix, salt in ((b, 'b', 0xB001), (c, 'c', 0xC001)):
        stair = spec[prefix + '_stair']
        room.face(1, 14, crest, body, stair)
        room.waterfall(spec[prefix + '_river'], spec[prefix + '_width'],
                       crest, body, Random(seed ^ salt))
    b_stair, c_stair = spec['b_stair'], spec['c_stair']
    b.portal((15, low), 'RIGHT')
    b.portal((15, high), 'RIGHT')
    b.path(0, 'lower-approach', [(15, low), (b_stair, low), (b_stair, bottom)])
    b.path('stairs', 'ascend', [(b_stair, bottom), (b_stair, high)])
    if (seed >> 2) & 1:
        b.path(1, 'upper-return', [(b_stair, high), (12, high),
                                  (12, 2), (13, 2), (13, high), (15, high)])
    else:
        b.path(1, 'upper-return', [(b_stair, high), (15, high)])
    branch_x = b_stair if b_stair >= 9 else 9
    branch_end = 6 + ((seed >> 8) % 3)
    b.path(1, 'upper-branch', [(branch_x, high), (branch_x, 2), (branch_end, 2)])

    c.portal((0, high), 'LEFT')
    c.portal((spec['c_exit'], 11), 'BOTTOM')
    c.path(1, 'upper-approach', [(0, high), (1, high), (1, 2),
                               (c_stair, 2), (c_stair, high)])
    c.path('stairs', 'descend', [(c_stair, high), (c_stair, bottom)])
    c.path(0, 'lower-exit', [(c_stair, bottom), (c_stair, low),
                            (spec['c_exit'], low), (spec['c_exit'], 11)])
    branch_x = c_stair - 1 - ((seed >> 5) & 1)
    branch_end_y = 10 if (seed >> 6) & 1 else low
    c.path(0, 'lower-branch', [(c_stair, low), (branch_x, low), (branch_x, branch_end_y)])
    for key, room in rooms.items():
        room.decorate(seed ^ (ord(key) * 0x9E3779B9))
    result = dict(schema=1, scope='design-only; not runtime integrated',
                  seed=seed, family=index, name=spec['name'], recipe=dict(spec),
                  links=[dict(a=['A', 0, 0, low], b=['B', 0, 15, low]),
                         dict(a=['B', 1, 15, high], b=['A', 1, 0, high]),
                         dict(a=['A', 1, 15, high], b=['C', 1, 0, high])],
                  rooms={key: room.data() for key, room in rooms.items()})
    result['checks'] = validate(result)
    return result


def validate(data):
    spec = data['recipe']
    totals = dict(route_samples=0, portals=0, waterfalls=0)
    for key, room in data['rooms'].items():
        cells = lambda name: set(map(tuple, room[name]))
        ports = {tuple(p['visual_anchor']) for p in room['portals']}
        walls, water, cliffs, stairs, decor = [cells(k) for k in
                                               ('walls', 'water', 'cliffs', 'stairs', 'decor')]
        blocked = (walls - ports) | water | cliffs | decor
        bridge = cells('bridge')
        assert not decor & (walls | water | cliffs | stairs)
        for path in room['paths']:
            points = list(map(tuple, path['cells']))
            allowed = bridge if path['level'] == 1 else set()
            obstacles = blocked - allowed
            assert len(points) == len(set(points)), (key, 'route revisit', path['name'])
            assert not set(points) & obstacles, (key, 'blocked route', path['name'])
            for start, end in zip(points, points[1:]):
                assert abs(start[0] - end[0]) + abs(start[1] - end[1]) == 1
                if start in stairs or end in stairs:
                    assert start[0] == end[0], (key, 'sideways stair')
                for k in range(17):
                    for dx, dy in ((0, 0), (-6, 9), (0, 9), (6, 9)):
                        x = int(((start[0] + .5 + (end[0] - start[0]) * k / 16) * 26 + dx) // 26)
                        y = int(((start[1] + .5 + (end[1] - start[1]) * k / 16) * 26 + dy) // 26)
                        assert 0 <= x < WIDTH and 0 <= y < HEIGHT and (x, y) not in obstacles, (key, 'foot blocked', (x, y))
                        totals['route_samples'] += 1
        # A branch must meet the actual authored route on the same level,
        # rather than merely sit on reachable bare floor beside that route.
        level_routes = {level: {tuple(p) for path in room['paths']
                                if path['level'] == level for p in path['cells']}
                        for level in (0, 1)}
        for points in level_routes.values():
            assert points and len(flood(points, min(points))) == len(points), (key, 'disconnected route branch')
        for path in room['paths']:
            if path['level'] == 'stairs':
                start, end = tuple(path['cells'][0]), tuple(path['cells'][-1])
                assert ((start in level_routes[0] and end in level_routes[1]) or
                        (start in level_routes[1] and end in level_routes[0]))
        for portal in room['portals']:
            assert not set(map(tuple, portal['approach'])) & (blocked | stairs)
            totals['portals'] += 1
        dry = rect(0, 0, 15, 11) - blocked
        if key == 'A':
            seen = flood(dry, (spec['entrance'], 11))
            assert (0, 9) in seen
            assert not seen & bridge and (0, spec['bridge']) not in seen
            for y in range(2, spec['bridge']):
                for x, source in ((spec['river'] - 1, 590),
                                  (spec['river'] + spec['width'], 588)):
                    i = y * WIDTH + x
                    assert room['layers'][1][i] == 6000 + source
                    assert room['layers'][0][i] == 6610
        else:
            low = (15, 9) if key == 'B' else (spec['c_exit'], 11)
            high = (15, spec['bridge']) if key == 'B' else (0, spec['bridge'])
            assert high in flood(dry, low), (key, 'stairs disconnected')
            assert high not in flood(dry - stairs, low), (key, 'stairs bypass')
        for fall in room['falls']:
            left, splash = fall['left'], fall['splash']
            right = left + fall['width'] - 1
            pool = set(map(tuple, fall['pool']))
            assert rect(left - 1, splash, right + 1, splash) <= pool
            assert rect(left, splash + 1, right, splash + 1) <= pool
            for x in range(left, right + 1):
                tile = room['layers'][1][(splash + 1) * WIDTH + x]
                quarters = room['water_quarters'].get(tile, room['water_quarters'].get(str(tile)))
                assert quarters[0][0] == 26 and quarters[1][0] == 27
            assert len(flood(water, (left, splash))) == len(water)
            totals['waterfalls'] += 1
        for module in room['modules']:
            if module['kind'] == 'crater':
                x, y = module['origin']
                for dy, row in enumerate(module['tiles']):
                    for dx, source in enumerate(row):
                        assert room['layers'][2][(y + dy) * WIDTH + x + dx] == 6000 + source
                        assert room['layers'][0][(y + dy) * WIDTH + x + dx] == 6610
    for link in data['links']:
        a, level_a, ax, ay = link['a']
        b, level_b, bx, by = link['b']
        assert level_a == level_b and ay == by and {ax, bx} == {0, 15}
        for key, p in ((a, (ax, ay)), (b, (bx, by))):
            assert p in {tuple(port['visual_anchor']) for port in data['rooms'][key]['portals']}
    totals.update(A_lower_cannot_reach_bridge=True, B_ascent_required=True,
                  C_descent_required=True, linked_portals_match=True,
                  waterfall_receivers_open=True, result='pass')
    return totals


def fingerprint(data, structure_only=False):
    if structure_only:
        data = dict(links=data['links'], rooms={
            key: {k: room[k] for k in ('paths', 'water', 'stairs', 'bridge', 'cliffs')}
            for key, room in data['rooms'].items()})
    else:
        data = dict(links=data['links'], rooms=data['rooms'])
    return hashlib.sha256(json.dumps(data, sort_keys=True).encode()).hexdigest()
