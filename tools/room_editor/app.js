const state = {
  width: 240,
  height: 135,
  editWidth: 240,
  editHeight: 135,
  mode: "day",
  editMode: "furniture",
  previewZoomed: false,
  base: null,
  baseFit: "cover",
  baseOpacity: 0.65,
  sourceScale: 4,
  furnitureImportMode: "source_scale",
  aspectLocked: true,
  aspectRatio: 240 / 135,
  items: [],
  selectedId: null,
  nextId: 1,
  points: [],
  faces: [],
  selectedFaceId: null,
  selectedPointIndex: null,
  selectedPointId: null,
  nextFaceId: 1,
  nextPointId: 1,
  draftPoints: [],
  drawingFace: false,
  draggingPoint: false,
  dragPointId: null,
  dragging: false,
  dragOffsetX: 0,
  dragOffsetY: 0,
  guides: {
    show: true,
    spriteX: 88,
    spriteY: 50,
    spriteW: 64,
    spriteH: 64,
    hudX: 168,
    hudY: 2,
    hudW: 68,
    hudH: 54
  },
  night: {
    tint: 46,
    darken: 34,
    lamp: 36
  }
};

const canvas = document.getElementById("stage");
const ctx = canvas.getContext("2d");
const previewCanvas = document.getElementById("preview");
const previewCtx = previewCanvas.getContext("2d");
const statusEl = document.getElementById("status");
const assetList = document.getElementById("assetList");
const selectedPanel = document.getElementById("selectedPanel");
const faceList = document.getElementById("faceList");
const selectedFacePanel = document.getElementById("selectedFacePanel");
const baseImageMeta = document.getElementById("baseImageMeta");

function setStatus(text) {
  statusEl.textContent = text;
}

function updateBaseImageMeta() {
  if (!state.base) {
    baseImageMeta.textContent = "Reference image: none";
    return;
  }
  baseImageMeta.textContent = `Reference image: ${state.base.width}x${state.base.height}; game preview: ${state.width}x${state.height}`;
}

function updateCanvasDisplaySize() {
  canvas.style.width = `${state.editWidth}px`;
  canvas.style.height = "auto";
  canvas.style.aspectRatio = `${state.editWidth} / ${state.editHeight}`;
  previewCanvas.style.width = `${Math.min(240, state.width)}px`;
  previewCanvas.style.height = "auto";
  previewCanvas.style.aspectRatio = `${state.width} / ${state.height}`;
}

function editScaleX() {
  return state.editWidth / Math.max(1, state.width);
}

function editScaleY() {
  return state.editHeight / Math.max(1, state.height);
}

function targetToEditPoint(point) {
  return {
    x: point.x * editScaleX(),
    y: point.y * editScaleY()
  };
}

function editToTargetPoint(point) {
  return {
    x: point.x / editScaleX(),
    y: point.y / editScaleY()
  };
}

function faceTargetPoints(face) {
  return face.points.map((point) => {
    const target = editToTargetPoint(point);
    return { x: Math.round(target.x), y: Math.round(target.y) };
  });
}

function createShapePoint(x, y) {
  const point = {
    id: `p${state.nextPointId++}`,
    x: Math.round(x),
    y: Math.round(y)
  };
  state.points.push(point);
  return point;
}

function selectedPoint() {
  return state.points.find((point) => point.id === state.selectedPointId) || null;
}

function findPointMembership(point) {
  if (!point) return null;
  for (let faceIndex = state.faces.length - 1; faceIndex >= 0; --faceIndex) {
    const face = state.faces[faceIndex];
    const pointIndex = face.points.findIndex((entry) => entry.id === point.id);
    if (pointIndex >= 0) return { face, pointIndex };
  }
  return null;
}

function setSelectedPoint(point, membership = null) {
  state.selectedPointId = point ? point.id : null;
  if (membership) {
    state.selectedFaceId = membership.face.id;
    state.selectedPointIndex = membership.pointIndex;
    return;
  }
  state.selectedFaceId = null;
  state.selectedPointIndex = null;
}

function selectPointInFace(face, index) {
  if (!face || index < 0 || index >= face.points.length) {
    setSelectedPoint(null);
    return;
  }
  setSelectedPoint(face.points[index], { face, pointIndex: index });
}

function selectDraftPoint(index) {
  if (index < 0 || index >= state.draftPoints.length) {
    setSelectedPoint(null);
    return;
  }
  state.selectedFaceId = null;
  state.selectedPointIndex = index;
  state.selectedPointId = state.draftPoints[index].id;
}

function isPointReferenced(point) {
  if (!point) return false;
  if (state.draftPoints.some((entry) => entry.id === point.id)) return true;
  return state.faces.some((face) => face.points.some((entry) => entry.id === point.id));
}

function pruneUnusedPoints() {
  state.points = state.points.filter((point) => isPointReferenced(point));
  if (state.selectedPointId && !state.points.some((point) => point.id === state.selectedPointId)) {
    state.selectedPointId = null;
    state.selectedPointIndex = null;
  }
}

function clearDraftPoints() {
  state.draftPoints = [];
  state.drawingFace = false;
  if (!state.selectedFaceId) {
    state.selectedPointIndex = null;
    state.selectedPointId = null;
  }
  pruneUnusedPoints();
}

function addDraftPoint(point) {
  if (!point) return;
  const last = state.draftPoints[state.draftPoints.length - 1];
  if (last && last.id === point.id) return;
  state.draftPoints.push(point);
  selectDraftPoint(state.draftPoints.length - 1);
}

function targetRectToEdit(x, y, w, h) {
  const p = targetToEditPoint({ x, y });
  return {
    x: p.x,
    y: p.y,
    w: w * editScaleX(),
    h: h * editScaleY()
  };
}

function editHandleRadius() {
  return Math.max(5, Math.min(14, Math.max(editScaleX(), editScaleY()) * 3.5));
}

function setRoomInputs(width, height) {
  document.getElementById("canvasW").value = Math.round(width);
  document.getElementById("canvasH").value = Math.round(height);
}

function applyRoomSizeInput(changedField) {
  const widthInput = document.getElementById("canvasW");
  const heightInput = document.getElementById("canvasH");
  let width = Math.max(16, Math.min(2048, Number(widthInput.value) || state.width));
  let height = Math.max(16, Math.min(2048, Number(heightInput.value) || state.height));

  state.aspectLocked = document.getElementById("aspectLock").checked;
  if (state.aspectLocked) {
    const ratio = state.aspectRatio || width / Math.max(1, height);
    if (changedField === "width") {
      height = Math.max(16, Math.min(2048, Math.round(width / ratio)));
    } else if (changedField === "height") {
      width = Math.max(16, Math.min(2048, Math.round(height * ratio)));
    }
  } else {
    state.aspectRatio = width / Math.max(1, height);
  }

  state.width = width;
  state.height = height;
  if (!state.base) {
    state.editWidth = width;
    state.editHeight = height;
  }
  setRoomInputs(width, height);
  updateBaseImageMeta();
  render();
}

