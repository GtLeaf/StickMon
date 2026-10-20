// Shared room coordinates stay independent of the selected display.
(function (root) {
  "use strict";
  const defaults = Object.freeze({
    sticks3: Object.freeze({ label: "StickS3", width: 240, height: 135, roomTop: 0, roomHeight: 135, scale: 1,
      camera: Object.freeze({ focusY: 84 }) }),
    amoled: Object.freeze({ label: "微雪 AMOLED 1.8 V1 / V2", width: 368, height: 448, roomTop: 48, roomHeight: 296, scale: 2,
      camera: Object.freeze({ left: 62, right: 122, top: 58, bottom: 126 }) })
  });
  let profiles = defaults;
  const clamp = (value, low, high) => Math.max(low, Math.min(high, value));
  function configure(values = {}) {
    const next = {};
    for (const id of Object.keys(defaults)) {
      const p = { ...defaults[id], ...values[id], camera: { ...defaults[id].camera, ...values[id]?.camera } };
      for (const key of ["width", "height", "roomHeight", "scale"]) {
        if (!Number.isFinite(p[key]) || p[key] <= 0) throw new Error(`Invalid device ${id}.${key}`);
      }
      if (!Number.isFinite(p.roomTop) || p.roomTop < 0 || p.roomTop + p.roomHeight > p.height) {
        throw new Error(`Invalid room area for ${id}`);
      }
      if (!Object.values(p.camera).every(Number.isFinite) ||
        (id === "amoled" && (p.camera.left > p.camera.right || p.camera.top > p.camera.bottom))) {
        throw new Error(`Invalid camera settings for ${id}`);
      }
      p.camera = Object.freeze(p.camera);
      next[id] = Object.freeze(p);
    }
    profiles = Object.freeze(next);
  }
  function profile(id) { return profiles[id] || profiles.sticks3; }
  function viewport(id) {
    const p = profile(id);
    return { width: p.width / p.scale, height: p.roomHeight / p.scale };
  }
  function camera(id, room, previous = {}, focus = null) {
    const v = viewport(id);
    const settings = profile(id).camera;
    const minimumY = id === "amoled" ? room.y || 0 : 0;
    const maximumY = Math.max(minimumY, (room.y || 0) + room.height - v.height);
    let x = Number(previous.x) || 0;
    let y = Number(previous.y) || 0;
    if (focus) {
      if (id === "amoled") {
        const sx = focus.x - x, sy = focus.y - y;
        if (sx < settings.left) x = focus.x - settings.left;
        else if (sx > settings.right) x = focus.x - settings.right;
        if (sy < settings.top) y = focus.y - settings.top;
        else if (sy > settings.bottom) y = focus.y - settings.bottom;
      } else {
        x = 0;
        y = focus.y - settings.focusY;
      }
    }
    return {
      x: clamp(Math.round(x), 0, Math.max(0, room.width - v.width)),
      y: clamp(Math.round(y), minimumY, maximumY)
    };
  }
  function ground(item) {
    const sx = Number(item.scaleX ?? item.scale ?? 1);
    const sy = Number(item.scaleY ?? item.scale ?? 1);
    const offset = Number.isFinite(item.groundOffsetY) ? item.groundOffsetY : clamp(Math.trunc(item.sourceHeight * 0.42), 16, 32);
    return {
      x: item.x + item.sourceWidth * sx / 2,
      y: item.y + (Math.floor(item.sourceHeight / 2) + offset) * sy,
      actorY: item.y + Math.floor(item.sourceHeight / 2) * sy
    };
  }
  const api = { get profiles() { return profiles; }, configure, profile, viewport, camera, ground };
  root.RoomPreviewModel = api;
  if (typeof module !== "undefined" && module.exports) module.exports = api;
})(typeof window !== "undefined" ? window : globalThis);
