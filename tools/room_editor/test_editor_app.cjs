const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");
const vm = require("node:vm");
const model = require("./preview_model.js");

// Exercise app state and canvas commands without claiming browser layout coverage.
function editor() {
  model.configure();
  const nodes = new Map();
  const drawCalls = [];
  function node(tag = "div") {
    const listeners = new Map();
    const classes = new Set();
    const element = {
      tagName: tag.toUpperCase(), value: "", checked: false, disabled: false,
      width: 240, height: 135, style: {}, dataset: {}, children: [],
      classList: {
        add: (...values) => values.forEach((value) => classes.add(value)),
        remove: (...values) => values.forEach((value) => classes.delete(value)),
        contains: (value) => classes.has(value),
        toggle(value, enabled = !classes.has(value)) {
          if (enabled) classes.add(value); else classes.delete(value);
        }
      },
      addEventListener(type, listener) { listeners.set(type, listener); },
      dispatch(type) { return listeners.get(type)?.({ target: element }); },
      replaceChildren(...children) { element.children = children; },
      appendChild(child) { element.children.push(child); return child; },
      add(child) { element.children.push(child); },
      setAttribute() {}, querySelectorAll() { return []; },
      getBoundingClientRect() { return { width: 240, height: 135, left: 0, top: 0 }; }
    };
    if (tag === "canvas") {
      const context = new Proxy({
        canvas: element,
        drawImage(...args) { drawCalls.push({ canvas: element, args }); },
        measureText(text) { return { width: text.length * 6 }; },
        createRadialGradient() { return { addColorStop() {} }; },
        createLinearGradient() { return { addColorStop() {} }; },
        getImageData(x, y, width, height) { return { data: new Uint8ClampedArray(width * height * 4) }; }
      }, { get(target, key) { return key in target ? target[key] : () => {}; } });
      element.getContext = () => context;
    }
    return element;
  }
  const html = fs.readFileSync(path.join(__dirname, "index.html"), "utf8");
  for (const [, tag, attributes, id] of html.matchAll(/<([\w-]+)\b([^>]*\bid="([^"]+)"[^>]*)>/g)) {
    assert.ok(!nodes.has(id), `Duplicate HTML id: ${id}`);
    const element = node(tag);
    element.id = id;
    element.value = attributes.match(/\bvalue="([^"]*)"/)?.[1] || "";
    element.checked = /\bchecked\b/.test(attributes);
    nodes.set(id, element);
  }
  const window = { RoomPreviewModel: model, addEventListener() {}, setInterval: () => 1 };
  const sandbox = vm.createContext({
    window, console, clearInterval() {}, performance: { now: () => 1000 },
    Option: function (text, value) { return { text, value }; },
    document: {
      getElementById: (id) => nodes.get(id) || null,
      querySelector: () => node(), querySelectorAll: () => [],
      createElement: node, addEventListener() {}, hidden: false
    }
  });
  const run = (code) => vm.runInContext(code, sandbox);
  const source = fs.readFileSync(path.join(__dirname, "app.js"), "utf8");
  run(source.replace(/\ninitializeEditor\(\);\s*$/, ""));
  // Generated inspector HTML and image decoding belong to browser integration.
  run(`
    refreshList = refreshSelectedPanel = refreshFaceList = refreshSelectedFacePanel =
      updateEditModeButtons = updateBaseImageMeta = updateToolButtons = () => {};
    loadImageSource = async () => ({ naturalWidth: 64, naturalHeight: 64 });
    loadExportableImageSource = async (src) => ({ img: { name: src }, dataUrl: src, exportable: true });
    state.baseTrim = { x: 0, y: 0, width: 960, height: 644 };
    syncCanvasSizeToPreparedRoom();
    syncInputsFromState();
    commitHistory();
  `);
  return { run, nodes, drawCalls };
}

function currentSceneFixture() {
  const layout = JSON.parse(fs.readFileSync(path.join(__dirname, "../../origin_asset/room/standar/room_layout.json"), "utf8"));
  const dataUrl = "data:image/png;base64,test";
  return {
    type: "stickmon-project-scene", version: 1, layout,
    backgroundItems: layout.backgroundLayers.map((layer) => ({ ...layer, dataUrl,
      width: layout.base.sourceWidth, height: layout.base.sourceHeight })),
    items: layout.furniture.map((item) => ({ ...item, dataUrl })),
    devices: model.profiles, sprites: [], warnings: [],
    source: { layout: "origin_asset/room/standar/room_layout.json", packId: "dev" }
  };
}