function autoSourceScaleForImage(width, height) {
  const scaleX = width / Math.max(1, state.width);
  const scaleY = height / Math.max(1, state.height);
  if (scaleX <= 1 && scaleY <= 1) return 1;
  const closeAspect = Math.abs(scaleX - scaleY) / Math.max(scaleX, scaleY) < 0.08;
  const scale = closeAspect ? (scaleX + scaleY) / 2 : Math.min(scaleX, scaleY);
  return Math.max(1, Math.round(scale * 100) / 100);
}

function furnitureInitialScale(width, height) {
  if (state.furnitureImportMode === "native") return 1;
  if (state.furnitureImportMode === "fit_canvas") {
    return Math.min(1, state.width / Math.max(1, width), state.height / Math.max(1, height));
  }
  return 1 / Math.max(0.01, state.sourceScale);
}

function readImageFile(file) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onerror = () => reject(reader.error);
    reader.onload = () => {
      const img = new Image();
      img.onload = () => resolve({ file, img, dataUrl: reader.result });
      img.onerror = reject;
      img.src = reader.result;
    };
    reader.readAsDataURL(file);
  });
}

function loadImageSource(src) {
  return new Promise((resolve, reject) => {
    const img = new Image();
    img.onload = () => resolve(img);
    img.onerror = reject;
    img.src = src;
  });
}

function projectSpriteCatalog() {
  return Array.isArray(window.STICKMON_PROJECT_SPRITES) ? window.STICKMON_PROJECT_SPRITES : [];
}

function selectedProjectSpriteEntry() {
  const select = document.getElementById("projectSpriteSelect");
  const speciesId = Number(select.value) || 0;
  return projectSpriteCatalog().find((entry) => entry.speciesId === speciesId) || null;
}

function updateProjectSpriteMeta() {
  const meta = document.getElementById("projectSpriteMeta");
  const entry = selectedProjectSpriteEntry();
  if (!entry) {
    meta.textContent = "No generated project sprites found.";
    return;
  }
  meta.textContent = `${entry.frame}, ${entry.width}x${entry.height}`;
}

function initProjectSpriteControls() {
  const select = document.getElementById("projectSpriteSelect");
  const addButton = document.getElementById("addProjectSprite");
  const catalog = projectSpriteCatalog();
  select.innerHTML = "";
  for (const entry of catalog) {
    const option = document.createElement("option");
    option.value = String(entry.speciesId);
    option.textContent = entry.name;
    select.appendChild(option);
  }
  const empty = catalog.length === 0;
  select.disabled = empty;
  addButton.disabled = empty;
  select.addEventListener("input", updateProjectSpriteMeta);
  addButton.addEventListener("click", addProjectSpritePreview);
  updateProjectSpriteMeta();
}

async function addProjectSpritePreview() {
  const entry = selectedProjectSpriteEntry();
  if (!entry) return;
  const img = await loadImageSource(entry.path);
  const guideW = Math.max(1, Number(state.guides.spriteW) || entry.width);
  const guideH = Math.max(1, Number(state.guides.spriteH) || entry.height);
  const scale = Math.min(guideW / Math.max(1, entry.width), guideH / Math.max(1, entry.height));
  const targetW = entry.width * scale;
  const targetH = entry.height * scale;
  const item = {
    id: `f${state.nextId++}`,
    name: entry.name,
    fileName: entry.path,
    img,
    dataUrl: entry.path,
    sourceWidth: entry.width,
    sourceHeight: entry.height,
    x: Math.round(state.guides.spriteX + (guideW - targetW) / 2),
    y: Math.round(state.guides.spriteY + (guideH - targetH) / 2),
    scale,
    sourceScale: 1,
    opacity: 1,
    z: 30,
    visible: true,
    anchor: "top_left",
    slot: "sprite_preview",
    layer: "sprite_preview",
    source: "project_sprite",
    speciesId: entry.speciesId,
    frame: entry.frame
  };
  state.items.push(item);
  state.selectedId = item.id;
  state.selectedFaceId = null;
  state.selectedPointIndex = null;
  state.selectedPointId = null;
  state.editMode = "furniture";
  updateEditModeButtons();
  setStatus(`Added sprite preview: ${entry.name}.`);
  refreshList();
  refreshSelectedPanel();
  refreshFaceList();
  refreshSelectedFacePanel();
  render();
}

async function loadBase(file) {
  const loaded = await readImageFile(file);
  state.base = {
    name: file.name,
    img: loaded.img,
    dataUrl: loaded.dataUrl,
    width: loaded.img.naturalWidth,
    height: loaded.img.naturalHeight
  };
  const detectedScale = autoSourceScaleForImage(state.base.width, state.base.height);
  const targetWidth = Math.max(16, Math.round(state.base.width / detectedScale));
  const targetHeight = Math.max(16, Math.round(state.base.height / detectedScale));
  state.sourceScale = detectedScale;
  state.width = targetWidth;
  state.height = targetHeight;
  state.editWidth = state.base.width;
  state.editHeight = state.base.height;
  state.aspectRatio = state.base.width / Math.max(1, state.base.height);
  document.getElementById("sourceScale").value = detectedScale;
  setRoomInputs(targetWidth, targetHeight);
  updateBaseImageMeta();
  setStatus(`Reference loaded: ${file.name} (${state.base.width}x${state.base.height}), sampled to ${state.width}x${state.height}, source scale ${detectedScale}x.`);
  render();
}

async function addFurnitureFiles(files) {
  for (const file of files) {
    const loaded = await readImageFile(file);
    const initialScale = furnitureInitialScale(loaded.img.naturalWidth, loaded.img.naturalHeight);
    const targetW = loaded.img.naturalWidth * initialScale;
    const targetH = loaded.img.naturalHeight * initialScale;
    const item = {
      id: `f${state.nextId++}`,
      name: file.name.replace(/\.[^.]+$/, ""),
      fileName: file.name,
      img: loaded.img,
      dataUrl: loaded.dataUrl,
      sourceWidth: loaded.img.naturalWidth,
      sourceHeight: loaded.img.naturalHeight,
      x: Math.round((state.width - Math.min(targetW, state.width)) / 2),
      y: Math.round((state.height - Math.min(targetH, state.height)) / 2),
      scale: initialScale,
      sourceScale: state.sourceScale,
      opacity: 1,
      z: 20,
      visible: true,
      anchor: "top_left",
      slot: "floor",
      layer: "main"
    };
    state.items.push(item);
    state.selectedId = item.id;
  }
  setStatus(`Loaded ${files.length} furniture file(s), sampled with ${state.furnitureImportMode.replace("_", " ")}.`);
  refreshList();
  refreshSelectedPanel();
  render();
}

function selectedItem() {
  return state.items.find((item) => item.id === state.selectedId) || null;
}

function sortedItems() {
  return [...state.items].sort((a, b) => {
    if (a.z !== b.z) return a.z - b.z;
    return a.id.localeCompare(b.id);
  });
}

function itemBounds(item) {
  return {
    x: item.x,
    y: item.y,
    w: item.sourceWidth * item.scale,
    h: item.sourceHeight * item.scale
  };
}

