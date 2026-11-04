import assert from 'node:assert/strict';
import fs from 'node:fs';
import {sampleCells,flatten} from './model.js';
const catalog=JSON.parse(fs.readFileSync(0,'utf8'));
const rect=(id,w=1,h=1)=>({x:(id-384)%8,y:Math.floor((id-384)/8),w,h});
const bench=sampleCells(catalog,'outside',rect(1381),0);
assert.equal(bench[0].layer,1);
const lamp=sampleCells(catalog,'outside',rect(1411,1,3),0);
assert.deepEqual(lamp.map(c=>c.layer),[1,1,1]);
assert.deepEqual(lamp.map(c=>c.asset),['outside:1411','outside:1419','outside:1427']);
assert.equal(sampleCells(catalog,'outside',rect(546),0)[0].layer,0);
assert.equal(sampleCells(catalog,'outside',rect(1381),2)[0].layer,1); // Registered artwork uses its required layer.
assert.equal(sampleCells(catalog,'outside',rect(1341),2)[0].layer,2); // Unregistered artwork respects explicit foreground.
assert.deepEqual(sampleCells(catalog,'outside',rect(99999),0),[]);
const p={prefabs:[{id:'sample',cells:bench}],map:{layers:[Array(192).fill('outside:385'),Array(192).fill(null),Array(192).fill(null)],objects:[{id:'object',prefab:'sample',x:2,y:3}]}};
assert.equal(flatten(p).layers[0][50],'outside:385');assert.equal(flatten(p).layers[1][50],'outside:1381');
const forced=structuredClone(catalog);forced.assets['outside:1381'].requiredLayer=2;
assert.equal(sampleCells(forced,'outside',rect(1381),0)[0].layer,2);
console.log('Source sampling: transparent objects preserve ground, opaque terrain stays on ground, explicit foreground/required layers, empty cells and complete multi-cell sampling passed.');
