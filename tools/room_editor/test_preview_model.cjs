const assert = require("node:assert/strict");
const test = require("node:test");
const model = require("./preview_model.js");

test("AMOLED uses the room area rather than the whole portrait screen", () => {
  assert.deepEqual(model.viewport("amoled"), { width: 184, height: 148 });
  const p = model.profile("amoled");
  assert.equal(p.roomTop + p.roomHeight, 344);
  assert.equal(p.height - p.roomTop - p.roomHeight, 104);
});

test("device cameras clamp to the same shared room without mutating its geometry", () => {
  const room = Object.freeze({ width: 240, height: 161, y: 0 });
  assert.deepEqual(model.camera("amoled", room, { x: 999, y: 999 }), { x: 56, y: 13 });
  assert.deepEqual(model.camera("sticks3", room, { x: 999, y: 999 }), { x: 0, y: 26 });
  assert.deepEqual(model.camera("amoled", { width: 240, height: 80, y: 27 }, { x: -10, y: -10 }), { x: 0, y: 27 });
});

test("AMOLED follows its dead zone while StickS3 focuses 84 pixels below the top", () => {
  const room = { width: 400, height: 300, y: 0 };
  assert.deepEqual(model.camera("amoled", room, { x: 10, y: 20 }, { x: 100, y: 100 }), { x: 10, y: 20 });
  assert.deepEqual(model.camera("amoled", room, { x: 10, y: 20 }, { x: 180, y: 180 }), { x: 58, y: 54 });
  assert.deepEqual(model.camera("sticks3", room, {}, { x: 180, y: 180 }), { x: 0, y: 96 });
});

test("ground anchors account for frame padding and keep actor centers separate", () => {
  const item = { x: 20, y: 30, sourceWidth: 64, sourceHeight: 64, scale: 1, groundOffsetY: 24 };
  assert.deepEqual(model.ground(item), { x: 52, y: 86, actorY: 62 });
  assert.deepEqual(model.ground({ ...item, scaleX: 2, scaleY: 2 }), { x: 84, y: 142, actorY: 94 });
  assert.equal(model.ground({ ...item, groundOffsetY: undefined }).y, 88);
});

test("device geometry and camera settings can come from firmware with atomic validation", () => {
  try {
    model.configure({ sticks3: { camera: { focusY: 90 } },
      amoled: { width: 400, camera: { left: 60, right: 140, top: 50, bottom: 130 } } });
    assert.deepEqual(model.viewport("amoled"), { width: 200, height: 148 });
    const room = { width: 500, height: 300 };
    assert.deepEqual(model.camera("amoled", room, {}, { x: 220, y: 180 }), { x: 80, y: 50 });
    assert.equal(model.camera("sticks3", room, {}, { x: 220, y: 180 }).y, 90);
    assert.throws(() => model.configure({ amoled: { scale: 0 } }), /Invalid device/);
    assert.equal(model.viewport("amoled").width, 200);
  } finally {
    model.configure();
  }
});