function drawBase() {
  ctx.clearRect(0, 0, state.editWidth, state.editHeight);
  ctx.fillStyle = "#111318";
  ctx.fillRect(0, 0, state.editWidth, state.editHeight);

  if (!state.base) {
    ctx.fillStyle = "#202734";
    ctx.fillRect(0, 0, state.editWidth, state.editHeight);
    ctx.strokeStyle = "#526070";
    ctx.strokeRect(0.5, 0.5, state.editWidth - 1, state.editHeight - 1);
    ctx.fillStyle = "#a8b0bf";
    ctx.font = "10px sans-serif";
    ctx.fillText("Load reference image", 12, 24);
    return;
  }

  const img = state.base.img;
  ctx.imageSmoothingEnabled = false;
  ctx.globalAlpha = state.baseOpacity;
  ctx.drawImage(img, 0, 0, state.editWidth, state.editHeight);
  ctx.globalAlpha = 1;
}

function drawItems() {
  for (const item of sortedItems()) {
    if (!item.visible) continue;
    const b = itemBounds(item);
    const editBounds = targetRectToEdit(item.x, item.y, b.w, b.h);
    ctx.globalAlpha = item.opacity;
    ctx.imageSmoothingEnabled = false;
    ctx.drawImage(item.img, editBounds.x, editBounds.y, editBounds.w, editBounds.h);
  }
  ctx.globalAlpha = 1;
}

function faceFillColor(type, alpha = 0.35) {
  return type === "wall" ? `rgba(120, 214, 238, ${alpha})` : `rgba(242, 189, 114, ${alpha})`;
}

function faceStrokeColor(type) {
  return type === "wall" ? "#78d6ee" : "#f2bd72";
}

function drawPolygonPath(targetCtx, points) {
  if (!points.length) return;
  targetCtx.beginPath();
  targetCtx.moveTo(points[0].x, points[0].y);
  for (let i = 1; i < points.length; i++) {
    targetCtx.lineTo(points[i].x, points[i].y);
  }
  targetCtx.closePath();
}

function drawPointHandle(point, selected) {
  const radius = editHandleRadius();
  ctx.beginPath();
  ctx.fillStyle = selected ? "#ffffff" : "#111318";
  ctx.strokeStyle = selected ? "#ffffff" : "#78d6ee";
  ctx.lineWidth = selected ? 2 : 1;
  ctx.arc(point.x, point.y, radius, 0, Math.PI * 2);
  ctx.fill();
  ctx.stroke();
}

function drawFacesOverlay() {
  ctx.save();
  for (const face of state.faces) {
    if (face.points.length < 3) continue;
    drawPolygonPath(ctx, face.points);
    ctx.fillStyle = faceFillColor(face.type, face.id === state.selectedFaceId ? 0.42 : 0.24);
    ctx.fill();
    ctx.strokeStyle = face.id === state.selectedFaceId ? "#ffffff" : faceStrokeColor(face.type);
    ctx.lineWidth = face.id === state.selectedFaceId ? 2 : 1;
    ctx.stroke();
  }

  if (state.draftPoints.length) {
    ctx.strokeStyle = "#ffffff";
    ctx.fillStyle = "#ffffff";
    ctx.lineWidth = 1;
    ctx.setLineDash([3, 2]);
    ctx.beginPath();
    ctx.moveTo(state.draftPoints[0].x, state.draftPoints[0].y);
    for (let i = 1; i < state.draftPoints.length; i++) {
      ctx.lineTo(state.draftPoints[i].x, state.draftPoints[i].y);
    }
    ctx.stroke();
    ctx.setLineDash([]);
    const first = state.draftPoints[0];
    ctx.strokeStyle = "#78d6ee";
    ctx.strokeRect(first.x - 4, first.y - 4, 8, 8);
  }
  for (const point of state.points) {
    drawPointHandle(point, point.id === state.selectedPointId);
  }
  ctx.restore();
}

function renderRoomGeometry(targetCtx, width, height) {
  targetCtx.clearRect(0, 0, width, height);
  targetCtx.fillStyle = "#05070c";
  targetCtx.fillRect(0, 0, width, height);
  for (const face of state.faces) {
    if (face.points.length < 3) continue;
    drawPolygonPath(targetCtx, faceTargetPoints(face));
    targetCtx.fillStyle = face.type === "wall" ? "#b6916c" : "#d99752";
    targetCtx.fill();
    targetCtx.strokeStyle = "#4e352a";
    targetCtx.lineWidth = 1;
    targetCtx.stroke();
  }
}

function renderPreview() {
  if (previewCanvas.width !== state.width || previewCanvas.height !== state.height) {
    previewCanvas.width = state.width;
    previewCanvas.height = state.height;
  }
  renderRoomGeometry(previewCtx, state.width, state.height);
  previewCtx.save();
  previewCtx.globalAlpha = 0.85;
  for (const item of sortedItems()) {
    if (!item.visible) continue;
    const b = itemBounds(item);
    previewCtx.imageSmoothingEnabled = false;
    previewCtx.drawImage(item.img, item.x, item.y, b.w, b.h);
  }
  previewCtx.restore();

  if (state.mode === "night") {
    previewCtx.fillStyle = `rgba(8, 18, 42, ${state.night.tint / 100})`;
    previewCtx.fillRect(0, 0, state.width, state.height);
    previewCtx.fillStyle = `rgba(0, 0, 0, ${state.night.darken / 100})`;
    previewCtx.fillRect(0, 0, state.width, state.height);
  }
}

function drawNightOverlay() {
  if (state.mode !== "night") return;

  ctx.save();
  ctx.globalCompositeOperation = "source-over";
  ctx.fillStyle = `rgba(8, 18, 42, ${state.night.tint / 100})`;
  ctx.fillRect(0, 0, state.editWidth, state.editHeight);
  ctx.fillStyle = `rgba(0, 0, 0, ${state.night.darken / 100})`;
  ctx.fillRect(0, 0, state.editWidth, state.editHeight);

  const lampAlpha = state.night.lamp / 100;
  if (lampAlpha > 0) {
    const lamp = targetToEditPoint({ x: 170, y: 70 });
    const radius = 68 * Math.max(editScaleX(), editScaleY());
    const g = ctx.createRadialGradient(lamp.x, lamp.y, 4, lamp.x, lamp.y, radius);
    g.addColorStop(0, `rgba(255, 206, 120, ${lampAlpha})`);
    g.addColorStop(0.45, `rgba(255, 173, 80, ${lampAlpha * 0.22})`);
    g.addColorStop(1, "rgba(255, 173, 80, 0)");
    ctx.globalCompositeOperation = "screen";
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, state.editWidth, state.editHeight);
  }

  ctx.restore();
}