test("project scene loads backgrounds, all furniture and semantic regions in one undoable step", async () => {
  const e = editor();
  const payload = currentSceneFixture();
  e.run(`const scenePayload = ${JSON.stringify(payload)}; state.sourceScale = 3; commitHistory();`);
  await e.run("applyProjectScene(scenePayload)");
  assert.equal(e.run("state.backgroundItems.length"), payload.backgroundItems.length);
  assert.equal(e.run("state.items.length"), payload.items.length);
  assert.equal(e.run("state.faces.length"), payload.layout.roomGeometry.faces.length);
  assert.equal(e.run("state.faces.some(face => face.type === 'sprite_area')"), true);
  assert.equal(e.run("state.faces.some(face => face.type === 'doorway')"), true);
  assert.equal(e.run("state.items.find(item => item.name === 'window_night').visibleInDay"), false);
  assert.equal(e.run("projectDirty"), false);
  assert.ok(e.run("state.nextId > Math.max(...state.items.map(item => Number(item.id.slice(1))))"));
  e.run("undo()");
  assert.equal(e.run("state.items.length"), 0);
  assert.equal(e.run("state.sourceScale"), 3);
  e.run("redo()");
  assert.equal(e.run("state.items.length"), payload.items.length);
  assert.equal(e.run("projectDirty"), false);
});

test("failed project imports retain the previous scene and history", async () => {
  const e = editor();
  const payload = currentSceneFixture();
  payload.layout.roomGeometry.faces = "invalid";
  e.run(`const scenePayload = ${JSON.stringify(payload)}; const before = projectSaveKey(); const historyLength = state.history.length;`);
  await assert.rejects(e.run("applyProjectScene(scenePayload)"), /map is not a function/);
  assert.equal(e.run("projectSaveKey()"), e.run("before"));
  assert.equal(e.run("state.history.length"), e.run("historyLength"));
  e.run("loadImageSource = async () => { throw new Error('image unavailable'); };");
  await assert.rejects(e.run("applyProjectScene(scenePayload)"), /image unavailable/);
  assert.equal(e.run("projectSaveKey()"), e.run("before"));
});

test("live loader requests fresh project data and reports failures without replacing content", async () => {
  const e = editor();
  const payload = currentSceneFixture();
  e.run(`
    const scenePayload = ${JSON.stringify(payload)};
    window.location = { protocol: 'http:' };
    fetch = async (url, options) => {
      if (url !== '/api/project-scene' || options.cache !== 'no-store') throw new Error('incorrect request');
      return { ok: true, json: async () => scenePayload };
    };
  `);
  assert.equal(await e.run("loadCurrentProjectScene()"), true);
  assert.equal(e.nodes.get("loadProjectScene").disabled, false);
  const before = e.run("projectSaveKey()");
  e.run("fetch = async () => ({ ok: false, json: async () => ({ error: 'missing background' }) });");
  assert.equal(await e.run("loadCurrentProjectScene()"), false);
  assert.equal(e.run("projectSaveKey()"), before);
  assert.match(e.nodes.get("projectSceneMeta").textContent, /missing background/);
});

test("device controls and follow camera leave exported world geometry unchanged", () => {
  const e = editor();
  const layout = JSON.parse(fs.readFileSync(path.join(__dirname, "../../origin_asset/room/standar/room_layout.json"), "utf8"));
  e.run(`state.items = ${JSON.stringify(layout.furniture)}.map((item) => ({ ...item, img: {} }));`);
  e.run(`
    state.points = [{id: 'p1', x: 10, y: 10}, {id: 'p2', x: 800, y: 10}, {id: 'p3', x: 800, y: 600}];
    state.faces = [{ id: 'face1', type: 'sprite_area', points: state.points }];
    state.items.push({ id: 'sprite', source: 'project_sprite', name: 'test', x: 160, y: 105,
      sourceWidth: 64, sourceHeight: 64, scale: 1, groundOffsetY: 24, opacity: 1, z: 30, img: {} });
    const originalExport = JSON.stringify(exportLayoutObject());
  `);
  const change = (id, value) => {
    e.nodes.get(id).value = value;
    e.nodes.get(id).dispatch("change");
    assert.equal(e.run("JSON.stringify(exportLayoutObject())"), e.run("originalExport"));
  };
  change("previewDevice", "amoled");
  assert.equal(e.nodes.get("preview").width, 368);
  assert.equal(e.nodes.get("preview").height, 448);
  change("previewCameraX", "999");
  change("previewCameraY", "999");
  assert.equal(e.run("state.preview.x"), 56);
  assert.equal(e.run("state.preview.y"), 13);
  change("previewMode", "follow");
  change("previewEffect", "firmware");
  change("previewDevice", "sticks3");
  assert.equal(e.nodes.get("preview").width, 240);
  assert.equal(e.nodes.get("preview").height, 135);
  change("previewMode", "room");
  assert.equal(e.nodes.get("preview").height, 161);
});

