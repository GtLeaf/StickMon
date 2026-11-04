import assert from 'node:assert/strict';
import {copy,History,flatten,visibleFootprint,flood,addLine} from './model.js';
import {resizeMap} from './resize.js';
import {lakeSeed,editLake,retileLake} from './lake.js';
import {paintZone,NpcEditor} from './npc-editor.js';
import {recognizeRoads,storeRoadRoute,currentRoadNetwork,roadRouteCurrent} from './road-network.js';
import {rotateObject} from './rotation.js';
const blank=()=>({prefabs:[],map:{size:[16,12],layers:[Array(192).fill('grass'),Array(192).fill(null),Array(192).fill(null)],objects:[],paths:[],entry:{edge:'bottom',point:[7,11]},junction:[7,11]}});
const spec={defaultGround:'grass',compatibleKeys:[],byMask:Object.fromEntries(Array.from({length:256},(_,i)=>[i,`lake:${i}`]))};
const p=blank();p.map.layers[0][2*16+15]='road';p.map.layers[1][3*16+2]='flower';
editLake(p,[[2,9],[3,9]],false,spec);
const before=copy(p),history=new History(p);
history.change(v=>{resizeMap(v,[32,24],'grass');retileLake(v,spec);});
const large=history.project;
assert.deepEqual(large.map.entry.point,[7,23]);assert.deepEqual(large.map.junction,[7,23]);
for(let z=0;z<3;z++)for(let y=0;y<12;y++)for(let x=0;x<16;x++)assert.equal(large.map.layers[z][y*32+x],before.map.layers[z][y*16+x]);
assert.equal(large.map.layers[0][767],'grass');assert.equal(large.map.lakeGround[9*32+2],'grass');
history.undo();assert.deepEqual(history.project,before);history.redo();assert.deepEqual(history.project,large);
resizeMap(large,[16,12],'grass');assert.deepEqual(large,before);
const fresh=()=>{const v=blank();resizeMap(v,[32,24],'grass');return v;};
for(const change of [v=>v.map.layers[0][767]='road',v=>v.map.layers[1][767]='flower',
 v=>v.map.activityZones=[{name:'远处',cells:[[31,23]]}],
 v=>v.map.npcs=[{name:'远处精灵',spawn:[31,23],patrol:[]}],
 v=>v.map.npcs=[{name:'巡逻',spawn:[1,1],patrol:[[31,23]]}],
 v=>v.map.paths=[{points:[[31,23]],exit:{edge:'right',point:[31,23]}}],
 v=>{v.prefabs=[{id:'table',width:2,height:1}];v.map.objects=[{id:'one',prefab:'table',x:15,y:5}];}]){
 const v=fresh();change(v);const original=copy(v);assert.throws(()=>resizeMap(v,[16,12],'grass'),/无法缩小/);assert.deepEqual(v,original);
}
const lake=fresh();editLake(lake,lakeSeed(lake,31,23,spec),false,spec);
assert.equal(lake.map.layers[0].filter(k=>k.startsWith('lake:')).length,9);
assert.equal(lake.map.layers[0][767],'lake:25');assert.equal(lake.map.layers[0][0],'grass');
editLake(lake,[[31,23]],true,spec);assert.equal(lake.map.layers[0][767],'grass');
const layer=Array(768).fill('wall');layer[31]='grass';layer[32]='grass';flood(layer,31,'road',new Set(),[32,24]);assert.equal(layer[32],'grass');
layer[767]='grass';layer[735]='grass';flood(layer,767,'road',new Set(),[32,24]);assert.equal(layer[735],'road');
const zone={cells:[]};paintZone(zone,[[31,23],[0,1],[31,0],[32,23]],false,[32,24]);assert.deepEqual(zone.cells,[[31,0],[0,1],[31,23]]);
paintZone(zone,[[31,23]],true,[32,24]);assert.equal(zone.cells.length,2);
const objects=fresh(),pref={id:'bench',width:2,height:1,anchor:[1,1],footprint:[[0,0],[1,0]],cells:[{x:0,y:0,layer:1,asset:'bench'},{x:1,y:0,layer:1,asset:'bench'}]};
objects.prefabs=[pref];const object={id:'o',prefab:'bench',x:30,y:23};objects.map.objects=[object];
assert.equal(flatten(objects).owners[1][767],'o');assert.equal(flatten(objects).layers[1][767],'bench');
assert.deepEqual(visibleFootprint(object,pref,flatten(objects).owners,[32,24]),[[0,0],[1,0]]);
const turned={...copy(pref),id:'turned',width:1,height:2,cells:[{x:0,y:0,layer:1,asset:'bench'},{x:0,y:1,layer:1,asset:'bench'}]};
const rotations={prefabs:[pref,turned],rotations:{bench:['bench','turned','bench','turned']}};
assert.throws(()=>rotateObject(objects,'o',rotations,1,[false,false,false]),/超出地图/);
object.y=22;rotateObject(objects,'o',rotations,1,[false,false,false]);assert.equal(object.prefab,'turned');
const catalog={assets:{grass:{},road:{road:true,runtimeId:546}}}, roads=fresh();
roads.map.entry={edge:'right',point:[31,20]};roads.map.junction=[31,20];
for(let y=20;y<22;y++)for(let x=0;x<32;x++)roads.map.layers[0][y*32+x]='road';
roads.map.roadNetwork=recognizeRoads(roads,catalog);storeRoadRoute(roads);
assert.equal(roads.map.roadNetwork.edges.length,32);assert.deepEqual(roads.map.paths[0].points[0],[32,21]);
assert.equal(roads.map.paths[0].points.length,65);assert(roadRouteCurrent(roads,roads.map.paths[0]));
const old=blank();old.map.layers[0].fill('road');old.map.roadNetwork=recognizeRoads(old,catalog);storeRoadRoute(old);
resizeMap(old,[32,24],'grass');assert.equal(currentRoadNetwork(old),null);assert(!roadRouteCurrent(old,old.map.paths[0]));
const dense=fresh();dense.map.layers[0].fill('road');dense.map.roadNetwork=recognizeRoads(dense,catalog);storeRoadRoute(dense);
assert(dense.map.paths[0].points.length>1025);assert.equal(dense.map.paths[0].points.length,dense.map.roadNetwork.edges.length*2+1);
assert.equal(addLine([[0,0]],[31,23],56).length,55);
console.log('Map sizes: preservation, restoration, undo, shrink rejection, far-edge lake/fill/zone/object/rotation, large/dense road traversal passed.');

