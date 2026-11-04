import assert from "node:assert/strict";
import { History, flatten, flood, worldPoints, addLine, libraryPrefabs, terrainBrush, bringToFront, visibleFootprint } from "./model.js";
const p = {
  name: "test",
  prefabs: [
    {
      id: "tree",
      cells: [
        { x: 0, y: 0, layer: 1, asset: "leaf" },
        { x: 0, y: 1, layer: 1, asset: "root" },
      ],
    },
  ],
  map: {
    layers: [
      Array(192).fill("grass"),
      Array(192).fill(null),
      Array(192).fill(null),
    ],
    objects: [],
  },
};
const h = new History(p);
h.begin();
h.project.map.objects.push({ id: "one", prefab: "tree", x: 2, y: 3 });
h.project.map.objects[0].x = 4;
h.finish();
assert.equal(h.undoStack.length, 1);
assert.equal(flatten(h.project).layers[1][52], "leaf");
assert.equal(flatten(h.project).layers[0][52], "grass");
h.markSaved();
assert.equal(h.dirty, false);
h.change((p) => (p.map.objects[0].x = 5));
h.undo();
assert.equal(h.dirty, false);
h.redo();
assert.equal(h.project.map.objects[0].x, 5);
h.change((p) => (p.map.objects = []));
assert.equal(flatten(h.project).layers[1][53], null);
assert.equal(flatten(h.project).layers[0][53], "grass");
h.undo();
assert.equal(h.project.map.objects.length, 1);
const before = structuredClone(h.project);
assert.throws(() =>
  h.change((p) => {
    p.name = "bad";
    throw Error("cancel");
  }),
);
assert.deepEqual(h.project, before);
h.begin();
h.project.name = "drag";
h.cancel();
assert.deepEqual(h.project, before);
const cells = Array(192).fill("grass");
flood(cells, 0, "road", new Set([1, 16]));
assert.equal(cells[0], "road");
assert.equal(cells[17], "grass");
assert.deepEqual(
  worldPoints({
    cellCentered: false,
    points: [
      [3, 3],
      [3, 4],
      [4, 4],
    ],
  }),
  [
    [104, 91],
    [104, 130],
    [117, 130],
  ],
);
assert.deepEqual(
  worldPoints({
    cellCentered: true,
    points: [
      [3, 3],
      [3, 4],
    ],
  }),
  [
    [91, 91],
    [91, 117],
  ],
);
assert.deepEqual(addLine([[1, 1]], [3, 2]), [
  [1, 1],
  [2, 1],
  [3, 1],
  [3, 2],
]);
console.log(
  "Editor state: undo/redo, atomic cancellation, object ownership, fill boundaries and route geometry passed.",
);

const oldProject = {prefabs:[
  {id:'market-pot', category:'绿宝石集市', cells:[]},
  {id:'market-umbrella-blue', category:'绿宝石集市', cells:[]},
  {id:'custom', category:'自定义', cells:[]},
]};
const oldCopy=structuredClone(oldProject);
const updated=libraryPrefabs(oldProject,[
  {id:'market-pot', category:'集市', cells:[]},
  {id:'market-umbrella-blue', hidden:true, cells:[]},
  {id:'market-umbrella-blue-full', category:'集市', cells:[]},
]);
assert.deepEqual(updated.map(p=>p.id),['market-pot','custom','market-umbrella-blue-full']);
assert.equal(updated[0].category,'集市');
assert.deepEqual(oldProject,oldCopy);
const legacy = {prefabs:[
  {id:'tree', category:'树木', cells:[]},
  {id:'flower', category:'草花', cells:[]},
  {id:'rock', category:'岩石', cells:[]},
  {id:'truck', category:'交通', cells:[]},
  {id:'fence', category:'围栏', cells:[]},
  {id:'ladder', category:'洞穴', cells:[]},
  {id:'import-tree', category:'导入物件', cells:[]},
  {id:'user', category:'我的组合', cells:[]},
]};
const legacyBefore = structuredClone(legacy);
const ground = {id:'ground-546',category:'道路地面',width:1,height:1,footprint:[],
  brushAsset:'outside:546',cells:[{x:0,y:0,layer:0,asset:'outside:546'}]};
assert.deepEqual(libraryPrefabs(legacy,[ground]).map(p=>p.category),
  ['自然景物','自然景物','自然景物','集市','围栏设施','围栏设施','自定义组合','我的组合','道路地面']);
assert.deepEqual(legacy,legacyBefore);
assert.equal(terrainBrush(ground),'outside:546');
assert.equal(terrainBrush({...ground,width:2}),null);
assert.equal(terrainBrush({...ground,footprint:[[0,0]]}),null);
assert.equal(terrainBrush({...ground,cells:[{...ground.cells[0],layer:1}]}),null);
assert.equal(terrainBrush({...ground,cells:[{...ground.cells[0],asset:'custom'}]}),null);
assert.equal(terrainBrush(legacy.prefabs[0]),null);
console.log('Library: legacy categories, custom groups and terrain brush eligibility passed.');
const stacked=structuredClone(p);
stacked.prefabs[0].footprint=[[0,1]];
stacked.map.objects=[{id:'old',prefab:'tree',x:2,y:3},{id:'new',prefab:'tree',x:2,y:4}];
const stackHistory=new History(stacked);
let resolved=flatten(stackHistory.project);
assert.equal(resolved.layers[1][4*16+2],'leaf');
assert.equal(resolved.owners[1][4*16+2],'new');
assert.deepEqual(visibleFootprint(stackHistory.project.map.objects[0],stacked.prefabs[0],resolved.owners),[]);
assert.deepEqual(visibleFootprint(stackHistory.project.map.objects[1],stacked.prefabs[0],resolved.owners),[[0,1]]);
stackHistory.change(p=>bringToFront(p,['old']));
resolved=flatten(stackHistory.project);
assert.equal(resolved.layers[1][4*16+2],'root');
assert.deepEqual(visibleFootprint(stackHistory.project.map.objects.at(-1),stacked.prefabs[0],resolved.owners),[[0,1]]);
stackHistory.undo();assert.deepEqual(stackHistory.project,stacked);
stackHistory.redo();assert.equal(stackHistory.project.map.objects.at(-1).id,'old');
console.log('Overlapping objects: last placement wins, visible footprints and atomic stacking undo/redo passed.');
