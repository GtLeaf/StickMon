import {flatten, prefabFor, visibleFootprint, grid, dimensions} from './model.js';

// Occupied inner quadrants: top-left, top-right, bottom-left, bottom-right.
// Checking the touching corners avoids connecting two adjacent road borders.
const quarters = {537:8, 538:12, 539:4, 545:10, 546:15, 547:5,
  553:2, 554:3, 555:1, 558:14, 556:13, 542:11, 540:7};

export function roadSource(project) {
  return JSON.stringify([project.map.layers, project.map.objects, project.prefabs, dimensions(project)]);
}

function sourceMatches(project,source) {
  if (source === roadSource(project)) return true;
  // Previously only these two dimensions were supported; their areas differ.
  const [width,height]=dimensions(project);
  return ((width===16&&height===12)||(width===32&&height===24)) &&
    source === JSON.stringify([project.map.layers,project.map.objects,project.prefabs]);
}

export function currentRoadNetwork(project) {
  const network = project.map.roadNetwork;
  return network && sourceMatches(project,network.source) ? network : null;
}

export function roadRouteCurrent(project, path) {
  return path?.kind !== 'roadNetwork' || (sourceMatches(project,path.roadSource) &&
    JSON.stringify(path.roadEntry) === JSON.stringify(project.map.entry));
}

export function routeFromRoads(project) {
  const {width,height}=grid(project);
  const network = currentRoadNetwork(project);
  if (!network) throw Error('请先识别当前道路');
  const {nodes,edges} = network;
  if (!edges.length) throw Error('道路太短，无法生成行进路线');
  const links = nodes.map(() => []);
  edges.forEach(([a,b],i) => { links[a].push([b,i]); links[b].push([a,i]); });
  const entry = project.map.entry, target = entry.point.map(v => v+.5);
  if (entry.edge === 'bottom') target[1]=height;
  if (entry.edge === 'top') target[1]=0;
  if (entry.edge === 'left') target[0]=0;
  if (entry.edge === 'right') target[0]=width;
  const distance = ([x,y]) => Math.abs(x-target[0])+Math.abs(y-target[1]);
  const start = nodes.reduce((best,p,i) => distance(p)<distance(nodes[best]) ? i : best,0);
  // Visit each undirected edge and return along it. An explicit stack avoids
  // recursion limits on dense road networks and preserves every branch/loop.
  const seen = new Set(), stack=[start], walk=[start];
  while (stack.length) {
    const at=stack.at(-1), next=links[at].find(([,edge])=>!seen.has(edge));
    if (next) { seen.add(next[1]); stack.push(next[0]); walk.push(next[0]); }
    else { stack.pop(); if(stack.length)walk.push(stack.at(-1)); }
  }
  if (seen.size!==edges.length || new Set(walk).size!==nodes.length)
    throw Error('道路不连通，请先连接各段道路，再保存为一条路线');
  return {kind:'roadNetwork',roadSource:network.source,roadEntry:structuredClone(entry),points:walk.map(i=>[...nodes[i]]),
    exit:structuredClone(entry),cellCentered:false,fallsToNextLevel:false,nextValleyNode:255,
    nextValleyEntrance:0,terminal:false,interiorExit:false};
}

export function storeRoadRoute(project) {
  const route=routeFromRoads(project);
  const existing=project.map.paths.findIndex(p=>p.kind==='roadNetwork');
  if(existing<0 && project.map.paths.length>=3)throw Error('已有三条路线，请先删除一条再保存');
  const index=existing<0?project.map.paths.length:existing;
  project.map.paths[index]=route;
  return index;
}

// Double-width roads use the shared boundaries between their four tiles as
// centerline nodes. Keep cycles and branches; do not invent a walking order.
export function recognizeRoads(project, catalog) {
  const {width,height}=grid(project);
  const {layers, owners} = flatten(project);
  const road = layers[0].map(key => !!catalog.assets[key]?.road);
  const open = [...road];
  for (const object of project.map.objects) {
    const prefab = prefabFor(project, object.prefab);
    for (const [dx, dy] of visibleFootprint(object, prefab, owners, project.map.size)) {
      const x = object.x + dx, y = object.y + dy;
      if (x >= 0 && x < width && y >= 0 && y < height) open[y * width + x] = false;
    }
  }
  for (let z = 1; z < 3; z++) layers[z].forEach((key, i) => {
    if (!key || owners[z][i]) return;
    const asset = catalog.assets[key];
    if (!asset || !(asset.bridge || [391,4773,4747].includes(asset.runtimeId) ||
        (asset.behind?.[z] && asset.depth?.[z] !== 1))) open[i] = false;
  });
  const nodes = [], edges = [], lookup = new Map(), covered = new Set();
  const addNode = (x, y) => { const i = nodes.length; nodes.push([x,y]); lookup.set(`${x},${y}`,i); return i; };
  for (let y = 0; y < height-1; y++) for (let x = 0; x < width-1; x++) {
    const cells = [y*width+x, y*width+x+1, (y+1)*width+x, (y+1)*width+x+1];
    if (!cells.every((i,j) => open[i] && ((quarters[catalog.assets[layers[0][i]].runtimeId] ?? 15) & [8,4,2,1][j]))) continue;
    addNode(x+1, y+1); cells.forEach(i => covered.add(i));
  }
  const centers = nodes.length;
  for (let i = 0; i < centers; i++) {
    const [x,y] = nodes[i];
    for (const [dx,dy] of [[1,0],[0,1]]) {
      const j = lookup.get(`${x+dx},${y+dy}`);
      if (j !== undefined) edges.push([i,j]);
    }
    // Extend roads to map edges without moving the authored entrance marker.
    for (const [match,bx,by] of [[x===1,0,y],[x===width-1,width,y],[y===1,x,0],[y===height-1,x,height]])
      if (match) edges.push([i,addNode(bx,by)]);
  }
  if (!centers) throw Error(road.some(Boolean) ? '未识别到连续双格道路，请检查路宽或遮挡' : '地图上还没有道路');
  const links = nodes.map(() => []);
  edges.forEach(([a,b]) => { links[a].push(b); links[b].push(a); });
  let components = 0;
  const seen = new Set();
  for (let i = 0; i < nodes.length; i++) if (!seen.has(i)) {
    components++; const queue = [i]; seen.add(i);
    while (queue.length) for (const j of links[queue.pop()]) if (!seen.has(j)) { seen.add(j); queue.push(j); }
  }
  return {version:1, source:roadSource(project), nodes, edges, components,
    junctions:links.filter(v => v.length > 2).length,
    uncovered:road.map((v,i) => v && !covered.has(i) ? [i%width,Math.floor(i/width)] : null).filter(Boolean)};
}
