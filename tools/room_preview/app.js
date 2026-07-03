const SCREEN_W = 240;
const SCREEN_H = 135;

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
const gridToggle = document.getElementById("gridToggle");
const hudToggle = document.getElementById("hudToggle");
const roomStats = document.getElementById("roomStats");
const spriteStats = document.getElementById("spriteStats");

const state = {
  room: null,
  sprite: null,
  spriteFrame: null,
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

function getSpriteRect() {
  if (!state.sprite) return null;
  const x = Number(xInput.value) || 0;
  const y = Number(yInput.value) || 0;
  const w = state.sprite.naturalWidth;
  const h = state.sprite.naturalHeight;
  const drawX = centerToggle.checked ? Math.round(x - w / 2) : x;
  const drawY = centerToggle.checked ? Math.round(y - h) : y;
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
  roomStats.textContent = `${source.naturalWidth}x${source.naturalHeight} -> ${trim.width}x${trim.height} -> ${SCREEN_W}x${height}, y=${y}`;
}

function speciesLabel(species) {
  return `${String(species.id).padStart(3, "0")} ${species.cn} / ${species.en}`;
}

function populateSpecies() {
  const species = window.STICKMON_SPRITES?.species || [];
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
  return (window.STICKMON_SPRITES?.species || []).find((item) => item.slug === speciesSelect.value);
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
  const defaultAction = species.defaultFrame.path.match(/processed\/[^/]+\/([^/]+)\//)?.[1];
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
  const preferred = frames.find((frame) => frame.path === species.defaultFrame.path)
    || frames.find((frame) => frame.file === "front_0.png")
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

function render() {
  ctx.imageSmoothingEnabled = false;
  ctx.fillStyle = "#050612";
  ctx.fillRect(0, 0, SCREEN_W, SCREEN_H);

  if (state.room) {
    ctx.drawImage(state.room.canvas, 0, state.room.y);
  }

  if (hudToggle.checked) drawHudSafeArea();
  if (gridToggle.checked) drawGrid();

  if (state.sprite) {
    const x = Number(xInput.value) || 0;
    const y = Number(yInput.value) || 0;
    const drawX = centerToggle.checked ? Math.round(x - state.sprite.naturalWidth / 2) : x;
    const drawY = centerToggle.checked ? Math.round(y - state.sprite.naturalHeight) : y;
    ctx.drawImage(state.sprite, drawX, drawY);
    ctx.strokeStyle = "#ffffff";
    ctx.globalAlpha = 0.7;
    ctx.strokeRect(drawX + 0.5, drawY + 0.5, state.sprite.naturalWidth - 1, state.sprite.naturalHeight - 1);
    ctx.globalAlpha = 1;
    spriteStats.textContent = `${speciesLabel(currentSpecies())}, ${state.spriteFrame.file}, ${state.sprite.naturalWidth}x${state.sprite.naturalHeight}, at ${drawX},${drawY}`;
  } else {
    spriteStats.textContent = "-";
  }
}

async function refreshSpriteAndRender() {
  await loadSelectedSprite();
  render();
}

async function init() {
  populateSpecies();
  populateActions();
  populateFrames();
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
for (const input of [xInput, yInput, centerToggle, gridToggle, hudToggle]) {
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
