const SCREEN_W = 240;
const SCREEN_H = 135;
const DEFAULT_ROOM = "../../origin_asset/room/standar/standar.png";
const DEFAULT_LAYOUT = "../../origin_asset/room/standar/room_layout.json";

const canvas = document.getElementById("screen");
const ctx = canvas.getContext("2d", { alpha: false });
ctx.imageSmoothingEnabled = false;

const roomInput = document.getElementById("roomInput");
const speciesSelect = document.getElementById("speciesSelect");
const actionSelect = document.getElementById("actionSelect");
const frameSelect = document.getElementById("frameSelect");
const xInput = document.getElementById("xInput");
const yInput = document.getElementById("yInput");
const centerToggle = document.getElementById("centerToggle");
const nightToggle = document.getElementById("nightToggle");
const walkToggle = document.getElementById("walkToggle");
const gridToggle = document.getElementById("gridToggle");
const hudToggle = document.getElementById("hudToggle");
const roomStats = document.getElementById("roomStats");
const spriteStats = document.getElementById("spriteStats");

const state = {
  room: null,
  roomLayout: null,
  walkPolygon: [],
  sprite: null,
  spriteFrame: null,
  cameraY: 0,
};

const drag = {
  active: false,
  startX: 0,
  startY: 0,
  spriteX: 0,
  spriteY: 0,
};

function loadImage(src) {
  return new Promise((resolve, reject) => {
    const img = new Image();
    img.onload = () => resolve(img);
    img.onerror = () => reject(new Error(`failed to load ${src}`));
    img.src = src;
  });
}

function getCanvasPoint(event) {
  const rect = canvas.getBoundingClientRect();
  const scaleX = SCREEN_W / rect.width;
  const scaleY = SCREEN_H / rect.height;
  return {
    x: (event.clientX - rect.left) * scaleX,
    y: (event.clientY - rect.top) * scaleY,
  };
}

function maxCameraY() {
  if (!state.room) return 0;
  return Math.max(0, state.room.y + state.room.height - SCREEN_H);
}

function cameraForWorldY(worldY) {
  return Math.max(0, Math.min(maxCameraY(), worldY - 84));
}

function updateCamera() {
  const y = Number(yInput.value) || 0;
  state.cameraY = cameraForWorldY(y);
}

function worldToScreenY(worldY) {
  return worldY - state.cameraY;
}

function getSpriteRect() {
  if (!state.sprite) return null;
  const x = Number(xInput.value) || 0;
  const y = Number(yInput.value) || 0;
  const w = state.sprite.naturalWidth;
  const h = state.sprite.naturalHeight;
  const drawX = centerToggle.checked ? Math.round(x - w / 2) : x;
  const screenY = worldToScreenY(y);
  const drawY = centerToggle.checked ? Math.round(screenY - h) : Math.round(screenY);
  return { x: drawX, y: drawY, w, h };
}

function isHit(pt) {
  const r = getSpriteRect();
  if (!r) return false;
  return pt.x >= r.x && pt.x < r.x + r.w && pt.y >= r.y && pt.y < r.y + r.h;
}

function colorDistance(a, b) {
  const dr = a[0] - b[0];
  const dg = a[1] - b[1];
  const db = a[2] - b[2];
  return Math.sqrt(dr * dr + dg * dg + db * db);
}

function detectTrimBox(imageData, threshold = 34, padding = 4) {
  const { width, height, data } = imageData;
  const samplePoints = [
    [0, 0],
    [width - 1, 0],
    [0, height - 1],
    [width - 1, height - 1],
    [Math.floor(width / 2), 0],
    [Math.floor(width / 2), height - 1],
  ];
  const bg = [0, 0, 0];
  for (const [x, y] of samplePoints) {
    const index = (y * width + x) * 4;
    bg[0] += data[index];
    bg[1] += data[index + 1];
    bg[2] += data[index + 2];
  }
  bg[0] /= samplePoints.length;
  bg[1] /= samplePoints.length;
  bg[2] /= samplePoints.length;

  let minX = width;
  let minY = height;
  let maxX = -1;
  let maxY = -1;
  for (let y = 0; y < height; y += 1) {
    for (let x = 0; x < width; x += 1) {
      const index = (y * width + x) * 4;
      if (colorDistance([data[index], data[index + 1], data[index + 2]], bg) <= threshold) {
        continue;
      }
      minX = Math.min(minX, x);
      minY = Math.min(minY, y);
      maxX = Math.max(maxX, x);
      maxY = Math.max(maxY, y);
    }
  }

  if (maxX < minX || maxY < minY) {
    return { x: 0, y: 0, width, height };
  }

  const x = Math.max(0, minX - padding);
  const y = Math.max(0, minY - padding);
  const right = Math.min(width, maxX + padding + 1);
  const bottom = Math.min(height, maxY + padding + 1);
  return { x, y, width: right - x, height: bottom - y };
}