test("native sprite addition, frame changes and animation preserve the ground anchor", async () => {
  const e = editor();
  e.run(`
    window.STICKMON_PROJECT_SPRITES = [{ speciesId: 1, name: 'Test', actions: { walking: [
      { path: 'a', file: 'a.png', kindName: 'TEST_WALKING_FRONT_0', width: 80, height: 96, groundOffsetY: 40 },
      { path: 'b', file: 'b.png', kindName: 'TEST_WALKING_FRONT_1', width: 60, height: 64, groundOffsetY: 25 }
    ] }}];
    document.getElementById('spriteSpeciesSelect').value = '1';
    document.getElementById('spriteActionSelect').value = 'walking';
    document.getElementById('spriteFrameSelect').value = 'a';
    document.getElementById('spriteSizing').value = 'native';
  `);
  await e.run("addProjectSpritePreview()");
  assert.equal(e.run("itemTargetWidth(state.items[0])"), 80);
  assert.equal(e.run("itemTargetHeight(state.items[0])"), 96);
  const ground = e.run("JSON.stringify(previewModel.ground(state.items[0]))");
  await e.run("updateProjectSpriteFrame(state.items[0], 1, 'walking', 'b')");
  assert.equal(e.run("itemTargetWidth(state.items[0])"), 60);
  const point = JSON.parse(ground);
  assert.equal(e.run("previewModel.ground(state.items[0]).x"), point.x);
  assert.equal(e.run("previewModel.ground(state.items[0]).y"), point.y);
  const snapshot = e.run("projectSaveKey()");
  e.run("performance.now = () => 0; previewAnimationTimer = 1; animatedSpriteVisual(state.items[0]);");
  await Promise.resolve();
  assert.equal(e.run("animatedSpriteVisual(state.items[0]).sourceWidth"), 80);
  assert.equal(e.run("previewModel.ground(animatedSpriteVisual(state.items[0])).y"), point.y);
  assert.equal(e.run("projectSaveKey()"), snapshot);
});

test("legacy sessions gain ground metadata while retaining custom sprite scale", async () => {
  const e = editor();
  await e.run(`(async () => {
    window.STICKMON_PROJECT_SPRITES = [{ speciesId: 1, actions: { idle: [
      { path: 'a', kindName: 'TEST_IDLE_FRONT_0', width: 64, height: 64, groundOffsetY: 23 }
    ] }}];
    const snapshot = createSessionSnapshot();
    delete snapshot.preview;
    snapshot.items = [{ id: 'old', source: 'project_sprite', speciesId: 1, action: 'idle',
      frame: 'TEST_IDLE_FRONT_0', x: 40, y: 50, sourceWidth: 64, sourceHeight: 64, scale: 0.5 }];
    restoreSnapshot(await hydrateSessionSnapshot(snapshot));
  })()`);
  assert.equal(e.run("state.preview.device"), "sticks3");
  assert.equal(e.run("state.preview.mode"), "room");
  assert.equal(e.run("state.items[0].groundOffsetY"), 23);
  assert.equal(e.run("state.items[0].scaleX"), 0.5);
  assert.equal(e.run("state.items[0].x"), 40);
  assert.equal(e.run("state.items[0].y"), 50);
});

test("view changes remain clean and undo restores the saved content state", () => {
  const e = editor();
  e.run("savedProjectKey = projectSaveKey(); state.preview.device = 'amoled'; renderPreview(); updateProjectSaveState();");
  assert.equal(e.run("projectDirty"), false);
  e.run("state.sourceScale = 2; commitHistory();");
  assert.equal(e.run("projectDirty"), true);
  e.run("undo()");
  assert.equal(e.run("projectDirty"), false);
  e.run("redo()");
  assert.equal(e.run("projectDirty"), true);
});