function drawGuides() {
  if (!state.guides.show) return;
  ctx.save();
  ctx.lineWidth = 1;
  ctx.setLineDash([3, 2]);

  ctx.strokeStyle = "rgba(120, 214, 238, 0.9)";
  const spriteGuide = targetRectToEdit(
    state.guides.spriteX,
    state.guides.spriteY,
    state.guides.spriteW,
    state.guides.spriteH
  );
  ctx.strokeRect(
    spriteGuide.x + 0.5,
    spriteGuide.y + 0.5,
    spriteGuide.w,
    spriteGuide.h
  );

  ctx.strokeStyle = "rgba(255, 190, 110, 0.9)";
  const hudGuide = targetRectToEdit(
    state.guides.hudX,
    state.guides.hudY,
    state.guides.hudW,
    state.guides.hudH
  );
  ctx.strokeRect(
    hudGuide.x + 0.5,
    hudGuide.y + 0.5,
    hudGuide.w,
    hudGuide.h
  );

  ctx.setLineDash([]);
  ctx.fillStyle = "rgba(120, 214, 238, 0.9)";
  ctx.font = "6px sans-serif";
  ctx.fillText("sprite", spriteGuide.x, spriteGuide.y - 2);
  ctx.fillStyle = "rgba(255, 190, 110, 0.9)";
  ctx.fillText("hud", hudGuide.x, hudGuide.y + hudGuide.h + 7);
  ctx.restore();
}

function drawSelection() {
  const item = selectedItem();
  if (!item) return;
  const b = itemBounds(item);
  const editBounds = targetRectToEdit(b.x, b.y, b.w, b.h);
  ctx.save();
  ctx.strokeStyle = "#78d6ee";
  ctx.lineWidth = 1;
  ctx.setLineDash([2, 2]);
  ctx.strokeRect(Math.round(editBounds.x) + 0.5, Math.round(editBounds.y) + 0.5, Math.round(editBounds.w), Math.round(editBounds.h));
  ctx.restore();
}

function render() {
  state.width = Number(document.getElementById("canvasW").value) || 240;
  state.height = Number(document.getElementById("canvasH").value) || 135;
  state.baseFit = document.getElementById("baseFit").value;
  state.baseOpacity = (Number(document.getElementById("baseOpacity").value) || 0) / 100;
  state.sourceScale = Number(document.getElementById("sourceScale").value) || 1;
  state.furnitureImportMode = document.getElementById("furnitureImportMode").value;
  state.aspectLocked = document.getElementById("aspectLock").checked;
  state.guides.show = document.getElementById("showGuides").checked;
  state.guides.spriteX = Number(document.getElementById("spriteX").value) || 0;
  state.guides.spriteY = Number(document.getElementById("spriteY").value) || 0;
  state.guides.spriteW = Number(document.getElementById("spriteW").value) || 64;
  state.guides.spriteH = Number(document.getElementById("spriteH").value) || 64;
  state.night.tint = Number(document.getElementById("nightTint").value) || 0;
  state.night.darken = Number(document.getElementById("nightDarken").value) || 0;
  state.night.lamp = Number(document.getElementById("lampStrength").value) || 0;

  if (canvas.width !== state.editWidth || canvas.height !== state.editHeight) {
    canvas.width = state.editWidth;
    canvas.height = state.editHeight;
  }
  updateCanvasDisplaySize();

  drawBase();
  drawItems();
  drawNightOverlay();
  drawFacesOverlay();
  drawGuides();
  drawSelection();
  renderPreview();
}

function refreshList() {
  assetList.innerHTML = "";
  for (const item of sortedItems()) {
    const el = document.createElement("div");
    el.className = `asset-item${item.id === state.selectedId ? " selected" : ""}`;
    el.innerHTML = `
      <img class="asset-thumb" alt="" src="${item.dataUrl}">
      <div>
        <div class="asset-name">${escapeHtml(item.name)}</div>
        <div class="asset-meta">x ${Math.round(item.x)}, y ${Math.round(item.y)}, z ${item.z}, ${Math.round(item.scale * 100)}%</div>
      </div>
    `;
    el.addEventListener("click", () => {
      state.selectedId = item.id;
      state.selectedFaceId = null;
      state.selectedPointIndex = null;
      state.selectedPointId = null;
      refreshList();
      refreshSelectedPanel();
      refreshFaceList();
      refreshSelectedFacePanel();
      render();
    });
    assetList.appendChild(el);
  }
  if (state.items.length === 0) {
    assetList.innerHTML = '<div class="hint">No furniture loaded.</div>';
  }
}

function refreshSelectedPanel() {
  const item = selectedItem();
  if (!item) {
    selectedPanel.innerHTML = '<div class="hint">Select furniture to edit transform and layer values.</div>';
    return;
  }

  selectedPanel.innerHTML = `
    <label class="field">Name<input id="itemName" type="text" value="${escapeAttr(item.name)}"></label>
    <div class="row">
      <label class="field">X<input id="itemX" type="number" value="${Math.round(item.x)}"></label>
      <label class="field">Y<input id="itemY" type="number" value="${Math.round(item.y)}"></label>
    </div>
    <div class="row">
      <label class="field">Z<input id="itemZ" type="number" value="${item.z}"></label>
      <label class="field">Scale<input id="itemScale" type="number" step="0.05" min="0.05" value="${item.scale}"></label>
    </div>
    <div class="row">
      <label class="field">Opacity<input id="itemOpacity" type="number" step="0.05" min="0" max="1" value="${item.opacity}"></label>
      <label class="field">Slot<input id="itemSlot" type="text" value="${escapeAttr(item.slot)}"></label>
    </div>
    <label class="check-row"><input id="itemVisible" type="checkbox" ${item.visible ? "checked" : ""}> Visible</label>
    <div class="hint">Source: ${escapeHtml(item.fileName)} (${item.sourceWidth}x${item.sourceHeight}) -> target ${Math.round(item.sourceWidth * item.scale)}x${Math.round(item.sourceHeight * item.scale)}</div>
  `;

  const bind = (id, fn) => {
    const input = document.getElementById(id);
    input.addEventListener("input", () => {
      fn(input);
      refreshList();
      render();
    });
  };
  bind("itemName", (input) => { item.name = input.value; });
  bind("itemX", (input) => { item.x = Number(input.value) || 0; });
  bind("itemY", (input) => { item.y = Number(input.value) || 0; });
  bind("itemZ", (input) => { item.z = Number(input.value) || 0; });
  bind("itemScale", (input) => { item.scale = Math.max(0.05, Number(input.value) || 1); });
  bind("itemOpacity", (input) => { item.opacity = Math.max(0, Math.min(1, Number(input.value))); });
  bind("itemSlot", (input) => { item.slot = input.value; });
  bind("itemVisible", (input) => { item.visible = input.checked; });
}

function refreshFaceList() {
  faceList.innerHTML = "";
  for (const face of state.faces) {
    const el = document.createElement("div");
    el.className = `face-item${face.id === state.selectedFaceId ? " selected" : ""}`;
    el.innerHTML = `
      <div class="face-color" style="background:${faceStrokeColor(face.type)}"></div>
      <div>
        <div class="asset-name">${escapeHtml(face.id)}</div>
        <div class="asset-meta">${escapeHtml(face.type)}, ${face.points.length} points</div>
      </div>
    `;
        el.addEventListener("click", () => {
          state.selectedFaceId = face.id;
          state.selectedPointIndex = null;
          state.selectedPointId = null;
          state.selectedId = null;
          state.editMode = "shape";
      updateEditModeButtons();
      refreshFaceList();
      refreshSelectedFacePanel();
      refreshList();
      refreshSelectedPanel();
      render();
    });
    faceList.appendChild(el);
  }
  if (state.faces.length === 0) {
    faceList.innerHTML = '<div class="hint">No closed faces yet.</div>';
  }
}