// Rendering must use map stride even when a sprite has its own pixel width.
const renderMap=fresh();renderMap.map.npcs=[{id:'actor',sprite:'demo',name:'Actor',spawn:[30,20],direction:0}];
renderMap.map.layers[1][767]='canopy';
const frame={x:0,y:0,width:20,height:30,groundOffsetY:2};
const sprite={id:'demo',scale:26,actions:{idle:Array.from({length:4},()=>[frame])}};
const renderer=Object.create(NpcEditor.prototype);renderer.images={demo:{}};renderer.elapsed=0;
renderer.e={project:()=>renderMap,tool:()=>'',catalog:{npcSprites:[sprite],assets:{canopy:{depth:[0,2,0]}}},animation:()=>0};
globalThis.document={getElementById:()=>({checked:true})};
const ctx=new Proxy({}, {get:(o,key)=>o[key]||(()=>{}),set:(o,key,v)=>(o[key]=v,true)});
const draws=[];const flat=flatten(renderMap);
renderer.draw(ctx,32,flat.layers,flat.owners,[true,true,true],(...args)=>draws.push(args));
assert.equal(draws.length,1);assert.deepEqual(draws[0].slice(2,4),[31*32,23*32]);
delete globalThis.document;
console.log('NPC occlusion renderer: map stride independent of sprite dimensions passed.');

const stale=fresh();stale.map.layers[0].fill('road');stale.map.roadNetwork=recognizeRoads(stale,catalog);
stale.map.layers[0].fill('grass');resizeMap(stale,[16,12],'grass');assert.equal(stale.map.roadNetwork,undefined);

// Independent dimensions, including same-area reshape and one-axis changes.
for(const size of [[17,13],[25,18],[16,64],[64,12],[64,64]]){
  const p=blank();p.map.layers[1][11*16+15]='flower';resizeMap(p,size,'grass');
  assert.deepEqual(p.map.size,size);assert.equal(p.map.layers[1][11*size[0]+15],'flower');
  assert.equal(p.map.layers[0].length,size[0]*size[1]);
  resizeMap(p,[16,12],'grass');assert.equal(p.map.layers[1][191],'flower');
}
for(const size of [[15,12],[16,11],[65,12],[16,65],[17.5,13],[17,NaN],[Infinity,12],[32],[32,24,1],null]){
 const p=blank(),before=copy(p);assert.throws(()=>resizeMap(p,size,'grass'),/整数/);assert.deepEqual(p,before);
}
const shape=blank();resizeMap(shape,[24,16],'grass');shape.map.layers[0].fill('road');
shape.map.roadNetwork=recognizeRoads(shape,catalog);storeRoadRoute(shape);
const sameArea=copy(shape);sameArea.map.size=[32,12];
assert.equal(currentRoadNetwork(sameArea),null);assert(!roadRouteCurrent(sameArea,sameArea.map.paths[0]));
// Keep saved legacy road graphs usable without forcing a user migration.
const legacy=blank();legacy.map.layers[0].fill('road');legacy.map.roadNetwork=recognizeRoads(legacy,catalog);storeRoadRoute(legacy);
legacy.map.roadNetwork.source=JSON.stringify([legacy.map.layers,legacy.map.objects,legacy.prefabs]);
legacy.map.paths[0].roadSource=legacy.map.roadNetwork.source;
assert(currentRoadNetwork(legacy));assert(roadRouteCurrent(legacy,legacy.map.paths[0]));
const odd=blank();resizeMap(odd,[25,17],'grass');
editLake(odd,lakeSeed(odd,24,16,spec),false,spec);assert.equal(odd.map.layers[0][424],'lake:25');
console.log('Custom sizes: independent/odd dimensions, limits, preservation, same-area road freshness and legacy graphs passed.');