test("saving during an edit keeps newer changes dirty and failed file saves stay visible", async () => {
  const e = editor();
  e.run(`
    writeSessionFile = async () => { state.sourceScale = 3; return 'updated sessions/test.json'; };
    saveSessionRecord = async () => 'test draft';
  `);
  await e.run("saveEditorSession()");
  assert.equal(e.run("projectDirty"), true);
  e.run("writeSessionFile = async () => 'downloaded';");
  await e.run("saveEditorSession()");
  assert.equal(e.run("projectDirty"), false);
  e.run("state.sourceScale = 2; writeSessionFile = async () => 'export location not selected';");
  await e.run("saveEditorSession()");
  assert.equal(e.run("projectDirty"), true);
  assert.match(e.nodes.get("projectSaveState").textContent, /工程文件未保存/);
  await e.run("restoreSessionRecord(createSessionRecord(), { draft: true })");
  assert.equal(e.run("projectDirty"), true);
});

test("a saved project remains saved when browser storage is unavailable", async () => {
  const e = editor();
  e.run(`
    console = { ...console, warn() {} };
    state.sourceScale = 2;
    writeSessionFile = async () => 'updated sessions/test.json';
    saveSessionRecord = async () => { throw new Error('storage unavailable'); };
  `);
  await e.run("saveEditorSession()");
  assert.equal(e.run("projectDirty"), false);
  assert.match(e.nodes.get("projectSaveState").textContent, /工程已保存/);
  assert.match(e.nodes.get("status").textContent, /draft unavailable/);
  e.run("state.sourceScale = 3; writeSessionFile = async () => 'export location not selected';");
  await e.run("saveEditorSession()");
  assert.equal(e.run("projectDirty"), true);
  assert.match(e.nodes.get("projectSaveState").textContent, /均未保存/);
});

test("firmware schematic draws furniture before sprites and PNG excludes editing helpers", () => {
  const e = editor();
  e.run(`
    state.items = [
      { id: 'sprite', source: 'project_sprite', x: 10, y: 10, sourceWidth: 32, sourceHeight: 32, scale: 1, z: 1, opacity: 1, img: { name: 'sprite' }, castsShadow: false },
      { id: 'chair', source: 'furniture', x: 10, y: 10, sourceWidth: 32, sourceHeight: 32, scale: 1, z: 50, opacity: 1, img: { name: 'chair' }, castsShadow: false }
    ];
    state.night.castShadows = false;
    const helperCalls = [];
    drawGroundGuides = () => helperCalls.push('ground');
    drawScreenFrame = () => helperCalls.push('frame');
  `);
  const imageNames = () => e.drawCalls.map((call) => call.args[0]?.name).filter(Boolean);
  e.run("renderPreview()");
  assert.deepEqual(imageNames(), ["sprite", "chair"]);
  assert.equal(e.run("helperCalls.length"), 2);
  e.drawCalls.length = 0;
  e.run("helperCalls.length = 0; state.preview.effect = 'firmware'; renderPreview({ helpers: false });");
  assert.deepEqual(imageNames(), ["chair", "sprite"]);
  assert.equal(e.run("helperCalls.length"), 0);
});

function furnitureSwapEditor() {
  const e = editor();
  const manifest = JSON.parse(fs.readFileSync(path.join(__dirname, 'generated/furniture_library/manifest.json'), 'utf8'));
  e.run(`
    // DOM layout and filtering are exercised separately in the browser.
    refreshFurnitureLibraryPanel = () => {};
    window.STICKMON_FURNITURE_LIBRARY = ${JSON.stringify(manifest)};
    state.items = [{ id: 'f20', name: 'bed', fileName: 'bed.png', source: 'furniture',
      sourceWidth: 80, sourceHeight: 51, scale: 1, scaleX: 1, scaleY: 1,
      x: 36, y: 62, z: 27, sortY: 111, opacity: 0.8, visible: true,
      visibleInDay: false, visibleInNight: true, slot: 'floor', layer: 'main',
      furnitureType: 'bed', kind: 'bed_sofa', img: {}, dataUrl: 'original' }];
    state.selectedId = 'f20'; state.editMode = 'furniture'; state.nextId = 21;
    state.mode = 'night'; syncLightInputsFromState();
    syncFurnitureLibrarySelection();
    commitHistory(); savedProjectKey = projectSaveKey(); updateProjectSaveState();
    const originalKey = projectSaveKey();
    const historyCount = state.history.length;
    const originalExport = JSON.stringify(exportLayoutObject());
    const candidateEntry = normalizeFurnitureLibraryEntry({ id: 'test-bed', name: 'Test bed',
      fileName: 'new-bed.png', dataUrl: 'candidate', sourceWidth: 60, sourceHeight: 80,
      defaultScale: 1, furnitureType: 'bed', kind: 'bed_sofa', heightPx: 15,
      footprintPolygon: [[0, 0], [1, 0], [0, 1]], shadowOpacity: 12, castsShadow: false });
  `);
  return e;
}

