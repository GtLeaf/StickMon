import assert from 'node:assert/strict';
import {copy, History, index, flatten} from './model.js';
import {editLake,lakeSeed,lakeMask,retileLake,lakeDirections} from './lake.js';

const masks=new Set();
for(let n=0;n<256;n++) {
  const cells=Array(192).fill('land');
  lakeDirections.forEach(([dx,dy],bit)=>{if(n & (1<<bit)) cells[index(5+dx,5+dy)]='water';});
  masks.add(lakeMask(cells,5,5,new Set(['water'])));
}
assert.equal(masks.size,47);
const spec={byMask:Object.fromEntries([...masks].map(n=>[n,`lake:${n}`])),
  compatibleKeys:['runtime:48','runtime:84','runtime:68','runtime:80','runtime:64','runtime:72','runtime:5001','runtime:5002','runtime:5000'],
  defaultGround:'grass'};
const blank=()=>({prefabs:[],map:{layers:[Array(192).fill('grass'),Array(192).fill(null),Array(192).fill(null)],objects:[]}});
const count=p=>p.map.layers[0].filter(v=>v.startsWith('lake:')).length;
const p=blank();
p.map.layers[0][index(3,3)]='road';
editLake(p,lakeSeed(p,3,3,spec),false,spec);
assert.equal(count(p),9);
for(const [x,y,mask] of [[2,2,70],[4,2,140],[2,4,35],[4,4,25],[3,3,255]])
  assert.equal(p.map.layers[0][index(x,y)],`lake:${mask}`);
assert.equal(p.map.lakeGround[index(3,3)],'road');
editLake(p,[[3,3]],true,spec);
assert.equal(p.map.layers[0][index(3,3)],'road');
assert.equal(p.map.layers[0][index(2,2)],'lake:6'); // concave SE corner
const saved=JSON.parse(JSON.stringify(p));retileLake(saved,spec);assert.deepEqual(saved,p);
editLake(p,[[2,3],[4,3]],true,spec);
assert.equal(count(p),6); // two disconnected horizontal strips
const before=copy(p);editLake(p,[[0,0]],true,spec);assert.deepEqual(p,before);
editLake(p,[[3,3]],false,spec);assert.equal(count(p),7); // reconnect

const edge=blank();editLake(edge,lakeSeed(edge,0,0,spec),false,spec);assert.equal(count(edge),9);
assert.ok(Object.keys(edge.map.lakeGround).every(i=>+i>=0 && +i<192));
const diagonal=blank();editLake(diagonal,[[2,2],[3,3]],false,spec);
assert.equal(diagonal.map.layers[0][index(2,2)],'lake:0');
assert.equal(diagonal.map.layers[0][index(3,3)],'lake:0');

const fixed=blank();fixed.prefabs=[{id:'old-lake',cells:[{x:0,y:0,layer:0,asset:'runtime:48'},{x:1,y:0,layer:0,asset:'runtime:48'}]}];
fixed.map.objects=[{id:'old',prefab:'old-lake',x:3,y:3}];
editLake(fixed,[[5,3]],false,spec);assert.equal(fixed.map.objects.length,0);assert.equal(count(fixed),3);
editLake(fixed,[[3,3],[4,3],[5,3]],true,spec);assert.equal(count(fixed),0);assert.equal(fixed.map.lakeGround,undefined);
assert.equal(fixed.map.layers[0][index(3,3)],'grass');

const blocked=blank();blocked.prefabs=[{id:'flowers',cells:[{x:0,y:0,layer:0,asset:'flower'}]}];
blocked.map.objects=[{id:'f',prefab:'flowers',x:3,y:3}];const unchanged=copy(blocked);
assert.throws(()=>editLake(blocked,lakeSeed(blocked,3,3,spec),false,spec));assert.deepEqual(blocked,unchanged);
const h=new History(blank());h.begin();editLake(h.project,lakeSeed(h.project,3,3,spec),false,spec);
editLake(h.project,[[5,3],[6,3]],false,spec);h.finish();assert.equal(count(h.project),11);
h.undo();assert.equal(count(h.project),0);h.redo();assert.equal(count(h.project),11);
h.begin();editLake(h.project,[[5,3]],true,spec);h.cancel();assert.equal(count(h.project),11);
h.project.map.layers[0][index(5,3)]='path';retileLake(h.project,spec);
assert.equal(h.project.map.lakeGround[index(5,3)],undefined);
console.log('Automatic lakes: 47 masks, corners, islands, split/merge, edges, restoration, old stamps, object protection and atomic undo passed.');
