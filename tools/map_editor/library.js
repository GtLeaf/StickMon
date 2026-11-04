import {copy, prefabFor} from './model.js';
import {currentRoadNetwork, roadRouteCurrent, roadSource} from './road-network.js';

export function deleteLibraryPrefab(project, builtins, id) {
  const own = prefabFor(project,id), builtin = builtins.find(p => p.id === id);
  if (!own && !builtin) throw Error('素材已不存在');
  const used = project.map.objects.some(o => o.prefab === id);
  if (!own && project.prefabs.length >= 200) throw Error('组合库已满，请先删除未使用的自定义素材');
  const network = currentRoadNetwork(project);
  const routes = project.map.paths.filter(p => p.kind === 'roadNetwork' && roadRouteCurrent(project,p));
  // Keep definitions referenced by the map; builtins also need a tombstone
  // because the catalog fills missing IDs when the library is rendered.
  if (used || builtin) {
    const pref = own || copy(builtin);
    pref.hidden = true;
    if (!own) project.prefabs.push(pref);
  } else project.prefabs = project.prefabs.filter(p => p.id !== id);
  // Only library membership changed. Preserve fresh navigation without
  // reviving navigation that was already stale before this operation.
  const source = roadSource(project);
  if (network) network.source = source;
  for (const route of routes) route.roadSource = source;
}
