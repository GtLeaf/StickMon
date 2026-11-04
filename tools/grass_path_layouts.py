"""Approved grass-path recipe: open meadows and occasional fenced woodland."""
from functools import lru_cache
import generate_runtime_tile_map as g

GRID={(x,y) for x in range(16) for y in range(12)}
FENCED_CHANCE=15
NAMES=('开阔花甸','林缘缓弯','疏林岔路')
NOTES=('舒展道路 · 两片草甸 · 三棵完整小树','绕过树群 · 草边花簇 · 保留空地','偏侧分岔 · 错落树群 · 清晰出口')
SHAPES=[
 ((1,2),(0,3),(0,4),(1,3)),
 ((1,3),(0,4),(1,4),(2,3)),
 ((0,2),(0,4),(1,4),(2,3)),
 ((1,3),(0,4),(0,3)),
 ((0,2),(0,3),(1,3)),
 ((1,2),(0,3),(0,2)),
]

def choose(rng,items): return items[rng.bounded(len(items))]
def dist(a,b): return min(abs(x-xx)+abs(y-yy) for x,y in a for xx,yy in b)
def paint(m,cells,tile,layer=0):
 for x,y in cells:m.layers[layer][y*16+x]=tile

def topology(seed,edge,style):
 rng=g.XorShift32(seed ^ 0x6A09E667)
 for attempt in range(1000):
  candidate=g.park_topology_candidate(rng,edge,1,False)
  if candidate is None:continue
  entry,junction,paths,coast,road=candidate
  shared=next((i for i,(a,b) in enumerate(zip(paths[0].points,paths[1].points)) if a!=b),min(map(lambda p:len(p.points),paths)))
  if shared<4 or len(road)>(58,66,64)[style]:continue
  if style==1:
   lateral={g.Edge.LEFT,g.Edge.RIGHT} if edge in (g.Edge.TOP,g.Edge.BOTTOM) else {g.Edge.TOP,g.Edge.BOTTOM}
   if {p.exit.edge for p in paths}!=lateral:continue
  if style==2 and 5<=junction[0]<=9 and 4<=junction[1]<=6:continue
  runs=[]
  for path in paths:
   segments=[];previous=None
   for a,b in zip(path.points,path.points[1:]):
    direction=(b[0]-a[0],b[1]-a[1])
    if direction==previous:segments[-1]+=1
    else:segments.append(1);previous=direction
   runs.append(segments)
  if any(len(run)>4 or any(length<3 for length in run[1:-1]) for run in runs):continue
  try:g.validate_topology(paths,junction)
  except ValueError:continue
  return entry,junction,paths,road
 raise ValueError(('no road',seed,edge,style))

def stamp_trees(m,rng,road,style,clear,woodland=None):
 occupied=set(woodland or ());anchors=[]
 for ordinal in range(2 if woodland else (3,4,4)[style]):
  tiles=g.PARK_CENTERED_TREE_TILES if ordinal==0 and style==2 else g.PARK_TREE_TILES
  candidates=[]
  for y in range(0,10):
   for x in range(0,17-len(tiles[0])):
    cells={(x+dx,y+dy) for dy,row in enumerate(tiles) for dx,_ in enumerate(row)}
    if cells&(road|occupied|clear):continue
    # Alternate a small group with another scene anchor, not a row of trees.
    distance=dist(cells,occupied) if occupied else 0
    if ordinal in (1,3) and (not 1<=distance<=3 or y==anchors[-1]['y']):continue
    if ordinal==2 and distance<4:continue
    margin=min(x,16-x-len(tiles[0]),y,9-y)
    score=12-abs(margin-1)*2-abs(dist(cells,road)-2)*2
    if ordinal in (1,3):score-=abs(distance-1)*3
    score+=rng.bounded(6)
    candidates.append((score,x,y,tiles,cells))
  if not candidates:return None
  best=max(item[0] for item in candidates)
  _,x,y,tiles,cells=choose(rng,[c for c in candidates if c[0]>=best-1])
  g.stamp_park_tree(m,occupied,x,y,tiles)
  anchors.append({'x':x,'y':y,'tiles':tiles})
 return occupied,anchors

@lru_cache(None)
def shapes(large):
 result=set()
 for rows in SHAPES[:4] if large else SHAPES[4:]:
  points={(x,y) for y,(left,width) in enumerate(rows) for x in range(left,left+width)}
  for mirror in (False,True):
   p={(-x if mirror else x,y) for x,y in points}
   for rotation in range(4):
    mx=min(x for x,y in p);my=min(y for x,y in p)
    result.add(tuple(sorted((x-mx,y-my) for x,y in p)))
    p={(-y,x) for x,y in p}
 return sorted(result)

def stamp_grass(m,rng,road,trees,clear):
 patches=[];all_grass=set()
 for ordinal in range(2):
  candidates=[]
  forbidden=road|trees|clear|g.expanded(all_grass)
  for shape in shapes(ordinal==0):
   w=1+max(x for x,y in shape);h=1+max(y for x,y in shape)
   for y in range(1,12-h):
    for x in range(1,16-w):
     cells={(x+dx,y+dy) for dx,dy in shape}
     if cells&forbidden:continue
     if ordinal and dist(cells,all_grass)<4:continue
     # Keep the larger patch by a tree group, the smaller one near another road edge.
     score=18-abs(dist(cells,trees)-1)*3-abs(dist(cells,road)-1)*2
     score+=rng.bounded(8)
     candidates.append((score,cells))
  if not candidates:return None
  best=max(score for score,cells in candidates)
  cells=choose(rng,[cells for score,cells in candidates if score>=best-2])
  paint(m,cells,g.DENSE_GRASS_TILE);all_grass|=cells;patches.append(cells)
 return all_grass,patches