function selectPoint(index) {
  const face = selectedFace();
  selectPointInFace(face, index);
  refreshSelectedFacePanel();
  render();
}

function insertPointAfterSelected() {
  const face = selectedFace();
  if (!face) return;
  const index = state.selectedPointIndex == null ? face.points.length - 1 : state.selectedPointIndex;
  const nextIndex = (index + 1) % face.points.length;
  const a = face.points[index];
  const b = face.points[nextIndex];
  const point = {
    x: Math.round((a.x + b.x) / 2),
    y: Math.round((a.y + b.y) / 2)
  };
  const createdPoint = createShapePoint(point.x, point.y);
  face.points.splice(index + 1, 0, createdPoint);
  selectPointInFace(face, index + 1);
  refreshFaceList();
  refreshSelectedFacePanel();
  render();
}

function deleteSelectedPoint() {
  const face = selectedFace();
  if (face) {
    if (state.selectedPointIndex == null || face.points.length <= 3) return;
    face.points.splice(state.selectedPointIndex, 1);
    pruneUnusedPoints();
    selectPointInFace(face, Math.min(state.selectedPointIndex, face.points.length - 1));
    refreshFaceList();
    refreshSelectedFacePanel();
    render();
    return;
  }
  if (state.selectedPointIndex == null || state.selectedPointIndex < 0 ||
      state.selectedPointIndex >= state.draftPoints.length) {
    return;
  }
  state.draftPoints.splice(state.selectedPointIndex, 1);
  if (state.draftPoints.length === 0) state.drawingFace = false;
  pruneUnusedPoints();
  selectDraftPoint(Math.min(state.selectedPointIndex, state.draftPoints.length - 1));
  refreshFaceList();
  refreshSelectedFacePanel();
  render();
}

function refreshSelectedFacePanel() {
  const face = selectedFace();
  if (!face) {
    if (!state.draftPoints.length) {
      selectedFacePanel.innerHTML = '<div class="hint">Select or draw a closed face.</div>';
      return;
    }
    selectedFacePanel.innerHTML = `
      <div class="hint">Draft: ${state.draftPoints.length} point(s). Click the first point on the canvas to close.</div>
      <div class="toolbar">
        <button id="deletePoint" class="danger" type="button">Delete point</button>
      </div>
      <div class="point-list">
        ${state.draftPoints.map((point, index) => `
          <div class="point-row${point.id === state.selectedPointId ? " selected" : ""}" data-point-index="${index}">
            <button class="point-select" type="button" data-point-index="${index}">${index + 1}</button>
            <label class="field">X<input id="pointX${index}" type="number" value="${Math.round(point.x)}"></label>
            <label class="field">Y<input id="pointY${index}" type="number" value="${Math.round(point.y)}"></label>
          </div>
        `).join("")}
      </div>
      <div class="hint">Draft points can be dragged, deleted, or reused with closed-face points.</div>
    `;
    selectedFacePanel.querySelector("#deletePoint").addEventListener("click", deleteSelectedPoint);
    for (let index = 0; index < state.draftPoints.length; ++index) {
      selectedFacePanel.querySelector(`.point-select[data-point-index="${index}"]`).addEventListener("click", () => {
        selectDraftPoint(index);
        refreshSelectedFacePanel();
        render();
      });
      selectedFacePanel.querySelector(`#pointX${index}`).addEventListener("input", (event) => {
        state.draftPoints[index].x = Math.round(Number(event.target.value) || 0);
        selectDraftPoint(index);
        render();
      });
      selectedFacePanel.querySelector(`#pointY${index}`).addEventListener("input", (event) => {
        state.draftPoints[index].y = Math.round(Number(event.target.value) || 0);
        selectDraftPoint(index);
        render();
      });
    }
    return;
  }

  selectedFacePanel.innerHTML = `
    <label class="field">
      Type
      <select id="faceType">
        <option value="floor" ${face.type === "floor" ? "selected" : ""}>floor</option>
        <option value="wall" ${face.type === "wall" ? "selected" : ""}>wall</option>
      </select>
    </label>
    <div class="toolbar">
      <button id="insertPoint" type="button">Add point</button>
      <button id="deletePoint" class="danger" type="button">Delete point</button>
    </div>
    <div class="point-list">
      ${face.points.map((point, index) => `
        <div class="point-row${point.id === state.selectedPointId ? " selected" : ""}" data-point-index="${index}">
          <button class="point-select" type="button" data-point-index="${index}">${index + 1}</button>
          <label class="field">X<input id="pointX${index}" type="number" value="${Math.round(point.x)}"></label>
          <label class="field">Y<input id="pointY${index}" type="number" value="${Math.round(point.y)}"></label>
        </div>
      `).join("")}
    </div>
    <div class="hint">${escapeHtml(face.id)} source points. Export scales these to ${state.width}x${state.height}.</div>
  `;

  document.getElementById("faceType").addEventListener("input", (event) => {
    face.type = event.target.value;
    refreshFaceList();
    render();
  });
  document.getElementById("insertPoint").addEventListener("click", insertPointAfterSelected);
  document.getElementById("deletePoint").addEventListener("click", deleteSelectedPoint);
  for (let index = 0; index < face.points.length; ++index) {
    selectedFacePanel.querySelector(`.point-select[data-point-index="${index}"]`).addEventListener("click", () => {
      selectPoint(index);
    });
    selectedFacePanel.querySelector(`#pointX${index}`).addEventListener("input", (event) => {
      face.points[index].x = Math.round(Number(event.target.value) || 0);
      selectPointInFace(face, index);
      render();
    });
    selectedFacePanel.querySelector(`#pointY${index}`).addEventListener("input", (event) => {
      face.points[index].y = Math.round(Number(event.target.value) || 0);
      selectPointInFace(face, index);
      render();
    });
  }
}

function pointerToCanvas(event) {
  const rect = canvas.getBoundingClientRect();
  return {
    x: (event.clientX - rect.left) * canvas.width / rect.width,
    y: (event.clientY - rect.top) * canvas.height / rect.height
  };
}

function pointerToTarget(event) {
  return editToTargetPoint(pointerToCanvas(event));
}

function hitTest(x, y) {
  const items = sortedItems().reverse();
  for (const item of items) {
    if (!item.visible) continue;
    const b = itemBounds(item);
    if (x >= b.x && x <= b.x + b.w && y >= b.y && y <= b.y + b.h) {
      return item;
    }
  }
  return null;
}

function selectedFace() {
  return state.faces.find((face) => face.id === state.selectedFaceId) || null;
}

function distance(a, b) {
  const dx = a.x - b.x;
  const dy = a.y - b.y;
  return Math.sqrt(dx * dx + dy * dy);
}

