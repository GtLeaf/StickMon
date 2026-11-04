import assert from 'node:assert/strict';
import {copy, History, objectsInRect, moveObjects, flatten} from './model.js';
const p={prefabs:[
  {id:'small',width:1,height:1,cells:[{x:0,y:0,layer:1,asset:'box'}]},
  {id:'large',width:2,height:2,cells:[{x:0,y:0,layer:1,asset:'top'},{x:1,y:1,layer:2,asset:'foot'}]},
],map:{size:[18,17],layers:[Array(306).fill('grass'),Array(306).fill(null),Array(306).fill(null)],objects:[
  {id:'hiddenUnder',prefab:'small',x:2,y:2},{id:'one',prefab:'small',x:2,y:2},
  {id:'two',prefab:'large',x:4,y:3},{id:'outside',prefab:'small',x:8,y:8}
]}};
const visible=[true,true,true], unlocked=[false,false,false], box={x:1,y:1,w:5,h:4};
assert.deepEqual(objectsInRect(p,box,visible,unlocked),['one','two']);
assert.deepEqual(objectsInRect(p,{...box,w:4},visible,unlocked),['one']);
assert.deepEqual(objectsInRect(p,box,[true,false,false],unlocked),[]);
assert.deepEqual(objectsInRect(p,box,visible,[false,false,true]),['one']);
assert.deepEqual(objectsInRect(p,box,visible,[false,true,false]),[]);
assert.deepEqual(objectsInRect(p,{x:0,y:0,w:1,h:1},visible,unlocked),[]);
const positions=p.map.objects.filter(o=>['one','two'].includes(o.id)).map(({id,x,y})=>({id,x,y}));
const h=new History(p), before=copy(p);
h.begin();moveObjects(h.project,positions,3,4);moveObjects(h.project,positions,99,99);h.finish();
assert.deepEqual(h.project.map.objects.filter(o=>['one','two'].includes(o.id)).map(o=>[o.x,o.y]),[[14,14],[16,15]]);
assert.deepEqual(h.project.map.objects[3],before.map.objects[3]);
assert.deepEqual(h.project.map.layers,before.map.layers);
assert.equal(h.undoStack.length,1);
const moved=copy(h.project);h.undo();assert.deepEqual(h.project,before);h.redo();assert.deepEqual(h.project,moved);
h.begin();moveObjects(h.project,positions,-99,-99);h.cancel();assert.deepEqual(h.project,moved);
const fresh=copy(p);moveObjects(fresh,positions,4,3);moveObjects(fresh,positions,0,0);assert.deepEqual(fresh,p);
assert.equal(flatten(moved).owners[1][14*18+14],'one');
assert.equal(flatten(moved).owners[2][16*18+17],'two');
console.log('Rectangle selection: complete assemblies, covered/hidden/locked objects, empty boxes, group bounds, reverse drag, undo/redo and cancellation passed.');