async function buildRoomImage(src) {
  const source = await loadImage(src);
  const sourceCanvas = document.createElement("canvas");
  sourceCanvas.width = source.naturalWidth;
  sourceCanvas.height = source.naturalHeight;
  const sourceCtx = sourceCanvas.getContext("2d");
  sourceCtx.imageSmoothingEnabled = false;
  sourceCtx.drawImage(source, 0, 0);
  const trim = detectTrimBox(sourceCtx.getImageData(0, 0, sourceCanvas.width, sourceCanvas.height));
  const height = Math.max(1, Math.round(trim.height * SCREEN_W / trim.width));
  const y = Math.max(0, Math.floor((SCREEN_H - height) / 2));

  const roomCanvas = document.createElement("canvas");
  roomCanvas.width = SCREEN_W;
  roomCanvas.height = height;
  const roomCtx = roomCanvas.getContext("2d");
  roomCtx.imageSmoothingEnabled = false;
  roomCtx.drawImage(sourceCanvas, trim.x, trim.y, trim.width, trim.height, 0, 0, SCREEN_W, height);

  state.room = { canvas: roomCanvas, source, trim, width: SCREEN_W, height, y };
  updateWalkPolygon();
  updateRoomStats();
}

async function loadRoomLayout(src) {
  const response = await fetch(src);
  if (!response.ok) throw new Error(`failed to load ${src}`);
  state.roomLayout = await response.json();
  updateWalkPolygon();
  updateRoomStats();
}

function updateWalkPolygon() {
  const faces = state.roomLayout?.roomGeometry?.faces || [];
  const floor = faces.find((face) => face.type === "floor");
  if (!floor) {
    state.walkPolygon = [];
    return;
  }

  if (state.room && Array.isArray(floor.sourcePoints) && floor.sourcePoints.length > 0) {
    const scale = SCREEN_W / state.room.trim.width;
    state.walkPolygon = floor.sourcePoints.map(([x, y]) => ({
      x: Math.round((x - state.room.trim.x) * scale),
      y: Math.round((y - state.room.trim.y) * scale + state.room.y),
    }));
    return;
  }

  state.walkPolygon = (floor.points || []).map(([x, y]) => ({ x, y }));
}

function updateRoomStats() {
  if (!state.room) {
    roomStats.textContent = "-";
    return;
  }
  const r = state.room;
  roomStats.textContent = `${r.source.naturalWidth}x${r.source.naturalHeight} -> ${r.trim.width}x${r.trim.height} -> ${SCREEN_W}x${r.height}, y=${r.y}, camera=0-${Math.round(maxCameraY())}; walk=${state.walkPolygon.length}`;
}

function projectSprites() {
  if (Array.isArray(window.STICKMON_PROJECT_SPRITES)) return window.STICKMON_PROJECT_SPRITES;
  return window.STICKMON_SPRITES?.species || [];
}

function speciesId(species) {
  return species.id ?? species.speciesId ?? 0;
}

function speciesLabel(species) {
  if (!species) return "-";
  const id = String(speciesId(species)).padStart(3, "0");
  if (species.cn && species.en) return `${id} ${species.cn} / ${species.en}`;
  return species.name || `${id} ${species.slug || "unknown"}`;
}

function populateSpecies() {
  const species = projectSprites();
  speciesSelect.replaceChildren();
  for (const item of species) {
    const option = document.createElement("option");
    option.value = item.slug;
    option.textContent = speciesLabel(item);
    speciesSelect.append(option);
  }
  speciesSelect.value = species.find((item) => item.slug === "eevee")?.slug || species[0]?.slug || "";
}

function currentSpecies() {
  return projectSprites().find((item) => item.slug === speciesSelect.value);
}