function pointInPolygon(point, points) {
  let inside = false;
  for (let i = 0, j = points.length - 1; i < points.length; j = i++) {
    const xi = points[i].x;
    const yi = points[i].y;
    const xj = points[j].x;
    const yj = points[j].y;
    const intersects = ((yi > point.y) !== (yj > point.y)) &&
      (point.x < (xj - xi) * (point.y - yi) / ((yj - yi) || 1e-6) + xi);
    if (intersects) inside = !inside;
  }
  return inside;
}

function hitTestFace(x, y) {
  const point = { x, y };
  for (let i = state.faces.length - 1; i >= 0; --i) {
    if (state.faces[i].points.length < 3) continue;
    if (pointInPolygon(point, state.faces[i].points)) return state.faces[i];
  }
  return null;
}

function hitTestPoint(x, y) {
  const radius = editHandleRadius() + 3;
  for (let pointIndex = state.points.length - 1; pointIndex >= 0; --pointIndex) {
    const point = state.points[pointIndex];
    if (distance({ x, y }, point) <= radius) {
      return { point };
    }
  }
  return null;
}

function closeDraftFace() {
  const uniquePointIds = new Set(state.draftPoints.map((point) => point.id));
  if (state.draftPoints.length < 3 || uniquePointIds.size < 3) return;
  const face = {
    id: `face${state.nextFaceId++}`,
    type: "floor",
    points: state.draftPoints.slice()
  };
  state.faces.push(face);
  state.selectedFaceId = face.id;
  state.selectedPointIndex = 0;
  state.selectedPointId = face.points[0].id;
  state.draftPoints = [];
  state.drawingFace = false;
  setStatus(`Closed ${face.id}. Set its type on the right.`);
  refreshFaceList();
  refreshSelectedFacePanel();
  pruneUnusedPoints();
  render();
}

function handleShapePointerDown(event) {
  const p = pointerToCanvas(event);
  const point = { x: Math.round(p.x), y: Math.round(p.y) };

  const pointHit = hitTestPoint(point.x, point.y);
  if (state.drawingFace) {
    if (pointHit) {
      const draftIndex = state.draftPoints.findIndex((entry) => entry.id === pointHit.point.id);
      if (draftIndex === 0 && state.draftPoints.length >= 3) {
        closeDraftFace();
        return;
      }
      if (draftIndex >= 0) {
        selectDraftPoint(draftIndex);
        state.selectedId = null;
        state.draggingPoint = true;
        state.dragPointId = pointHit.point.id;
        canvas.classList.add("dragging");
        canvas.setPointerCapture(event.pointerId);
      } else {
        addDraftPoint(pointHit.point);
        setStatus(`${state.draftPoints.length} point(s). Existing endpoint reused.`);
      }
    } else {
      addDraftPoint(createShapePoint(point.x, point.y));
      setStatus(`${state.draftPoints.length} point(s). Click the first point to close the face.`);
    }
    state.selectedId = null;
    state.selectedFaceId = null;
    updateEditModeButtons();
    refreshFaceList();
    refreshSelectedFacePanel();
    refreshList();
    refreshSelectedPanel();
    render();
    return;
  }

  if (pointHit) {
    const membership = findPointMembership(pointHit.point);
    setSelectedPoint(pointHit.point, membership);
    state.selectedId = null;
    state.draggingPoint = true;
    state.dragPointId = pointHit.point.id;
    canvas.classList.add("dragging");
    canvas.setPointerCapture(event.pointerId);
    refreshFaceList();
    refreshSelectedFacePanel();
    refreshList();
    refreshSelectedPanel();
    render();
    return;
  }

  if (!state.drawingFace && state.draftPoints.length === 0) {
    const face = hitTestFace(point.x, point.y);
        if (face) {
          state.selectedFaceId = face.id;
          state.selectedPointIndex = null;
          state.selectedPointId = null;
          state.selectedId = null;
          refreshFaceList();
          refreshSelectedFacePanel();
      render();
      return;
    }
  }

  state.drawingFace = true;
  state.selectedFaceId = null;
  state.selectedPointIndex = null;
  state.selectedPointId = null;
  state.selectedId = null;
  addDraftPoint(createShapePoint(point.x, point.y));
  setStatus(`${state.draftPoints.length} point(s). Click the first point to close the face.`);
  refreshFaceList();
  refreshSelectedFacePanel();
  render();
}

function updateModeButtons() {
  document.getElementById("dayMode").classList.toggle("active", state.mode === "day");
  document.getElementById("nightMode").classList.toggle("active", state.mode === "night");
}

function updateEditModeButtons() {
  document.getElementById("furnitureMode").classList.toggle("active", state.editMode === "furniture");
  document.getElementById("shapeMode").classList.toggle("active", state.editMode === "shape");
}

function updatePreviewZoom() {
  previewCanvas.classList.toggle("zoomed", state.previewZoomed);
}

function exportLayoutObject() {
  return {
    version: 1,
    canvas: { width: state.width, height: state.height },
    base: {
      fileName: state.base ? state.base.name : "",
      fit: state.baseFit,
      sourceWidth: state.base ? state.base.width : 0,
      sourceHeight: state.base ? state.base.height : 0
    },
    sourceScale: Number(state.sourceScale.toFixed(4)),
    baseOpacity: Number(state.baseOpacity.toFixed(4)),
    roomGeometry: exportRoomGeometryObject(),
    guides: state.guides,
    night: state.night,
    furniture: sortedItems().map((item) => ({
      id: item.id,
      name: item.name,
      fileName: item.fileName,
      x: Math.round(item.x),
      y: Math.round(item.y),
      z: item.z,
      scale: Number(item.scale.toFixed(4)),
      targetWidth: Math.round(item.sourceWidth * item.scale),
      targetHeight: Math.round(item.sourceHeight * item.scale),
      sourceScale: Number((item.sourceScale || state.sourceScale).toFixed(4)),
      opacity: Number(item.opacity.toFixed(4)),
      visible: item.visible,
      anchor: item.anchor,
      slot: item.slot,
      layer: item.layer,
      source: item.source || "furniture",
      speciesId: item.speciesId || 0,
      frame: item.frame || "",
      sourceWidth: item.sourceWidth,
      sourceHeight: item.sourceHeight
    }))
  };
}

function exportRoomGeometryObject() {
  return {
    version: 2,
    room: {
      width: state.width,
      height: state.height
    },
    edit: {
      width: state.editWidth,
      height: state.editHeight
    },
    reference: {
      fileName: state.base ? state.base.name : "",
      sourceWidth: state.base ? state.base.width : 0,
      sourceHeight: state.base ? state.base.height : 0,
      fit: state.baseFit,
      opacity: Number(state.baseOpacity.toFixed(4))
    },
    faces: state.faces.map((face, index) => ({
      id: face.id,
      type: face.type,
      drawOrder: index,
      points: faceTargetPoints(face).map((point) => [Math.round(point.x), Math.round(point.y)]),
      sourcePoints: face.points.map((point) => [Math.round(point.x), Math.round(point.y)])
    }))
  };
}

function downloadText(fileName, text, mime = "text/plain") {
  const blob = new Blob([text], { type: mime });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = fileName;
  document.body.appendChild(a);
  a.click();
  a.remove();
  URL.revokeObjectURL(url);
}

