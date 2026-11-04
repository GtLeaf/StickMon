import assert from 'node:assert/strict';
import {History,copy} from './model.js';
import {paintZone,deleteZone,makeNpc,makeVisitors,previewActors,spriteFrame,spriteBounds} from './npc-editor.js';
const p={map:{activityZones:[{id:'zone',name:'Market',color:'#aabbcc',cells:[]}],npcs:[],layers:[],objects:[]}};
const h=new History(p);
h.begin();paintZone(h.project.map.activityZones[0],[[1,1],[2,1],[2,1],[-1,1]]);h.finish();
assert.deepEqual(h.project.map.activityZones[0].cells,[[1,1],[2,1]]);
h.undo();assert.deepEqual(h.project,p);h.redo();
h.change(p=>paintZone(p.map.activityZones[0],[[1,1]],true));
assert.deepEqual(h.project.map.activityZones[0].cells,[[2,1]]);
const npc=makeNpc('zone');npc.spawn=[2,1];h.change(p=>p.map.npcs.push(npc));
const before=copy(h.project);
assert.throws(()=>h.change(p=>deleteZone(p,'zone')));assert.deepEqual(h.project,before);
h.change(p=>p.map.npcs[0].scope='map');h.change(p=>deleteZone(p,'zone'));
assert.deepEqual(h.project.map.npcs[0].zoneIds,[]);h.undo();
assert.equal(h.project.map.activityZones.length,1);assert.deepEqual(h.project.map.npcs[0].zoneIds,['zone']);
h.begin();h.project.map.npcs[0].spawn=[8,9];h.cancel();assert.deepEqual(h.project.map.npcs[0].spawn,[2,1]);
assert.equal(makeNpc(null).scope,'map');assert.notEqual(makeNpc(null).id,makeNpc(null).id);
assert.equal(makeNpc(null).sprite,'pokemon-025');
const idle=[{width:64,height:64,groundOffsetY:26},{width:64,height:64,groundOffsetY:26}],walk=[{}, {}, {}];
const sprite={scale:26,actions:{idle:[idle,idle,idle,idle],walking:[walk,walk,walk,walk]}};
assert.equal(spriteFrame(sprite,[3.5,4.5,0,0,0],280),idle[1]);
assert.equal(spriteFrame(sprite,[3.5,4.5,3,0,1],320),walk[2]);
assert.equal(spriteFrame(sprite,[3.5,4.5,3,0,1],480),walk[0]);
for(const size of [26,32]){
  const b=spriteBounds(sprite,idle[0],[3.5,4.5,0,0,0],size);
  assert.equal(b.left+b.width/2,3.5*size);
  assert.ok(Math.abs(b.top+(32+26)*b.ratio-4.5*size)<1e-10);
}
console.log('NPC editor: stable identities, paint/erase, atomic undo, referenced zone deletion and spawn drag cancellation passed.');
const v=makeVisitors();assert.deepEqual(v.families,['025-172']);
const history=new History({map:{activityZones:[{id:'a',cells:[[3,9],[3,10]]}],npcs:[]}});
history.change(p=>p.map.activityZones[0].visitors=makeVisitors());history.undo();
assert.equal(history.project.map.activityZones[0].visitors,undefined);history.redo();
assert.deepEqual(history.project.map.activityZones[0].visitors,v);
const actor={id:'visitors:a:adult',visitor:true,name:'adult'};
const timeline={stepMs:100,actors:[actor],frames:[
  [[7.5,12.5,2,0,1,0,0]],[[7.5,12.5,2,0,1,1,25]],[[7.5,12.3333,2,1,1,1,25]],
  [[7.5,12.5,0,0,1,0,25]],[[7.5,12.5,2,0,1,1,184]]
]};
const source=copy(timeline);
assert.deepEqual(previewActors(history.project,timeline,50),[]);
const arrival=previewActors(history.project,timeline,150)[0];
assert.equal(arrival.n.sprite,'pokemon-025');assert.ok(arrival.pose[1]<12.5&&arrival.pose[1]>12.3333);
assert.equal(previewActors(history.project,timeline,250)[0].pose[1],12.3333);
assert.deepEqual(previewActors(history.project,timeline,350),[]);
assert.equal(previewActors(history.project,timeline,400)[0].n.sprite,'pokemon-184');
assert.deepEqual(timeline,source);
console.log('Visitor editor: undo/redo, hidden actors, continuous arrival, species refresh and source immutability passed.');

// Body lift changes drawing only, never the world foot or source timeline.
const raised=[3.5,4.5,0,0,0,1,25,1,4];
const raisedBounds=spriteBounds(sprite,idle[0],raised,26);
const groundedBounds=spriteBounds(sprite,idle[0],[3.5,4.5,0,0,0],26);
assert.equal(raisedBounds.top,groundedBounds.top-4);
assert.equal(raisedBounds.left,groundedBounds.left);
const liftTimeline={stepMs:100,actors:[actor],frames:[[raised],[[3.5,4.5,0,0,0,1,25,1,0]]]};
const liftPose=previewActors(history.project,liftTimeline,50)[0].pose;
assert.deepEqual(liftPose.slice(0,2),[3.5,4.5]);assert.equal(liftPose[8],2);
assert.equal(raised[8],4);
console.log('Market performance preview: interpolated body lift preserves foot, shadow anchor and timeline.');
