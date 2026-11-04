// Called by test_map_editor.py with the production catalog and a routed map.
import assert from "node:assert/strict";
import fs from "node:fs";
import {copy, History, prefabFor, flatten} from "./model.js";
import {rotationInfo, rotatedPrefab, rotateObject} from "./rotation.js";

const {project,catalog} = JSON.parse(fs.readFileSync(0,"utf8"));
const unlocked = [false,false,false], cases = [];
for (const [family,ids] of Object.entries(catalog.rotations)) {
  const p = copy(project);
  p.map.objects = [{id:"target",prefab:family,x:1,y:2}];
  const history = new History(p);
  for (let turn=1;turn<=4;turn++) {
    const before = copy(history.project);
    history.change((p) => rotateObject(p,"target",catalog,1,unlocked));
    const current = copy(history.project);
    assert.equal(current.map.objects[0].prefab,ids[turn%4]);
    assert.deepEqual([current.map.objects[0].x,current.map.objects[0].y],[1,2]);
    const pref = prefabFor(current,current.map.objects[0].prefab);
    assert.equal(rotationInfo(pref,catalog).angle,turn%4*90);
    const original = prefabFor(p,family);
    assert.deepEqual([pref.width,pref.height],turn%2 ? [original.height,original.width] : [original.width,original.height]);
    history.undo();assert.deepEqual(history.project,before);
    history.redo();assert.deepEqual(history.project,current);
    cases.push(JSON.parse(JSON.stringify(current)));
  }
  assert.deepEqual(history.project,p);
  history.change((p) => rotateObject(p,"target",catalog,-1,unlocked));
  assert.equal(history.project.map.objects[0].prefab,ids[3]);
  // A copied instance keeps its direction without altering the original.
  const duplicated = copy(history.project);
  duplicated.map.objects.push({...duplicated.map.objects[0],id:"copy",x:10});
  rotateObject(duplicated,"copy",catalog,1,unlocked);
  assert.equal(duplicated.map.objects[0].prefab,ids[3]);
  assert.equal(duplicated.map.objects[1].prefab,ids[0]);
}

function failsUnchanged(p,locked,pattern) {
  const before=copy(p);
  assert.throws(() => rotateObject(p,"target",catalog,1,locked),pattern);
  assert.deepEqual(p,before);
}
const table = copy(project);
table.map.objects=[{id:"target",prefab:"table-check",x:1,y:2}];
failsUnchanged(table,[false,true,false],/锁定/);
const edge = copy(table);edge.map.objects[0].y=10;
failsUnchanged(edge,unlocked,/超出/);
const occupied = copy(table);
occupied.map.objects.push({id:"other",prefab:"market-crate",x:1,y:4});
rotateObject(occupied,'target',catalog,1,unlocked);
assert.equal(occupied.map.objects.at(-1).id,'target');
assert.equal(flatten(occupied).owners[1][4*16+1],'target');
const underneath=copy(table);
underneath.map.objects.unshift({id:"underneath",prefab:"market-crate",x:1,y:2});
rotateObject(underneath,'target',catalog,1,unlocked);
assert.equal(underneath.map.objects.at(-1).id,'target');
underneath.map.objects.reverse();
rotateObject(underneath,'target',catalog,-1,unlocked);
assert.equal(flatten(underneath).owners[1][2*16+1],'target');
const raw = copy(table);raw.map.layers[1][4*16+1]="slateport:814";
rotateObject(raw,'target',catalog,1,unlocked);
assert.equal(flatten(raw).owners[1][4*16+1],'target');
const custom = copy(table);prefabFor(custom,"table-check").anchor=[1,1];
assert.equal(rotationInfo(prefabFor(custom,"table-check"),catalog),null);
failsUnchanged(custom,unlocked,/自定义/);
const alteredTarget=copy(table);prefabFor(alteredTarget,"table-check-r90").footprint=[];
failsUnchanged(alteredTarget,unlocked,/自定义/);
assert.equal(rotationInfo(prefabFor(project,"stall-blue"),catalog),null);

// Old maps contain the original definition only. Rotation appends its variant.
const old = copy(table);
old.prefabs=old.prefabs.filter((p) => !p.id.includes("-r"));
const oldDefinition=copy(prefabFor(old,"table-check"));
const count=old.prefabs.length;
rotateObject(old,"target",catalog,1,unlocked);
assert.equal(old.prefabs.length,count+1);
assert.deepEqual(prefabFor(old,"table-check"),oldDefinition);
cases.push(old);
const full=copy(table);
full.prefabs=full.prefabs.filter((p) => p.id!=="table-check-r90");
while(full.prefabs.length<200) full.prefabs.push({...copy(oldDefinition),id:`custom-${full.prefabs.length}`});
failsUnchanged(full,unlocked,/组合库已满/);
assert.equal(rotatedPrefab(table,prefabFor(table,"table-check"),catalog,-1).id,"table-check-r270");
process.stdout.write(JSON.stringify(cases));