function downloadCanvas(fileName) {
  renderPreview();
  previewCanvas.toBlob((blob) => {
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = fileName;
    document.body.appendChild(a);
    a.click();
    a.remove();
    URL.revokeObjectURL(url);
  }, "image/png");
}

async function importLayout(file) {
  const text = await file.text();
  const layout = JSON.parse(text);
  if (layout.canvas) {
    document.getElementById("canvasW").value = layout.canvas.width || 240;
    document.getElementById("canvasH").value = layout.canvas.height || 135;
  }
  if (layout.base && layout.base.fit) {
    document.getElementById("baseFit").value = layout.base.fit;
  }
  if (layout.sourceScale) {
    document.getElementById("sourceScale").value = layout.sourceScale;
  }
  if (layout.baseOpacity != null) {
    document.getElementById("baseOpacity").value = Math.round(layout.baseOpacity * 100);
  }
  if (layout.roomGeometry) {
    const geometry = layout.roomGeometry;
    if (geometry.room) {
      document.getElementById("canvasW").value = geometry.room.width || layout.canvas?.width || 240;
      document.getElementById("canvasH").value = geometry.room.height || layout.canvas?.height || 135;
    }
    const roomWidth = Number(geometry.room?.width || layout.canvas?.width || state.width || 240);
    const roomHeight = Number(geometry.room?.height || layout.canvas?.height || state.height || 135);
    state.editWidth = Number(geometry.edit?.width || layout.base?.sourceWidth || state.base?.width || roomWidth);
    state.editHeight = Number(geometry.edit?.height || layout.base?.sourceHeight || state.base?.height || roomHeight);
    const pointByCoord = new Map();
    state.points = [];
    state.nextPointId = 1;
    state.faces = (geometry.faces || []).map((face, index) => {
      const points = (face.sourcePoints || face.points || []).map((rawPoint) => {
        let x;
        let y;
        if (face.sourcePoints) {
          x = Number(rawPoint[0]) || 0;
          y = Number(rawPoint[1]) || 0;
        } else {
          x = Math.round((Number(rawPoint[0]) || 0) * state.editWidth / Math.max(1, roomWidth));
          y = Math.round((Number(rawPoint[1]) || 0) * state.editHeight / Math.max(1, roomHeight));
        }
        const key = `${Math.round(x)},${Math.round(y)}`;
        if (!pointByCoord.has(key)) {
          pointByCoord.set(key, createShapePoint(x, y));
        }
        return pointByCoord.get(key);
      });
      return {
        id: face.id || `face${index + 1}`,
        type: face.type === "wall" ? "wall" : "floor",
        points
      };
    });
    state.nextFaceId = state.faces.reduce((next, face) => {
      const match = String(face.id).match(/(\d+)$/);
      return Math.max(next, match ? Number(match[1]) + 1 : next);
    }, 1);
    state.selectedFaceId = state.faces[0]?.id || null;
    state.selectedPointIndex = state.faces[0]?.points.length ? 0 : null;
    state.selectedPointId = state.faces[0]?.points[0]?.id || null;
    state.draftPoints = [];
    state.drawingFace = false;
    pruneUnusedPoints();
  }
  if (layout.guides) {
    document.getElementById("spriteX").value = layout.guides.spriteX ?? 88;
    document.getElementById("spriteY").value = layout.guides.spriteY ?? 50;
    document.getElementById("spriteW").value = layout.guides.spriteW ?? 64;
    document.getElementById("spriteH").value = layout.guides.spriteH ?? 64;
    document.getElementById("showGuides").checked = layout.guides.show ?? true;
  }
  if (layout.night) {
    document.getElementById("nightTint").value = layout.night.tint ?? 46;
    document.getElementById("nightDarken").value = layout.night.darken ?? 34;
    document.getElementById("lampStrength").value = layout.night.lamp ?? 36;
  }

  const byFile = new Map(state.items.map((item) => [item.fileName, item]));
  for (const entry of layout.furniture || []) {
    const item = byFile.get(entry.fileName);
    if (!item) continue;
    Object.assign(item, {
      id: entry.id || item.id,
      name: entry.name || item.name,
      x: entry.x ?? item.x,
      y: entry.y ?? item.y,
      z: entry.z ?? item.z,
      scale: entry.scale ?? item.scale,
      sourceScale: entry.sourceScale ?? item.sourceScale,
      opacity: entry.opacity ?? item.opacity,
      visible: entry.visible ?? item.visible,
      anchor: entry.anchor || item.anchor,
      slot: entry.slot || item.slot,
      layer: entry.layer || item.layer
    });
  }
  setStatus(`Layout imported: ${file.name}. Matching furniture files were updated.`);
  refreshList();
  refreshSelectedPanel();
  refreshFaceList();
  refreshSelectedFacePanel();
  render();
}

function escapeHtml(value) {
  return String(value).replace(/[&<>"']/g, (ch) => ({
    "&": "&amp;",
    "<": "&lt;",
    ">": "&gt;",
    '"': "&quot;",
    "'": "&#039;"
  }[ch]));
}

