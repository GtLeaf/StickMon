import { copy, prefabFor, bringToFront, dimensions } from "./model.js";

// Compare geometry, not labels. Never replace an edited user's definition.
export function geometry(pref) {
  const cells = pref.cells.map(({x,y,layer,asset}) => [x,y,layer,asset]);
  return JSON.stringify([pref.width,pref.height,pref.anchor,pref.anchorMode || "shared",
    pref.footprint.map((p) => [...p]).sort(),cells.sort()]);
}

export function rotationInfo(pref, catalog) {
  if (!pref) return null;
  for (const [family,ids] of Object.entries(catalog.rotations)) {
    const turn = ids.indexOf(pref.id);
    if (turn < 0) continue;
    const original = catalog.prefabs.find((p) => p.id === pref.id);
    if (!original || geometry(pref) !== geometry(original)) return null;
    return {family,ids,turn,angle:turn*90};
  }
  return null;
}

export function rotatedPrefab(project, pref, catalog, direction) {
  const info = rotationInfo(pref,catalog);
  if (!info) throw Error("此物件暂不支持旋转，或其锚点／占地已自定义");
  const id = info.ids[(info.turn+direction+4)%4];
  const existing = prefabFor(project,id), builtin = catalog.prefabs.find((p) => p.id === id);
  if (existing && geometry(existing) !== geometry(builtin))
    throw Error("目标方向已有自定义组合，请保留原定义后再旋转");
  return existing || builtin;
}

export function rotateObject(project, objectId, catalog, direction, locked) {
  const object = project.map.objects.find((o) => o.id === objectId);
  if (!object) throw Error("请先选择一个完整物件");
  const previous = prefabFor(project,object.prefab);
  const next = rotatedPrefab(project,previous,catalog,direction);
  if ([...previous.cells,...next.cells].some((c) => locked[c.layer]))
    throw Error("物件涉及的图层已锁定");
  if (object.x+next.width>dimensions(project)[0] || object.y+next.height>dimensions(project)[1])
    throw Error("旋转后超出地图，请先把物件移到空处");
  if (!prefabFor(project,next.id)) {
    if (project.prefabs.length >= 200) throw Error("组合库已满，请先整理自定义组合");
    project.prefabs.push(copy(next));
  }
  object.prefab = next.id;
  bringToFront(project,[objectId]);
  return next;
}
