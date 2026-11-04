import {copy, prefabFor, dimensions, bringToFront} from './model.js';
import {geometry, rotationInfo} from './rotation.js';

export function stackInfo(pref, catalog) {
  if (!pref) return null;
  for (const [family, ids] of Object.entries(catalog.stacks || {})) {
    const count = ids.indexOf(pref.id) + 1;
    if (count) {
      const builtin = catalog.prefabs.find(p => p.id === pref.id);
      if (!builtin || geometry(pref) !== geometry(builtin)) return null;
      return {family, ids, count, rotated: false};
    }
    const rotation = rotationInfo(pref, catalog);
    if (rotation?.family === family) return {family, ids, count: 1, rotated: true};
  }
  return null;
}

export function stackObject(project, id, catalog, count, locked) {
  if (!Number.isInteger(count) || count < 1 || count > 3) throw Error('堆叠层数只能为 1～3');
  const object = project.map.objects.find(o => o.id === id);
  const previous = object && prefabFor(project, object.prefab);
  const info = stackInfo(previous, catalog);
  if (!info) throw Error('请选择原始木货箱或标签货箱；自定义组合不能直接切换堆叠');
  if (info.rotated && count > 1) throw Error('请先将木货箱旋转恢复到 0°，再增加堆叠层数');
  if (count === info.count) return previous;
  const builtin = catalog.prefabs.find(p => p.id === info.ids[count - 1]);
  const existing = prefabFor(project, builtin.id);
  if (existing && geometry(existing) !== geometry(builtin)) throw Error('目标层数的组合已自定义，不能覆盖原定义');
  const next = existing || builtin;
  if ([...previous.cells, ...next.cells].some(c => locked[c.layer])) throw Error('物件涉及的图层已锁定');
  const x = object.x + previous.anchor[0] - next.anchor[0];
  const y = object.y + previous.anchor[1] - next.anchor[1];
  const [width, height] = dimensions(project);
  if (x < 0 || y < 0 || x + next.width > width || y + next.height > height)
    throw Error('堆叠后超出地图，请先向下移动箱子');
  if (!existing) {
    if (project.prefabs.length >= 200) throw Error('组合库已满，请先整理自定义组合');
    project.prefabs.push(copy(next));
  }
  object.prefab = next.id;
  object.x = x; object.y = y;
  bringToFront(project, [id]);
  return next;
}