function escapeAttr(value) {
  return escapeHtml(value).replace(/`/g, "&#096;");
}

document.getElementById("baseInput").addEventListener("change", (event) => {
  if (event.target.files[0]) loadBase(event.target.files[0]);
});

document.getElementById("furnitureInput").addEventListener("change", (event) => {
  addFurnitureFiles([...event.target.files]);
});

document.getElementById("layoutInput").addEventListener("change", (event) => {
  if (event.target.files[0]) importLayout(event.target.files[0]);
});

document.getElementById("canvasW").addEventListener("input", () => applyRoomSizeInput("width"));
document.getElementById("canvasH").addEventListener("input", () => applyRoomSizeInput("height"));
document.getElementById("aspectLock").addEventListener("change", () => {
  state.aspectLocked = document.getElementById("aspectLock").checked;
  state.aspectRatio = state.width / Math.max(1, state.height);
  render();
});

for (const id of ["baseOpacity", "sourceScale", "furnitureImportMode", "baseFit", "showGuides", "spriteX", "spriteY", "spriteW", "spriteH", "nightTint", "nightDarken", "lampStrength"]) {
  document.getElementById(id).addEventListener("input", render);
}

document.getElementById("dayMode").addEventListener("click", () => {
  state.mode = "day";
  updateModeButtons();
  render();
});

document.getElementById("nightMode").addEventListener("click", () => {
  state.mode = "night";
  updateModeButtons();
  render();
});

document.getElementById("furnitureMode").addEventListener("click", () => {
  state.editMode = "furniture";
  state.drawingFace = false;
  state.selectedFaceId = null;
  state.selectedPointIndex = null;
  state.selectedPointId = null;
  updateEditModeButtons();
  refreshFaceList();
  refreshSelectedFacePanel();
  render();
});

document.getElementById("shapeMode").addEventListener("click", () => {
  state.editMode = "shape";
  state.selectedId = null;
  updateEditModeButtons();
  refreshList();
  refreshSelectedPanel();
  render();
});

document.getElementById("newFace").addEventListener("click", () => {
  clearDraftPoints();
  state.editMode = "shape";
  state.drawingFace = true;
  state.selectedFaceId = null;
  state.selectedPointIndex = null;
  state.selectedPointId = null;
  state.selectedId = null;
  updateEditModeButtons();
  refreshList();
  refreshSelectedPanel();
  refreshFaceList();
  refreshSelectedFacePanel();
  setStatus("Click points on the edit canvas. Click the first point to close.");
  render();
});

document.getElementById("undoPoint").addEventListener("click", () => {
  state.draftPoints.pop();
  if (state.draftPoints.length === 0) state.drawingFace = false;
  pruneUnusedPoints();
  selectDraftPoint(Math.min(state.selectedPointIndex ?? state.draftPoints.length - 1,
                            state.draftPoints.length - 1));
  refreshSelectedFacePanel();
  render();
});

document.getElementById("clearDraft").addEventListener("click", () => {
  clearDraftPoints();
  refreshSelectedFacePanel();
  render();
});

document.getElementById("resetView").addEventListener("click", () => {
  state.selectedId = null;
  state.selectedFaceId = null;
  state.selectedPointIndex = null;
  state.selectedPointId = null;
  refreshList();
  refreshSelectedPanel();
  refreshFaceList();
  refreshSelectedFacePanel();
  render();
});

document.getElementById("exportJson").addEventListener("click", () => {
  downloadText("room_layout.json", JSON.stringify(exportLayoutObject(), null, 2), "application/json");
});

document.getElementById("exportGeometry").addEventListener("click", () => {
  downloadText("room_geometry.json", JSON.stringify(exportRoomGeometryObject(), null, 2), "application/json");
});

document.getElementById("exportPreview").addEventListener("click", () => {
  downloadCanvas(`room_${state.mode}_preview.png`);
});

previewCanvas.addEventListener("click", () => {
  state.previewZoomed = !state.previewZoomed;
  updatePreviewZoom();
});

document.getElementById("deleteFace").addEventListener("click", () => {
  const face = selectedFace();
  if (!face) return;
  state.faces = state.faces.filter((entry) => entry.id !== face.id);
  state.selectedFaceId = null;
  state.selectedPointIndex = null;
  state.selectedPointId = null;
  pruneUnusedPoints();
  refreshFaceList();
  refreshSelectedFacePanel();
  render();
});

document.getElementById("deleteItem").addEventListener("click", () => {
  const item = selectedItem();
  if (!item) return;
  state.items = state.items.filter((v) => v.id !== item.id);
  state.selectedId = null;
  refreshList();
  refreshSelectedPanel();
  render();
});

document.getElementById("duplicateItem").addEventListener("click", () => {
  const item = selectedItem();
  if (!item) return;
  const copy = { ...item, id: `f${state.nextId++}`, name: `${item.name}_copy`, x: item.x + 4, y: item.y + 4 };
  state.items.push(copy);
  state.selectedId = copy.id;
  refreshList();
  refreshSelectedPanel();
  render();
});

canvas.addEventListener("pointerdown", (event) => {
  if (state.editMode === "shape") {
    handleShapePointerDown(event);
    return;
  }
  const p = pointerToTarget(event);
  const item = hitTest(p.x, p.y);
  if (item) {
    state.selectedId = item.id;
    state.selectedFaceId = null;
    state.selectedPointIndex = null;
    state.selectedPointId = null;
    state.dragging = true;
    state.dragOffsetX = p.x - item.x;
    state.dragOffsetY = p.y - item.y;
    canvas.classList.add("dragging");
    canvas.setPointerCapture(event.pointerId);
  } else {
    state.selectedId = null;
  }
  refreshList();
  refreshSelectedPanel();
  render();
});

canvas.addEventListener("pointermove", (event) => {
  if (state.editMode === "shape" && state.draggingPoint) {
    const shapePoint = state.points.find((entry) => entry.id === state.dragPointId);
    if (!shapePoint) return;
    const point = pointerToCanvas(event);
    shapePoint.x = Math.round(point.x);
    shapePoint.y = Math.round(point.y);
    const membership = findPointMembership(shapePoint);
    setSelectedPoint(shapePoint, membership);
    render();
    return;
  }
  if (!state.dragging) return;
  const item = selectedItem();
  if (!item) return;
  const p = pointerToTarget(event);
  item.x = Math.round(p.x - state.dragOffsetX);
  item.y = Math.round(p.y - state.dragOffsetY);
  refreshList();
  refreshSelectedPanel();
  render();
});

canvas.addEventListener("pointerup", (event) => {
  state.dragging = false;
  state.draggingPoint = false;
  state.dragPointId = null;
  canvas.classList.remove("dragging");
  refreshSelectedFacePanel();
  try {
    canvas.releasePointerCapture(event.pointerId);
  } catch (_) {}
});

window.addEventListener("keydown", (event) => {
  if (event.key === "Escape" && state.previewZoomed) {
    state.previewZoomed = false;
    updatePreviewZoom();
    event.preventDefault();
    return;
  }

  if (state.editMode === "shape") {
    const shapePoint = selectedPoint();
    if (shapePoint) {
      let dx = 0;
      let dy = 0;
      if (event.key === "ArrowLeft") dx = -1;
      if (event.key === "ArrowRight") dx = 1;
      if (event.key === "ArrowUp") dy = -1;
      if (event.key === "ArrowDown") dy = 1;
      if (!dx && !dy) return;
      if (event.shiftKey) {
        dx *= 8;
        dy *= 8;
      }
      shapePoint.x += dx;
      shapePoint.y += dy;
      event.preventDefault();
      refreshSelectedFacePanel();
      render();
      return;
    }
  }

  const item = selectedItem();
  if (!item) return;
  let dx = 0;
  let dy = 0;
  if (event.key === "ArrowLeft") dx = -1;
  if (event.key === "ArrowRight") dx = 1;
  if (event.key === "ArrowUp") dy = -1;
  if (event.key === "ArrowDown") dy = 1;
  if (!dx && !dy) return;
  if (event.shiftKey) {
    dx *= 8;
    dy *= 8;
  }
  item.x += dx;
  item.y += dy;
  event.preventDefault();
  refreshList();
  refreshSelectedPanel();
  render();
});

const dropZone = document.getElementById("dropZone");
dropZone.addEventListener("dragover", (event) => {
  event.preventDefault();
  dropZone.classList.add("dragover");
});
dropZone.addEventListener("dragleave", () => dropZone.classList.remove("dragover"));
dropZone.addEventListener("drop", async (event) => {
  event.preventDefault();
  dropZone.classList.remove("dragover");
  const files = [...event.dataTransfer.files];
  const json = files.find((file) => file.name.endsWith(".json"));
  const images = files.filter((file) => file.type.startsWith("image/"));
  if (json) await importLayout(json);
  if (images.length === 1 && !state.base) {
    await loadBase(images[0]);
  } else if (images.length) {
    await addFurnitureFiles(images);
  }
});

refreshList();
refreshSelectedPanel();
refreshFaceList();
refreshSelectedFacePanel();
updateBaseImageMeta();
updateEditModeButtons();
updatePreviewZoom();
initProjectSpriteControls();
render();