test('library browsing distinguishes rugs, windows and shelves without changing firmware categories', () => {
  const e = furnitureSwapEditor();
  assert.equal(e.run("libraryBrowseCategory"), 'bed');
  assert.equal(e.run("furnitureBrowseCategory({fileName:'carpet.png', furnitureType:'decoration', kind:'floor_flat'})"), 'rug');
  assert.equal(e.run("furnitureBrowseCategory({fileName:'window_night.png', category:'decoration'})"), 'window');
  assert.equal(e.run("furnitureBrowseCategory({fileName:'furniture_0001_shelf.png', category:'decoration'})"), 'shelf');
  assert.equal(e.run("furnitureLibraryCatalog().filter(e => furnitureLibraryMatches(e, '', 'rug')).length"), 4);
  assert.equal(e.run("furnitureLibraryCatalog().filter(e => furnitureLibraryMatches(e, '森林', 'bed', 'forest-leaf')).length"), 1);
  e.run("state.items[0].fileName = 'carpet.png'; state.items[0].name = 'carpet'; state.items[0].furnitureType = 'decoration'; state.selectedId = null; syncFurnitureLibrarySelection(); state.selectedId = 'f20'; syncFurnitureLibrarySelection();");
  assert.equal(e.run('libraryBrowseCategory'), 'rug');
});

test('current library furniture is recognized and cannot create a no-op trial', async () => {
  const e = furnitureSwapEditor();
  assert.equal(e.run("furnitureLibraryEntryByItemReference(state.items[0]).id"), 'furniture_bed');
  assert.equal(await e.run("previewFurnitureLibraryEntry(furnitureLibraryEntryByItemReference(state.items[0]))"), false);
  assert.equal(e.run('furnitureSwap'), null);
  assert.match(e.nodes.get('status').textContent, /已经是当前家具/);
});

test('furniture trial changes rendering but leaves scene, layout, history and dirty state untouched', async () => {
  const e = furnitureSwapEditor();
  assert.equal(await e.run('previewFurnitureLibraryEntry(candidateEntry)'), true);
  assert.equal(e.run('furnitureSwap.candidate.x'), 46);
  assert.equal(e.run('furnitureSwap.candidate.y'), 33); // Original bottom remains 113.
  assert.equal(e.run('state.nextId'), 21);
  assert.equal(e.run('projectSaveKey()'), e.run('originalKey'));
  assert.equal(e.run('JSON.stringify(exportLayoutObject())'), e.run('originalExport'));
  assert.equal(e.run('state.history.length'), e.run('historyCount'));
  assert.equal(e.run('projectDirty'), false);
  assert.equal(e.run('sortedItems(true)[0].dataUrl'), 'candidate');
  assert.equal(e.run('sortedItems()[0].dataUrl'), 'original');
  assert.ok(e.drawCalls.some(({args}) => args[0]?.name === 'candidate'));
  e.run('cancelFurnitureSwap()');
  assert.equal(e.run('furnitureSwap'), null);
  assert.equal(e.run('projectSaveKey()'), e.run('originalKey'));
});

test('applying a trial replaces one object and one undo restores its original resources and semantics', async () => {
  const e = furnitureSwapEditor();
  await e.run('previewFurnitureLibraryEntry(candidateEntry)');
  assert.equal(e.run('applyFurnitureSwap()'), true);
  assert.equal(e.run('state.items.length'), 1);
  for (const [key, value] of Object.entries({id:'f20', z:27, sortY:111, opacity:0.8,
    visible:true, visibleInDay:false, visibleInNight:true, layer:'main', slot:'floor', heightPx:15, shadowOpacity:12, castsShadow:false})) {
    assert.equal(e.run(`state.items[0][${JSON.stringify(key)}]`), value);
  }
  assert.equal(e.run('state.items[0].footprintPolygon.length'), 3);
  assert.equal(e.run('state.items[0].libraryId'), 'test-bed');
  assert.equal(e.run('state.history.length'), e.run('historyCount') + 1);
  assert.equal(e.run('projectDirty'), true);
  e.run('undo()');
  assert.equal(e.run('projectSaveKey()'), e.run('originalKey'));
  e.run('redo()');
  assert.equal(e.run('state.items[0].libraryId'), 'test-bed');
});

