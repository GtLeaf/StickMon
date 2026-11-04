export const copy = (value) => structuredClone(value);
export const uid = (prefix) => `${prefix}-${crypto.randomUUID()}`;
export const dimensions = (project) => project.map.size || [16,12];
export const grid = (project) => {
  const [width,height] = dimensions(project);
  return {width,height,index:(x,y)=>y*width+x,inside:(x,y)=>x>=0&&y>=0&&x<width&&y<height};
};
export const index = (x, y, width = 16) => y * width + x;
export const inside = (x, y) => x >= 0 && y >= 0 && x < 16 && y < 12;
export const prefabFor = (project, id) =>
  project.prefabs.find((p) => p.id === id);
export const libraryCategories = ['道路地面', '湖泊', '自然景物', '集市', '围栏设施', '自定义组合'];
const categoryAliases = {'树木':'自然景物', '草花':'自然景物', '岩石':'自然景物',
  '交通':'集市', '绿宝石集市':'集市', '围栏':'围栏设施', '洞穴':'围栏设施',
  '导入物件':'自定义组合', '自定义':'自定义组合'};
export function terrainBrush(pref) {
  const c = pref?.cells?.[0];
  return pref?.brushAsset && pref.width === 1 && pref.height === 1 &&
    pref.cells.length === 1 && c.x === 0 && c.y === 0 && c.layer === 0 &&
    c.asset === pref.brushAsset && !pref.footprint?.length ? c.asset : null;
}
export function libraryPrefabs(project, builtins) {
  const hidden = new Set(builtins.filter((p) => p.hidden).map((p) => p.id));
  return [...project.prefabs, ...builtins.filter((p) => !prefabFor(project, p.id))]
    .filter((p) => !p.hidden && !hidden.has(p.id))
    .map((p) => ({ ...p, category: categoryAliases[p.category] || p.category || '自定义组合' }));
}
export function sampleCells(catalog, source, rect, layer) {
  const cells = [];
  for (let y=0;y<rect.h;y++) for (let x=0;x<rect.w;x++) {
    const asset = `${source}:${384+(rect.y+y)*8+rect.x+x}`;
    if (catalog.assets[asset] && !catalog.assets[asset].empty) cells.push({x,y,asset});
  }
  // Transparent artwork must overlay the ground instead of replacing it.
  const target = layer === 0 && cells.some(c => catalog.assets[c.asset].hasTransparency) ? 1 : layer;
  return cells.map(c => ({...c,layer:catalog.assets[c.asset].requiredLayer ?? target}));
}
export function flatten(project) {
  const {width,height,index,inside}=grid(project);
  const layers = copy(project.map.layers),
    owners = Array.from({ length: 3 }, () => Array(width*height).fill(null));
  for (const object of project.map.objects) {
    const prefab = prefabFor(project, object.prefab);
    for (const cell of prefab.cells) {
      const x = object.x + cell.x,
        y = object.y + cell.y;
      if (inside(x, y)) {
        layers[cell.layer][index(x, y)] = cell.asset;
        owners[cell.layer][index(x, y)] = object.id;
      }
    }
  }
  return { layers, owners };
}
// Select complete, visible objects; touching an edge never splits an assembly.
export function objectsInRect(project, rect, visible, locked) {
  const {index,inside} = grid(project), {owners} = flatten(project);
  return project.map.objects.filter(object => {
    const pref = prefabFor(project, object.prefab);
    return object.x >= rect.x && object.y >= rect.y &&
      object.x + pref.width <= rect.x + rect.w && object.y + pref.height <= rect.y + rect.h &&
      !pref.cells.some(c => locked[c.layer]) && pref.cells.some(c => {
        const x = object.x + c.x, y = object.y + c.y;
        return visible[c.layer] && inside(x,y) && owners[c.layer][index(x,y)] === object.id;
      });
  }).map(o => o.id);
}
export function moveObjects(project, positions, dx, dy) {
  const [width,height] = dimensions(project);
  for (const pos of positions) {
    const object = project.map.objects.find(o => o.id === pos.id), pref = prefabFor(project,object.prefab);
    dx = Math.max(-pos.x, Math.min(width - pref.width - pos.x, dx));
    dy = Math.max(-pos.y, Math.min(height - pref.height - pos.y, dy));
  }
  for (const pos of positions) {
    const object = project.map.objects.find(o => o.id === pos.id);
    object.x = pos.x + dx; object.y = pos.y + dy;
  }
  return {dx,dy};
}
export function bringToFront(project, ids) {
  const selected = new Set(ids);
  project.map.objects = [...project.map.objects.filter(o=>!selected.has(o.id)),
    ...project.map.objects.filter(o=>selected.has(o.id))];
}
export function visibleFootprint(object, prefab, owners, size = [16,12]) {
  const {index,inside}=grid({map:{size}});
  if (!prefab.cells.some(c=>inside(object.x+c.x,object.y+c.y)&&
      owners[c.layer][index(object.x+c.x,object.y+c.y)]===object.id)) return [];
  return prefab.footprint.filter(([dx,dy])=>{
    const x=object.x+dx,y=object.y+dy;if(!inside(x,y))return false;
    const parts=prefab.cells.filter(c=>c.x===dx&&c.y===dy);
    if(parts.length)return parts.some(c=>owners[c.layer][index(x,y)]===object.id);
    return !prefab.cells.some(c=>owners[c.layer][index(x,y)]&&owners[c.layer][index(x,y)]!==object.id);
  });
}
export function worldPoints(path) {
  if (path.kind === 'roadNetwork') return path.points.map(([x,y])=>[x*26,y*26]);
  return path.points.map(([x, y], i, points) => {
    let xx = x * 26 + 13,
      yy = y * 26 + 13;
    if (!path.cellCentered && points.length > 1) {
      const near = points
        .slice(Math.max(0, i - 1), i)
        .concat(points.slice(i + 1, i + 2));
      if (near.some(([a]) => a === x)) xx += 13;
      if (near.some(([, b]) => b === y)) yy += 13;
    }
    return [xx, yy];
  });
}
export function addLine(points, target, limit = 48) {
  const out = copy(points);
  if (!out.length) return [target];
  let [x, y] = out.at(-1);
  while (x !== target[0] || y !== target[1]) {
    if (x !== target[0]) x += Math.sign(target[0] - x);
    else y += Math.sign(target[1] - y);
    if (out.length >= limit) throw Error(`每条路线最多 ${limit} 个点`);
    out.push([x, y]);
  }
  return out;
}
export function syncJunction(project) {
  const paths = project.map.paths.filter((p) => p.kind !== 'roadNetwork' && p.points.length);
  if (!paths.length) return;
  let shared = paths[0].points[0];
  for (let i = 0; i < Math.min(...paths.map((p) => p.points.length)); i++) {
    if (
      paths.every(
        (p) =>
          p.points[i][0] === paths[0].points[i][0] &&
          p.points[i][1] === paths[0].points[i][1],
      )
    )
      shared = paths[0].points[i];
    else break;
  }
  project.map.junction = copy(paths.length === 1 ? paths[0].points[0] : shared);
}
export function flood(layer, start, asset, blocked = new Set(), size = [16,12]) {
  const {width,index,inside}=grid({map:{size}});
  const from = layer[start];
  if (from === asset) return;
  const queue = [start],
    seen = new Set();
  while (queue.length) {
    const i = queue.pop();
    if (seen.has(i) || blocked.has(i) || layer[i] !== from) continue;
    seen.add(i);
    layer[i] = asset;
    const x = i % width,
      y = Math.floor(i / width);
    for (const [dx, dy] of [
      [1, 0],
      [-1, 0],
      [0, 1],
      [0, -1],
    ])
      if (inside(x + dx, y + dy)) queue.push(index(x + dx, y + dy));
  }
}
export class History {
  constructor(project) {
    this.project = copy(project);
    this.undoStack = [];
    this.redoStack = [];
    this.saved = JSON.stringify(project);
    this.before = null;
  }
  begin() {
    if (!this.before) this.before = copy(this.project);
  }
  finish() {
    if (!this.before) return false;
    const changed =
      JSON.stringify(this.before) !== JSON.stringify(this.project);
    if (changed) {
      this.undoStack.push(this.before);
      if (this.undoStack.length > 100) this.undoStack.shift();
      this.redoStack = [];
    }
    this.before = null;
    return changed;
  }
  cancel() {
    if (this.before) this.project = this.before;
    this.before = null;
  }
  change(fn) {
    this.begin();
    try {
      fn(this.project);
      return this.finish();
    } catch (error) {
      this.cancel();
      throw error;
    }
  }
  undo() {
    if (this.before) this.cancel();
    if (!this.undoStack.length) return false;
    this.redoStack.push(copy(this.project));
    this.project = this.undoStack.pop();
    return true;
  }
  redo() {
    if (!this.redoStack.length) return false;
    this.undoStack.push(copy(this.project));
    this.project = this.redoStack.pop();
    return true;
  }
  get dirty() {
    return JSON.stringify(this.project) !== this.saved;
  }
  markSaved() {
    this.saved = JSON.stringify(this.project);
  }
}