def flowers(m,rng,grass,road,trees,clear):
 result=set()
 for patch in grass:
  candidates=g.flower_cluster_candidates(patch,road|trees|clear|set.union(*grass)|g.expanded(result),road)
  candidates=[(score,pts) for score,pts in candidates if len(pts)<=4 and all(0<x<15 and 0<y<11 for x,y in pts)]
  if not candidates:continue
  best=max(score for score,pts in candidates)
  cells=set(choose(rng,[pts for score,pts in candidates if score>=best-8]))
  paint(m,cells,g.FLOWER_TILE);result|=cells
 return result

def wants_fence(seed):
 return g.XorShift32(seed ^ 0xB7E15162).bounded(100)<FENCED_CHANCE

def stamp_small_woodland(m,seed,road,clear):
 rng=g.XorShift32(seed ^ 0x243F6A88)
 candidates=[]
 # Two trees across; complete crown/body/root sequence, with the original L fence.
 for top in range(0,8):
  for left in range(1,13):
   spec=(left,top,4,top+4)
   cells=g.forest_footprint(spec)
   if not cells<=GRID or cells&(road|clear):continue
   margin=min(left-1,12-left,top,7-top)
   score=12-margin*3-abs(dist(cells,road)-1)*2+rng.bounded(5)
   candidates.append((score,spec,cells))
 if not candidates:return None
 best=max(c[0] for c in candidates)
 _,spec,cells=choose(rng,[c for c in candidates if c[0]>=best-1])
 g.stamp_forest(m,spec)
 return spec,cells

def generate(seed,edge,style,fenced=None):
 if fenced is None:fenced=wants_fence(seed)
 for retry in range(64 if fenced else 32):
  feature_seed=(seed ^ (retry*0x9E3779B9))&0xffffffff
  try:entry,junction,paths,road=topology(feature_seed,edge,style)
  except ValueError:continue
  rng=g.XorShift32(feature_seed ^ 0xC2B2AE35)
  layers=[[choose(rng,g.GRASS_TILES) for _ in range(192)],[0]*192,[0]*192]
  m=g.RuntimeMap(seed,0,entry,junction,paths,layers,False,True,False,False,False)
  for x,y in road:m.layers[0][y*16+x]=g.road_tile(m,road,x,y)
  clear=g.expanded({entry.point,*(p.exit.point for p in paths),junction})&GRID
  woodland=set();fence_spec=None
  if fenced:
   result=stamp_small_woodland(m,feature_seed,road,clear)
   if result is None:continue
   fence_spec,woodland=result
  tree_result=stamp_trees(m,rng,road,style,clear,woodland)
  if tree_result is None:continue
  trees,anchors=tree_result
  grass_result=stamp_grass(m,rng,road,trees,clear)
  if grass_result is None:continue
  grass,patches=grass_result
  flower=flowers(m,rng,patches,road,trees,clear)
  if len(g.connected_components(flower))!=2:continue
  shrubs=set()
  candidates=[]
  forbidden=road|trees|grass|flower|clear
  for x,y in sorted(GRID-forbidden):
   for dx,dy in ((1,0),(0,1)):
    pair={(x,y),(x+dx,y+dy)}
    if not pair<=GRID or pair&forbidden:continue
    if dist(pair,grass)!=1 or dist(pair,trees)>3:continue
    candidates.append(pair)
  if candidates:
   shrubs=choose(rng,candidates);paint(m,shrubs,g.SHRUB_TILE,1)
  g.validate_topology(paths,junction);g.validate_border_road(entry,paths,road);g.validate_park_wide_road(paths,road)
  assert not road&(trees|grass|flower|shrubs)
  assert len(g.connected_components(grass))==2
  assert all(len(p.points)<=48 and len(p.points)==len(set(p.points)) for p in paths)
  assert all(abs(a[0]-b[0])+abs(a[1]-b[1])==1 for p in paths for a,b in zip(p.points,p.points[1:]))
  if fenced:
   fx,top,width,bottom=fence_spec
   assert bottom-top==4 and width==4
   assert not woodland&(road|clear)
   assert all(m.layers[0][bottom*16+x]==(818 if (x-fx)%2==0 else 819) for x in range(fx,fx+width))
   assert all(m.layers[0][(bottom-1)*16+x]==(810 if x==fx else 808 if (x-fx)%2==0 else 809) for x in range(fx,fx+width))
  return m,{'fencedWoodland':fenced,'fenceSpec':fence_spec,'style':style,'seed':seed,'retry':retry,'roadCells':len(road),'trees':anchors,'grassPatches':[sorted(p) for p in patches],'flowerCells':sorted(flower),'shrubCells':sorted(shrubs)}
 raise ValueError(('no scenery',seed,edge,style))

