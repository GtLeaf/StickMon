import assert from 'node:assert/strict';
import fs from 'node:fs';
import {copy, History, prefabFor, flatten} from './model.js';
import {stackInfo, stackObject} from './stacking.js';
import {rotateObject, rotationInfo} from './rotation.js';

const {project, catalog} = JSON.parse(fs.readFileSync(0, 'utf8'));
const unlocked = [false,false,false], cases = [];
for (const family of Object.keys(catalog.stacks)) {
  const p = copy(project);
  p.map.objects = [{id:'target',prefab:family,x:2,y:6}];
  const h = new History(p);
  for (const count of [2,3,1,3,2,1]) {
    const before = copy(h.project);
    h.change(p => stackObject(p, 'target', catalog, count, unlocked));
    const current = copy(h.project), object = current.map.objects[0];
    const pref = prefabFor(current,object.prefab);
    assert.equal(stackInfo(pref,catalog).count,count);
    assert.equal(object.y+pref.anchor[1],7);
    assert.equal(object.x+pref.anchor[0],2.5);
    assert.deepEqual(pref.footprint,[[0,pref.height-1]]);
    assert.equal(rotationInfo(pref,catalog) === null,count > 1 || family !== 'market-crate');
    h.undo(); assert.deepEqual(h.project,before);
    h.redo(); assert.deepEqual(h.project,current);
    // JSON and duplicate preserve the stack without per-instance side channels.
    const reopened = JSON.parse(JSON.stringify(current));
    reopened.map.objects.push({...object,id:'copy',x:3});
    assert.equal(stackInfo(prefabFor(reopened,reopened.map.objects[1].prefab),catalog).count,count);
    cases.push(reopened);
  }
  function fails(p,count,locked,pattern) {
    const before = copy(p);
    assert.throws(() => stackObject(p,'target',catalog,count,locked),pattern);
    assert.deepEqual(p,before);
  }
  const edge=copy(p);edge.map.objects[0].y=0;
  fails(edge,2,unlocked,/超出/);
  fails(p,3,[false,true,false],/锁定/);
  fails(p,2,[false,false,true],/锁定/);
  for (const count of [0,4,1.5]) fails(p,count,unlocked,/1～3/);
  const edited=copy(p);prefabFor(edited,family).anchor=[.5,.5];
  assert.equal(stackInfo(prefabFor(edited,family),catalog),null);
  fails(edited,2,unlocked,/自定义/);
  const target=copy(p);prefabFor(target,catalog.stacks[family][1]).footprint=[];
  fails(target,2,unlocked,/自定义/);
  const cover=copy(p);
  cover.map.objects.push({id:'under',prefab:'market-crate',x:2,y:5});
  stackObject(cover,'target',catalog,3,unlocked);
  assert.equal(flatten(cover).owners[2][5*16+2],'target');
  assert.equal(flatten(cover).owners[1][5*16+2],'under');
  cover.map.objects=cover.map.objects.filter(o=>o.id!=='target');
  assert.equal(flatten(cover).owners[1][5*16+2],'under');
  const full=copy(p);while(full.prefabs.length<200)full.prefabs.push({...full.prefabs[0],id:`extra-${full.prefabs.length}`});
  // Base projects contain builtins. Removing the hidden target tests the append bound.
  full.prefabs=full.prefabs.filter(v=>v.id!==catalog.stacks[family][1]);
  full.prefabs.push({...full.prefabs[0],id:'last'});
  fails(full,2,unlocked,/组合库已满/);
}
const rotated=copy(project);rotated.map.objects=[{id:'target',prefab:'market-crate',x:2,y:6}];
rotateObject(rotated,'target',catalog,1,unlocked);
assert.equal(stackInfo(prefabFor(rotated,rotated.map.objects[0].prefab),catalog).rotated,true);
assert.throws(()=>stackObject(rotated,'target',catalog,2,unlocked),/0°/);
rotateObject(rotated,'target',catalog,-1,unlocked);
stackObject(rotated,'target',catalog,3,unlocked);
assert.throws(()=>rotateObject(rotated,'target',catalog,1,unlocked),/不支持旋转/);
process.stdout.write(JSON.stringify(cases));
