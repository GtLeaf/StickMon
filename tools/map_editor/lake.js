import { copy, flatten, prefabFor, grid } from "./model.js";

export const lakeDirections = [[0,-1],[1,0],[0,1],[-1,0],[-1,-1],[1,-1],[1,1],[-1,1]];
export function lakeMask(values, x, y, water, size = [16,12]) {
  const {index,inside}=grid({map:{size}});
  let mask = 0;
  lakeDirections.forEach(([dx,dy], bit) => {
    if (inside(x+dx,y+dy) && water.has(values[index(x+dx,y+dy)])) mask |= 1 << bit;
  });
  for (const [diagonal,a,b] of [[4,0,3],[5,0,1],[6,2,1],[7,2,3]])
    if (!(mask & (1 << a)) || !(mask & (1 << b))) mask &= ~(1 << diagonal);
  return mask;
}

const keys = (spec) => new Set([...Object.values(spec.byMask), ...spec.compatibleKeys]);

export function lakeSeed(project, x, y, spec) {
  const {width,height,index,inside}=grid(project);
  const values = flatten(project).layers[0], water = keys(spec);
  if ([[0,0], ...lakeDirections].some(([dx,dy]) => inside(x+dx,y+dy) && water.has(values[index(x+dx,y+dy)])))
    return [[x,y]];
  const left = Math.max(0,Math.min(width-3,x-1)), top = Math.max(0,Math.min(height-3,y-1));
  return Array.from({length:9}, (_,i) => [left+i%3,top+Math.floor(i/3)]);
}

export function retileLake(project, spec) {
  const {width,height,index,inside}=grid(project);
  const automatic = new Set(Object.values(spec.byMask)), water = keys(spec);
  const values = flatten(project).layers[0], ground = project.map.lakeGround || {};
  // Ordinary brushes can replace auto-water. Drop obsolete restoration records.
  for (const i of Object.keys(ground)) if (!automatic.has(project.map.layers[0][i])) delete ground[i];
  project.map.layers[0].forEach((key,i) => {
    if (automatic.has(key)) {
      if (!(i in ground)) ground[i] = spec.defaultGround;
      project.map.layers[0][i] = spec.byMask[lakeMask(values,i%width,Math.floor(i/width),water,project.map.size)];
    }
  });
  if (Object.keys(ground).length) project.map.lakeGround = ground;
  else delete project.map.lakeGround;
}

export function editLake(project, cells, erase, spec) {
  const {width,height,index,inside}=grid(project);
  // Preflight and edit a copy: a blocked seed must not leave half a lake behind.
  const next = copy(project), water = keys(spec), automatic = new Set(Object.values(spec.byMask));
  const {layers,owners} = flatten(next), targets = cells.filter(([x,y]) =>
    inside(x,y) && (!erase || water.has(layers[0][index(x,y)])));
  if (!targets.length) return;
  const convertible = new Set(next.map.objects.filter((o) =>
    prefabFor(next,o.prefab).cells.every((c) => c.layer === 0 && water.has(c.asset))).map((o) => o.id));
  for (const [x,y] of targets) {
    const owner = owners[0][index(x,y)];
    if (owner && !convertible.has(owner)) throw Error("这里有完整地面物件，请先移动或拆分后再铺湖");
  }
  const connected = new Set(), queue = targets.flatMap(([x,y]) =>
    [[x,y], ...lakeDirections.map(([dx,dy]) => [x+dx,y+dy])]);
  while (queue.length) {
    const [x,y] = queue.pop(), i = index(x,y);
    if (!inside(x,y) || connected.has(i) || !water.has(layers[0][i])) continue;
    const owner = owners[0][i];
    if (owner && !convertible.has(owner)) throw Error("相连水域属于混合物件，请先拆分后再自动铺湖");
    connected.add(i);
    for (const [dx,dy] of lakeDirections) queue.push([x+dx,y+dy]);
  }
  const removed = new Set([...connected].map((i) => owners[0][i]).filter(Boolean));
  next.map.objects = next.map.objects.filter((o) => !removed.has(o.id));
  const ground = next.map.lakeGround ||= {};
  function wet(i) {
    if (!(i in ground)) {
      const previous = next.map.layers[0][i];
      ground[i] = previous && !water.has(previous) ? previous : spec.defaultGround;
    }
    next.map.layers[0][i] = spec.byMask[0];
  }
  for (const i of connected) wet(i);
  for (const [x,y] of targets) {
    const i = index(x,y);
    if (erase) {
      if (automatic.has(next.map.layers[0][i])) {
        next.map.layers[0][i] = ground[i] || spec.defaultGround;
        delete ground[i];
      }
    } else wet(i);
  }
  retileLake(next,spec);
  project.map = next.map;
}