function populateActions() {
  const species = currentSpecies();
  actionSelect.replaceChildren();
  if (!species) return;
  for (const action of Object.keys(species.actions)) {
    const option = document.createElement("option");
    option.value = action;
    option.textContent = action;
    actionSelect.append(option);
  }
  const defaultPath = species.defaultFrame?.path || species.path || "";
  const defaultAction = defaultPath.match(/processed\/[^/]+\/([^/]+)\//)?.[1];
  if (defaultAction && species.actions[defaultAction]) {
    actionSelect.value = defaultAction;
  } else if (species.actions.idle) {
    actionSelect.value = "idle";
  }
}

function populateFrames() {
  const species = currentSpecies();
  frameSelect.replaceChildren();
  if (!species) return;
  const frames = species.actions[actionSelect.value] || [];
  for (const frame of frames) {
    const option = document.createElement("option");
    option.value = frame.path;
    option.textContent = `${frame.file} (${frame.width}x${frame.height})`;
    frameSelect.append(option);
  }
  const defaultPath = species.defaultFrame?.path || species.path || "";
  const preferred = frames.find((frame) => frame.path === defaultPath)
    || frames.find((frame) => frame.file === "front_0.png")
    || frames.find((frame) => frame.kindName?.endsWith("_FRONT_0"))
    || frames[0];
  if (preferred) frameSelect.value = preferred.path;
}

async function loadSelectedSprite() {
  const species = currentSpecies();
  const frames = species?.actions[actionSelect.value] || [];
  const frame = frames.find((item) => item.path === frameSelect.value) || frames[0];
  if (!frame) {
    state.sprite = null;
    state.spriteFrame = null;
    return;
  }
  state.sprite = await loadImage(frame.path);
  state.spriteFrame = frame;
}

function drawGrid() {
  ctx.save();
  ctx.globalAlpha = 0.28;
  ctx.strokeStyle = "#ffffff";
  ctx.lineWidth = 1;
  for (let x = 0; x <= SCREEN_W; x += 8) {
    ctx.beginPath();
    ctx.moveTo(x + 0.5, 0);
    ctx.lineTo(x + 0.5, SCREEN_H);
    ctx.stroke();
  }
  for (let y = 0; y <= SCREEN_H; y += 8) {
    ctx.beginPath();
    ctx.moveTo(0, y + 0.5);
    ctx.lineTo(SCREEN_W, y + 0.5);
    ctx.stroke();
  }
  ctx.restore();
}

function drawHudSafeArea() {
  ctx.save();
  ctx.globalAlpha = 0.18;
  ctx.fillStyle = "#72b7ff";
  ctx.fillRect(190, 0, 50, 34);
  ctx.strokeStyle = "#72b7ff";
  ctx.strokeRect(190.5, 0.5, 49, 33);
  ctx.restore();
}

function drawWalkArea() {
  if (state.walkPolygon.length < 3) return;
  ctx.save();
  ctx.beginPath();
  ctx.moveTo(state.walkPolygon[0].x, worldToScreenY(state.walkPolygon[0].y));
  for (const point of state.walkPolygon.slice(1)) {
    ctx.lineTo(point.x, worldToScreenY(point.y));
  }
  ctx.closePath();
  ctx.globalAlpha = 0.16;
  ctx.fillStyle = "#76f2a5";
  ctx.fill();
  ctx.globalAlpha = 0.75;
  ctx.strokeStyle = "#76f2a5";
  ctx.lineWidth = 1;
  ctx.stroke();
  ctx.restore();
}

function drawRadialLight(x, y, rx, ry, color, alpha) {
  ctx.save();
  ctx.translate(x, y);
  ctx.scale(rx, ry);
  const g = ctx.createRadialGradient(0, 0, 0, 0, 0, 1);
  g.addColorStop(0, color.replace("ALPHA", alpha.toFixed(3)));
  g.addColorStop(0.62, color.replace("ALPHA", (alpha * 0.22).toFixed(3)));
  g.addColorStop(1, color.replace("ALPHA", "0"));
  ctx.globalCompositeOperation = "screen";
  ctx.fillStyle = g;
  ctx.beginPath();
  ctx.arc(0, 0, 1, 0, Math.PI * 2);
  ctx.fill();
  ctx.restore();
}

function clamp(value, min, max) {
  return Math.max(min, Math.min(max, value));
}

function isFloatingSpecies(species) {
  const id = speciesId(species);
  return id === 92 || id === 93 || id === 151 || id === 380 || id === 381;
}

function drawShadowEllipse(x, y, rx, ry, rgb, alpha, soft) {
  if (rx <= 0 || ry <= 0 || alpha <= 0) return;
  const [r, g, b] = rgb;
  ctx.save();
  ctx.translate(x, y);
  ctx.scale(rx, ry);
  ctx.globalCompositeOperation = "source-over";
  if (soft) {
    const gradient = ctx.createRadialGradient(0, 0, 0, 0, 0, 1);
    gradient.addColorStop(0, `rgba(${r}, ${g}, ${b}, ${alpha.toFixed(3)})`);
    gradient.addColorStop(0.68, `rgba(${r}, ${g}, ${b}, ${(alpha * 0.28).toFixed(3)})`);
    gradient.addColorStop(1, `rgba(${r}, ${g}, ${b}, 0)`);
    ctx.fillStyle = gradient;
  } else {
    ctx.fillStyle = `rgba(${r}, ${g}, ${b}, ${alpha.toFixed(3)})`;
  }
  ctx.beginPath();
  ctx.arc(0, 0, 1, 0, Math.PI * 2);
  ctx.fill();
  ctx.restore();
}

function drawSpriteShadow() {
  if (!state.sprite) return;
  const species = currentSpecies();
  const floating = isFloatingSpecies(species);
  const night = nightToggle.checked;
  const w = state.sprite.naturalWidth;
  const h = state.sprite.naturalHeight;
  const inputX = Number(xInput.value) || 0;
  const inputY = Number(yInput.value) || 0;
  const shadowX = centerToggle.checked ? inputX : inputX + w / 2;
  const shadowY = centerToggle.checked ? worldToScreenY(inputY) : worldToScreenY(inputY) + h;

  let rx = clamp(Math.round(w * (floating ? 0.25 : 0.44)), floating ? 10 : 16, floating ? 22 : 40);
  let ry = clamp(Math.round(h * (floating ? 0.055 : 0.14)), floating ? 3 : 6, floating ? 6 : 14);
  if (night) {
    rx = Math.round(rx * 1.1);
    ry = Math.round(ry * 1.1);
  }

  const color = night ? [18, 16, 24] : [36, 29, 24];
  const outerAlpha = night ? (floating ? 0.33 : 0.48) : (floating ? 0.27 : 0.45);
  const coreAlpha = night ? (floating ? 0 : 0.36) : (floating ? 0 : 0.34);
  drawShadowEllipse(shadowX, shadowY, rx, ry, color, outerAlpha, true);
  if (!floating) {
    drawShadowEllipse(shadowX, shadowY, Math.max(5, Math.round(rx / 2)), Math.max(2, Math.round(ry / 2)),
      color, coreAlpha, false);
  }
}

function drawTopDownLight(height, color, alpha) {
  const g = ctx.createLinearGradient(0, 0, 0, height);
  g.addColorStop(0, color.replace("ALPHA", alpha.toFixed(3)));
  g.addColorStop(0.48, color.replace("ALPHA", (alpha * 0.32).toFixed(3)));
  g.addColorStop(1, color.replace("ALPHA", "0"));
  ctx.save();
  ctx.globalCompositeOperation = "screen";
  ctx.fillStyle = g;
  ctx.fillRect(0, 0, SCREEN_W, height);
  ctx.restore();
}

function drawNightOverlay() {
  if (!nightToggle.checked) return;
  ctx.save();
  ctx.globalCompositeOperation = "source-over";
  ctx.fillStyle = "rgba(8, 18, 42, 0.36)";
  ctx.fillRect(0, 0, SCREEN_W, SCREEN_H);
  ctx.fillStyle = "rgba(0, 0, 0, 0.19)";
  ctx.fillRect(0, 0, SCREEN_W, SCREEN_H);
  ctx.restore();

  drawTopDownLight(86, "rgba(114, 150, 214, ALPHA)", 0.16);

  const x = Number(xInput.value) || 0;
  const y = Number(yInput.value) || 0;
  const glowY = worldToScreenY(y) - 10;
  drawRadialLight(x, glowY, 42, 34, "rgba(255, 210, 128, ALPHA)", 0.32);
  drawRadialLight(x, glowY, 76, 50, "rgba(255, 151, 92, ALPHA)", 0.10);
}

function render() {
  updateCamera();
  ctx.imageSmoothingEnabled = false;
  ctx.fillStyle = "#050612";
  ctx.fillRect(0, 0, SCREEN_W, SCREEN_H);

  if (state.room) {
    ctx.drawImage(state.room.canvas, 0, state.room.y - state.cameraY);
  }

  if (state.sprite) {
    drawSpriteShadow();
    const x = Number(xInput.value) || 0;
    const y = Number(yInput.value) || 0;
    const screenY = worldToScreenY(y);
    const drawX = centerToggle.checked ? Math.round(x - state.sprite.naturalWidth / 2) : x;
    const drawY = centerToggle.checked ? Math.round(screenY - state.sprite.naturalHeight) : Math.round(screenY);
    ctx.drawImage(state.sprite, drawX, drawY);
    ctx.strokeStyle = "#ffffff";
    ctx.globalAlpha = 0.7;
    ctx.strokeRect(drawX + 0.5, drawY + 0.5, state.sprite.naturalWidth - 1, state.sprite.naturalHeight - 1);
    ctx.globalAlpha = 1;
    spriteStats.textContent = `${speciesLabel(currentSpecies())}, ${state.spriteFrame.file}, ${state.sprite.naturalWidth}x${state.sprite.naturalHeight}, world ${Math.round(x)},${Math.round(y)}, screen ${drawX},${drawY}, cameraY=${Math.round(state.cameraY)}`;
  } else {
    spriteStats.textContent = "-";
  }

  drawNightOverlay();
  if (walkToggle.checked) drawWalkArea();
  if (hudToggle.checked) drawHudSafeArea();
  if (gridToggle.checked) drawGrid();
}

async function refreshSpriteAndRender() {
  await loadSelectedSprite();
  render();
}

async function init() {
  populateSpecies();
  populateActions();
  populateFrames();
  await buildRoomImage(DEFAULT_ROOM);
  await loadRoomLayout(DEFAULT_LAYOUT);
  await refreshSpriteAndRender();
}

speciesSelect.addEventListener("change", async () => {
  populateActions();
  populateFrames();
  await refreshSpriteAndRender();
});
actionSelect.addEventListener("change", async () => {
  populateFrames();
  await refreshSpriteAndRender();
});
frameSelect.addEventListener("change", refreshSpriteAndRender);
for (const input of [xInput, yInput, centerToggle, nightToggle, walkToggle, gridToggle, hudToggle]) {
  input.addEventListener("input", render);
  input.addEventListener("change", render);
}

roomInput.addEventListener("change", async () => {
  const file = roomInput.files[0];
  if (!file) return;
  const url = URL.createObjectURL(file);
  try {
    await buildRoomImage(url);
    render();
  } catch (error) {
    roomStats.textContent = error.message;
    console.error(error);
  } finally {
    URL.revokeObjectURL(url);
  }
});

canvas.addEventListener("mousedown", (e) => {
  const pt = getCanvasPoint(e);
  if (!isHit(pt)) return;
  drag.active = true;
  drag.startX = pt.x;
  drag.startY = pt.y;
  drag.spriteX = Number(xInput.value) || 0;
  drag.spriteY = Number(yInput.value) || 0;
  canvas.style.cursor = "grabbing";
  e.preventDefault();
});

window.addEventListener("mousemove", (e) => {
  if (!drag.active) return;
  const pt = getCanvasPoint(e);
  const dx = pt.x - drag.startX;
  const dy = pt.y - drag.startY;
  xInput.value = Math.round(drag.spriteX + dx);
  yInput.value = Math.round(drag.spriteY + dy);
  render();
});

window.addEventListener("mouseup", () => {
  if (drag.active) {
    drag.active = false;
    canvas.style.cursor = "";
  }
});

canvas.addEventListener("mousemove", (e) => {
  if (drag.active) return;
  const pt = getCanvasPoint(e);
  canvas.style.cursor = isHit(pt) ? "grab" : "";
});

init().catch((error) => {
  roomStats.textContent = error.message;
  console.error(error);
});