test('rug trials preserve the center and selecting or editing a different target discards the trial', async () => {
  const e = furnitureSwapEditor();
  e.run("Object.assign(state.items[0], {fileName:'rug.png', name:'rug', furnitureType:'decoration', kind:'floor_flat'});");
  await e.run('previewFurnitureLibraryEntry(candidateEntry)');
  assert.equal(e.run('furnitureSwap.candidate.y'), 47.5);
  e.run('state.items[0].x += 1; render()');
  assert.equal(e.run('furnitureSwap'), null);
  await e.run('previewFurnitureLibraryEntry(candidateEntry)');
  e.run('state.selectedId = null; render()');
  assert.equal(e.run('furnitureSwap'), null);
});

test('rapid trial requests ignore stale images and cancellation invalidates pending decodes', async () => {
  const e = furnitureSwapEditor();
  e.run(`const pendingImages = [];
    loadExportableImageSource = src => new Promise((resolve, reject) => pendingImages.push({resolve, reject, src}));
    const trialA = previewFurnitureLibraryEntry(candidateEntry);
    const trialB = previewFurnitureLibraryEntry({...candidateEntry, id:'second', dataUrl:'second'});
    pendingImages[1].resolve({img:{}, dataUrl:'second'});`);
  await e.run('trialB');
  e.run("pendingImages[0].resolve({img:{}, dataUrl:'candidate'})");
  assert.equal(await e.run('trialA'), false);
  assert.equal(e.run('furnitureSwap.candidate.libraryId'), 'second');
  e.run('const trialC = previewFurnitureLibraryEntry(candidateEntry); cancelFurnitureSwap(); pendingImages[2].resolve({img:{}, dataUrl:"candidate"});');
  assert.equal(await e.run('trialC'), false);
  assert.equal(e.run('furnitureSwap'), null);
  assert.equal(e.run('projectSaveKey()'), e.run('originalKey'));
});

test('decode failure retains the last working trial and Undo cancels without undoing earlier edits', async () => {
  const e = furnitureSwapEditor();
  await e.run('previewFurnitureLibraryEntry(candidateEntry)');
  e.run("loadExportableImageSource = async () => { throw new Error('missing image'); }");
  assert.equal(await e.run('previewFurnitureLibraryEntry({...candidateEntry, id:"missing"})'), false);
  assert.equal(e.run('furnitureSwap.candidate.libraryId'), 'test-bed');
  e.run('undo()');
  assert.equal(e.run('furnitureSwap'), null);
  assert.equal(e.run('state.history.length'), e.run('historyCount'));
  assert.equal(e.run('projectSaveKey()'), e.run('originalKey'));
});

test('duplicate and delete stay locked while a furniture trial is active', async () => {
  const e = furnitureSwapEditor();
  await e.run('previewFurnitureLibraryEntry(candidateEntry)');
  const count = e.run('state.items.length');
  e.nodes.get('duplicateItem').dispatch('click');
  e.nodes.get('deleteItem').dispatch('click');
  assert.equal(e.run('state.items.length'), count);
  assert.equal(e.run('state.items[0].dataUrl'), 'original');
});

test('pending furniture trials cannot silently enter a saved session or exported layout', async () => {
  const e = furnitureSwapEditor();
  e.run('let savedFiles = 0; let exportedFiles = 0; writeSessionFile = async () => { savedFiles++; }; downloadText = () => { exportedFiles++; };');
  await e.run('previewFurnitureLibraryEntry(candidateEntry)');
  await e.run('saveEditorSession()');
  e.nodes.get('exportJson').dispatch('click');
  assert.equal(e.run('savedFiles'), 0);
  assert.equal(e.run('exportedFiles'), 0);
  assert.equal(e.run('projectSaveKey()'), e.run('originalKey'));
  e.run("state.items[0].dataUrl = 'changed-image'; render()");
  assert.equal(e.run('furnitureSwap'), null);
});
