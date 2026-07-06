const TRIM_PADDING = 4;
const TRIM_THRESHOLD = 34;
const ITEM_MIN_TARGET_SIZE = 1;
const SESSION_STORAGE_KEY = "stickmon.roomEditor.lastSession.v1";
const SESSION_DB_NAME = "stickmon-room-editor";
const SESSION_DB_STORE = "sessions";
const SESSION_DB_KEY = "last";
const SESSION_EXPORT_HANDLE_KEY = "sessionExportFileHandle";
const GAME_SCREEN_WIDTH = 240;
const GAME_SCREEN_HEIGHT = 135;
const PREVIEW_MAX_WIDTH = 240;
const LIGHT_HANDLE_RADIUS = 7;
const BASE_FIT_WIDTH = "fit_width";
const TOOLTIP_DELAY_MS = 900;
const CONTROL_TOOLTIPS = {
  baseInput: "导入房间背景参考图。编辑器会自动裁剪透明/黑边，并按 240 宽生成预览和导出尺寸。",
  replaceBaseButton: "替换当前背景图，保留已有家具、面和光照配置。",
  furnitureInput: "导入家具或道具 PNG。导入后可以在主画布拖动、缩放并配置影子。",
  layoutInput: "导入之前导出的 layout/session JSON，用于恢复房间配置。",
  baseOpacity: "调整主编辑画布中背景参考图的透明度，不影响最终导出。",
  sourceScale: "家具导入时的默认缩放比例。用源图像素除以这个值，得到游戏坐标尺寸。",
  furnitureImportMode: "选择家具 PNG 导入时如何确定初始大小。",
  baseFit: "背景适配方式。当前项目固定为宽度填满 240px 游戏画布。",
  spriteSpeciesSelect: "选择一个项目内精灵，用于预览精灵与家具遮挡关系。",
  spriteActionSelect: "选择精灵动作帧类型。",
  spriteFrameSelect: "选择具体帧。",
  addProjectSprite: "把当前选择的精灵帧放到画面中心做尺寸和遮挡预览。",
  showGuides: "显示或隐藏 HUD 参考线。",
  showGrid: "显示 8px 网格，方便按游戏像素对齐家具。",
  gridOpacity: "调整 8px 网格线的透明度。只影响主编辑画布显示，不影响预览和导出。",
  snapToGrid: "拖动家具时吸附到 8px 网格。",
  dayMode: "切换到白天预览。只影响预览，不改变当前编辑的光源配置对象。",
  nightMode: "切换到夜晚预览。只影响预览，不改变当前编辑的光源配置对象。",
  furnitureMode: "家具模式：选择、拖动、缩放家具和精灵预览物。",
  shapeMode: "描面模式：绘制墙面和地板面，用于承接投影。",
  editZoom: "缩放主编辑画布的显示比例。只影响查看细节，不影响坐标和导出。",
  zoomOut: "缩小主编辑画布显示比例。",
  zoomIn: "放大主编辑画布显示比例。",
  zoomReset: "把主编辑画布显示比例恢复到 100%。",
  undoAction: "撤销上一步配置修改。",
  redoAction: "重做刚撤销的修改。",
  resetView: "清空当前选择并把主编辑画布缩放恢复到 100%。",
  preview: "最终游戏坐标预览。点击可放大查看。",
  selectTool: "选择工具：用于普通选择、拖动和编辑。",
  measureHeightTool: "测量工具：在高清主画布上测量高度，并换算成游戏像素。",
  clearMeasure: "清除当前测量线。",
  saveSession: "保存当前编辑器会话，同时尽量更新本地 session JSON。",
  loadSession: "从浏览器保存的会话恢复编辑器状态。",
  changeSessionFile: "重新选择 session JSON 的本地保存位置。",
  clearSession: "清除浏览器里保存的编辑器会话。",
  exportJson: "导出完整 layout JSON，供脚本生成游戏资源。",
  exportGeometry: "只导出房间面数据，用于检查墙/地板投影配置。",
  exportPreview: "导出当前预览 PNG，用于快速检查最终效果。",
  duplicateItem: "复制当前选中的家具或精灵预览物。",
  deleteItem: "删除当前选中的家具或精灵预览物。",
  lightProfileShared: "编辑白天和夜晚共用的光源配置。",
  lightProfileDay: "单独编辑白天光源配置。",
  lightProfileNight: "单独编辑夜晚光源配置。",
  copyLightToDay: "把当前光源参数复制到白天配置。",
  copyLightToNight: "把当前光源参数复制到夜晚配置。",
  lightShape: "选择光照形状。Cone 适合模拟从窗户射入的光束。",
  lightStrength: "光照强度。数值越大，光照区域越亮。",
  lightRadius: "光照影响范围。",
  lightX: "光源在游戏坐标中的 X 位置，也可以直接拖动画布中的光源点。",
  lightY: "光源在游戏坐标中的 Y 位置，也可以直接拖动画布中的光源点。",
  lightDepth: "光源深度，用于估算物体投影长度和透视感。",
  lightAngle: "锥形光中心方向角度，也可以拖动画布中的方向点调整。",
  lightSpread: "锥形光展开角度。方向点离光源越远，展开越大。",
  nightTint: "夜晚叠加的蓝色氛围强度。",
  nightDarken: "夜晚整体压暗强度。",
  castShadows: "开启或关闭家具投影绘制。",
  shadowMode: "选择投影算法。Auto 会根据光源形状自动选择。",
  shadowAlpha: "投影不透明度。",
  shadowLength: "投影长度增益。",
  shadowBlur: "投影模糊半径。",
  itemName: "当前物体名称，仅用于管理和导出识别。",
  itemKind: "选择物体预设类型，会影响默认高度、投影锚点和层级。",
  itemFurnitureType: "物体分类，供后续家具/道具逻辑使用。",
  itemX: "物体左上角 X 坐标，单位是游戏像素。",
  itemY: "物体左上角 Y 坐标，单位是游戏像素。",
  itemW: "物体目标宽度，单位是游戏像素。",
  itemH: "物体目标高度，单位是游戏像素。",
  itemScale: "按源图尺寸缩放物体。",
  itemOpacity: "编辑器内物体透明度。",
  itemVisible: "显示或隐藏这个物体。",
  itemAspectLock: "锁定宽高比例，调整宽度时同步高度。",
  itemCastsShadow: "这个物体是否产生投影。",
  itemHeightPx: "物体估算高度，用于计算影子长度和墙面投影。",
  itemFootprint: "物体接触地面的形状，用于生成更自然的投影。",
  itemFootprintPolygon: "自定义接地区域多边形，使用 0-1 的物体本地坐标，每行一个 x,y。",
  itemShadowPolygon: "自定义投影轮廓多边形，使用 0-1 的物体本地坐标。留空时使用图片 alpha。",
  itemShadowAnchor: "投影锚点。墙上架子等物体应使用墙面锚点。",
  itemWallShadowDepthPx: "墙面投影深度，用于控制墙上物体影子下落距离。",
  itemWallShadowOffsetY: "墙面投影垂直偏移，用于微调墙上影子位置。",
  itemReceivesShadow: "这个物体是否接收其它物体投影。",
  itemOccludesSprite: "精灵经过时，这个物体是否遮挡精灵。",
  itemZ: "层级排序用 Z 值。数值越大越靠上。",
  itemSortY: "同一 Z 层内的排序参考 Y 值。",
  itemSlot: "物体挂载位置标记，例如 floor 或 wall。",
  itemLayer: "导出层标记，用于后续游戏内分层绘制。"
};
const CONTROL_TOOLTIP_RULES = [
  [".drop-zone", "拖入背景图、家具 PNG 或 layout JSON。没有背景时单张图片会作为背景导入。"],
  [".preview-canvas", CONTROL_TOOLTIPS.preview],
  [".asset-visibility", "显示或隐藏这个图层。隐藏后不会在预览里绘制。"],
  [".face-toggle", "展开或收起这个房间面的点位列表。"],
  [".face-delete", "删除这个房间面。"],
  [".point-select", "选择这个点。选中后可在画布拖动或在右侧输入坐标。"],
  [".point-x", "点在高清编辑画布中的 X 坐标，导出时会换算成游戏像素。"],
  [".point-y", "点在高清编辑画布中的 Y 坐标，导出时会换算成游戏像素。"],
  [".face-type-select", "设置这个面是地板、墙面或精灵可活动区域。可活动区域只用于导出标注，不参与阴影承接。"],
  [".face-shadow-surface", "指定投影承接面：地板、左墙或右墙。"],
  [".face-receives-shadow", "控制这个房间面是否接收家具投影。"],
  [".insert-point", "在当前边后插入一个新点，用于细化墙面或地板轮廓。"],
  [".delete-point", "删除当前选中的点。房间面至少保留 3 个点。"]
];
const FURNITURE_TYPES = [
  { value: "bed", label: "床" },
  { value: "bowl", label: "碗" },
  { value: "decoration", label: "装饰物品" },
  { value: "toy", label: "玩具" }
];
const ITEM_KINDS = [
  { value: "wall_shelf", label: "墙上架子", heightPx: 18, wallDepthPx: 36, footprint: "rect", shadowAnchor: "wall", castsShadow: true, receivesShadow: true, occludesSprite: false, slot: "wall", layer: "wall" },
  { value: "bed_sofa", label: "床/沙发", heightPx: 34, footprint: "ellipse", shadowAnchor: "floor", castsShadow: true, receivesShadow: true, occludesSprite: true, slot: "floor", layer: "main" },
  { value: "food_bowl", label: "食盆", heightPx: 8, footprint: "ellipse", shadowAnchor: "floor", castsShadow: true, receivesShadow: true, occludesSprite: false, slot: "floor", layer: "main" },
  { value: "decoration_prop", label: "装饰物", heightPx: 16, footprint: "ellipse", shadowAnchor: "floor", castsShadow: true, receivesShadow: true, occludesSprite: false, slot: "floor", layer: "main" },
  { value: "rug", label: "地毯", heightPx: 0, footprint: "polygon", castsShadow: false, receivesShadow: true, occludesSprite: false, slot: "floor", layer: "floor" },
  { value: "floor_decal", label: "地面贴花", heightPx: 0, footprint: "polygon", castsShadow: false, receivesShadow: true, occludesSprite: false, slot: "floor", layer: "floor" },
  { value: "low_prop", label: "小物件", heightPx: 8, footprint: "ellipse", castsShadow: true, receivesShadow: true, occludesSprite: false, slot: "floor", layer: "main" },
  { value: "furniture", label: "家具", heightPx: 32, footprint: "rect", castsShadow: true, receivesShadow: true, occludesSprite: true, slot: "floor", layer: "main" },
  { value: "tall_furniture", label: "高家具", heightPx: 56, footprint: "rect", castsShadow: true, receivesShadow: true, occludesSprite: true, slot: "floor", layer: "main" },
  { value: "wall_furniture", label: "墙挂家具", heightPx: 24, wallDepthPx: 24, footprint: "rect", castsShadow: true, receivesShadow: true, occludesSprite: false, slot: "wall", layer: "wall" },
  { value: "wall_prop", label: "墙面物件", heightPx: 0, wallDepthPx: 4, footprint: "none", castsShadow: false, receivesShadow: false, occludesSprite: false, slot: "wall", layer: "wall" }
];
const FOOTPRINT_TYPES = [
  { value: "auto", label: "自动" },
  { value: "ellipse", label: "椭圆" },
  { value: "rect", label: "矩形" },
  { value: "polygon", label: "多边形" },
  { value: "none", label: "无" }
];
const LIGHT_SHAPES = [
  { value: "radial", label: "圆形" },
  { value: "cone", label: "锥形" }
];
const SHADOW_MODES = [
  { value: "auto", label: "自动" },
  { value: "point", label: "点光源" },
  { value: "directional", label: "方向光" }
];
const SHADOW_SURFACES = [
  { value: "floor", label: "地板" },
  { value: "left_wall", label: "左墙" },
  { value: "right_wall", label: "右墙" }
];
const SHADOW_ANCHORS = [
  { value: "auto", label: "自动" },
  { value: "floor", label: "地面" },
  { value: "wall", label: "墙面" }
];
const LIGHT_SETTING_KEYS = [
  "lightShape",
  "lightStrength",
  "lightX",
  "lightY",
  "lightDepth",
  "lightRadius",
  "lightAngle",
  "lightSpread"
];
const WALL_MOUNTED_NAME_PATTERN = /(sheld|wall[_ -]?shelf|shelf[_ -]?wall|mounted[_ -]?shelf|hanging[_ -]?shelf|壁架|挂架|墙架|层板|搁板)/i;
const FACE_TYPE_SPRITE_AREA = "sprite_area";

const DEFAULT_NIGHT = {
  tint: 46,
  darken: 34,
  lightShape: "radial",
  lightStrength: 28,
  lightX: 170,
  lightY: 70,
  lightDepth: 180,
  lightRadius: 68,
  lightAngle: 125,
  lightSpread: 42,
  castShadows: true,
  shadowMode: "auto",
  shadowAlpha: 38,
  shadowLength: 28,
  shadowBlur: 0.6
};

function normalizeFurnitureType(value) {
  return FURNITURE_TYPES.some((type) => type.value === value) ? value : "decoration";
}

function furnitureTypeLabel(value) {
  const normalized = normalizeFurnitureType(value);
  return FURNITURE_TYPES.find((type) => type.value === normalized)?.label || "装饰物品";
}

function normalizeItemKind(value) {
  return ITEM_KINDS.some((kind) => kind.value === value) ? value : "furniture";
}

function itemKindPreset(value) {
  const normalized = normalizeItemKind(value);
  return ITEM_KINDS.find((kind) => kind.value === normalized) || ITEM_KINDS[3];
}

function itemKindLabel(value) {
  return itemKindPreset(value).label;
}

function normalizeFootprint(value) {
  return FOOTPRINT_TYPES.some((type) => type.value === value) ? value : "auto";
}

function normalizeLightShape(value) {
  return LIGHT_SHAPES.some((shape) => shape.value === value) ? value : "radial";
}

function normalizeShadowMode(value) {
  return SHADOW_MODES.some((mode) => mode.value === value) ? value : "auto";
}

function normalizeShadowSurface(value, faceType = "floor") {
  if (SHADOW_SURFACES.some((surface) => surface.value === value)) return value;
  return faceType === "wall" ? "left_wall" : "floor";
}

function normalizeShadowAnchor(value) {
  return SHADOW_ANCHORS.some((anchor) => anchor.value === value) ? value : "auto";
}

function normalizeLightProfile(value) {
  return ["shared", "day", "night"].includes(value) ? value : "shared";
}

function normalizeFaceType(value) {
  if (value === "wall" || value === FACE_TYPE_SPRITE_AREA) return value;
  return "floor";
}

function faceTypeLabel(value) {
  const type = normalizeFaceType(value);
  if (type === "wall") return "wall";
  if (type === FACE_TYPE_SPRITE_AREA) return "sprite area";
  return "floor";
}

function isSpriteAreaFace(face) {
  return normalizeFaceType(face?.type) === FACE_TYPE_SPRITE_AREA;
}

function normalizeLightSettings(value = {}, fallback = DEFAULT_NIGHT) {
  return {
    lightShape: normalizeLightShape(value.lightShape ?? fallback.lightShape),
    lightStrength: coerceNumber(value.lightStrength, fallback.lightStrength, 0, 200),
    lightX: coerceNumber(value.lightX, fallback.lightX, -2048, 2048),
    lightY: coerceNumber(value.lightY, fallback.lightY, -2048, 2048),
    lightDepth: coerceNumber(value.lightDepth, fallback.lightDepth, 40, 480),
    lightRadius: coerceNumber(value.lightRadius, fallback.lightRadius, 8, 240),
    lightAngle: coerceNumber(value.lightAngle, fallback.lightAngle, 0, 359),
    lightSpread: coerceNumber(value.lightSpread, fallback.lightSpread, 5, 160)
  };
}

function clampNumber(value, min, max) {
  return Math.max(min, Math.min(max, Number(value)));
}

function coerceNumber(value, fallback, min, max) {
  const number = Number(value);
  const next = Number.isFinite(number) ? number : fallback;
  return clampNumber(next, min, max);
}

function normalizeNightSettings(value = {}) {
  const sharedLight = normalizeLightSettings(value, DEFAULT_NIGHT);
  return {
    tint: coerceNumber(value.tint, DEFAULT_NIGHT.tint, 0, 90),
    darken: coerceNumber(value.darken, DEFAULT_NIGHT.darken, 0, 90),
    ...sharedLight,
    separateModeLights: value.separateModeLights === true,
    dayLight: normalizeLightSettings(value.dayLight, sharedLight),
    nightLight: normalizeLightSettings(value.nightLight, sharedLight),
    castShadows: value.castShadows ?? DEFAULT_NIGHT.castShadows,
    shadowMode: normalizeShadowMode(value.shadowMode),
    shadowAlpha: coerceNumber(value.shadowAlpha, DEFAULT_NIGHT.shadowAlpha, 0, 80),
    shadowLength: coerceNumber(value.shadowLength, DEFAULT_NIGHT.shadowLength, 0, 80),
    shadowBlur: coerceNumber(value.shadowBlur, DEFAULT_NIGHT.shadowBlur, 0, 12)
  };
}

function inferFurnitureType(name) {
  const text = String(name || "").toLowerCase();
  if (text.includes("bed") || text.includes("床")) return "bed";
  if (text.includes("bowl") || text.includes("dish") || text.includes("碗")) return "bowl";
  if (text.includes("toy") || text.includes("玩具")) return "toy";
  return "decoration";
}

function inferItemKind(name, furnitureType = "") {
  const text = String(name || "").toLowerCase();
  if (text.includes("rug") || text.includes("carpet") || text.includes("mat") ||
      text.includes("地毯") || text.includes("毯") || text.includes("垫")) {
    return "rug";
  }
  if (text.includes("decal") || text.includes("floor") || text.includes("贴花") || text.includes("地面")) {
    return "floor_decal";
  }
  if (WALL_MOUNTED_NAME_PATTERN.test(text)) {
    return "wall_shelf";
  }
  if (text.includes("wall") || text.includes("window") || text.includes("painting") ||
      text.includes("frame") || text.includes("poster") || text.includes("窗") ||
      text.includes("画") || text.includes("墙") || text.includes("壁")) {
    return "wall_prop";
  }
  if (text.includes("cabinet") || text.includes("wardrobe") || text.includes("closet") ||
      text.includes("shelf") || text.includes("bookcase") || text.includes("fridge") ||
      text.includes("柜") || text.includes("书架") || text.includes("冰箱")) {
    return "tall_furniture";
  }
  if (text.includes("sofa") || text.includes("couch") || text.includes("chair") ||
      text.includes("table") || text.includes("desk") || text.includes("bed") ||
      text.includes("沙发") || text.includes("椅") || text.includes("桌") || text.includes("床")) {
    return "bed_sofa";
  }
  if (furnitureType === "bowl" || furnitureType === "toy" ||
      text.includes("bowl") || text.includes("dish") || text.includes("toy") ||
      text.includes("ball") || text.includes("food") || text.includes("碗") ||
      text.includes("玩具") || text.includes("球") || text.includes("食")) {
    return furnitureType === "bowl" || text.includes("bowl") || text.includes("dish") || text.includes("碗") || text.includes("食")
      ? "food_bowl"
      : "low_prop";
  }
  return "decoration_prop";
}

const state = {
  width: 240,
  height: 135,
  editWidth: 240,
  editHeight: 135,
  mode: "day",
  lightProfile: "shared",
  editMode: "furniture",
  toolMode: "select",
  previewZoomed: false,
  editZoom: 1,
  base: null,
  baseFit: BASE_FIT_WIDTH,
  baseOpacity: 0.65,
  sourceScale: 4,
  baseTrim: null,
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
  expandedFaceIds: new Set(),
  draftExpanded: true,
  nextFaceId: 1,
  nextPointId: 1,
  draftPoints: [],
  drawingFace: false,
  draggingPoint: false,
  dragPointId: null,
  dragging: false,
  draggingItemPolygon: false,
  activeItemPolygonKey: "footprintPolygon",
  dragPolygonKey: null,
  dragPolygonIndex: null,
  draggingLight: false,
  draggingConeControl: false,
  lightSelected: false,
  dragOffsetX: 0,
  dragOffsetY: 0,
  showGrid: false,
  gridOpacity: 0.18,
  snapToGrid: false,
  measure: {
    dragging: false,
    start: null,
    end: null
  },
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
  night: normalizeNightSettings(DEFAULT_NIGHT),
  history: [],
  historyIndex: -1,
  maxHistory: 50,
  statusHoldUntil: 0
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
const roomFacesPanel = document.querySelector(".room-faces-panel");
const layersPanel = document.getElementById("layersPanel");
const propertiesPanel = document.getElementById("propertiesPanel");
const nightOverlayPanel = document.getElementById("nightOverlayPanel");
const sessionExportMeta = document.getElementById("sessionExportMeta");
const baseImageMeta = document.getElementById("baseImageMeta");
const hoverTooltip = document.getElementById("hoverTooltip");
let tooltipTimer = null;
let tooltipAnchor = null;
let tooltipPointer = { x: 0, y: 0 };

function setStatus(text, holdMs = 2500) {
  statusEl.textContent = text;
  state.statusHoldUntil = Date.now() + holdMs;
}

function setWorkflowStatus(text) {
  if (Date.now() < state.statusHoldUntil) return;
  statusEl.textContent = text;
}

function updateCanvasSizeReadouts() {
  const exportSize = document.getElementById("exportCanvasSize");
  const editSize = document.getElementById("editCanvasSize");
  if (exportSize) exportSize.textContent = `${Math.round(state.width)}x${Math.round(state.height)}`;
  if (editSize) editSize.textContent = `${Math.round(state.editWidth)}x${Math.round(state.editHeight)}`;
}

function updateBaseImageMeta() {
  updateCanvasSizeReadouts();
  if (!state.base) {
    baseImageMeta.textContent = "Reference image: none";
    return;
  }
  const prepared = preparedRoomMetrics();
  const trim = prepared.trim;
  baseImageMeta.textContent =
    `Reference image: ${state.base.width}x${state.base.height}; ` +
    `trim ${trim.x},${trim.y},${trim.width}x${trim.height}; ` +
    `export ${state.width}x${state.height}; edit ${state.editWidth}x${state.editHeight}; screen ${GAME_SCREEN_WIDTH}x${GAME_SCREEN_HEIGHT}`;
}

function normalizedEditZoom(value) {
  const percent = Number(value);
  const safePercent = Number.isFinite(percent) ? percent : 100;
  return clampNumber(Math.round(safePercent / 25) * 25, 25, 300) / 100;
}

function updateZoomControls() {
  const input = document.getElementById("editZoom");
  const reset = document.getElementById("zoomReset");
  if (input) input.value = Math.round(state.editZoom * 100);
  if (reset) reset.textContent = `${Math.round(state.editZoom * 100)}%`;
}

function updateGridOpacityLabel() {
  const label = document.getElementById("gridOpacityValue");
  if (label) label.textContent = `${Math.round(state.gridOpacity * 100)}%`;
}

function controlForTooltip(anchor) {
  if (!anchor) return null;
  if (anchor.id && CONTROL_TOOLTIPS[anchor.id]) return anchor;
  return anchor.querySelector?.("input[id], select[id], button[id], canvas[id]") || null;
}

function tooltipTextForElement(anchor) {
  if (!anchor) return "";
  const direct = anchor.dataset?.tooltip;
  if (direct) return direct;
  const control = controlForTooltip(anchor);
  if (control?.id && CONTROL_TOOLTIPS[control.id]) return CONTROL_TOOLTIPS[control.id];
  for (const [selector, text] of CONTROL_TOOLTIP_RULES) {
    if (anchor.matches?.(selector) || anchor.closest?.(selector)) return text;
  }
  return anchor.getAttribute?.("title") || "";
}

function tooltipAnchorForTarget(target) {
  if (!target?.closest) return null;
  const candidate = target.closest("[data-tooltip], button, label, select, input, canvas, summary, .asset-visibility, .drop-zone");
  if (!candidate) return null;
  return tooltipTextForElement(candidate) ? candidate : null;
}

function positionTooltip() {
  if (!hoverTooltip || hoverTooltip.hidden) return;
  const margin = 8;
  const offset = 14;
  const rect = hoverTooltip.getBoundingClientRect();
  let left = tooltipPointer.x + offset;
  let top = tooltipPointer.y + offset;
  if (left + rect.width > window.innerWidth - margin) {
    left = window.innerWidth - rect.width - margin;
  }
  if (top + rect.height > window.innerHeight - margin) {
    top = tooltipPointer.y - rect.height - offset;
  }
  hoverTooltip.style.left = `${Math.max(margin, left)}px`;
  hoverTooltip.style.top = `${Math.max(margin, top)}px`;
}

function showTooltip() {
  if (!hoverTooltip || !tooltipAnchor) return;
  const text = tooltipTextForElement(tooltipAnchor);
  if (!text) return;
  hoverTooltip.textContent = text;
  hoverTooltip.hidden = false;
  positionTooltip();
}

function hideTooltip() {
  if (tooltipTimer) {
    clearTimeout(tooltipTimer);
    tooltipTimer = null;
  }
  tooltipAnchor = null;
  if (hoverTooltip) hoverTooltip.hidden = true;
}

function scheduleTooltip(event) {
  const anchor = tooltipAnchorForTarget(event.target);
  if (!anchor) {
    hideTooltip();
    return;
  }
  tooltipPointer = { x: event.clientX, y: event.clientY };
  if (tooltipAnchor === anchor) {
    positionTooltip();
    return;
  }
  hideTooltip();
  tooltipAnchor = anchor;
  tooltipTimer = window.setTimeout(() => {
    tooltipTimer = null;
    showTooltip();
  }, TOOLTIP_DELAY_MS);
}

function setupHoverTooltips() {
  document.addEventListener("pointerover", scheduleTooltip, true);
  document.addEventListener("pointermove", (event) => {
    if (!tooltipAnchor) return;
    tooltipPointer = { x: event.clientX, y: event.clientY };
    positionTooltip();
  }, true);
  document.addEventListener("pointerout", (event) => {
    if (!tooltipAnchor) return;
    const next = event.relatedTarget;
    if (next && tooltipAnchor.contains?.(next)) return;
    hideTooltip();
  }, true);
  document.addEventListener("scroll", hideTooltip, true);
  document.addEventListener("keydown", hideTooltip, true);
}

function setEditZoom(percent) {
  state.editZoom = normalizedEditZoom(percent);
  updateCanvasDisplaySize();
}

function updateCanvasDisplaySize() {
  const displayWidth = Math.max(1, Math.round(state.editWidth * state.editZoom));
  canvas.style.width = `${displayWidth}px`;
  canvas.style.height = "auto";
  canvas.style.aspectRatio = `${state.editWidth} / ${state.editHeight}`;
  previewCanvas.style.width = `${Math.min(PREVIEW_MAX_WIDTH, state.width)}px`;
  previewCanvas.style.height = "auto";
  previewCanvas.style.aspectRatio = `${state.width} / ${state.height}`;
  updateZoomControls();
  updateCanvasSizeReadouts();
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

function clampCanvasPoint(point) {
  return {
    x: clampNumber(Math.round(point.x), 0, state.editWidth),
    y: clampNumber(Math.round(point.y), 0, state.editHeight)
  };
}

function formatPixelValue(value) {
  if (value == null || !Number.isFinite(value)) return "--";
  const rounded = Math.round(value * 10) / 10;
  return `${Number.isInteger(rounded) ? rounded : rounded.toFixed(1)} px`;
}

function currentMeasurement() {
  if (!state.measure.start || !state.measure.end) return null;
  const start = state.measure.start;
  const end = state.measure.end;
  const targetStart = editToTargetPoint(start);
  const targetEnd = editToTargetPoint(end);
  return {
    start,
    end,
    top: Math.min(start.y, end.y),
    bottom: Math.max(start.y, end.y),
    canvasHeight: Math.abs(end.y - start.y),
    gameHeight: Math.abs(targetEnd.y - targetStart.y),
    gameTop: Math.min(targetStart.y, targetEnd.y),
    gameBottom: Math.max(targetStart.y, targetEnd.y)
  };
}

function measurementStatusText() {
  const measurement = currentMeasurement();
  if (!measurement) return "Measure height tool active.";
  return `Height: ${formatPixelValue(measurement.canvasHeight)} canvas, ${formatPixelValue(measurement.gameHeight)} game.`;
}

function updateMeasurePanel() {
  const measurement = currentMeasurement();
  const canvasHeight = document.getElementById("measureCanvasHeight");
  const gameHeight = document.getElementById("measureGameHeight");
  const yRange = document.getElementById("measureYRange");
  const clearMeasure = document.getElementById("clearMeasure");
  const toolboxMeta = document.getElementById("toolboxMeta");

  canvasHeight.textContent = measurement ? formatPixelValue(measurement.canvasHeight) : "--";
  gameHeight.textContent = measurement ? formatPixelValue(measurement.gameHeight) : "--";
  yRange.textContent = measurement ?
    `canvas ${Math.round(measurement.top)}-${Math.round(measurement.bottom)} / game ${Math.round(measurement.gameTop * 10) / 10}-${Math.round(measurement.gameBottom * 10) / 10}` :
    "--";
  clearMeasure.disabled = !measurement;
  toolboxMeta.textContent = state.toolMode === "measureHeight" ? "Measure height" : "Select";
}

function updateToolButtons() {
  document.getElementById("selectTool").classList.toggle("active", state.toolMode === "select");
  document.getElementById("measureHeightTool").classList.toggle("active", state.toolMode === "measureHeight");
  canvas.classList.toggle("measure-tool", state.toolMode === "measureHeight");
  updateMeasurePanel();
}

function setToolMode(mode) {
  state.toolMode = mode === "measureHeight" ? "measureHeight" : "select";
  state.measure.dragging = false;
  canvas.classList.remove("dragging");
  updateToolButtons();
  render();
  setStatus(state.toolMode === "measureHeight" ? "Measure height tool active." : "Select tool active.");
}

function setMeasureStart(point) {
  const clamped = clampCanvasPoint(point);
  state.measure.start = clamped;
  state.measure.end = { ...clamped };
}

function setMeasureEnd(point) {
  if (!state.measure.start) {
    setMeasureStart(point);
    return;
  }
  const clamped = clampCanvasPoint(point);
  state.measure.end = {
    x: state.measure.start.x,
    y: clamped.y
  };
}

function clearMeasurement() {
  state.measure.dragging = false;
  state.measure.start = null;
  state.measure.end = null;
  canvas.classList.remove("dragging");
  updateMeasurePanel();
  render();
  setStatus("Measure cleared.");
}

function colorDistanceRgb(a, b) {
  const dr = a[0] - b[0];
  const dg = a[1] - b[1];
  const db = a[2] - b[2];
  return Math.sqrt(dr * dr + dg * dg + db * db);
}

function detectTrimBox(image) {
  const sourceCanvas = document.createElement("canvas");
  sourceCanvas.width = image.naturalWidth;
  sourceCanvas.height = image.naturalHeight;
  const sourceCtx = sourceCanvas.getContext("2d", { willReadFrequently: true });
  sourceCtx.imageSmoothingEnabled = false;
  sourceCtx.drawImage(image, 0, 0);

  const { width, height } = sourceCanvas;
  const { data } = sourceCtx.getImageData(0, 0, width, height);
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
  for (let y = 0; y < height; ++y) {
    for (let x = 0; x < width; ++x) {
      const index = (y * width + x) * 4;
      const color = [data[index], data[index + 1], data[index + 2]];
      if (colorDistanceRgb(color, bg) <= TRIM_THRESHOLD) continue;
      minX = Math.min(minX, x);
      minY = Math.min(minY, y);
      maxX = Math.max(maxX, x);
      maxY = Math.max(maxY, y);
    }
  }

  if (maxX < minX || maxY < minY) {
    return { x: 0, y: 0, width, height };
  }

  const x = Math.max(0, minX - TRIM_PADDING);
  const y = Math.max(0, minY - TRIM_PADDING);
  const right = Math.min(width, maxX + TRIM_PADDING + 1);
  const bottom = Math.min(height, maxY + TRIM_PADDING + 1);
  return { x, y, width: right - x, height: bottom - y };
}

function fallbackTrimBox() {
  return { x: 0, y: 0, width: state.base?.width || state.width, height: state.base?.height || state.height };
}

function preparedRoomMetrics() {
  const trim = state.baseTrim || fallbackTrimBox();
  const width = GAME_SCREEN_WIDTH;
  const scale = width / Math.max(1, trim.width);
  const height = Math.max(1, Math.round(trim.height * scale));
  const canvasHeight = Math.max(GAME_SCREEN_HEIGHT, height);
  const y = Math.max(0, Math.floor((canvasHeight - height) / 2));
  return { width, height, y, scale, trim, canvasWidth: GAME_SCREEN_WIDTH, canvasHeight };
}

function editRoomMetrics(prepared = preparedRoomMetrics()) {
  const sourcePerTarget = 1 / Math.max(0.000001, prepared.scale);
  const roomY = Math.max(0, Math.round(prepared.y * sourcePerTarget));
  const canvasHeight = Math.max(
    Math.round(prepared.canvasHeight * sourcePerTarget),
    roomY + prepared.trim.height
  );
  return {
    x: 0,
    y: roomY,
    width: Math.max(1, prepared.trim.width),
    height: Math.max(1, prepared.trim.height),
    canvasWidth: Math.max(1, prepared.trim.width),
    canvasHeight: Math.max(1, canvasHeight),
    sourcePerTarget
  };
}

function syncCanvasSizeToPreparedRoom() {
  const prepared = preparedRoomMetrics();
  const edit = editRoomMetrics(prepared);
  state.width = prepared.canvasWidth;
  state.height = prepared.canvasHeight;
  state.editWidth = edit.canvasWidth;
  state.editHeight = edit.canvasHeight;
  state.aspectRatio = state.width / Math.max(1, state.height);
  setRoomInputs(state.width, state.height);
}

function sourcePointToTargetPoint(point, prepared = preparedRoomMetrics()) {
  return {
    x: (point.x - prepared.trim.x) * prepared.scale,
    y: (point.y - prepared.trim.y) * prepared.scale + prepared.y
  };
}

function targetPointToSourcePoint(point, prepared = preparedRoomMetrics()) {
  return {
    x: (point.x / Math.max(0.000001, prepared.scale)) + prepared.trim.x,
    y: ((point.y - prepared.y) / Math.max(0.000001, prepared.scale)) + prepared.trim.y
  };
}

function coerceTrimBox(raw) {
  if (!raw) return null;
  const x = Number(raw.x);
  const y = Number(raw.y);
  const width = Number(raw.width);
  const height = Number(raw.height);
  if (![x, y, width, height].every(Number.isFinite) || width <= 0 || height <= 0) return null;
  return {
    x: Math.round(x),
    y: Math.round(y),
    width: Math.round(width),
    height: Math.round(height)
  };
}

function faceTargetPoints(face) {
  return face.points.map((point) => {
    const target = editToTargetPoint(point);
    return {
      x: Math.round(clampNumber(target.x, 0, state.width)),
      y: Math.round(clampNumber(target.y, 0, state.height))
    };
  });
}

function faceSourcePoints(face) {
  const prepared = preparedRoomMetrics();
  return face.points.map((point) => {
    const source = targetPointToSourcePoint(editToTargetPoint(point), prepared);
    return [Math.round(source.x), Math.round(source.y)];
  });
}

function screenFrameRect() {
  return {
    x: 0,
    y: 0,
    w: Math.min(GAME_SCREEN_WIDTH, state.width),
    h: Math.min(GAME_SCREEN_HEIGHT, state.height)
  };
}

function snapValue(value, grid = 8) {
  if (!state.snapToGrid) return value;
  return Math.round(value / grid) * grid;
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
  updateCanvasSizeReadouts();
}

function applyRoomSizeInput(changedField) {
  syncCanvasSizeToPreparedRoom();
  updateBaseImageMeta();
  render();
  commitHistory();
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

function drawImagePreview(canvasEl, image) {
  const maxW = 300;
  const maxH = 260;
  const sourceWidth = image.naturalWidth || image.width;
  const sourceHeight = image.naturalHeight || image.height;
  const scale = Math.min(1, maxW / Math.max(1, sourceWidth), maxH / Math.max(1, sourceHeight));
  canvasEl.width = Math.max(1, Math.round(sourceWidth * scale));
  canvasEl.height = Math.max(1, Math.round(sourceHeight * scale));
  const previewCtx = canvasEl.getContext("2d");
  previewCtx.clearRect(0, 0, canvasEl.width, canvasEl.height);
  previewCtx.imageSmoothingEnabled = false;
  previewCtx.drawImage(image, 0, 0, canvasEl.width, canvasEl.height);
}

function averageCornerColor(data, width, height) {
  const size = Math.min(8, width, height);
  const corners = [
    [0, 0],
    [width - size, 0],
    [0, height - size],
    [width - size, height - size]
  ];
  const sum = [0, 0, 0];
  let count = 0;
  for (const [startX, startY] of corners) {
    for (let y = startY; y < startY + size; ++y) {
      for (let x = startX; x < startX + size; ++x) {
        const index = (y * width + x) * 4;
        if (data[index + 3] < 8) continue;
        sum[0] += data[index];
        sum[1] += data[index + 1];
        sum[2] += data[index + 2];
        count += 1;
      }
    }
  }
  if (!count) return [0, 255, 0];
  return [sum[0] / count, sum[1] / count, sum[2] / count];
}

function clampByte(value) {
  return Math.max(0, Math.min(255, Math.round(value)));
}

function rgbToHex(color) {
  return "#" + color.map((value) => clampByte(value).toString(16).padStart(2, "0")).join("");
}

function hexToRgb(value) {
  const hex = String(value || "").replace("#", "");
  if (!/^[0-9a-fA-F]{6}$/.test(hex)) return [3, 249, 3];
  return [
    parseInt(hex.slice(0, 2), 16),
    parseInt(hex.slice(2, 4), 16),
    parseInt(hex.slice(4, 6), 16)
  ];
}

function sourceSizeOf(image) {
  return {
    width: image.naturalWidth || image.width,
    height: image.naturalHeight || image.height
  };
}

function previewPointerToSource(previewCanvas, event, sourceWidth, sourceHeight) {
  const rect = previewCanvas.getBoundingClientRect();
  const previewX = (event.clientX - rect.left) * previewCanvas.width / Math.max(1, rect.width);
  const previewY = (event.clientY - rect.top) * previewCanvas.height / Math.max(1, rect.height);
  return {
    x: Math.max(0, Math.min(sourceWidth - 1, Math.floor(previewX * sourceWidth / Math.max(1, previewCanvas.width)))),
    y: Math.max(0, Math.min(sourceHeight - 1, Math.floor(previewY * sourceHeight / Math.max(1, previewCanvas.height))))
  };
}

function sampleImageColor(image, previewCanvas, event) {
  const { width, height } = sourceSizeOf(image);
  const sourcePoint = previewPointerToSource(previewCanvas, event, width, height);

  const sampleCanvas = document.createElement("canvas");
  sampleCanvas.width = width;
  sampleCanvas.height = height;
  const sampleCtx = sampleCanvas.getContext("2d", { willReadFrequently: true });
  sampleCtx.imageSmoothingEnabled = false;
  sampleCtx.drawImage(image, 0, 0);
  const pixel = sampleCtx.getImageData(sourcePoint.x, sourcePoint.y, 1, 1).data;
  return [pixel[0], pixel[1], pixel[2]];
}

function cornerKeyColor(image) {
  const { width, height } = sourceSizeOf(image);
  const sourceCanvas = document.createElement("canvas");
  sourceCanvas.width = width;
  sourceCanvas.height = height;
  const sourceCtx = sourceCanvas.getContext("2d", { willReadFrequently: true });
  sourceCtx.imageSmoothingEnabled = false;
  sourceCtx.drawImage(image, 0, 0);
  const { data } = sourceCtx.getImageData(0, 0, sourceCanvas.width, sourceCanvas.height);
  return averageCornerColor(data, sourceCanvas.width, sourceCanvas.height);
}

function cloneImageData(imageData) {
  return new ImageData(new Uint8ClampedArray(imageData.data), imageData.width, imageData.height);
}

function cutoutImageData(imageData, keyColor, tolerance, feather) {
  const output = cloneImageData(imageData);
  const { data } = output;
  const removeRange = Math.max(0, Number(tolerance) || 0);
  const featherRange = Math.max(0, Number(feather) || 0);
  const softEnd = removeRange + featherRange;

  for (let i = 0; i < data.length; i += 4) {
    const r = data[i];
    const g = data[i + 1];
    const b = data[i + 2];
    const a = data[i + 3];
    if (a === 0) continue;

    const distanceToKey = colorDistanceRgb([r, g, b], keyColor);
    if (distanceToKey <= removeRange) {
      data[i + 3] = 0;
      continue;
    }

    if (featherRange > 0 && distanceToKey <= softEnd) {
      const alphaRatio = (distanceToKey - removeRange) / featherRange;
      data[i + 3] = Math.min(data[i + 3], Math.round(a * alphaRatio));
    }
  }

  return output;
}

function pixelColorFromImageData(imageData, x, y) {
  const index = (y * imageData.width + x) * 4;
  const data = imageData.data;
  return [data[index], data[index + 1], data[index + 2], data[index + 3]];
}

function eraseConnectedRegion(imageData, startX, startY, tolerance) {
  const width = imageData.width;
  const height = imageData.height;
  const data = imageData.data;
  const startIndex = startY * width + startX;
  const startOffset = startIndex * 4;
  if (data[startOffset + 3] === 0) return { removed: 0, color: [data[startOffset], data[startOffset + 1], data[startOffset + 2]] };

  const seedColor = [data[startOffset], data[startOffset + 1], data[startOffset + 2]];
  const removeRange = Math.max(0, Number(tolerance) || 0);
  const visited = new Uint8Array(width * height);
  const stack = [startIndex];
  let removed = 0;

  while (stack.length) {
    const index = stack.pop();
    if (visited[index]) continue;
    visited[index] = 1;
    const offset = index * 4;
    if (data[offset + 3] === 0) continue;

    const color = [data[offset], data[offset + 1], data[offset + 2]];
    if (colorDistanceRgb(color, seedColor) > removeRange) continue;

    data[offset + 3] = 0;
    removed += 1;

    const x = index % width;
    const y = Math.floor(index / width);
    if (x > 0) stack.push(index - 1);
    if (x < width - 1) stack.push(index + 1);
    if (y > 0) stack.push(index - width);
    if (y < height - 1) stack.push(index + width);
  }

  return { removed, color: seedColor };
}

function applyBrushToImageData(imageData, originalImageData, centerX, centerY, size, mode) {
  const width = imageData.width;
  const height = imageData.height;
  const data = imageData.data;
  const original = originalImageData.data;
  const brushSize = Math.max(1, Math.round(Number(size) || 1));
  const minX = Math.max(0, centerX - Math.floor((brushSize - 1) / 2));
  const minY = Math.max(0, centerY - Math.floor((brushSize - 1) / 2));
  const maxX = Math.min(width - 1, minX + brushSize - 1);
  const maxY = Math.min(height - 1, minY + brushSize - 1);
  let changed = 0;

  for (let y = minY; y <= maxY; ++y) {
    for (let x = minX; x <= maxX; ++x) {
      const offset = (y * width + x) * 4;
      if (mode === "restore") {
        const differs =
          data[offset] !== original[offset] ||
          data[offset + 1] !== original[offset + 1] ||
          data[offset + 2] !== original[offset + 2] ||
          data[offset + 3] !== original[offset + 3];
        if (!differs) continue;
        data[offset] = original[offset];
        data[offset + 1] = original[offset + 1];
        data[offset + 2] = original[offset + 2];
        data[offset + 3] = original[offset + 3];
      } else {
        if (data[offset + 3] === 0) continue;
        data[offset + 3] = 0;
      }
      changed += 1;
    }
  }

  return changed;
}

function promptFurnitureCutout(loaded) {
  return new Promise((resolve) => {
    const modal = document.getElementById("cutoutModal");
    const beforeCanvas = document.getElementById("cutoutBefore");
    const afterCanvas = document.getElementById("cutoutAfter");
    const fileName = document.getElementById("cutoutFileName");
    const hint = document.getElementById("cutoutHint");
    const cancel = document.getElementById("cutoutCancel");
    const useOriginal = document.getElementById("cutoutUseOriginal");
    const runCutout = document.getElementById("cutoutRun");
    const useResult = document.getElementById("cutoutUseResult");
    const resetEdit = document.getElementById("cutoutReset");
    const toolInput = document.getElementById("cutoutTool");
    const colorInput = document.getElementById("cutoutColor");
    const toleranceInput = document.getElementById("cutoutTolerance");
    const toleranceValue = document.getElementById("cutoutToleranceValue");
    const featherInput = document.getElementById("cutoutFeather");
    const featherValue = document.getElementById("cutoutFeatherValue");
    const brushInput = document.getElementById("cutoutBrushSize");
    const brushValue = document.getElementById("cutoutBrushSizeValue");

    const width = loaded.img.naturalWidth;
    const height = loaded.img.naturalHeight;
    const originalCanvas = document.createElement("canvas");
    originalCanvas.width = width;
    originalCanvas.height = height;
    const originalCtx = originalCanvas.getContext("2d", { willReadFrequently: true });
    originalCtx.imageSmoothingEnabled = false;
    originalCtx.drawImage(loaded.img, 0, 0);
    const originalImageData = originalCtx.getImageData(0, 0, width, height);

    const workCanvas = document.createElement("canvas");
    workCanvas.width = width;
    workCanvas.height = height;
    const workCtx = workCanvas.getContext("2d", { willReadFrequently: true });
    workCtx.imageSmoothingEnabled = false;
    let workImageData = cloneImageData(originalImageData);
    let hasEdits = false;
    let isPainting = false;
    let settled = false;

    const updateRangeLabels = () => {
      toleranceValue.textContent = toleranceInput.value;
      featherValue.textContent = featherInput.value;
      brushValue.textContent = brushInput.value;
    };
    const toolDescription = () => {
      if (toolInput.value === "pick") return "Click either image to pick a color.";
      if (toolInput.value === "wand") return "Click the after image to remove one connected color region.";
      if (toolInput.value === "restore") return "Drag on the after image to restore pixels from the original.";
      return "Drag on the after image to erase pixels manually.";
    };
    const updateToolClass = () => {
      afterCanvas.classList.remove("pick-tool", "wand-tool", "brush-tool");
      beforeCanvas.classList.remove("pick-tool", "wand-tool", "brush-tool");
      beforeCanvas.classList.add("pick-tool");
      if (toolInput.value === "pick") afterCanvas.classList.add("pick-tool");
      if (toolInput.value === "wand") afterCanvas.classList.add("wand-tool");
      if (toolInput.value === "erase" || toolInput.value === "restore") afterCanvas.classList.add("brush-tool");
    };
    const updateToolHint = () => {
      hint.textContent = `${toolDescription()} Cutout applies to the current after image, so repeated cutouts combine.`;
    };
    const renderWorkPreview = () => {
      workCtx.putImageData(workImageData, 0, 0);
      drawImagePreview(afterCanvas, workCanvas);
      updateToolClass();
    };
    const markEdited = (message) => {
      hasEdits = true;
      useResult.disabled = false;
      if (message) hint.textContent = message;
    };
    const setWorkImageData = (imageData, edited, message) => {
      workImageData = imageData;
      renderWorkPreview();
      if (edited) {
        markEdited(message);
      } else {
        useResult.disabled = !hasEdits;
      }
    };
    const resultFromWorkCanvas = async () => {
      workCtx.putImageData(workImageData, 0, 0);
      const dataUrl = workCanvas.toDataURL("image/png");
      return {
        img: await loadImageSource(dataUrl),
        dataUrl,
        cutoutApplied: hasEdits
      };
    };
    const applyWand = (event) => {
      const point = previewPointerToSource(afterCanvas, event, width, height);
      const removed = eraseConnectedRegion(workImageData, point.x, point.y, Number(toleranceInput.value));
      colorInput.value = rgbToHex(removed.color);
      if (!removed.removed) {
        renderWorkPreview();
        hint.textContent = "Magic wand found no opaque pixels at that point.";
        return;
      }
      setWorkImageData(workImageData, true, `Magic wand removed ${removed.removed} connected pixel(s).`);
    };
    const applyBrush = (event) => {
      const point = previewPointerToSource(afterCanvas, event, width, height);
      const changed = applyBrushToImageData(
        workImageData,
        originalImageData,
        point.x,
        point.y,
        Number(brushInput.value),
        toolInput.value
      );
      if (!changed) return;
      setWorkImageData(
        workImageData,
        true,
        `${toolInput.value === "restore" ? "Restored" : "Erased"} ${changed} pixel(s).`
      );
    };
    const cleanup = () => {
      cancel.onclick = null;
      useOriginal.onclick = null;
      runCutout.onclick = null;
      useResult.onclick = null;
      resetEdit.onclick = null;
      beforeCanvas.onclick = null;
      afterCanvas.onclick = null;
      afterCanvas.onpointerdown = null;
      afterCanvas.onpointermove = null;
      afterCanvas.onpointerup = null;
      afterCanvas.onpointercancel = null;
      afterCanvas.onpointerleave = null;
      toolInput.onchange = null;
      colorInput.oninput = null;
      toleranceInput.oninput = null;
      featherInput.oninput = null;
      brushInput.oninput = null;
      beforeCanvas.classList.remove("pick-tool", "wand-tool", "brush-tool");
      afterCanvas.classList.remove("pick-tool", "wand-tool", "brush-tool");
      modal.hidden = true;
    };
    const settle = (value) => {
      if (settled) return;
      settled = true;
      cleanup();
      resolve(value);
    };

    fileName.textContent = `${loaded.file.name} (${loaded.img.naturalWidth}x${loaded.img.naturalHeight})`;
    colorInput.value = rgbToHex(cornerKeyColor(loaded.img));
    toolInput.value = "pick";
    toleranceInput.value = "28";
    featherInput.value = "8";
    brushInput.value = "1";
    updateRangeLabels();
    updateToolHint();
    useResult.disabled = true;
    drawImagePreview(beforeCanvas, loaded.img);
    renderWorkPreview();
    modal.hidden = false;

    beforeCanvas.onclick = (event) => {
      colorInput.value = rgbToHex(sampleImageColor(loaded.img, beforeCanvas, event));
      updateToolHint();
    };
    afterCanvas.onclick = (event) => {
      if (toolInput.value === "pick") {
        const point = previewPointerToSource(afterCanvas, event, width, height);
        colorInput.value = rgbToHex(pixelColorFromImageData(workImageData, point.x, point.y).slice(0, 3));
        updateToolHint();
      } else if (toolInput.value === "wand") {
        applyWand(event);
      }
    };
    afterCanvas.onpointerdown = (event) => {
      if (toolInput.value !== "erase" && toolInput.value !== "restore") return;
      event.preventDefault();
      isPainting = true;
      afterCanvas.setPointerCapture(event.pointerId);
      applyBrush(event);
    };
    afterCanvas.onpointermove = (event) => {
      if (!isPainting) return;
      applyBrush(event);
    };
    afterCanvas.onpointerup = (event) => {
      isPainting = false;
      if (afterCanvas.hasPointerCapture(event.pointerId)) {
        afterCanvas.releasePointerCapture(event.pointerId);
      }
    };
    afterCanvas.onpointercancel = () => {
      isPainting = false;
    };
    afterCanvas.onpointerleave = () => {
      isPainting = false;
    };
    toolInput.onchange = () => {
      updateToolClass();
      updateToolHint();
    };
    colorInput.oninput = updateToolHint;
    toleranceInput.oninput = () => {
      updateRangeLabels();
      updateToolHint();
    };
    featherInput.oninput = () => {
      updateRangeLabels();
      updateToolHint();
    };
    brushInput.oninput = () => {
      updateRangeLabels();
      updateToolHint();
    };

    cancel.onclick = () => settle(null);

    useOriginal.onclick = () => settle({
      img: loaded.img,
      dataUrl: loaded.dataUrl,
      cutoutApplied: false
    });

    resetEdit.onclick = () => {
      hasEdits = false;
      setWorkImageData(cloneImageData(originalImageData), false, null);
      useResult.disabled = true;
      updateToolHint();
    };

    runCutout.onclick = async () => {
      runCutout.disabled = true;
      hint.textContent = "Cutout running...";
      try {
        const nextImageData = cutoutImageData(
          workImageData,
          hexToRgb(colorInput.value),
          Number(toleranceInput.value),
          Number(featherInput.value)
        );
        setWorkImageData(nextImageData, true, "Cutout applied to the current after image. Pick another edge color and run cutout again to combine cleanup.");
      } catch (error) {
        console.error(error);
        hint.textContent = "Cutout failed. Use original or try another image.";
      } finally {
        runCutout.disabled = false;
      }
    };

    useResult.onclick = async () => {
      if (!hasEdits) return;
      useResult.disabled = true;
      hint.textContent = "Preparing edited PNG...";
      try {
        settle(await resultFromWorkCanvas());
      } catch (error) {
        console.error(error);
        hint.textContent = "Export failed. Use original or try again.";
        useResult.disabled = false;
      }
    };
  });
}

function projectSpriteCatalog() {
  return Array.isArray(window.STICKMON_PROJECT_SPRITES) ? window.STICKMON_PROJECT_SPRITES : [];
}

function selectedProjectSpriteEntry() {
  const select = document.getElementById("spriteSpeciesSelect");
  const speciesId = Number(select.value) || 0;
  return projectSpriteCatalog().find((entry) => entry.speciesId === speciesId) || null;
}

function selectedProjectSpriteFrame() {
  const entry = selectedProjectSpriteEntry();
  if (!entry) return null;
  const action = document.getElementById("spriteActionSelect").value;
  const framePath = document.getElementById("spriteFrameSelect").value;
  const frames = entry.actions[action] || [];
  return frames.find((frame) => frame.path === framePath) || frames[0] || null;
}

function updateProjectSpriteMeta() {
  const meta = document.getElementById("projectSpriteMeta");
  const frame = selectedProjectSpriteFrame();
  if (!frame) {
    meta.textContent = "No generated project sprites found.";
    return;
  }
  meta.textContent = `${frame.kindName}, ${frame.width}x${frame.height}`;
}

function updateSpriteActionOptions() {
  const entry = selectedProjectSpriteEntry();
  const actionSelect = document.getElementById("spriteActionSelect");
  actionSelect.innerHTML = "";
  if (!entry || !entry.actions) {
    updateSpriteFrameOptions();
    return;
  }
  for (const action of Object.keys(entry.actions)) {
    const option = document.createElement("option");
    option.value = action;
    option.textContent = action;
    actionSelect.appendChild(option);
  }
  updateSpriteFrameOptions();
}

function updateSpriteFrameOptions() {
  const entry = selectedProjectSpriteEntry();
  const actionSelect = document.getElementById("spriteActionSelect");
  const frameSelect = document.getElementById("spriteFrameSelect");
  const action = actionSelect.value;
  frameSelect.innerHTML = "";
  if (!entry || !entry.actions || !entry.actions[action]) {
    updateProjectSpriteMeta();
    return;
  }
  for (const frame of entry.actions[action]) {
    const option = document.createElement("option");
    option.value = frame.path;
    option.textContent = frame.kindName;
    frameSelect.appendChild(option);
  }
  updateProjectSpriteMeta();
}

function initProjectSpriteControls() {
  const speciesSelect = document.getElementById("spriteSpeciesSelect");
  const addButton = document.getElementById("addProjectSprite");
  const catalog = projectSpriteCatalog();
  speciesSelect.innerHTML = "";
  for (const entry of catalog) {
    const option = document.createElement("option");
    option.value = String(entry.speciesId);
    option.textContent = entry.name;
    speciesSelect.appendChild(option);
  }
  const empty = catalog.length === 0;
  speciesSelect.disabled = empty;
  document.getElementById("spriteActionSelect").disabled = empty;
  document.getElementById("spriteFrameSelect").disabled = empty;
  addButton.disabled = empty;
  speciesSelect.addEventListener("change", updateSpriteActionOptions);
  document.getElementById("spriteActionSelect").addEventListener("change", updateSpriteFrameOptions);
  document.getElementById("spriteFrameSelect").addEventListener("change", updateProjectSpriteMeta);
  addButton.addEventListener("click", () => {
    addProjectSpritePreview();
  });
  updateSpriteActionOptions();
}

async function addProjectSpritePreview() {
  const entry = selectedProjectSpriteEntry();
  const frame = selectedProjectSpriteFrame();
  if (!entry || !frame) return;
  const img = await loadImageSource(frame.path);
  const maxSize = 64;
  const scale = Math.min(
    maxSize / Math.max(1, frame.width),
    maxSize / Math.max(1, frame.height)
  );
  const targetW = frame.width * scale;
  const targetH = frame.height * scale;
  const frameRect = screenFrameRect();
  const targetX = frameRect.x + (frameRect.w - targetW) / 2;
  const targetY = frameRect.y + (frameRect.h - targetH) / 2;
  const item = {
    id: `f${state.nextId++}`,
    name: entry.name,
    fileName: frame.file,
    img,
    dataUrl: frame.path,
    sourceWidth: frame.width,
    sourceHeight: frame.height,
    x: Math.round(targetX),
    y: Math.round(targetY),
    scale,
    scaleX: scale,
    scaleY: scale,
    aspectLocked: true,
    sourceScale: 1,
    opacity: 1,
    z: 30,
    visible: true,
    anchor: "top_left",
    slot: "sprite_preview",
    layer: "sprite_preview",
    source: "project_sprite",
    kind: "low_prop",
    heightPx: 16,
    footprint: "ellipse",
    castsShadow: true,
    receivesShadow: true,
    occludesSprite: false,
    sortY: Math.round(targetY + targetH),
    speciesId: entry.speciesId,
    action: document.getElementById("spriteActionSelect").value,
    frame: frame.kindName
  };
  state.items.push(item);
  state.selectedId = item.id;
  state.selectedFaceId = null;
  state.selectedPointIndex = null;
  state.selectedPointId = null;
  state.editMode = "furniture";
  updateEditModeButtons();
  setStatus(`Added sprite preview: ${entry.name} ${frame.kindName}.`);
  refreshList();
  refreshSelectedPanel();
  refreshFaceList();
  refreshSelectedFacePanel();
  render();
  commitHistory();
}

async function updateProjectSpriteFrame(item, speciesId, action, framePath) {
  const entry = projectSpriteCatalog().find((e) => e.speciesId === speciesId);
  if (!entry || !entry.actions || !entry.actions[action]) return;
  const frame = entry.actions[action].find((f) => f.path === framePath);
  if (!frame) return;
  const img = await loadImageSource(frame.path);
  item.name = entry.name;
  item.fileName = frame.file;
  item.img = img;
  item.dataUrl = frame.path;
  item.sourceWidth = frame.width;
  item.sourceHeight = frame.height;
  item.speciesId = speciesId;
  item.action = action;
  item.frame = frame.kindName;
  item.sortY = autoSortY(item);
  render();
  commitHistory();
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
  state.baseTrim = detectTrimBox(loaded.img);
  const prepared = preparedRoomMetrics();
  const detectedScale = Math.max(0.01, Math.round((prepared.trim.width / GAME_SCREEN_WIDTH) * 100) / 100);
  state.sourceScale = detectedScale;
  state.baseFit = BASE_FIT_WIDTH;
  syncCanvasSizeToPreparedRoom();
  document.getElementById("sourceScale").value = detectedScale;
  document.getElementById("baseFit").value = BASE_FIT_WIDTH;
  updateBaseImageMeta();
  setStatus(`Background loaded: ${file.name} (${state.base.width}x${state.base.height}), fit to ${GAME_SCREEN_WIDTH}x${prepared.height}, canvas ${state.width}x${state.height}.`);
  refreshList();
  render();
  commitHistory();
}

async function addFurnitureFiles(files) {
  let loadedCount = 0;
  let skippedCount = 0;
  for (const file of files) {
    const loaded = await readImageFile(file);
    const prepared = await promptFurnitureCutout(loaded);
    if (!prepared) {
      skippedCount += 1;
      continue;
    }
    const initialScale = furnitureInitialScale(prepared.img.naturalWidth, prepared.img.naturalHeight);
    const targetW = prepared.img.naturalWidth * initialScale;
    const targetH = prepared.img.naturalHeight * initialScale;
    const furnitureType = inferFurnitureType(file.name);
    const kindPreset = itemKindPreset(inferItemKind(file.name, furnitureType));
    const item = {
      id: `f${state.nextId++}`,
      name: file.name.replace(/\.[^.]+$/, ""),
      fileName: file.name,
      img: prepared.img,
      dataUrl: prepared.dataUrl,
      originalDataUrl: loaded.dataUrl,
      sourceWidth: prepared.img.naturalWidth,
      sourceHeight: prepared.img.naturalHeight,
      x: Math.round((state.width - Math.min(targetW, state.width)) / 2),
      y: Math.round((state.height - Math.min(targetH, state.height)) / 2),
      scale: initialScale,
      scaleX: initialScale,
      scaleY: initialScale,
      aspectLocked: true,
      sourceScale: state.sourceScale,
      opacity: 1,
      z: 20,
      visible: true,
      anchor: "top_left",
      slot: kindPreset.slot,
      layer: kindPreset.layer,
      source: "furniture",
      furnitureType,
      kind: kindPreset.value,
      heightPx: kindPreset.heightPx,
      footprint: kindPreset.footprint,
      castsShadow: kindPreset.castsShadow,
      receivesShadow: kindPreset.receivesShadow,
      occludesSprite: kindPreset.occludesSprite,
      sortY: Math.round(((state.height - Math.min(targetH, state.height)) / 2) + targetH),
      cutoutApplied: prepared.cutoutApplied
    };
    normalizeItemSemantics(item);
    state.items.push(item);
    state.selectedId = item.id;
    loadedCount += 1;
  }
  if (!loadedCount) {
    setStatus(skippedCount ? `Skipped ${skippedCount} furniture file(s).` : "No furniture files loaded.");
    return;
  }
  const skippedSuffix = skippedCount ? ` Skipped ${skippedCount}.` : "";
  setStatus(`Loaded ${loadedCount} furniture file(s), sampled with ${state.furnitureImportMode.replace("_", " ")}.${skippedSuffix}`);
  refreshList();
  refreshSelectedPanel();
  render();
  commitHistory();
}

function selectedItem() {
  return state.items.find((item) => item.id === state.selectedId) || null;
}

function positiveNumber(value, fallback) {
  const number = Number(value);
  return Number.isFinite(number) && number > 0 ? number : fallback;
}

function itemScaleX(item) {
  return positiveNumber(item.scaleX, positiveNumber(item.scale, 1));
}

function itemScaleY(item) {
  return positiveNumber(item.scaleY, positiveNumber(item.scale, 1));
}

function itemAspectLocked(item) {
  return item.aspectLocked !== false;
}

function itemTargetWidth(item) {
  return Math.max(ITEM_MIN_TARGET_SIZE, item.sourceWidth * itemScaleX(item));
}

function itemTargetHeight(item) {
  return Math.max(ITEM_MIN_TARGET_SIZE, item.sourceHeight * itemScaleY(item));
}

function setItemUniformScale(item, scale) {
  const value = positiveNumber(scale, itemScaleX(item));
  item.scale = value;
  item.scaleX = value;
  item.scaleY = value;
}

function setItemTargetWidth(item, width) {
  const targetWidth = Math.max(ITEM_MIN_TARGET_SIZE, positiveNumber(width, itemTargetWidth(item)));
  const nextScale = targetWidth / Math.max(1, item.sourceWidth);
  item.scaleX = nextScale;
  if (itemAspectLocked(item)) {
    item.scaleY = nextScale;
  }
  item.scale = item.scaleX;
}

function setItemTargetHeight(item, height) {
  const targetHeight = Math.max(ITEM_MIN_TARGET_SIZE, positiveNumber(height, itemTargetHeight(item)));
  const nextScale = targetHeight / Math.max(1, item.sourceHeight);
  item.scaleY = nextScale;
  if (itemAspectLocked(item)) {
    item.scaleX = nextScale;
  }
  item.scale = item.scaleX;
}

function setItemAspectLocked(item, locked) {
  item.aspectLocked = locked;
  if (locked) {
    item.scaleY = itemScaleX(item);
  }
  item.scale = itemScaleX(item);
}

function deleteItemById(itemId) {
  const item = state.items.find((value) => value.id === itemId);
  if (!item) return false;

  state.items = state.items.filter((value) => value.id !== itemId);
  if (state.selectedId === itemId) {
    state.selectedId = null;
  }
  setStatus(`Deleted ${item.name}.`);
  refreshList();
  refreshSelectedPanel();
  render();
  commitHistory();
  return true;
}

function sortedItems() {
  return [...state.items].sort((a, b) => {
    if (a.z !== b.z) return a.z - b.z;
    const sortDiff = itemSortY(a) - itemSortY(b);
    if (sortDiff !== 0) return sortDiff;
    return a.id.localeCompare(b.id);
  });
}

function itemBounds(item) {
  return {
    x: item.x,
    y: item.y,
    w: itemTargetWidth(item),
    h: itemTargetHeight(item)
  };
}

function autoSortY(item) {
  return Math.round((Number(item.y) || 0) + itemTargetHeight(item));
}

function itemSortY(item) {
  const value = Number(item.sortY);
  return Number.isFinite(value) ? value : autoSortY(item);
}

function itemHeightPx(item) {
  return coerceNumber(item.heightPx, itemKindPreset(item.kind).heightPx, 0, 160);
}

function itemWallShadowOffsetY(item) {
  return coerceNumber(item.wallShadowOffsetY, 0, -80, 80);
}

function itemWallShadowDepthPx(item) {
  const preset = itemKindPreset(item.kind);
  const fallback = preset.wallDepthPx ?? (itemUsesWallShadowAnchor(item) ? 24 : 0);
  return coerceNumber(item.wallShadowDepthPx, fallback, 0, 160);
}

function itemCastsShadow(item) {
  if (item.castsShadow != null) return item.castsShadow !== false;
  return itemKindPreset(item.kind).castsShadow;
}

function itemUsesWallShadowAnchor(item) {
  const anchor = normalizeShadowAnchor(item.shadowAnchor);
  if (anchor === "wall") return true;
  if (anchor === "floor") return false;

  const kind = normalizeItemKind(item.kind);
  if (kind === "wall_prop" || kind === "wall_furniture") return true;
  if (String(item.slot || "").toLowerCase() === "wall") return true;

  const name = `${item.fileName || ""} ${item.name || ""}`;
  return WALL_MOUNTED_NAME_PATTERN.test(name);
}

function itemFootprint(item) {
  return normalizeFootprint(item.footprint);
}

function clampUnit(value) {
  return clampNumber(Number(value) || 0, 0, 1);
}

function normalizeLocalPolygon(value) {
  if (!Array.isArray(value)) return [];
  const points = [];
  for (const raw of value) {
    let x;
    let y;
    if (Array.isArray(raw)) {
      x = raw[0];
      y = raw[1];
    } else if (raw && typeof raw === "object") {
      x = raw.x;
      y = raw.y;
    }
    if (!Number.isFinite(Number(x)) || !Number.isFinite(Number(y))) continue;
    points.push({
      x: Number(clampUnit(x).toFixed(4)),
      y: Number(clampUnit(y).toFixed(4))
    });
  }
  return points.length >= 3 ? points : [];
}

function compactLocalPolygon(value) {
  return normalizeLocalPolygon(value).map((point) => [
    Number(point.x.toFixed(4)),
    Number(point.y.toFixed(4))
  ]);
}

function defaultFootprintPolygon() {
  return [
    { x: 0.18, y: 0.72 },
    { x: 0.82, y: 0.72 },
    { x: 0.96, y: 0.96 },
    { x: 0.04, y: 0.96 }
  ];
}

function defaultShadowPolygon() {
  return [
    { x: 0, y: 0 },
    { x: 1, y: 0 },
    { x: 1, y: 1 },
    { x: 0, y: 1 }
  ];
}

function effectiveFootprintPolygon(item) {
  const custom = normalizeLocalPolygon(item.footprintPolygon);
  if (custom.length) return custom;
  return itemFootprint(item) === "polygon" ? defaultFootprintPolygon() : [];
}

function polygonToText(value) {
  return normalizeLocalPolygon(value)
    .map((point) => `${Number(point.x.toFixed(4))}, ${Number(point.y.toFixed(4))}`)
    .join("\n");
}

function parsePolygonText(text) {
  const points = [];
  for (const line of String(text || "").split(/\n+/)) {
    const match = line.trim().match(/^(-?\d*\.?\d+)\s*[, ]\s*(-?\d*\.?\d+)$/);
    if (!match) continue;
    points.push({ x: Number(match[1]), y: Number(match[2]) });
  }
  return normalizeLocalPolygon(points);
}

function polygonSummary(value) {
  const count = normalizeLocalPolygon(value).length;
  return count ? `${count} pts` : "default";
}

function polygonEditorIds(key) {
  return key === "shadowPolygon"
    ? { editor: "shadowPolygonEditor", textarea: "itemShadowPolygon", meta: "itemShadowPolygonMeta" }
    : { editor: "footprintPolygonEditor", textarea: "itemFootprintPolygon", meta: "itemFootprintPolygonMeta" };
}

function polygonDisplayPoints(item, key) {
  return key === "shadowPolygon" ? normalizeLocalPolygon(item.shadowPolygon) : effectiveFootprintPolygon(item);
}

function itemPolygonEditPoints(item, key) {
  const bounds = itemBounds(item);
  return polygonDisplayPoints(item, key).map((point) => targetToEditPoint({
    x: bounds.x + point.x * bounds.w,
    y: bounds.y + point.y * bounds.h
  }));
}

function setActiveItemPolygon(key) {
  state.activeItemPolygonKey = key === "shadowPolygon" ? "shadowPolygon" : "footprintPolygon";
  updatePolygonEditorState();
}

function syncPolygonEditorField(item, key, options = {}) {
  const updateTextarea = options.updateTextarea !== false;
  const ids = polygonEditorIds(key);
  const textarea = document.getElementById(ids.textarea);
  if (textarea && updateTextarea) textarea.value = polygonToText(item[key]);
  const meta = document.getElementById(ids.meta);
  if (meta) meta.textContent = polygonSummary(item[key]);
  if (key === "footprintPolygon") {
    const footprintInput = document.getElementById("itemFootprint");
    if (footprintInput) footprintInput.value = item.footprint;
  }
}

function updatePolygonEditorState() {
  for (const key of ["footprintPolygon", "shadowPolygon"]) {
    const editor = document.getElementById(polygonEditorIds(key).editor);
    if (editor) editor.classList.toggle("active", state.activeItemPolygonKey === key);
  }
}

function materializeItemPolygon(item, key) {
  if (key === "shadowPolygon") {
    item.shadowPolygon = normalizeLocalPolygon(item.shadowPolygon);
    return item.shadowPolygon;
  }
  const current = normalizeLocalPolygon(item.footprintPolygon);
  item.footprint = "polygon";
  item.footprintPolygon = current.length ? current : defaultFootprintPolygon();
  return item.footprintPolygon;
}

function applyItemKindPreset(item, kindValue) {
  const preset = itemKindPreset(kindValue);
  item.kind = preset.value;
  item.heightPx = preset.heightPx;
  item.footprint = preset.footprint;
  item.castsShadow = preset.castsShadow;
  item.receivesShadow = preset.receivesShadow;
  item.occludesSprite = preset.occludesSprite;
  item.slot = preset.slot;
  item.layer = preset.layer;
  item.shadowAnchor = preset.shadowAnchor || "auto";
  item.wallShadowDepthPx = preset.wallDepthPx ?? 0;
  item.wallShadowOffsetY = 0;
  item.sortY = autoSortY(item);
}

function normalizeItemSemantics(item) {
  const preset = itemKindPreset(item.kind || inferItemKind(item.fileName || item.name, item.furnitureType));
  item.kind = preset.value;
  item.heightPx = coerceNumber(item.heightPx, preset.heightPx, 0, 160);
  item.footprint = normalizeFootprint(item.footprint || preset.footprint);
  item.castsShadow = item.castsShadow ?? preset.castsShadow;
  item.receivesShadow = item.receivesShadow ?? preset.receivesShadow;
  item.occludesSprite = item.occludesSprite ?? preset.occludesSprite;
  item.slot = item.slot || preset.slot;
  item.layer = item.layer || preset.layer;
  item.shadowAnchor = normalizeShadowAnchor(item.shadowAnchor || preset.shadowAnchor);
  item.wallShadowOffsetY = itemWallShadowOffsetY(item);
  item.wallShadowDepthPx = itemWallShadowDepthPx(item);
  item.footprintPolygon = normalizeLocalPolygon(item.footprintPolygon);
  item.shadowPolygon = normalizeLocalPolygon(item.shadowPolygon);
  if (!Number.isFinite(Number(item.sortY))) item.sortY = autoSortY(item);
  return item;
}

function modeLightKey(mode = state.mode) {
  return mode === "night" ? "nightLight" : "dayLight";
}

function lightProfileMode(profile = state.lightProfile) {
  return profile === "night" ? "night" : "day";
}

function copyLightSettings(light) {
  return Object.fromEntries(LIGHT_SETTING_KEYS.map((key) => [key, light[key]]));
}

function assignLightSettings(target, source) {
  const normalized = normalizeLightSettings(source, target || DEFAULT_NIGHT);
  for (const key of LIGHT_SETTING_KEYS) {
    target[key] = normalized[key];
  }
  return target;
}

function previewLightSettings(mode = state.mode) {
  if (!state.night.dayLight || !state.night.nightLight) {
    state.night = normalizeNightSettings(state.night);
  }
  if (!state.night.separateModeLights) return state.night;
  return state.night[modeLightKey(mode)];
}

function activeLightSettings(mode = state.mode) {
  return previewLightSettings(mode);
}

function editLightSettings() {
  if (!state.night.dayLight || !state.night.nightLight) {
    state.night = normalizeNightSettings(state.night);
  }
  const profile = currentLightProfile();
  if (profile === "shared" || !state.night.separateModeLights) return state.night;
  return state.night[modeLightKey(lightProfileMode(profile))];
}

function activeLightPoint(mode = state.mode) {
  const light = activeLightSettings(mode);
  return { x: light.lightX, y: light.lightY };
}

function editLightPointTarget() {
  const light = editLightSettings();
  return { x: light.lightX, y: light.lightY };
}

function readLightInputs() {
  return normalizeLightSettings({
    lightShape: document.getElementById("lightShape").value,
    lightStrength: Number(document.getElementById("lightStrength").value) || 0,
    lightX: Number(document.getElementById("lightX").value) || 0,
    lightY: Number(document.getElementById("lightY").value) || 0,
    lightDepth: Number(document.getElementById("lightDepth").value) || DEFAULT_NIGHT.lightDepth,
    lightRadius: Number(document.getElementById("lightRadius").value) || DEFAULT_NIGHT.lightRadius,
    lightAngle: Number(document.getElementById("lightAngle").value) || 0,
    lightSpread: Number(document.getElementById("lightSpread").value) || DEFAULT_NIGHT.lightSpread
  }, editLightSettings());
}

function commitLightInputsToState() {
  assignLightSettings(editLightSettings(), readLightInputs());
}

function syncLightInputsFromState() {
  const light = editLightSettings();
  document.getElementById("lightShape").value = light.lightShape;
  document.getElementById("lightStrength").value = light.lightStrength;
  document.getElementById("lightX").value = light.lightX;
  document.getElementById("lightY").value = light.lightY;
  document.getElementById("lightDepth").value = light.lightDepth;
  document.getElementById("lightRadius").value = light.lightRadius;
  document.getElementById("lightAngle").value = light.lightAngle;
  document.getElementById("lightSpread").value = light.lightSpread;
  updateLightProfileButtons();
}

function currentLightProfile() {
  return state.night.separateModeLights ? normalizeLightProfile(state.lightProfile || state.mode) : "shared";
}

function updateLightProfileButtons() {
  const profile = currentLightProfile();
  document.getElementById("lightProfileShared").classList.toggle("active", profile === "shared");
  document.getElementById("lightProfileDay").classList.toggle("active", profile === "day");
  document.getElementById("lightProfileNight").classList.toggle("active", profile === "night");
  document.getElementById("copyLightToDay").hidden = profile === "day";
  document.getElementById("copyLightToNight").hidden = profile === "night";
}

function ensureSeparateModeLights() {
  if (state.night.separateModeLights) return;
  const shared = copyLightSettings(state.night);
  state.night.dayLight = normalizeLightSettings(state.night.dayLight, shared);
  state.night.nightLight = normalizeLightSettings(state.night.nightLight, shared);
  state.night.separateModeLights = true;
}

function setLightProfile(profile) {
  profile = normalizeLightProfile(profile);
  commitLightInputsToState();
  if (profile === "shared") {
    if (state.night.separateModeLights) {
      assignLightSettings(state.night, editLightSettings());
    }
    state.night.separateModeLights = false;
    state.lightProfile = "shared";
  } else {
    ensureSeparateModeLights();
    state.lightProfile = profile;
  }
  syncLightInputsFromState();
  updateModeButtons();
}

function copyActiveLightToProfile(profile) {
  profile = normalizeLightProfile(profile);
  if (profile === "shared") return;
  commitLightInputsToState();
  const source = copyLightSettings(editLightSettings());
  ensureSeparateModeLights();
  assignLightSettings(state.night[modeLightKey(lightProfileMode(profile))], source);
  if (currentLightProfile() === "shared") {
    state.lightProfile = profile;
  }
  syncLightInputsFromState();
  updateModeButtons();
}

function syncLightProfileWithMode() {
  if (state.night.separateModeLights) {
    state.lightProfile = state.mode === "night" ? "night" : "day";
  } else {
    state.lightProfile = "shared";
  }
  updateLightProfileButtons();
}

const shadowMaskCache = new WeakMap();

function localPolygonMask(item, polygon) {
  const width = item.img?.naturalWidth || item.img?.width || item.sourceWidth || 1;
  const height = item.img?.naturalHeight || item.img?.height || item.sourceHeight || 1;
  const points = normalizeLocalPolygon(polygon);
  if (!points.length) return null;
  const mask = document.createElement("canvas");
  mask.width = Math.max(1, Math.round(width));
  mask.height = Math.max(1, Math.round(height));
  const maskCtx = mask.getContext("2d");
  maskCtx.fillStyle = "#000000";
  maskCtx.beginPath();
  points.forEach((point, index) => {
    const x = point.x * mask.width;
    const y = point.y * mask.height;
    if (index === 0) maskCtx.moveTo(x, y);
    else maskCtx.lineTo(x, y);
  });
  maskCtx.closePath();
  maskCtx.fill();
  return mask;
}

function itemShadowMask(item) {
  if (!item.img) return null;
  const customMask = localPolygonMask(item, item.shadowPolygon);
  if (customMask) return customMask;
  if (shadowMaskCache.has(item.img)) return shadowMaskCache.get(item.img);
  const width = item.img.naturalWidth || item.img.width || item.sourceWidth || 1;
  const height = item.img.naturalHeight || item.img.height || item.sourceHeight || 1;
  const mask = document.createElement("canvas");
  mask.width = width;
  mask.height = height;
  const maskCtx = mask.getContext("2d");
  maskCtx.imageSmoothingEnabled = false;
  maskCtx.drawImage(item.img, 0, 0, width, height);
  maskCtx.globalCompositeOperation = "source-in";
  maskCtx.fillStyle = "#000000";
  maskCtx.fillRect(0, 0, width, height);
  shadowMaskCache.set(item.img, mask);
  return mask;
}

function activeShadowMode() {
  const mode = normalizeShadowMode(state.night.shadowMode);
  if (mode !== "auto") return mode;
  return normalizeLightShape(activeLightSettings().lightShape) === "cone" ? "directional" : "point";
}

function shadowDirectionForPoint(footX, footY, lightPoint) {
  if (activeShadowMode() === "directional") {
    const angle = angleToRad(activeLightSettings().lightAngle);
    return { ux: Math.cos(angle), uy: Math.sin(angle) };
  }

  let dx = footX - lightPoint.x;
  let dy = footY - lightPoint.y;
  let distance = Math.hypot(dx, dy);
  if (distance < 0.001) {
    dx = 0;
    dy = 1;
    distance = 1;
  }
  return {
    ux: dx / distance,
    uy: dy / distance
  };
}

function shadowLightFactorForPoint(footX, footY, lightPoint, scale = 1) {
  const light = activeLightSettings();
  const strength = clampNumber(Number(light.lightStrength) || 0, 0, 200) / 100;
  if (strength <= 0) return 0;

  const radius = Math.max(1, (Number(light.lightRadius) || DEFAULT_NIGHT.lightRadius) * scale);
  const dx = footX - lightPoint.x;
  const dy = footY - lightPoint.y;
  const dist = Math.hypot(dx, dy);
  if (dist > radius) return 0;

  let coneFactor = 1;
  if (normalizeLightShape(light.lightShape) === "cone" && dist > 0.001) {
    const halfSpread = Math.max(0.001, angleToRad(light.lightSpread) * 0.5);
    const delta = Math.abs(angleDeltaRad(Math.atan2(dy, dx), angleToRad(light.lightAngle)));
    if (delta > halfSpread) return 0;
    coneFactor = 1 - (delta / halfSpread) * 0.28;
  }

  const falloff = Math.max(0, 1 - dist / radius) ** 0.7;
  return Math.sqrt(strength) * (0.45 + 0.55 * falloff) * coneFactor;
}

function faceReceivesShadow(face) {
  return face && !isSpriteAreaFace(face) && face.points.length >= 3 && face.receivesShadow !== false;
}

function shadowSurfaceForFace(face) {
  return normalizeShadowSurface(face?.shadowSurface, face?.type);
}

function receivingShadowFaces() {
  return state.faces.filter(faceReceivesShadow);
}

function wallShadowSkew(surface) {
  if (surface === "left_wall") return -0.16;
  if (surface === "right_wall") return 0.16;
  return 0;
}

function faceProjectionBasis(points) {
  if (!points || points.length < 3) return null;
  const origin = points[0];
  const bottomLeft = points[1];
  const topRight = points[points.length - 1];
  const rawU = { x: topRight.x - origin.x, y: topRight.y - origin.y };
  const rawV = { x: bottomLeft.x - origin.x, y: bottomLeft.y - origin.y };
  const uLength = Math.hypot(rawU.x, rawU.y);
  const vLength = Math.hypot(rawV.x, rawV.y);
  if (uLength < 0.001 || vLength < 0.001) return null;
  const u = { x: rawU.x / uLength, y: rawU.y / uLength };
  const v = { x: rawV.x / vLength, y: rawV.y / vLength };
  const det = u.x * v.y - u.y * v.x;
  if (Math.abs(det) < 0.001) return null;
  return { origin, u, v, det, uLength, vLength };
}

function screenToFaceLocal(basis, point) {
  const dx = point.x - basis.origin.x;
  const dy = point.y - basis.origin.y;
  return {
    u: (dx * basis.v.y - dy * basis.v.x) / basis.det,
    v: (basis.u.x * dy - basis.u.y * dx) / basis.det
  };
}

function faceLocalToScreen(basis, point) {
  return {
    x: basis.origin.x + basis.u.x * point.u + basis.v.x * point.v,
    y: basis.origin.y + basis.u.y * point.u + basis.v.y * point.v
  };
}

function projectWallLocalPoint(point, light, lightDepth, casterDepth) {
  const safeDepth = Math.min(Math.max(0, casterDepth), Math.max(0, lightDepth - 1));
  if (safeDepth <= 0) return { ...point };
  const factor = lightDepth / Math.max(1, lightDepth - safeDepth);
  return {
    u: light.u + (point.u - light.u) * factor,
    v: light.v + (point.v - light.v) * factor
  };
}

function itemWallShadowDepthScaled(item, scale) {
  const depthScale = Math.max(0.2, (Number(state.night.shadowLength) || DEFAULT_NIGHT.shadowLength) / DEFAULT_NIGHT.shadowLength);
  return itemWallShadowDepthPx(item) * scale * depthScale;
}

function wallShadowOpacity(item, lightFactor) {
  const heightFactor = Math.max(0.25, Math.min(2.2, itemHeightPx(item) / 32));
  const modeAlpha = state.mode === "night" ? 1 : 0.42;
  const alpha = (state.night.shadowAlpha / 100) *
    modeAlpha *
    lightFactor *
    Math.max(0, Math.min(1, item.opacity ?? 1)) *
    Math.min(1.15, 0.65 + heightFactor * 0.22) *
    0.94;
  return clampNumber(alpha, 0, 1);
}

function baseShadowOpacity(item, lightFactor, heightFactor, isWallSurface = false) {
  const modeAlpha = state.mode === "night" ? 1 : 0.42;
  const alpha = (state.night.shadowAlpha / 100) *
    modeAlpha *
    lightFactor *
    Math.max(0, Math.min(1, item.opacity ?? 1)) *
    Math.min(1.15, 0.65 + heightFactor * 0.22) *
    (isWallSurface ? 0.92 : 1);
  return clampNumber(alpha, 0, 1);
}

function drawFloorFootprintShadow(targetCtx, item, bounds, ux, uy, lightFactor, scale, heightFactor) {
  const polygon = effectiveFootprintPolygon(item);
  if (polygon.length) {
    const shadowLength = state.night.shadowLength * scale * heightFactor;
    const offsetX = ux * shadowLength * 0.72;
    const offsetY = uy * shadowLength * 0.32;
    const alpha = baseShadowOpacity(item, lightFactor, heightFactor);
    if (alpha <= 0) return true;
    targetCtx.save();
    targetCtx.globalAlpha = alpha;
    targetCtx.globalCompositeOperation = "multiply";
    targetCtx.filter = state.night.shadowBlur > 0 ? `blur(${Math.max(0.2 * scale, state.night.shadowBlur * scale)}px)` : "none";
    targetCtx.fillStyle = "#000000";
    targetCtx.beginPath();
    polygon.forEach((point, index) => {
      const x = bounds.x + point.x * bounds.w + offsetX;
      const y = bounds.y + point.y * bounds.h + offsetY;
      if (index === 0) targetCtx.moveTo(x, y);
      else targetCtx.lineTo(x, y);
    });
    targetCtx.closePath();
    targetCtx.fill();
    targetCtx.restore();
    return true;
  }
  if (itemFootprint(item) !== "ellipse") return false;
  const shadowLength = state.night.shadowLength * scale * heightFactor;
  const footX = bounds.x + bounds.w * 0.5;
  const footY = bounds.y + bounds.h * 0.88;
  const shadowWidth = Math.max(1, bounds.w * (0.72 + Math.min(0.22, state.night.shadowLength / 220) * heightFactor));
  const shadowHeight = Math.max(1, bounds.h * (0.16 + Math.min(0.12, itemHeightPx(item) / 360)));
  const centerX = footX + ux * shadowLength * 0.72;
  const centerY = footY + uy * shadowLength * 0.32;
  const blur = Math.max(0.2 * scale, state.night.shadowBlur * scale);

  targetCtx.save();
  targetCtx.globalAlpha = baseShadowOpacity(item, lightFactor, heightFactor);
  targetCtx.globalCompositeOperation = "multiply";
  targetCtx.filter = blur > 0 ? `blur(${blur}px)` : "none";
  targetCtx.translate(centerX, centerY);
  targetCtx.rotate(Math.atan2(uy, ux) * 0.04);
  targetCtx.fillStyle = "#000000";
  targetCtx.beginPath();
  targetCtx.ellipse(0, 0, shadowWidth * 0.5, shadowHeight * 0.5, 0, 0, Math.PI * 2);
  targetCtx.fill();
  targetCtx.restore();
  return true;
}

function drawWallContactShadow(targetCtx, item, bounds, lightPoint, scale, facePoints) {
  const basis = faceProjectionBasis(facePoints);
  if (!basis) return;
  const depth = itemWallShadowDepthScaled(item, scale);
  if (depth <= 0) return;
  const center = { x: bounds.x + bounds.w * 0.5, y: bounds.y + bounds.h * 0.76 };
  const lightLocal = screenToFaceLocal(basis, lightPoint);
  const centerLocal = screenToFaceLocal(basis, center);
  const dx = centerLocal.u - lightLocal.u;
  const dy = centerLocal.v - lightLocal.v;
  const length = Math.hypot(dx, dy) || 1;
  const ux = dx / length;
  const uy = dy / length;
  const bottomLeft = screenToFaceLocal(basis, { x: bounds.x + bounds.w * 0.12, y: bounds.y + bounds.h * 0.78 });
  const bottomRight = screenToFaceLocal(basis, { x: bounds.x + bounds.w * 0.88, y: bounds.y + bounds.h * 0.78 });
  const drop = Math.max(3 * scale, Math.min(bounds.h * 0.55, depth * 0.7));
  const side = Math.max(1 * scale, depth * 0.22);
  const p1 = faceLocalToScreen(basis, { u: bottomLeft.u + ux * side, v: bottomLeft.v + uy * side });
  const p2 = faceLocalToScreen(basis, { u: bottomRight.u + ux * side, v: bottomRight.v + uy * side });
  const p3 = faceLocalToScreen(basis, { u: bottomRight.u + ux * side * 1.7, v: bottomRight.v + drop + uy * side * 1.7 });
  const p4 = faceLocalToScreen(basis, { u: bottomLeft.u + ux * side * 1.7, v: bottomLeft.v + drop + uy * side * 1.7 });
  const lightFactor = shadowLightFactorForPoint(center.x, center.y, lightPoint, scale);
  if (lightFactor <= 0) return;

  targetCtx.save();
  targetCtx.globalCompositeOperation = "multiply";
  targetCtx.globalAlpha = wallShadowOpacity(item, lightFactor) * 0.62;
  targetCtx.filter = `blur(${Math.max(1.2, state.night.shadowBlur * scale + 1.4 * scale)}px)`;
  targetCtx.fillStyle = "#000000";
  targetCtx.beginPath();
  targetCtx.moveTo(p1.x, p1.y);
  targetCtx.lineTo(p2.x, p2.y);
  targetCtx.lineTo(p3.x, p3.y);
  targetCtx.lineTo(p4.x, p4.y);
  targetCtx.closePath();
  targetCtx.fill();
  targetCtx.restore();
}

function drawWallPlaneShadow(targetCtx, item, bounds, lightPoint, scale, facePoints) {
  const basis = faceProjectionBasis(facePoints);
  const mask = itemShadowMask(item);
  if (!basis || !mask) return false;
  const lightSettings = activeLightSettings();
  const center = { x: bounds.x + bounds.w * 0.5, y: bounds.y + bounds.h * 0.5 };
  const lightFactor = shadowLightFactorForPoint(center.x, center.y, lightPoint, scale);
  if (lightFactor <= 0) return true;

  const lightLocal = screenToFaceLocal(basis, lightPoint);
  const lightDepth = Math.max(2, lightSettings.lightDepth * scale);
  const casterDepth = Math.min(itemWallShadowDepthScaled(item, scale), lightDepth - 1);
  if (casterDepth <= 0) return false;

  const offsetV = itemWallShadowOffsetY(item) * scale;
  const cornerLocal = [
    screenToFaceLocal(basis, { x: bounds.x, y: bounds.y }),
    screenToFaceLocal(basis, { x: bounds.x + bounds.w, y: bounds.y }),
    screenToFaceLocal(basis, { x: bounds.x, y: bounds.y + bounds.h })
  ].map((point) => ({ u: point.u, v: point.v + offsetV }));
  const projected = cornerLocal
    .map((point) => projectWallLocalPoint(point, lightLocal, lightDepth, casterDepth))
    .map((point) => faceLocalToScreen(basis, point));

  drawWallContactShadow(targetCtx, item, bounds, lightPoint, scale, facePoints);

  targetCtx.save();
  targetCtx.globalCompositeOperation = "multiply";
  targetCtx.globalAlpha = wallShadowOpacity(item, lightFactor);
  targetCtx.filter = state.night.shadowBlur > 0 ? `blur(${state.night.shadowBlur * scale}px)` : "none";
  targetCtx.transform(
    (projected[1].x - projected[0].x) / mask.width,
    (projected[1].y - projected[0].y) / mask.width,
    (projected[2].x - projected[0].x) / mask.height,
    (projected[2].y - projected[0].y) / mask.height,
    projected[0].x,
    projected[0].y
  );
  targetCtx.drawImage(mask, 0, 0);
  targetCtx.restore();
  return true;
}

function drawProjectedShadow(targetCtx, item, bounds, lightPoint, scale = 1, surface = "floor", facePoints = null) {
  if (!state.night.castShadows || state.night.shadowAlpha <= 0 || state.night.shadowLength <= 0) return;
  if (!itemCastsShadow(item)) return;
  if (!item.visible || item.opacity <= 0 || bounds.w <= 0 || bounds.h <= 0) return;
  const mask = itemShadowMask(item);
  if (!mask) return;

  const footX = bounds.x + bounds.w * 0.5;
  const footY = bounds.y + bounds.h * 0.88;
  const { ux, uy } = shadowDirectionForPoint(footX, footY, lightPoint);
  const lightFactor = shadowLightFactorForPoint(footX, footY, lightPoint, scale);
  if (lightFactor <= 0) return;
  const heightFactor = Math.max(0.25, Math.min(2.2, itemHeightPx(item) / 32));
  const shadowLength = state.night.shadowLength * scale * heightFactor;
  const isWallSurface = surface === "left_wall" || surface === "right_wall";
  const usesWallAnchor = isWallSurface && itemUsesWallShadowAnchor(item);
  if (usesWallAnchor && facePoints && drawWallPlaneShadow(targetCtx, item, bounds, lightPoint, scale, facePoints)) {
    return;
  }
  if (!isWallSurface && drawFloorFootprintShadow(targetCtx, item, bounds, ux, uy, lightFactor, scale, heightFactor)) {
    return;
  }
  const shadowWidth = isWallSurface ?
    bounds.w * (0.72 + Math.min(0.28, heightFactor * 0.12)) :
    bounds.w * (1 + Math.min(0.55, state.night.shadowLength / 180) * heightFactor);
  const shadowHeight = isWallSurface ?
    Math.max(1, bounds.h * (0.52 + Math.min(0.32, itemHeightPx(item) / 200))) :
    Math.max(1, bounds.h * (0.22 + Math.min(0.18, itemHeightPx(item) / 320)));
  const wallAnchorY = usesWallAnchor ? bounds.y + bounds.h * 0.86 : bounds.y + bounds.h * 0.34;
  const wallShadowLift = usesWallAnchor ? shadowHeight * 0.08 : shadowHeight * 0.36 + itemHeightPx(item) * scale * 0.22;
  const wallLightShift = uy * shadowLength * (usesWallAnchor ? 0.68 : 0.28);
  const wallOffsetY = usesWallAnchor ? itemWallShadowOffsetY(item) * scale : 0;
  const destX = isWallSurface ?
    footX - shadowWidth * 0.5 + ux * shadowLength * 0.82 :
    footX - shadowWidth * 0.5 + ux * shadowLength;
  const destY = isWallSurface ?
    wallAnchorY - wallShadowLift + wallLightShift + wallOffsetY :
    footY - shadowHeight * 0.5 + uy * shadowLength * 0.45;
  const blur = state.night.shadowBlur * scale;

  targetCtx.save();
  targetCtx.globalAlpha = baseShadowOpacity(item, lightFactor, heightFactor, isWallSurface);
  targetCtx.globalCompositeOperation = "multiply";
  targetCtx.filter = blur > 0 ? `blur(${blur}px)` : "none";
  targetCtx.translate(destX + shadowWidth * 0.5, destY + shadowHeight * 0.5);
  if (isWallSurface) {
    targetCtx.transform(1, 0, wallShadowSkew(surface), 1, 0, 0);
    targetCtx.rotate(Math.atan2(uy, ux) * 0.05);
  } else {
    targetCtx.rotate(Math.atan2(uy, ux) * 0.12);
  }
  targetCtx.drawImage(mask, -shadowWidth * 0.5, -shadowHeight * 0.5, shadowWidth, shadowHeight);
  targetCtx.restore();
}

function drawItemShadows(targetCtx, rectForItem, lightPoint, scale = 1, pointsForFace = (face) => face.points) {
  const faces = receivingShadowFaces();
  if (!faces.length) {
    for (const item of sortedItems()) {
      drawProjectedShadow(targetCtx, item, rectForItem(item), lightPoint, scale, "floor");
    }
    return;
  }

  for (const face of faces) {
    const points = pointsForFace(face);
    if (!points || points.length < 3) continue;
    targetCtx.save();
    drawPolygonPath(targetCtx, points);
    targetCtx.clip();
    for (const item of sortedItems()) {
      drawProjectedShadow(targetCtx, item, rectForItem(item), lightPoint, scale, shadowSurfaceForFace(face), points);
    }
    targetCtx.restore();
  }
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
  const prepared = preparedRoomMetrics();
  const edit = editRoomMetrics(prepared);
  ctx.imageSmoothingEnabled = false;
  ctx.globalAlpha = state.baseOpacity;
  ctx.drawImage(
    img,
    prepared.trim.x,
    prepared.trim.y,
    prepared.trim.width,
    prepared.trim.height,
    0,
    edit.y,
    edit.width,
    edit.height
  );
  ctx.globalAlpha = 1;
}

function drawPreviewBackground() {
  previewCtx.clearRect(0, 0, state.width, state.height);
  previewCtx.fillStyle = "#05070c";
  previewCtx.fillRect(0, 0, state.width, state.height);

  if (!state.base) {
    renderRoomGeometry(previewCtx, state.width, state.height);
    return;
  }

  const img = state.base.img;
  const prepared = preparedRoomMetrics();
  previewCtx.save();
  previewCtx.imageSmoothingEnabled = false;
  previewCtx.drawImage(
    img,
    prepared.trim.x,
    prepared.trim.y,
    prepared.trim.width,
    prepared.trim.height,
    0,
    prepared.y,
    prepared.width,
    prepared.height
  );
  previewCtx.restore();
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
  const normalized = normalizeFaceType(type);
  if (normalized === "wall") return `rgba(120, 214, 238, ${alpha})`;
  if (normalized === FACE_TYPE_SPRITE_AREA) return `rgba(112, 231, 157, ${alpha})`;
  return `rgba(242, 189, 114, ${alpha})`;
}

function faceStrokeColor(type) {
  const normalized = normalizeFaceType(type);
  if (normalized === "wall") return "#78d6ee";
  if (normalized === FACE_TYPE_SPRITE_AREA) return "#70e79d";
  return "#f2bd72";
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
    if (isSpriteAreaFace(face)) continue;
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
  const lightPoint = activeLightPoint();
  drawPreviewBackground();
  drawItemShadows(previewCtx, itemBounds, lightPoint, 1, faceTargetPoints);
  previewCtx.save();
  previewCtx.globalAlpha = 0.85;
  for (const item of sortedItems()) {
    if (!item.visible) continue;
    const b = itemBounds(item);
    previewCtx.imageSmoothingEnabled = false;
    previewCtx.drawImage(item.img, item.x, item.y, b.w, b.h);
  }
  previewCtx.restore();

  drawLightingOverlay(previewCtx, state.width, state.height, lightPoint, 1);
  drawScreenFrame(previewCtx);
}

function angleToRad(angleDeg) {
  return (Number(angleDeg) || 0) * Math.PI / 180;
}

function angleDeltaRad(a, b) {
  return ((a - b + Math.PI) % (Math.PI * 2)) - Math.PI;
}

function fillLightShapePath(targetCtx, lightPoint, radius, angleDeg = 0, spreadDeg = 45, shapeValue = "radial") {
  const shape = normalizeLightShape(shapeValue);
  targetCtx.beginPath();
  if (shape === "cone") {
    const angle = angleToRad(angleDeg);
    const half = angleToRad(spreadDeg) * 0.5;
    targetCtx.moveTo(lightPoint.x, lightPoint.y);
    targetCtx.arc(lightPoint.x, lightPoint.y, radius, angle - half, angle + half);
    targetCtx.closePath();
  } else {
    targetCtx.arc(lightPoint.x, lightPoint.y, radius, 0, Math.PI * 2);
  }
}

function drawLightShape(targetCtx, width, height, lightPoint, radiusScale) {
  const light = activeLightSettings();
  const strength = clampNumber(Number(light.lightStrength) || 0, 0, 200) / 100;
  if (strength <= 0) return;

  const radius = Math.max(1, light.lightRadius * radiusScale);
  const alpha = Math.min(1, strength * (state.mode === "night" ? 0.72 : 0.42));
  const gradient = targetCtx.createRadialGradient(
    lightPoint.x,
    lightPoint.y,
    Math.max(1, radius * 0.04),
    lightPoint.x,
    lightPoint.y,
    radius
  );
  gradient.addColorStop(0, `rgba(255, 255, 238, ${alpha})`);
  gradient.addColorStop(0.52, `rgba(255, 248, 214, ${alpha * 0.24})`);
  gradient.addColorStop(1, "rgba(255, 248, 214, 0)");

  targetCtx.save();
  fillLightShapePath(targetCtx, lightPoint, radius, light.lightAngle, light.lightSpread, light.lightShape);
  targetCtx.clip();
  targetCtx.globalCompositeOperation = "screen";
  targetCtx.fillStyle = gradient;
  targetCtx.fillRect(0, 0, width, height);
  targetCtx.restore();
}

function drawLightingOverlay(targetCtx, width, height, lightPoint, radiusScale) {
  targetCtx.save();
  targetCtx.globalCompositeOperation = "source-over";
  if (state.mode === "night") {
    targetCtx.fillStyle = `rgba(8, 18, 42, ${state.night.tint / 100})`;
    targetCtx.fillRect(0, 0, width, height);
    targetCtx.fillStyle = `rgba(0, 0, 0, ${state.night.darken / 100})`;
    targetCtx.fillRect(0, 0, width, height);
  }
  targetCtx.restore();
  drawLightShape(targetCtx, width, height, lightPoint, radiusScale);
}

function drawScreenFrame(targetCtx, toEdit = false) {
  const frame = screenFrameRect();
  const rect = toEdit ? targetRectToEdit(frame.x, frame.y, frame.w, frame.h) : frame;
  const x = Math.round(rect.x) + 0.5;
  const y = Math.round(rect.y) + 0.5;
  const w = Math.max(1, Math.round(rect.w) - 1);
  const h = Math.max(1, Math.round(rect.h) - 1);

  targetCtx.save();
  targetCtx.strokeStyle = "rgba(255, 255, 255, 0.94)";
  targetCtx.lineWidth = 1;
  targetCtx.setLineDash([4, 3]);
  targetCtx.strokeRect(x, y, w, h);
  targetCtx.setLineDash([]);
  targetCtx.strokeStyle = "rgba(120, 214, 238, 0.92)";
  targetCtx.strokeRect(x + 1, y + 1, Math.max(1, w - 2), Math.max(1, h - 2));
  if (rect.w >= 64 && rect.h >= 18) {
    targetCtx.fillStyle = "rgba(5, 7, 12, 0.62)";
    targetCtx.fillRect(x + 3, y + 3, 55, 11);
    targetCtx.fillStyle = "rgba(255, 255, 255, 0.94)";
    targetCtx.font = "7px sans-serif";
    targetCtx.fillText(`${GAME_SCREEN_WIDTH}x${GAME_SCREEN_HEIGHT}`, x + 6, y + 11);
  }
  targetCtx.restore();
}

function drawNightOverlay() {
  drawLightingOverlay(
    ctx,
    state.editWidth,
    state.editHeight,
    targetToEditPoint(activeLightPoint()),
    Math.max(editScaleX(), editScaleY())
  );
}

function lightEditPoint() {
  return targetToEditPoint(editLightPointTarget());
}

function coneControlDistanceRange() {
  const scale = Math.max(editScaleX(), editScaleY());
  return {
    min: 18 * scale,
    max: 90 * scale
  };
}

function coneControlDistance() {
  const range = coneControlDistanceRange();
  const t = (editLightSettings().lightSpread - 5) / (160 - 5);
  return range.min + Math.max(0, Math.min(1, t)) * (range.max - range.min);
}

function coneControlPoint() {
  const origin = lightEditPoint();
  const angle = angleToRad(editLightSettings().lightAngle);
  const dist = coneControlDistance();
  return {
    x: origin.x + Math.cos(angle) * dist,
    y: origin.y + Math.sin(angle) * dist
  };
}

function updateConeControlFromCanvasPoint(point) {
  const origin = lightEditPoint();
  const dx = point.x - origin.x;
  const dy = point.y - origin.y;
  const dist = Math.max(0.001, Math.hypot(dx, dy));
  const angle = (Math.atan2(dy, dx) * 180 / Math.PI + 360) % 360;
  const range = coneControlDistanceRange();
  const t = Math.max(0, Math.min(1, (dist - range.min) / Math.max(1, range.max - range.min)));
  const spread = 5 + t * (160 - 5);
  const light = editLightSettings();
  light.lightAngle = Math.round(angle);
  light.lightSpread = Math.round(spread);
  document.getElementById("lightAngle").value = light.lightAngle;
  document.getElementById("lightSpread").value = light.lightSpread;
}

function drawLightHandle() {
  const point = lightEditPoint();
  const radius = LIGHT_HANDLE_RADIUS;
  const scale = Math.max(editScaleX(), editScaleY());
  const light = editLightSettings();
  const lightRadius = Math.max(1, light.lightRadius * scale);
  const angle = angleToRad(light.lightAngle);

  ctx.save();
  ctx.strokeStyle = state.mode === "night" ? "rgba(242, 189, 114, 0.48)" : "rgba(255, 244, 194, 0.58)";
  ctx.lineWidth = 1;
  ctx.setLineDash([4, 4]);
  fillLightShapePath(ctx, point, lightRadius, light.lightAngle, light.lightSpread, light.lightShape);
  ctx.stroke();
  ctx.setLineDash([]);
  if (normalizeLightShape(light.lightShape) === "cone") {
    ctx.strokeStyle = "rgba(255, 241, 194, 0.86)";
    ctx.beginPath();
    ctx.moveTo(point.x, point.y);
    ctx.lineTo(point.x + Math.cos(angle) * Math.min(lightRadius, 42), point.y + Math.sin(angle) * Math.min(lightRadius, 42));
    ctx.stroke();
    if (state.lightSelected) {
      const control = coneControlPoint();
      ctx.strokeStyle = "rgba(120, 214, 238, 0.86)";
      ctx.fillStyle = "rgba(120, 214, 238, 0.16)";
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(point.x, point.y);
      ctx.lineTo(control.x, control.y);
      ctx.stroke();
      ctx.beginPath();
      ctx.arc(control.x, control.y, LIGHT_HANDLE_RADIUS - 1, 0, Math.PI * 2);
      ctx.fill();
      ctx.strokeStyle = "#d9f8ff";
      ctx.stroke();
    }
  }
  ctx.fillStyle = "#f2bd72";
  ctx.strokeStyle = "#111318";
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.arc(point.x, point.y, radius, 0, Math.PI * 2);
  ctx.fill();
  ctx.stroke();
  ctx.strokeStyle = "#fff1c2";
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(point.x - radius - 4, point.y);
  ctx.lineTo(point.x + radius + 4, point.y);
  ctx.moveTo(point.x, point.y - radius - 4);
  ctx.lineTo(point.x, point.y + radius + 4);
  ctx.stroke();
  ctx.fillStyle = "rgba(17, 19, 24, 0.78)";
  ctx.font = "9px sans-serif";
  const profileText = currentLightProfile() === "shared" ? "Shared" : currentLightProfile() === "day" ? "Day" : "Night";
  const labelW = Math.ceil(ctx.measureText(profileText).width + 12);
  ctx.fillRect(point.x + 9, point.y - 11, labelW, 13);
  ctx.fillStyle = "#fff1c2";
  ctx.fillText(profileText, point.x + 12, point.y - 2);
  ctx.restore();
}

function drawGrid() {
  if (!state.showGrid) return;
  ctx.save();
  ctx.lineWidth = 1;
  const step = 8;
  const alpha = clampNumber(state.gridOpacity, 0, 1);
  for (let x = 0; x <= state.width; x += step) {
    const editX = x * editScaleX();
    ctx.strokeStyle = `rgba(255, 255, 255, ${x % 32 === 0 ? Math.min(1, alpha * 1.55) : alpha})`;
    ctx.beginPath();
    ctx.moveTo(editX + 0.5, 0);
    ctx.lineTo(editX + 0.5, state.editHeight);
    ctx.stroke();
  }
  for (let y = 0; y <= state.height; y += step) {
    const editY = y * editScaleY();
    ctx.strokeStyle = `rgba(255, 255, 255, ${y % 32 === 0 ? Math.min(1, alpha * 1.55) : alpha})`;
    ctx.beginPath();
    ctx.moveTo(0, editY + 0.5);
    ctx.lineTo(state.editWidth, editY + 0.5);
    ctx.stroke();
  }
  ctx.restore();
}

function drawGuides() {
  if (!state.guides.show) return;
  ctx.save();
  ctx.lineWidth = 1;
  ctx.setLineDash([3, 2]);

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
  ctx.fillStyle = "rgba(255, 190, 110, 0.9)";
  ctx.font = "6px sans-serif";
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
  drawItemLocalPolygonOverlay(item, "footprintPolygon", "rgba(112, 231, 157, 0.82)", "rgba(112, 231, 157, 0.12)");
  drawItemLocalPolygonOverlay(item, "shadowPolygon", "rgba(205, 154, 255, 0.82)", "rgba(205, 154, 255, 0.10)");
  ctx.restore();
}

function drawItemLocalPolygonOverlay(item, key, stroke, fill) {
  const points = polygonDisplayPoints(item, key);
  if (!points.length) return;
  const editPoints = itemPolygonEditPoints(item, key);
  const active = state.activeItemPolygonKey === key;
  ctx.beginPath();
  editPoints.forEach((point, index) => {
    if (index === 0) ctx.moveTo(point.x, point.y);
    else ctx.lineTo(point.x, point.y);
  });
  ctx.closePath();
  ctx.fillStyle = fill;
  ctx.strokeStyle = stroke;
  ctx.lineWidth = active ? 2 : 1;
  ctx.setLineDash([4, 2]);
  ctx.fill();
  ctx.stroke();
  ctx.setLineDash([]);
  editPoints.forEach((point, index) => {
    const radius = active ? 5 : 4;
    ctx.beginPath();
    ctx.arc(point.x, point.y, radius, 0, Math.PI * 2);
    ctx.fillStyle = active ? "#ffffff" : "rgba(255, 255, 255, 0.86)";
    ctx.fill();
    ctx.strokeStyle = stroke;
    ctx.lineWidth = 1.5;
    ctx.stroke();
    if (active) {
      ctx.fillStyle = "#11151d";
      ctx.font = "7px sans-serif";
      ctx.textAlign = "center";
      ctx.textBaseline = "middle";
      ctx.fillText(String(index + 1), point.x, point.y + 0.5);
    }
  });
}

function drawMeasureOverlay() {
  const measurement = currentMeasurement();
  if (!measurement) return;

  const x = Math.round(measurement.start.x) + 0.5;
  const y1 = Math.round(measurement.start.y) + 0.5;
  const y2 = Math.round(measurement.end.y) + 0.5;
  const top = Math.round(measurement.top) + 0.5;
  const bottom = Math.round(measurement.bottom) + 0.5;
  const label = `H ${formatPixelValue(measurement.canvasHeight)} / game ${formatPixelValue(measurement.gameHeight)}`;

  ctx.save();
  ctx.lineWidth = 1;
  ctx.setLineDash([4, 3]);
  ctx.strokeStyle = "rgba(255, 236, 139, 0.42)";
  ctx.beginPath();
  ctx.moveTo(0.5, top);
  ctx.lineTo(state.editWidth + 0.5, top);
  ctx.moveTo(0.5, bottom);
  ctx.lineTo(state.editWidth + 0.5, bottom);
  ctx.stroke();

  ctx.setLineDash([]);
  ctx.strokeStyle = "#ffec8b";
  ctx.fillStyle = "#ffec8b";
  ctx.beginPath();
  ctx.moveTo(x, y1);
  ctx.lineTo(x, y2);
  ctx.moveTo(x - 6, y1);
  ctx.lineTo(x + 6, y1);
  ctx.moveTo(x - 6, y2);
  ctx.lineTo(x + 6, y2);
  ctx.stroke();

  ctx.beginPath();
  ctx.arc(x, y1, 3, 0, Math.PI * 2);
  ctx.arc(x, y2, 3, 0, Math.PI * 2);
  ctx.fill();

  ctx.font = "10px sans-serif";
  const textWidth = ctx.measureText(label).width;
  const labelW = Math.ceil(textWidth + 10);
  const labelH = 16;
  const labelX = clampNumber(x + 8, 2, Math.max(2, state.editWidth - labelW - 2));
  const labelY = clampNumber((top + bottom) / 2 - labelH / 2, 2, Math.max(2, state.editHeight - labelH - 2));
  ctx.fillStyle = "rgba(17, 19, 24, 0.86)";
  ctx.fillRect(labelX, labelY, labelW, labelH);
  ctx.strokeStyle = "rgba(255, 236, 139, 0.72)";
  ctx.strokeRect(labelX + 0.5, labelY + 0.5, labelW - 1, labelH - 1);
  ctx.fillStyle = "#fff5b8";
  ctx.fillText(label, labelX + 5, labelY + 11);
  ctx.restore();
}

function render() {
  state.width = GAME_SCREEN_WIDTH;
  state.height = Math.max(GAME_SCREEN_HEIGHT, Number(document.getElementById("canvasH").value) || GAME_SCREEN_HEIGHT);
  state.baseFit = BASE_FIT_WIDTH;
  syncCanvasSizeToPreparedRoom();
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
  commitLightInputsToState();
  state.night.castShadows = document.getElementById("castShadows").checked;
  state.night.shadowMode = normalizeShadowMode(document.getElementById("shadowMode").value);
  state.night.shadowAlpha = Number(document.getElementById("shadowAlpha").value) || 0;
  state.night.shadowLength = Number(document.getElementById("shadowLength").value) || 0;
  state.night.shadowBlur = Number(document.getElementById("shadowBlur").value) || 0;
  state.showGrid = document.getElementById("showGrid").checked;
  state.gridOpacity = clampNumber((Number(document.getElementById("gridOpacity").value) || 0) / 100, 0, 1);
  state.snapToGrid = document.getElementById("snapToGrid").checked;
  state.editZoom = normalizedEditZoom(document.getElementById("editZoom")?.value || Math.round(state.editZoom * 100));
  updateGridOpacityLabel();

  if (canvas.width !== state.editWidth || canvas.height !== state.editHeight) {
    canvas.width = state.editWidth;
    canvas.height = state.editHeight;
  }
  updateCanvasDisplaySize();

  drawBase();
  const lightPoint = activeLightPoint();
  drawItemShadows(
    ctx,
    (item) => {
      const bounds = itemBounds(item);
      return targetRectToEdit(bounds.x, bounds.y, bounds.w, bounds.h);
    },
    targetToEditPoint(lightPoint),
    Math.max(editScaleX(), editScaleY())
  );
  drawItems();
  drawNightOverlay();
  drawGrid();
  if (state.editMode === "shape") drawFacesOverlay();
  drawGuides();
  drawSelection();
  drawLightHandle();
  drawMeasureOverlay();
  drawScreenFrame(ctx, true);
  renderPreview();
  updateMeasurePanel();
  updateWorkflowStatus();
  updateSharedPointHint();
}

function refreshList() {
  assetList.innerHTML = "";
  const backgroundTitle = document.createElement("div");
  backgroundTitle.className = "asset-section-title";
  backgroundTitle.textContent = "Background";
  assetList.appendChild(backgroundTitle);

  const background = document.createElement("div");
  background.className = "asset-item static background";
  if (state.base) {
    const prepared = preparedRoomMetrics();
    background.innerHTML = `
      <img class="asset-thumb" alt="" src="${state.base.dataUrl}">
      <div>
        <div class="asset-kind">Reference image</div>
        <div class="asset-name">${escapeHtml(state.base.name)}</div>
        <div class="asset-meta">${state.base.width}x${state.base.height}, trim ${prepared.trim.width}x${prepared.trim.height}, opacity ${Math.round(state.baseOpacity * 100)}%</div>
      </div>
    `;
  } else {
    background.innerHTML = `
      <div class="asset-thumb"></div>
      <div>
        <div class="asset-kind">Reference image</div>
        <div class="asset-name">None</div>
        <div class="asset-meta">Drop or choose a background image.</div>
      </div>
    `;
  }
  assetList.appendChild(background);

  const itemTitle = document.createElement("div");
  itemTitle.className = "asset-section-title";
  itemTitle.textContent = `Scene layers (${state.items.length})`;
  assetList.appendChild(itemTitle);

  for (const item of sortedItems()) {
    const el = document.createElement("div");
    el.className = `asset-item${item.id === state.selectedId ? " selected" : ""}`;
    const isSprite = item.source === "project_sprite";
    const type = isSprite ? "Project sprite" : "Furniture";
    const cutout = item.cutoutApplied ? ", cutout" : "";
    const hidden = item.visible ? "" : ", hidden";
    const bounds = itemBounds(item);
    const scaleText = Math.abs(itemScaleX(item) - itemScaleY(item)) < 0.0001
      ? `${Math.round(itemScaleX(item) * 100)}%`
      : `${Math.round(bounds.w)}x${Math.round(bounds.h)}`;
    const meta = isSprite
      ? `${item.action || "-"}/${item.frame || "-"}, ${Math.round(bounds.w)}x${Math.round(bounds.h)}, x ${Math.round(item.x)}, y ${Math.round(item.y)}, z ${item.z}${hidden}`
      : `${itemKindLabel(item.kind)} h${Math.round(itemHeightPx(item))}, ${furnitureTypeLabel(item.furnitureType)}, x ${Math.round(item.x)}, y ${Math.round(item.y)}, z ${item.z}, ${scaleText}${cutout}${hidden}`;
    el.innerHTML = `
      <img class="asset-thumb" alt="" src="${item.dataUrl}">
      <div class="asset-details">
        <div class="asset-kind">${escapeHtml(type)}</div>
        <div class="asset-name">${escapeHtml(item.name)}</div>
        <div class="asset-meta">${escapeHtml(meta)}</div>
      </div>
      <label class="asset-visibility" title="${item.visible ? "Hide" : "Show"} ${escapeAttr(item.name)}">
        <input type="checkbox" ${item.visible ? "checked" : ""}>
        <span>${item.visible ? "On" : "Off"}</span>
      </label>
    `;
    el.addEventListener("click", () => {
      state.selectedId = item.id;
      state.selectedFaceId = null;
      state.selectedPointIndex = null;
      state.selectedPointId = null;
      state.lightSelected = false;
      state.editMode = "furniture";
      updateEditModeButtons();
      refreshList();
      refreshSelectedPanel();
      refreshFaceList();
      refreshSelectedFacePanel();
      render();
    });
    el.querySelector(".asset-visibility").addEventListener("click", (event) => {
      event.stopPropagation();
    });
    el.querySelector(".asset-visibility input").addEventListener("change", (event) => {
      event.stopPropagation();
      item.visible = event.target.checked;
      refreshList();
      refreshSelectedPanel();
      render();
      commitHistory();
    });
    assetList.appendChild(el);
  }
  if (state.items.length === 0) {
    const empty = document.createElement("div");
    empty.className = "hint";
    empty.textContent = "No furniture or sprite previews loaded.";
    assetList.appendChild(empty);
  }
}

function buildSpriteSelectorHTML(item) {
  const catalog = projectSpriteCatalog();
  const entry = catalog.find((e) => e.speciesId === item.speciesId) || catalog[0];
  const speciesOptions = catalog.map((e) =>
    `<option value="${e.speciesId}"${e.speciesId === item.speciesId ? " selected" : ""}>${escapeHtml(e.name)}</option>`
  ).join("");

  const actions = entry?.actions || {};
  const actionOptions = Object.keys(actions).map((action) =>
    `<option value="${escapeAttr(action)}"${action === item.action ? " selected" : ""}>${escapeHtml(action)}</option>`
  ).join("");

  const frames = actions[item.action] || [];
  const frameOptions = frames.map((frame) =>
    `<option value="${escapeAttr(frame.path)}"${frame.kindName === item.frame ? " selected" : ""}>${escapeHtml(frame.kindName)}</option>`
  ).join("");

  return `
    <label class="field">Species<select id="spriteItemSpecies">${speciesOptions}</select></label>
    <div class="row">
      <label class="field">Action<select id="spriteItemAction">${actionOptions}</select></label>
      <label class="field">Frame<select id="spriteItemFrame">${frameOptions}</select></label>
    </div>
  `;
}

function bindSpriteSelectorInputs(item) {
  const speciesInput = document.getElementById("spriteItemSpecies");
  const actionInput = document.getElementById("spriteItemAction");
  const frameInput = document.getElementById("spriteItemFrame");
  if (!speciesInput || !actionInput || !frameInput) return;

  function rebuildActionOptions() {
    const entry = projectSpriteCatalog().find((e) => e.speciesId === Number(speciesInput.value));
    const actions = entry?.actions || {};
    actionInput.innerHTML = Object.keys(actions).map((action) =>
      `<option value="${escapeAttr(action)}">${escapeHtml(action)}</option>`
    ).join("");
    rebuildFrameOptions();
  }

  function rebuildFrameOptions() {
    const entry = projectSpriteCatalog().find((e) => e.speciesId === Number(speciesInput.value));
    const frames = entry?.actions?.[actionInput.value] || [];
    frameInput.innerHTML = frames.map((frame) =>
      `<option value="${escapeAttr(frame.path)}">${escapeHtml(frame.kindName)}</option>`
    ).join("");
  }

  speciesInput.addEventListener("change", () => {
    rebuildActionOptions();
    updateProjectSpriteFrame(item, Number(speciesInput.value), actionInput.value, frameInput.value).then(() => {
      refreshList();
      refreshSelectedPanel();
    });
  });
  actionInput.addEventListener("change", () => {
    rebuildFrameOptions();
    updateProjectSpriteFrame(item, Number(speciesInput.value), actionInput.value, frameInput.value).then(() => {
      refreshList();
      refreshSelectedPanel();
    });
  });
  frameInput.addEventListener("change", () => {
    updateProjectSpriteFrame(item, Number(speciesInput.value), actionInput.value, frameInput.value).then(() => {
      refreshList();
      refreshSelectedPanel();
    });
  });
}

function refreshSelectedPanel() {
  const item = selectedItem();
  if (!item) {
    selectedPanel.innerHTML = '<div class="hint">Select furniture or a sprite preview to edit transform and layer values.</div>';
    updateEditModeButtons();
    return;
  }

  const isSprite = item.source === "project_sprite";
  normalizeItemSemantics(item);
  const furnitureTypeOptions = FURNITURE_TYPES.map((type) => (
    `<option value="${type.value}" ${normalizeFurnitureType(item.furnitureType) === type.value ? "selected" : ""}>${type.label}</option>`
  )).join("");
  const itemKindOptions = ITEM_KINDS.map((kind) => (
    `<option value="${kind.value}" ${normalizeItemKind(item.kind) === kind.value ? "selected" : ""}>${kind.label}</option>`
  )).join("");
  const footprintOptions = FOOTPRINT_TYPES.map((type) => (
    `<option value="${type.value}" ${normalizeFootprint(item.footprint) === type.value ? "selected" : ""}>${type.label}</option>`
  )).join("");
  const shadowAnchorOptions = SHADOW_ANCHORS.map((anchor) => (
    `<option value="${anchor.value}" ${normalizeShadowAnchor(item.shadowAnchor) === anchor.value ? "selected" : ""}>${anchor.label}</option>`
  )).join("");

  const bounds = itemBounds(item);
  const castsShadow = !isSprite && itemCastsShadow(item);
  const usesWallShadow = !isSprite && castsShadow && itemUsesWallShadowAnchor(item);
  const shadowMeta = !isSprite
    ? (castsShadow ? (usesWallShadow ? "wall" : "floor") : "off")
    : "";
  let html = `
    <details class="property-group" open>
      <summary>
        <span>Basic</span>
        <span class="section-meta">${Math.round(bounds.w)}x${Math.round(bounds.h)}</span>
      </summary>
      <div class="property-group-body stack">
        <label class="field">Name<input id="itemName" type="text" value="${escapeAttr(item.name)}"></label>
        ${!isSprite ? `
          <div class="row">
            <label class="field">Preset<select id="itemKind">${itemKindOptions}</select></label>
            <label class="field">Category<select id="itemFurnitureType">${furnitureTypeOptions}</select></label>
          </div>
        ` : ""}
        <div class="row">
          <label class="field">X<input id="itemX" type="number" value="${Math.round(item.x)}"></label>
          <label class="field">Y<input id="itemY" type="number" value="${Math.round(item.y)}"></label>
        </div>
        <div class="row">
          <label class="field">W<input id="itemW" type="number" step="1" min="1" value="${Math.round(bounds.w)}"></label>
          <label class="field">H<input id="itemH" type="number" step="1" min="1" value="${Math.round(bounds.h)}"></label>
        </div>
        <div class="row">
          <label class="field">Scale<input id="itemScale" type="number" step="0.001" min="0.0001" value="${itemScaleX(item)}"></label>
          <label class="field">Opacity<input id="itemOpacity" type="number" step="0.05" min="0" max="1" value="${item.opacity}"></label>
        </div>
        <div class="row">
          <label class="check-row inline-check"><input id="itemVisible" type="checkbox" ${item.visible ? "checked" : ""}> Visible</label>
          <label class="check-row inline-check"><input id="itemAspectLock" type="checkbox" ${itemAspectLocked(item) ? "checked" : ""}> Lock aspect</label>
        </div>
        <div id="itemSizeHint" class="hint"></div>
      </div>
    </details>
  `;

  if (isSprite) {
    html += `
      <details class="property-group">
        <summary>
          <span>Sprite source</span>
          <span class="section-meta">${escapeHtml(item.action || "-")}</span>
        </summary>
        <div class="property-group-body stack">
          ${buildSpriteSelectorHTML(item)}
        </div>
      </details>
    `;
  } else {
    html += `
      <details class="property-group">
        <summary>
          <span>Shadow</span>
          <span class="section-meta">${shadowMeta}</span>
        </summary>
        <div class="property-group-body stack">
          <label class="check-row"><input id="itemCastsShadow" type="checkbox" ${itemCastsShadow(item) ? "checked" : ""}> Cast shadow</label>
          ${castsShadow ? `
            <div class="row">
              <label class="field">Height<input id="itemHeightPx" type="number" min="0" max="160" step="1" value="${Math.round(itemHeightPx(item))}"></label>
              <label class="field">Footprint<select id="itemFootprint">${footprintOptions}</select></label>
            </div>
            <label class="field">Shadow anchor<select id="itemShadowAnchor">${shadowAnchorOptions}</select></label>
            <div id="footprintPolygonEditor" class="polygon-editor${state.activeItemPolygonKey === "footprintPolygon" ? " active" : ""}">
              <div class="section-title-row">
                <div class="section-title">Footprint polygon</div>
                <div id="itemFootprintPolygonMeta" class="section-meta">${polygonSummary(item.footprintPolygon)}</div>
              </div>
              <textarea id="itemFootprintPolygon" rows="4" spellcheck="false" placeholder="0.18, 0.72&#10;0.82, 0.72&#10;0.96, 0.96&#10;0.04, 0.96">${escapeHtml(polygonToText(item.footprintPolygon))}</textarea>
              <div class="toolbar polygon-actions">
                <button id="setFootprintBase" type="button">Base shape</button>
                <button id="setFootprintBounds" type="button">Bounds</button>
                <button id="clearFootprintPolygon" type="button">Clear</button>
              </div>
            </div>
            <div id="shadowPolygonEditor" class="polygon-editor${state.activeItemPolygonKey === "shadowPolygon" ? " active" : ""}">
              <div class="section-title-row">
                <div class="section-title">Shadow polygon</div>
                <div id="itemShadowPolygonMeta" class="section-meta">${polygonSummary(item.shadowPolygon)}</div>
              </div>
              <textarea id="itemShadowPolygon" rows="4" spellcheck="false" placeholder="0, 0&#10;1, 0&#10;1, 1&#10;0, 1">${escapeHtml(polygonToText(item.shadowPolygon))}</textarea>
              <div class="toolbar polygon-actions">
                <button id="setShadowBounds" type="button">Bounds</button>
                <button id="copyFootprintToShadow" type="button">Use footprint</button>
                <button id="clearShadowPolygon" type="button">Clear</button>
              </div>
            </div>
            ${usesWallShadow ? `
              <div class="row">
                <label class="field">Wall depth<input id="itemWallShadowDepthPx" type="number" min="0" max="160" step="1" value="${Math.round(itemWallShadowDepthPx(item))}"></label>
                <label class="field">Wall shadow Y<input id="itemWallShadowOffsetY" type="number" min="-80" max="80" step="1" value="${Math.round(itemWallShadowOffsetY(item))}"></label>
              </div>
            ` : ""}
          ` : ""}
          <label class="check-row"><input id="itemReceivesShadow" type="checkbox" ${item.receivesShadow !== false ? "checked" : ""}> Receive shadow</label>
          <label class="check-row"><input id="itemOccludesSprite" type="checkbox" ${item.occludesSprite ? "checked" : ""}> Occlude sprite</label>
          <div class="hint">Use presets first. Fine tune wall depth/Y only for wall-mounted objects.</div>
        </div>
      </details>
    `;
  }

  html += `
    <details class="property-group">
      <summary>
        <span>Advanced</span>
        <span class="section-meta">sortY ${Math.round(itemSortY(item))}</span>
      </summary>
      <div class="property-group-body stack">
        <div class="row">
          <label class="field">Z<input id="itemZ" type="number" value="${item.z}"></label>
          <label class="field">Sort Y<input id="itemSortY" type="number" value="${Math.round(itemSortY(item))}"></label>
        </div>
        <div class="row">
          <label class="field">Slot<input id="itemSlot" type="text" value="${escapeAttr(item.slot)}"></label>
          <label class="field">Layer<input id="itemLayer" type="text" value="${escapeAttr(item.layer || "main")}"></label>
        </div>
        <div class="hint">同一 Z 层内按 Sort Y 排序。默认使用物体底部 y 值，必要时可手动调整。</div>
      </div>
    </details>
  `;

  selectedPanel.innerHTML = html;

  if (isSprite) {
    bindSpriteSelectorInputs(item);
  }

  const bind = (id, fn) => {
    const input = document.getElementById(id);
    if (!input) return;
    input.addEventListener("input", () => {
      fn(input);
      refreshList();
      render();
      commitHistory();
    });
  };
  const setInputValue = (id, value, exceptId = "") => {
    if (id === exceptId) return;
    const input = document.getElementById(id);
    if (input) input.value = value;
  };
  const updateSizeFields = (exceptId = "") => {
    const nextBounds = itemBounds(item);
    setInputValue("itemScale", Number(itemScaleX(item).toFixed(4)), exceptId);
    setInputValue("itemW", Math.round(nextBounds.w), exceptId);
    setInputValue("itemH", Math.round(nextBounds.h), exceptId);
    setInputValue("itemSortY", Math.round(itemSortY(item)), exceptId);
    const hint = document.getElementById("itemSizeHint");
    if (hint) {
      hint.textContent =
        `Source: ${item.fileName} (${item.sourceWidth}x${item.sourceHeight}) -> ` +
        `target ${Math.round(nextBounds.w)}x${Math.round(nextBounds.h)}, ` +
        `scale ${Math.round(itemScaleX(item) * 100)}% x ${Math.round(itemScaleY(item) * 100)}%`;
    }
  };
  const bindSize = (id, fn) => {
    const input = document.getElementById(id);
    if (!input) return;
    input.addEventListener("input", () => {
      fn(input);
      item.sortY = autoSortY(item);
      updateSizeFields(id);
      refreshList();
      render();
      commitHistory();
    });
  };
  bind("itemName", (input) => { item.name = input.value; });
  bind("itemX", (input) => { item.x = Number(input.value) || 0; });
  bind("itemY", (input) => {
    item.y = Number(input.value) || 0;
    item.sortY = autoSortY(item);
  });
  bind("itemZ", (input) => { item.z = Number(input.value) || 0; });
  bindSize("itemScale", (input) => { setItemUniformScale(item, input.value); });
  bindSize("itemW", (input) => { setItemTargetWidth(item, input.value); });
  bindSize("itemH", (input) => { setItemTargetHeight(item, input.value); });
  if (!isSprite) {
    const furnitureTypeInput = document.getElementById("itemFurnitureType");
    if (furnitureTypeInput) furnitureTypeInput.addEventListener("change", (event) => {
      item.furnitureType = normalizeFurnitureType(event.target.value);
      refreshList();
      render();
      commitHistory();
    });
  }
  document.getElementById("itemAspectLock").addEventListener("change", (event) => {
    setItemAspectLocked(item, event.target.checked);
    updateSizeFields();
    refreshList();
    render();
    commitHistory();
  });
  bind("itemOpacity", (input) => { item.opacity = Math.max(0, Math.min(1, Number(input.value))); });
  bind("itemSlot", (input) => { item.slot = input.value; });
  bind("itemLayer", (input) => { item.layer = input.value; });
  bind("itemSortY", (input) => { item.sortY = Number(input.value) || autoSortY(item); });
  bind("itemVisible", (input) => { item.visible = input.checked; });
  if (!isSprite) {
    const itemKindInput = document.getElementById("itemKind");
    if (itemKindInput) itemKindInput.addEventListener("change", (event) => {
      applyItemKindPreset(item, event.target.value);
      refreshList();
      refreshSelectedPanel();
      render();
      commitHistory();
    });
    bind("itemHeightPx", (input) => { item.heightPx = coerceNumber(input.value, itemHeightPx(item), 0, 160); });
    const footprintInput = document.getElementById("itemFootprint");
    if (footprintInput) footprintInput.addEventListener("change", (event) => {
      item.footprint = normalizeFootprint(event.target.value);
      refreshList();
      render();
      commitHistory();
    });
    const shadowAnchorInput = document.getElementById("itemShadowAnchor");
    if (shadowAnchorInput) shadowAnchorInput.addEventListener("change", (event) => {
      item.shadowAnchor = normalizeShadowAnchor(event.target.value);
      refreshList();
      refreshSelectedPanel();
      render();
      commitHistory();
    });
    const bindPolygonTextarea = (id, key) => {
      const input = document.getElementById(id);
      if (!input) return;
      input.addEventListener("focus", () => {
        setActiveItemPolygon(key);
        render();
      });
      input.addEventListener("input", () => {
        setActiveItemPolygon(key);
        item[key] = parsePolygonText(input.value);
        syncPolygonEditorField(item, key, { updateTextarea: false });
        render();
        commitHistory();
      });
    };
    const setPolygon = (key, value) => {
      setActiveItemPolygon(key);
      item[key] = normalizeLocalPolygon(value);
      syncPolygonEditorField(item, key);
      render();
      commitHistory();
    };
    bindPolygonTextarea("itemFootprintPolygon", "footprintPolygon");
    bindPolygonTextarea("itemShadowPolygon", "shadowPolygon");
    document.getElementById("setFootprintBase")?.addEventListener("click", (event) => {
      event.preventDefault();
      item.footprint = "polygon";
      setPolygon("footprintPolygon", defaultFootprintPolygon());
    });
    document.getElementById("setFootprintBounds")?.addEventListener("click", (event) => {
      event.preventDefault();
      item.footprint = "polygon";
      setPolygon("footprintPolygon", defaultShadowPolygon());
    });
    document.getElementById("clearFootprintPolygon")?.addEventListener("click", (event) => {
      event.preventDefault();
      setPolygon("footprintPolygon", []);
    });
    document.getElementById("setShadowBounds")?.addEventListener("click", (event) => {
      event.preventDefault();
      setPolygon("shadowPolygon", defaultShadowPolygon());
    });
    document.getElementById("copyFootprintToShadow")?.addEventListener("click", (event) => {
      event.preventDefault();
      setPolygon("shadowPolygon", effectiveFootprintPolygon(item));
    });
    document.getElementById("clearShadowPolygon")?.addEventListener("click", (event) => {
      event.preventDefault();
      setPolygon("shadowPolygon", []);
    });
    bind("itemWallShadowDepthPx", (input) => { item.wallShadowDepthPx = coerceNumber(input.value, itemWallShadowDepthPx(item), 0, 160); });
    bind("itemWallShadowOffsetY", (input) => { item.wallShadowOffsetY = coerceNumber(input.value, 0, -80, 80); });
    const castsShadowInput = document.getElementById("itemCastsShadow");
    if (castsShadowInput) castsShadowInput.addEventListener("change", (event) => {
      item.castsShadow = event.target.checked;
      refreshList();
      refreshSelectedPanel();
      render();
      commitHistory();
    });
    bind("itemReceivesShadow", (input) => { item.receivesShadow = input.checked; });
    bind("itemOccludesSprite", (input) => { item.occludesSprite = input.checked; });
  }
  updateSizeFields();
  updateEditModeButtons();
}

function pointRowsHtml(points, selectedPointId) {
  return points.map((point, index) => `
    <div class="point-row${point.id === selectedPointId ? " selected" : ""}" data-point-index="${index}">
      <button class="point-select" type="button" data-point-index="${index}">${index + 1}</button>
      <label class="field">X<input class="point-x" data-point-index="${index}" type="number" value="${Math.round(point.x)}"></label>
      <label class="field">Y<input class="point-y" data-point-index="${index}" type="number" value="${Math.round(point.y)}"></label>
    </div>
  `).join("");
}

function faceDetailHtml(face) {
  const type = normalizeFaceType(face.type);
  const isSpriteArea = type === FACE_TYPE_SPRITE_AREA;
  const surfaceOptions = SHADOW_SURFACES.map((surface) => (
    `<option value="${surface.value}" ${normalizeShadowSurface(face.shadowSurface, face.type) === surface.value ? "selected" : ""}>${surface.label}</option>`
  )).join("");
  return `
    <div class="face-detail">
      <label class="field">
        Type
        <select class="face-type-select">
          <option value="floor" ${type === "floor" ? "selected" : ""}>floor</option>
          <option value="wall" ${type === "wall" ? "selected" : ""}>wall</option>
          <option value="${FACE_TYPE_SPRITE_AREA}" ${isSpriteArea ? "selected" : ""}>sprite area</option>
        </select>
      </label>
      ${isSpriteArea ? `
        <div class="hint">This polygon is exported as a sprite movement area. It does not receive furniture shadows.</div>
      ` : `
        <div class="row">
        <label class="field">
          Surface
          <select class="face-shadow-surface">${surfaceOptions}</select>
        </label>
        <label class="check-row inline-check">
          <input class="face-receives-shadow" type="checkbox" ${face.receivesShadow !== false ? "checked" : ""}>
          Receive shadows
        </label>
        </div>
      `}
      <div class="toolbar">
        <button class="insert-point" type="button">Add point</button>
        <button class="delete-point danger" type="button">Delete point</button>
      </div>
      <div class="point-list">
        ${pointRowsHtml(face.points, state.selectedPointId)}
      </div>
      <div class="hint">${escapeHtml(face.id)} ${escapeHtml(faceTypeLabel(face.type))} points. Export scales these to game pixels.</div>
    </div>
  `;
}

function draftDetailHtml() {
  return `
    <div class="face-detail">
      <div class="hint">Click the first point on the canvas to close this draft face.</div>
      <div class="toolbar">
        <button class="delete-point danger" type="button">Delete point</button>
      </div>
      <div class="point-list">
        ${pointRowsHtml(state.draftPoints, state.selectedPointId)}
      </div>
      <div class="hint">Draft points can be dragged, edited, deleted, or reused with closed-face points.</div>
    </div>
  `;
}

function selectFaceItem(face, options = {}) {
  state.selectedFaceId = face.id;
  state.selectedPointIndex = null;
  state.selectedPointId = null;
  state.selectedId = null;
  state.editMode = "shape";
  if (options.expand !== false) state.expandedFaceIds.add(face.id);
  updateEditModeButtons();
  refreshFaceList();
  refreshList();
  refreshSelectedPanel();
  render();
}

function selectDraftItem(options = {}) {
  state.selectedFaceId = null;
  state.selectedId = null;
  state.editMode = "shape";
  if (state.selectedPointIndex == null && state.draftPoints.length) selectDraftPoint(0);
  if (options.expand !== false) state.draftExpanded = true;
  updateEditModeButtons();
  refreshFaceList();
  refreshList();
  refreshSelectedPanel();
  render();
}

function toggleFaceExpanded(face) {
  if (state.expandedFaceIds.has(face.id)) {
    state.expandedFaceIds.delete(face.id);
  } else {
    state.expandedFaceIds.add(face.id);
  }
  selectFaceItem(face, { expand: false });
}

function toggleDraftExpanded() {
  state.draftExpanded = !state.draftExpanded;
  selectDraftItem({ expand: false });
}

function bindFaceDetailControls(face, root) {
  const detail = root.querySelector(".face-detail");
  if (!detail) return;
  detail.addEventListener("click", (event) => event.stopPropagation());

  detail.querySelector(".face-type-select").addEventListener("input", (event) => {
    const previousSurface = normalizeShadowSurface(face.shadowSurface, face.type);
    const previousType = normalizeFaceType(face.type);
    face.type = normalizeFaceType(event.target.value);
    if (face.type === FACE_TYPE_SPRITE_AREA) {
      face.shadowSurface = "floor";
      face.receivesShadow = false;
    } else if (previousType === FACE_TYPE_SPRITE_AREA) {
      face.receivesShadow = true;
      face.shadowSurface = normalizeShadowSurface(null, face.type);
    } else if ((previousSurface === "floor" && face.type === "wall") ||
        (previousSurface !== "floor" && face.type === "floor")) {
      face.shadowSurface = normalizeShadowSurface(null, face.type);
    }
    refreshFaceList();
    render();
    commitHistory();
  });
  const surfaceSelect = detail.querySelector(".face-shadow-surface");
  if (surfaceSelect) surfaceSelect.addEventListener("input", (event) => {
    face.shadowSurface = normalizeShadowSurface(event.target.value, face.type);
    refreshFaceList();
    render();
    commitHistory();
  });
  const receivesShadow = detail.querySelector(".face-receives-shadow");
  if (receivesShadow) receivesShadow.addEventListener("change", (event) => {
    face.receivesShadow = event.target.checked;
    refreshFaceList();
    render();
    commitHistory();
  });
  detail.querySelector(".insert-point").addEventListener("click", insertPointAfterSelected);
  detail.querySelector(".delete-point").addEventListener("click", deleteSelectedPoint);

  for (let index = 0; index < face.points.length; ++index) {
    detail.querySelector(`.point-select[data-point-index="${index}"]`).addEventListener("click", () => {
      selectPoint(index);
    });
    detail.querySelector(`.point-x[data-point-index="${index}"]`).addEventListener("input", (event) => {
      face.points[index].x = Math.round(Number(event.target.value) || 0);
      selectPointInFace(face, index);
      render();
      commitHistory();
    });
    detail.querySelector(`.point-y[data-point-index="${index}"]`).addEventListener("input", (event) => {
      face.points[index].y = Math.round(Number(event.target.value) || 0);
      selectPointInFace(face, index);
      render();
      commitHistory();
    });
  }
}

function bindDraftDetailControls(root) {
  const detail = root.querySelector(".face-detail");
  if (!detail) return;
  detail.addEventListener("click", (event) => event.stopPropagation());
  detail.querySelector(".delete-point").addEventListener("click", deleteSelectedPoint);

  for (let index = 0; index < state.draftPoints.length; ++index) {
    detail.querySelector(`.point-select[data-point-index="${index}"]`).addEventListener("click", () => {
      selectDraftPoint(index);
      refreshFaceList();
      render();
    });
    detail.querySelector(`.point-x[data-point-index="${index}"]`).addEventListener("input", (event) => {
      state.draftPoints[index].x = Math.round(Number(event.target.value) || 0);
      selectDraftPoint(index);
      render();
      commitHistory();
    });
    detail.querySelector(`.point-y[data-point-index="${index}"]`).addEventListener("input", (event) => {
      state.draftPoints[index].y = Math.round(Number(event.target.value) || 0);
      selectDraftPoint(index);
      render();
      commitHistory();
    });
  }
}

function deleteFaceById(faceId) {
  const face = state.faces.find((entry) => entry.id === faceId);
  if (!face) return;
  if (!window.confirm(`Delete ${face.id}?`)) return;
  state.faces = state.faces.filter((entry) => entry.id !== face.id);
  state.expandedFaceIds.delete(face.id);
  if (state.selectedFaceId === face.id) {
    state.selectedFaceId = null;
    state.selectedPointIndex = null;
    state.selectedPointId = null;
  }
  pruneUnusedPoints();
  refreshFaceList();
  render();
  commitHistory();
}

function refreshFaceList() {
  faceList.innerHTML = "";
  const hasFaceData = state.faces.length > 0 || state.draftPoints.length > 0;
  if (roomFacesPanel) {
    roomFacesPanel.hidden = !hasFaceData;
    if (hasFaceData && state.editMode === "shape" && !state.lightSelected) {
      roomFacesPanel.open = true;
    }
  }
  if (!hasFaceData) {
    selectedFacePanel.innerHTML = "";
    selectedFacePanel.hidden = true;
    return;
  }

  const validFaceIds = new Set(state.faces.map((face) => face.id));
  for (const faceId of [...state.expandedFaceIds]) {
    if (!validFaceIds.has(faceId)) state.expandedFaceIds.delete(faceId);
  }
  for (const face of state.faces) {
    const selected = face.id === state.selectedFaceId;
    const expanded = state.expandedFaceIds.has(face.id);
    const el = document.createElement("div");
    el.className = `face-item${selected ? " selected" : ""}`;
    el.innerHTML = `
      <div class="face-summary">
        <div class="face-color" style="background:${faceStrokeColor(face.type)}"></div>
        <div>
          <div class="asset-name">${escapeHtml(face.id)}</div>
          <div class="asset-meta">${escapeHtml(faceTypeLabel(face.type))}, ${isSpriteAreaFace(face) ? "movement area" : `${escapeHtml(shadowSurfaceForFace(face))}, ${face.receivesShadow === false ? "no shadow" : "receives shadow"}`}, ${face.points.length} points</div>
        </div>
        <button class="face-toggle" type="button" title="${expanded ? "Collapse points" : "Expand points"}">${expanded ? "^" : "v"}</button>
        <button class="face-delete danger" type="button" title="Delete face">x</button>
      </div>
      ${expanded ? faceDetailHtml(face) : ""}
    `;
    el.querySelector(".face-summary").addEventListener("click", () => selectFaceItem(face));
    el.querySelector(".face-toggle").addEventListener("click", (event) => {
      event.stopPropagation();
      toggleFaceExpanded(face);
    });
    el.querySelector(".face-delete").addEventListener("click", (event) => {
      event.stopPropagation();
      deleteFaceById(face.id);
    });
    if (expanded) bindFaceDetailControls(face, el);
    faceList.appendChild(el);
  }

  if (state.draftPoints.length > 0) {
    const selected = !state.selectedFaceId;
    const expanded = state.draftExpanded;
    const el = document.createElement("div");
    el.className = `face-item draft${selected ? " selected" : ""}`;
    el.innerHTML = `
      <div class="face-summary">
        <div class="face-color" style="background:#ffffff"></div>
        <div>
          <div class="asset-name">Draft points</div>
          <div class="asset-meta">${state.draftPoints.length} unclosed point(s)</div>
        </div>
        <button class="face-toggle" type="button" title="${expanded ? "Collapse points" : "Expand points"}">${expanded ? "^" : "v"}</button>
        <div></div>
      </div>
      ${expanded ? draftDetailHtml() : ""}
    `;
    el.querySelector(".face-summary").addEventListener("click", () => selectDraftItem());
    el.querySelector(".face-toggle").addEventListener("click", (event) => {
      event.stopPropagation();
      toggleDraftExpanded();
    });
    if (expanded) bindDraftDetailControls(el);
    faceList.appendChild(el);
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
  commitHistory();
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
    commitHistory();
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
  commitHistory();
}

function refreshSelectedFacePanel() {
  selectedFacePanel.innerHTML = "";
  selectedFacePanel.hidden = true;
  refreshFaceList();
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

function polygonHitKeys() {
  const active = state.activeItemPolygonKey === "shadowPolygon" ? "shadowPolygon" : "footprintPolygon";
  return active === "shadowPolygon"
    ? ["shadowPolygon", "footprintPolygon"]
    : ["footprintPolygon", "shadowPolygon"];
}

function hitTestItemPolygonPoint(x, y) {
  const item = selectedItem();
  if (!item || item.source === "project_sprite") return null;
  const radius = editHandleRadius() + 4;
  let best = null;
  for (const key of polygonHitKeys()) {
    const points = itemPolygonEditPoints(item, key);
    for (let index = points.length - 1; index >= 0; --index) {
      const hitDistance = distance({ x, y }, points[index]);
      if (hitDistance <= radius && (!best || hitDistance < best.distance)) {
        best = { item, key, index, distance: hitDistance };
      }
    }
  }
  return best;
}

function updateDraggedItemPolygon(point) {
  const item = selectedItem();
  if (!item || !state.dragPolygonKey || state.dragPolygonIndex == null) return;
  const polygon = materializeItemPolygon(item, state.dragPolygonKey);
  const bounds = itemBounds(item);
  if (!polygon[state.dragPolygonIndex] || bounds.w <= 0 || bounds.h <= 0) return;
  const target = editToTargetPoint(point);
  polygon[state.dragPolygonIndex] = {
    x: Number(clampUnit((target.x - bounds.x) / bounds.w).toFixed(4)),
    y: Number(clampUnit((target.y - bounds.y) / bounds.h).toFixed(4))
  };
  syncPolygonEditorField(item, state.dragPolygonKey);
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

function hitTestLight(x, y) {
  return distance({ x, y }, lightEditPoint()) <= LIGHT_HANDLE_RADIUS + 5;
}

function hitTestConeControl(x, y) {
  if (!state.lightSelected || normalizeLightShape(editLightSettings().lightShape) !== "cone") return false;
  return distance({ x, y }, coneControlPoint()) <= LIGHT_HANDLE_RADIUS + 5;
}

function setLightPositionFromCanvasPoint(point) {
  const targetPoint = editToTargetPoint(point);
  const x = Math.round(clampNumber(targetPoint.x, 0, state.width));
  const y = Math.round(clampNumber(targetPoint.y, 0, state.height));
  const light = editLightSettings();
  light.lightX = x;
  light.lightY = y;
  document.getElementById("lightX").value = x;
  document.getElementById("lightY").value = y;
}

function closeDraftFace() {
  const uniquePointIds = new Set(state.draftPoints.map((point) => point.id));
  if (state.draftPoints.length < 3 || uniquePointIds.size < 3) return;
  const face = {
    id: `face${state.nextFaceId++}`,
    type: "floor",
    shadowSurface: "floor",
    receivesShadow: true,
    points: state.draftPoints.slice()
  };
  state.faces.push(face);
  state.selectedFaceId = face.id;
  state.selectedPointIndex = 0;
  state.selectedPointId = face.points[0].id;
  state.expandedFaceIds.add(face.id);
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
  commitHistory();
}

function updateModeButtons() {
  document.getElementById("dayMode").classList.toggle("active", state.mode === "day");
  document.getElementById("nightMode").classList.toggle("active", state.mode === "night");
  updateLightProfileButtons();
}

function updateEditModeButtons() {
  const isFurniture = state.editMode === "furniture";
  const hasSelectedItem = Boolean(selectedItem());
  document.getElementById("furnitureMode").classList.toggle("active", isFurniture);
  document.getElementById("shapeMode").classList.toggle("active", !isFurniture);

  document.getElementById("duplicateItem").disabled = !isFurniture || !hasSelectedItem;
  document.getElementById("deleteItem").disabled = !isFurniture || !hasSelectedItem;
  syncInspectorWorkflow();
}

function setDetailsOpen(panel, open) {
  if (!panel || panel.hidden) return;
  panel.open = Boolean(open);
}

function syncInspectorWorkflow() {
  const isShape = state.editMode === "shape";
  const isLight = state.lightSelected;
  if (propertiesPanel) propertiesPanel.hidden = isShape || isLight;

  if (isLight) {
    setDetailsOpen(nightOverlayPanel, true);
    setDetailsOpen(roomFacesPanel, false);
    setDetailsOpen(layersPanel, false);
    return;
  }

  if (isShape) {
    setDetailsOpen(roomFacesPanel, true);
    setDetailsOpen(layersPanel, false);
    setDetailsOpen(nightOverlayPanel, false);
    return;
  }

  setDetailsOpen(layersPanel, true);
  setDetailsOpen(roomFacesPanel, false);
  setDetailsOpen(nightOverlayPanel, false);
}

function updatePreviewZoom() {
  previewCanvas.classList.toggle("zoomed", state.previewZoomed);
}

function updateUndoButtons() {
  document.getElementById("undoAction").disabled = state.historyIndex <= 0;
  document.getElementById("redoAction").disabled = state.historyIndex >= state.history.length - 1;
}

function takeSnapshot() {
  const pointMap = new Map();
  const copyPoint = (p) => {
    if (!p) return p;
    if (pointMap.has(p.id)) return pointMap.get(p.id);
    const copy = { ...p };
    pointMap.set(p.id, copy);
    return copy;
  };

  return {
    width: state.width,
    height: state.height,
    editWidth: state.editWidth,
    editHeight: state.editHeight,
    editZoom: state.editZoom,
    pointCoordinateSpace: "edit",
    mode: state.mode,
    lightProfile: state.lightProfile,
    base: state.base ? { ...state.base } : null,
    baseFit: BASE_FIT_WIDTH,
    baseOpacity: state.baseOpacity,
    sourceScale: state.sourceScale,
    baseTrim: state.baseTrim ? { ...state.baseTrim } : null,
    furnitureImportMode: state.furnitureImportMode,
    aspectLocked: state.aspectLocked,
    aspectRatio: state.aspectRatio,
    items: state.items.map((item) => {
      const { originalDataUrl, ...snapshotItem } = item;
      return snapshotItem;
    }),
    selectedId: state.selectedId,
    nextId: state.nextId,
    points: state.points.map(copyPoint),
    faces: state.faces.map((face) => ({
      ...face,
      points: face.points.map(copyPoint)
    })),
    selectedFaceId: state.selectedFaceId,
    selectedPointIndex: state.selectedPointIndex,
    selectedPointId: state.selectedPointId,
    expandedFaceIds: [...state.expandedFaceIds],
    draftExpanded: state.draftExpanded,
    nextFaceId: state.nextFaceId,
    nextPointId: state.nextPointId,
    draftPoints: state.draftPoints.map(copyPoint),
    drawingFace: state.drawingFace,
    guides: { ...state.guides },
    night: JSON.parse(JSON.stringify(state.night)),
    showGrid: state.showGrid,
    gridOpacity: state.gridOpacity,
    snapToGrid: state.snapToGrid
  };
}

function createSessionSnapshot() {
  return JSON.parse(JSON.stringify(takeSnapshot(), (key, value) => {
    if (key === "img") return undefined;
    return value;
  }));
}

function createSessionRecord() {
  const snapshot = createSessionSnapshot();
  return {
    version: 1,
    type: "stickmon-room-editor-session",
    savedAt: new Date().toISOString(),
    files: {
      base: snapshot.base?.name || "",
      items: (snapshot.items || []).map((item) => ({
        id: item.id,
        name: item.name,
        fileName: item.fileName,
        source: item.source || "furniture",
        furnitureType: item.furnitureType || "",
        kind: item.kind || ""
      }))
    },
    snapshot
  };
}

function createSessionFileName(record) {
  const stamp = String(record.savedAt || new Date().toISOString())
    .replace(/[:.]/g, "-")
    .replace("T", "_")
    .replace("Z", "");
  return `stickmon_room_editor_session_${stamp}.json`;
}

function isFilePickerAbort(error) {
  return error?.name === "AbortError";
}

function openSessionDb() {
  if (!window.indexedDB) return Promise.reject(new Error("IndexedDB is not available."));
  return new Promise((resolve, reject) => {
    const request = indexedDB.open(SESSION_DB_NAME, 1);
    request.onupgradeneeded = () => {
      const db = request.result;
      if (!db.objectStoreNames.contains(SESSION_DB_STORE)) {
        db.createObjectStore(SESSION_DB_STORE);
      }
    };
    request.onerror = () => reject(request.error);
    request.onsuccess = () => resolve(request.result);
  });
}

async function writeSessionToIndexedDb(record) {
  const db = await openSessionDb();
  try {
    await new Promise((resolve, reject) => {
      const transaction = db.transaction(SESSION_DB_STORE, "readwrite");
      transaction.objectStore(SESSION_DB_STORE).put(record, SESSION_DB_KEY);
      transaction.oncomplete = resolve;
      transaction.onerror = () => reject(transaction.error);
      transaction.onabort = () => reject(transaction.error);
    });
  } finally {
    db.close();
  }
}

async function writeSessionExportHandle(handle) {
  const db = await openSessionDb();
  try {
    await new Promise((resolve, reject) => {
      const transaction = db.transaction(SESSION_DB_STORE, "readwrite");
      transaction.objectStore(SESSION_DB_STORE).put(handle, SESSION_EXPORT_HANDLE_KEY);
      transaction.oncomplete = resolve;
      transaction.onerror = () => reject(transaction.error);
      transaction.onabort = () => reject(transaction.error);
    });
  } finally {
    db.close();
  }
}

async function readSessionFromIndexedDb() {
  const db = await openSessionDb();
  try {
    return await new Promise((resolve, reject) => {
      const transaction = db.transaction(SESSION_DB_STORE, "readonly");
      const request = transaction.objectStore(SESSION_DB_STORE).get(SESSION_DB_KEY);
      request.onsuccess = () => resolve(request.result || null);
      request.onerror = () => reject(request.error);
    });
  } finally {
    db.close();
  }
}

async function readSessionExportHandle() {
  const db = await openSessionDb();
  try {
    return await new Promise((resolve, reject) => {
      const transaction = db.transaction(SESSION_DB_STORE, "readonly");
      const request = transaction.objectStore(SESSION_DB_STORE).get(SESSION_EXPORT_HANDLE_KEY);
      request.onsuccess = () => resolve(request.result || null);
      request.onerror = () => reject(request.error);
    });
  } finally {
    db.close();
  }
}

async function clearSessionExportHandleFromIndexedDb() {
  const db = await openSessionDb();
  try {
    await new Promise((resolve, reject) => {
      const transaction = db.transaction(SESSION_DB_STORE, "readwrite");
      transaction.objectStore(SESSION_DB_STORE).delete(SESSION_EXPORT_HANDLE_KEY);
      transaction.oncomplete = resolve;
      transaction.onerror = () => reject(transaction.error);
      transaction.onabort = () => reject(transaction.error);
    });
  } finally {
    db.close();
  }
}

async function clearSessionFromIndexedDb() {
  const db = await openSessionDb();
  try {
    await new Promise((resolve, reject) => {
      const transaction = db.transaction(SESSION_DB_STORE, "readwrite");
      transaction.objectStore(SESSION_DB_STORE).delete(SESSION_DB_KEY);
      transaction.objectStore(SESSION_DB_STORE).delete(SESSION_EXPORT_HANDLE_KEY);
      transaction.oncomplete = resolve;
      transaction.onerror = () => reject(transaction.error);
      transaction.onabort = () => reject(transaction.error);
    });
  } finally {
    db.close();
  }
}

async function saveSessionRecord(record) {
  try {
    await writeSessionToIndexedDb(record);
    try {
      localStorage.removeItem(SESSION_STORAGE_KEY);
    } catch (_) {}
    return "IndexedDB";
  } catch (dbError) {
    try {
      localStorage.setItem(SESSION_STORAGE_KEY, JSON.stringify(record));
      return "localStorage";
    } catch (storageError) {
      throw new Error(`Could not save session. IndexedDB: ${dbError.message}; localStorage: ${storageError.message}`);
    }
  }
}

async function readSessionRecord() {
  try {
    const record = await readSessionFromIndexedDb();
    if (record) return record;
  } catch (_) {}
  try {
    const raw = localStorage.getItem(SESSION_STORAGE_KEY);
    return raw ? JSON.parse(raw) : null;
  } catch (_) {
    return null;
  }
}

async function clearSessionRecord() {
  try {
    await clearSessionFromIndexedDb();
  } catch (_) {}
  try {
    localStorage.removeItem(SESSION_STORAGE_KEY);
  } catch (_) {}
}

function updateSessionExportMeta(text) {
  if (sessionExportMeta) sessionExportMeta.textContent = text;
}

async function forgetSessionExportHandle() {
  try {
    await clearSessionExportHandleFromIndexedDb();
  } catch (_) {}
}

async function ensureSessionExportHandle(record, forcePick = false) {
  if (!window.showSaveFilePicker) return null;

  let handle = null;
  if (!forcePick) {
    try {
      handle = await readSessionExportHandle();
    } catch (_) {}
  }

  if (handle) {
    try {
      if (await verifyFileHandlePermission(handle, "readwrite")) return handle;
    } catch (_) {
      handle = null;
    }
  }

  handle = await window.showSaveFilePicker({
    suggestedName: createSessionFileName(record),
    types: [{
      description: "JSON files",
      accept: { "application/json": [".json"] }
    }]
  });
  await writeSessionExportHandle(handle);
  return handle;
}

async function verifyFileHandlePermission(handle, mode = "readwrite") {
  if (!handle?.queryPermission || !handle?.requestPermission) return false;
  const options = { mode };
  if (await handle.queryPermission(options) === "granted") return true;
  return await handle.requestPermission(options) === "granted";
}

async function writeSessionFile(record, forcePick = false) {
  const text = JSON.stringify(record, null, 2);
  if (!window.showSaveFilePicker) {
    downloadText(createSessionFileName(record), text, "application/json");
    return "downloaded";
  }

  const handle = await ensureSessionExportHandle(record, forcePick);
  if (!handle) {
    downloadText(createSessionFileName(record), text, "application/json");
    return "downloaded";
  }

  const writable = await handle.createWritable();
  try {
    await writable.write(text);
  } finally {
    await writable.close();
  }
  return `updated ${handle.name || "session file"}`;
}

async function hydrateSessionSnapshot(snapshot) {
  const hydrated = JSON.parse(JSON.stringify(snapshot));
  if (hydrated.base?.dataUrl) {
    const img = await loadImageSource(hydrated.base.dataUrl);
    hydrated.base.img = img;
    hydrated.base.width = hydrated.base.width || img.naturalWidth;
    hydrated.base.height = hydrated.base.height || img.naturalHeight;
  }

  hydrated.items = await Promise.all((hydrated.items || []).map(async (item) => {
    const next = { ...item };
    if (!next.dataUrl) {
      throw new Error(`Saved item is missing image data: ${next.fileName || next.name || next.id}`);
    }
    const img = await loadImageSource(next.dataUrl);
    next.img = img;
    next.sourceWidth = next.sourceWidth || img.naturalWidth;
    next.sourceHeight = next.sourceHeight || img.naturalHeight;
    next.scaleX = next.scaleX ?? next.scale ?? 1;
    next.scaleY = next.scaleY ?? next.scale ?? 1;
    next.aspectLocked = next.aspectLocked ?? Math.abs(next.scaleX - next.scaleY) < 0.0001;
    if (next.source === "furniture") {
      next.furnitureType = normalizeFurnitureType(next.furnitureType || inferFurnitureType(next.fileName || next.name));
    }
    normalizeItemSemantics(next);
    return next;
  }));
  return hydrated;
}

function refreshAfterStateRestore() {
  syncInputsFromState();
  refreshList();
  refreshSelectedPanel();
  refreshFaceList();
  refreshSelectedFacePanel();
  updateBaseImageMeta();
  updateModeButtons();
  updateEditModeButtons();
  updateToolButtons();
  updatePreviewZoom();
  render();
  updateUndoButtons();
}

async function saveEditorSession(options = {}) {
  const record = createSessionRecord();
  const backend = await saveSessionRecord(record);
  let exportResult = "";
  try {
    exportResult = await writeSessionFile(record, options.forcePick === true);
  } catch (error) {
    if (isFilePickerAbort(error)) {
      exportResult = "export location not selected";
    } else {
      console.error(error);
      exportResult = `export failed: ${error.message || error}`;
    }
  }
  const itemCount = record.snapshot.items?.length || 0;
  updateSessionExportMeta(`Last save: ${backend}, ${exportResult}.`);
  setStatus(`Session saved to ${backend}, ${exportResult}: ${record.files.base || "no background"}, ${itemCount} item(s).`);
}

async function loadEditorSession() {
  const record = await readSessionRecord();
  if (!record?.snapshot) {
    setStatus("No saved session found.");
    return;
  }
  const snapshot = await hydrateSessionSnapshot(record.snapshot);
  restoreSnapshot(snapshot);
  state.history = [];
  state.historyIndex = -1;
  refreshAfterStateRestore();
  commitHistory();
  updateSessionExportMeta(`Loaded browser session saved at ${record.savedAt ? new Date(record.savedAt).toLocaleString() : "unknown time"}.`);
  setStatus(`Session loaded from ${record.savedAt ? new Date(record.savedAt).toLocaleString() : "saved state"}.`);
}

async function clearEditorSession() {
  await clearSessionRecord();
  updateSessionExportMeta("No saved session. Save will ask for a new local JSON file when supported.");
  setStatus("Saved session cleared.");
}

function restoreSnapshot(snapshot) {
  const legacySourceCoordinateSpace =
    Number(snapshot.editWidth || 0) > Math.max(GAME_SCREEN_WIDTH * 1.5, Number(snapshot.width || 0) * 1.5);
  const pointCoordinateSpace = snapshot.pointCoordinateSpace || (legacySourceCoordinateSpace ? "source" : "target");
  const pointMap = new Map();
  for (const p of snapshot.points || []) {
    pointMap.set(p.id, { ...p });
  }
  const restorePoint = (p) => pointMap.get(p?.id);

  state.width = snapshot.width;
  state.height = snapshot.height;
  state.editWidth = snapshot.editWidth;
  state.editHeight = snapshot.editHeight;
  state.editZoom = normalizedEditZoom(snapshot.editZoom == null ? 100 : Number(snapshot.editZoom) <= 3 ? Number(snapshot.editZoom) * 100 : snapshot.editZoom);
  state.mode = snapshot.mode;
  state.lightProfile = normalizeLightProfile(snapshot.lightProfile || (snapshot.night?.separateModeLights ? state.mode : "shared"));
  state.base = snapshot.base;
  state.baseFit = BASE_FIT_WIDTH;
  state.baseOpacity = snapshot.baseOpacity;
  state.sourceScale = snapshot.sourceScale;
  state.baseTrim = snapshot.baseTrim ? { ...snapshot.baseTrim } : null;
  state.furnitureImportMode = snapshot.furnitureImportMode;
  state.aspectLocked = snapshot.aspectLocked;
  state.aspectRatio = snapshot.aspectRatio;
  state.items = (snapshot.items || []).map((item) => ({ ...item }));
  state.selectedId = snapshot.selectedId;
  state.nextId = snapshot.nextId;
  state.points = Array.from(pointMap.values());
  state.faces = (snapshot.faces || []).map((face) => ({
    ...face,
    points: face.points.map(restorePoint).filter(Boolean)
  }));
  state.selectedFaceId = snapshot.selectedFaceId;
  state.selectedPointIndex = snapshot.selectedPointIndex;
  state.selectedPointId = snapshot.selectedPointId;
  state.expandedFaceIds = new Set(snapshot.expandedFaceIds || []);
  state.draftExpanded = snapshot.draftExpanded ?? true;
  state.nextFaceId = snapshot.nextFaceId;
  state.nextPointId = snapshot.nextPointId;
  state.draftPoints = (snapshot.draftPoints || []).map(restorePoint).filter(Boolean);
  state.drawingFace = snapshot.drawingFace;
  state.guides = { ...snapshot.guides };
  state.night = normalizeNightSettings(snapshot.night);
  state.lightProfile = state.night.separateModeLights ?
    normalizeLightProfile(state.lightProfile === "shared" ? state.mode : state.lightProfile) :
    "shared";
  state.showGrid = snapshot.showGrid;
  state.gridOpacity = clampNumber(snapshot.gridOpacity ?? 0.18, 0, 1);
  state.snapToGrid = snapshot.snapToGrid;
  syncCanvasSizeToPreparedRoom();
  if (pointCoordinateSpace !== "edit") {
    for (const point of state.points) {
      const target = pointCoordinateSpace === "source" ? sourcePointToTargetPoint(point) : point;
      const edit = targetToEditPoint(target);
      point.x = Math.round(clampNumber(edit.x, 0, state.editWidth));
      point.y = Math.round(clampNumber(edit.y, 0, state.editHeight));
    }
  }
}

function snapshotCompareKey(snapshot) {
  return JSON.stringify(snapshot, (key, value) => {
    if (key === "img" || key === "dataUrl" || key === "originalDataUrl") return undefined;
    if (key === "editZoom") return undefined;
    return value;
  });
}

function snapshotsEqual(a, b) {
  if (!a || !b) return false;
  return snapshotCompareKey(a) === snapshotCompareKey(b);
}

function commitHistory() {
  const snapshot = takeSnapshot();
  const current = state.history[state.historyIndex];
  if (snapshotsEqual(current, snapshot)) {
    updateUndoButtons();
    return;
  }
  state.history = state.history.slice(0, state.historyIndex + 1);
  state.history.push(snapshot);
  if (state.history.length > state.maxHistory) {
    state.history.shift();
  }
  state.historyIndex = state.history.length - 1;
  updateUndoButtons();
}

function undo() {
  if (state.historyIndex <= 0) return;
  state.historyIndex -= 1;
  restoreSnapshot(state.history[state.historyIndex]);
  syncInputsFromState();
  refreshList();
  refreshSelectedPanel();
  refreshFaceList();
  refreshSelectedFacePanel();
  updateEditModeButtons();
  render();
  updateUndoButtons();
  setStatus("Undone.");
}

function redo() {
  if (state.historyIndex >= state.history.length - 1) return;
  state.historyIndex += 1;
  restoreSnapshot(state.history[state.historyIndex]);
  syncInputsFromState();
  refreshList();
  refreshSelectedPanel();
  refreshFaceList();
  refreshSelectedFacePanel();
  updateEditModeButtons();
  render();
  updateUndoButtons();
  setStatus("Redone.");
}

function syncInputsFromState() {
  document.getElementById("canvasW").value = state.width;
  document.getElementById("canvasH").value = state.height;
  document.getElementById("aspectLock").checked = state.aspectLocked;
  document.getElementById("baseOpacity").value = Math.round(state.baseOpacity * 100);
  document.getElementById("sourceScale").value = state.sourceScale;
  document.getElementById("furnitureImportMode").value = state.furnitureImportMode;
  document.getElementById("baseFit").value = BASE_FIT_WIDTH;
  document.getElementById("showGuides").checked = state.guides.show;
  document.getElementById("showGrid").checked = state.showGrid;
  document.getElementById("gridOpacity").value = Math.round(state.gridOpacity * 100);
  document.getElementById("snapToGrid").checked = state.snapToGrid;
  document.getElementById("spriteX").value = state.guides.spriteX;
  document.getElementById("spriteY").value = state.guides.spriteY;
  document.getElementById("spriteW").value = state.guides.spriteW;
  document.getElementById("spriteH").value = state.guides.spriteH;
  document.getElementById("nightTint").value = state.night.tint;
  document.getElementById("nightDarken").value = state.night.darken;
  syncLightInputsFromState();
  document.getElementById("castShadows").checked = state.night.castShadows;
  document.getElementById("shadowMode").value = state.night.shadowMode;
  document.getElementById("shadowAlpha").value = state.night.shadowAlpha;
  document.getElementById("shadowLength").value = state.night.shadowLength;
  document.getElementById("shadowBlur").value = state.night.shadowBlur;
  updateBaseImageMeta();
  updateModeButtons();
  updateEditModeButtons();
  updateToolButtons();
  updateZoomControls();
  updateGridOpacityLabel();
}

function updateWorkflowStatus() {
  if (!state.base) {
    setWorkflowStatus("Step 1/4: Drop a reference image to start.");
    return;
  }
  if (state.faces.length === 0) {
    setWorkflowStatus("Step 2/4: Switch to Shape mode and trace wall, floor, or sprite-area faces.");
    return;
  }
  if (state.items.length === 0) {
    setWorkflowStatus("Step 3/4: Switch to Furniture mode and import PNG furniture.");
    return;
  }
  setWorkflowStatus("Step 4/4: Tune preview, add sprites, and export.");
}

function sharedPointFaceCount(point) {
  if (!point) return 0;
  let count = 0;
  if (state.draftPoints.some((entry) => entry.id === point.id)) count += 1;
  for (const face of state.faces) {
    if (face.points.some((entry) => entry.id === point.id)) count += 1;
  }
  return count;
}

function updateSharedPointHint() {
  const point = selectedPoint();
  if (!point || state.editMode !== "shape") return;
  const count = sharedPointFaceCount(point);
  if (count > 1) {
    setWorkflowStatus(`Shared point used by ${count} faces/drafts. Dragging affects all of them.`);
  }
}

function exportLayoutObject() {
  const prepared = preparedRoomMetrics();
  return {
    version: 2,
    canvas: {
      width: state.width,
      height: state.height,
      screenWidth: GAME_SCREEN_WIDTH,
      screenHeight: GAME_SCREEN_HEIGHT
    },
    base: {
      fileName: state.base ? state.base.name : "",
      fit: BASE_FIT_WIDTH,
      sourceWidth: state.base ? state.base.width : 0,
      sourceHeight: state.base ? state.base.height : 0,
      trim: { ...prepared.trim }
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
      scale: Number(itemScaleX(item).toFixed(4)),
      scaleX: Number(itemScaleX(item).toFixed(4)),
      scaleY: Number(itemScaleY(item).toFixed(4)),
      aspectLocked: itemAspectLocked(item),
      targetWidth: Math.round(itemTargetWidth(item)),
      targetHeight: Math.round(itemTargetHeight(item)),
      sourceScale: Number((item.sourceScale || state.sourceScale).toFixed(4)),
      opacity: Number(item.opacity.toFixed(4)),
      visible: item.visible,
      anchor: item.anchor,
      slot: item.slot,
      layer: item.layer,
      source: item.source || "furniture",
      furnitureType: item.source === "furniture" ? normalizeFurnitureType(item.furnitureType) : "",
      kind: normalizeItemKind(item.kind),
      heightPx: Math.round(itemHeightPx(item)),
      footprint: normalizeFootprint(item.footprint),
      footprintPolygon: compactLocalPolygon(item.footprintPolygon),
      shadowPolygon: compactLocalPolygon(item.shadowPolygon),
      shadowAnchor: normalizeShadowAnchor(item.shadowAnchor),
      wallShadowDepthPx: Math.round(itemWallShadowDepthPx(item)),
      wallShadowOffsetY: Math.round(itemWallShadowOffsetY(item)),
      castsShadow: itemCastsShadow(item),
      receivesShadow: item.receivesShadow !== false,
      occludesSprite: !!item.occludesSprite,
      sortY: Math.round(itemSortY(item)),
      cutoutApplied: !!item.cutoutApplied,
      speciesId: item.speciesId || 0,
      action: item.action || "",
      frame: item.frame || "",
      sourceWidth: item.sourceWidth,
      sourceHeight: item.sourceHeight
    }))
  };
}

function exportRoomGeometryObject() {
  const prepared = preparedRoomMetrics();
  return {
    version: 2,
    coordinateSpace: "target",
    room: {
      width: prepared.width,
      height: prepared.height
    },
    viewport: {
      width: GAME_SCREEN_WIDTH,
      height: GAME_SCREEN_HEIGHT
    },
    canvas: {
      width: state.width,
      height: state.height
    },
    edit: {
      width: state.editWidth,
      height: state.editHeight
    },
    prepared: {
      width: prepared.width,
      height: prepared.height,
      y: prepared.y,
      scale: Number(prepared.scale.toFixed(8)),
      trim: { ...prepared.trim }
    },
    reference: {
      fileName: state.base ? state.base.name : "",
      sourceWidth: state.base ? state.base.width : 0,
      sourceHeight: state.base ? state.base.height : 0,
      trim: { ...prepared.trim },
      fit: BASE_FIT_WIDTH,
      opacity: Number(state.baseOpacity.toFixed(4))
    },
    faces: state.faces.map((face, index) => ({
      id: face.id,
      type: normalizeFaceType(face.type),
      shadowSurface: shadowSurfaceForFace(face),
      receivesShadow: !isSpriteAreaFace(face) && face.receivesShadow !== false,
      drawOrder: index,
      points: faceTargetPoints(face).map((point) => [Math.round(point.x), Math.round(point.y)]),
      sourcePoints: faceSourcePoints(face)
    })),
    spriteAreas: state.faces
      .filter(isSpriteAreaFace)
      .map((face) => ({
        id: face.id,
        points: faceTargetPoints(face).map((point) => [Math.round(point.x), Math.round(point.y)]),
        sourcePoints: faceSourcePoints(face)
      }))
  };
}

function downloadText(fileName, text, mime = "text/plain") {
  const blob = new Blob([text], { type: mime });
  downloadBlob(fileName, blob);
}

function downloadBlob(fileName, blob) {
  if (!blob) {
    setStatus(`Export failed: ${fileName}.`);
    return;
  }
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = fileName;
  document.body.appendChild(a);
  a.click();
  a.remove();
  window.setTimeout(() => URL.revokeObjectURL(url), 1000);
}

function downloadCanvas(fileName) {
  renderPreview();
  previewCanvas.toBlob((blob) => {
    downloadBlob(fileName, blob);
    if (blob) setStatus(`Preview PNG exported: ${fileName}.`);
  }, "image/png");
}

async function importLayout(file) {
  const text = await file.text();
  const layout = JSON.parse(text);
  if (layout.type === "stickmon-room-editor-session" || layout.snapshot) {
    const snapshot = await hydrateSessionSnapshot(layout.snapshot || layout);
    restoreSnapshot(snapshot);
    state.history = [];
    state.historyIndex = -1;
    refreshAfterStateRestore();
    commitHistory();
    setStatus(`Session file loaded: ${file.name}.`);
    return;
  }
  if (layout.canvas) {
    document.getElementById("canvasW").value = GAME_SCREEN_WIDTH;
    document.getElementById("canvasH").value = Math.max(GAME_SCREEN_HEIGHT, layout.canvas.height || 135);
  }
  document.getElementById("baseFit").value = BASE_FIT_WIDTH;
  if (layout.sourceScale) {
    document.getElementById("sourceScale").value = layout.sourceScale;
  }
  if (layout.baseOpacity != null) {
    document.getElementById("baseOpacity").value = Math.round(layout.baseOpacity * 100);
  }
  if (layout.roomGeometry) {
    const geometry = layout.roomGeometry;
    const canvasWidth = GAME_SCREEN_WIDTH;
    const canvasHeight = Math.max(
      GAME_SCREEN_HEIGHT,
      Number(geometry.canvas?.height || layout.canvas?.height || geometry.room?.height || state.height || GAME_SCREEN_HEIGHT)
    );
    document.getElementById("canvasW").value = canvasWidth;
    document.getElementById("canvasH").value = canvasHeight;
    const importedTrim = coerceTrimBox(geometry.prepared?.trim || geometry.reference?.trim || layout.base?.trim);
    if (importedTrim) {
      state.baseTrim = importedTrim;
    } else if (!state.base) {
      state.baseTrim = null;
    }
    const roomWidth = Number(geometry.room?.width || canvasWidth);
    const roomHeight = Number(geometry.room?.height || canvasHeight);
    const preparedScale = Number(geometry.prepared?.scale) ||
      (importedTrim ? roomWidth / Math.max(1, importedTrim.width) : 0);
    const preparedY = Number(geometry.prepared?.y) || 0;
    state.width = canvasWidth;
    state.height = canvasHeight;
    syncCanvasSizeToPreparedRoom();
    const pointByCoord = new Map();
    state.points = [];
    state.nextPointId = 1;
    state.faces = (geometry.faces || []).map((face, index) => {
      const points = (face.sourcePoints || face.points || []).map((rawPoint) => {
        let targetX;
        let targetY;
        if (face.sourcePoints && importedTrim && preparedScale > 0) {
          targetX = ((Number(rawPoint[0]) || 0) - importedTrim.x) * preparedScale;
          targetY = ((Number(rawPoint[1]) || 0) - importedTrim.y) * preparedScale + preparedY;
        } else {
          targetX = Number(rawPoint[0]) || 0;
          targetY = Number(rawPoint[1]) || 0;
        }
        targetX = clampNumber(targetX, 0, state.width);
        targetY = clampNumber(targetY, 0, state.height);
        const edit = targetToEditPoint({ x: targetX, y: targetY });
        const x = Math.round(clampNumber(edit.x, 0, state.editWidth));
        const y = Math.round(clampNumber(edit.y, 0, state.editHeight));
        const key = `${Math.round(x)},${Math.round(y)}`;
        if (!pointByCoord.has(key)) {
          pointByCoord.set(key, createShapePoint(x, y));
        }
        return pointByCoord.get(key);
      });
      const type = normalizeFaceType(face.type);
      return {
        id: face.id || `face${index + 1}`,
        type,
        shadowSurface: normalizeShadowSurface(face.shadowSurface, type),
        receivesShadow: type === FACE_TYPE_SPRITE_AREA ? false : face.receivesShadow !== false,
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
    const nextNight = normalizeNightSettings(layout.night);
    state.night = nextNight;
    document.getElementById("nightTint").value = nextNight.tint;
    document.getElementById("nightDarken").value = nextNight.darken;
    syncLightInputsFromState();
    document.getElementById("castShadows").checked = nextNight.castShadows;
    document.getElementById("shadowMode").value = nextNight.shadowMode;
    document.getElementById("shadowAlpha").value = nextNight.shadowAlpha;
    document.getElementById("shadowLength").value = nextNight.shadowLength;
    document.getElementById("shadowBlur").value = nextNight.shadowBlur;
  }

  const missing = [];
  const byFile = new Map(state.items.map((item) => [item.fileName, item]));
  for (const entry of layout.furniture || []) {
    const item = byFile.get(entry.fileName);
    if (!item) {
      missing.push(entry.fileName);
      continue;
    }
    Object.assign(item, {
      id: entry.id || item.id,
      name: entry.name || item.name,
      x: entry.x ?? item.x,
      y: entry.y ?? item.y,
      z: entry.z ?? item.z,
      scale: entry.scale ?? item.scale,
      scaleX: entry.scaleX ?? (entry.targetWidth ? entry.targetWidth / Math.max(1, item.sourceWidth) : entry.scale ?? item.scaleX ?? item.scale),
      scaleY: entry.scaleY ?? (entry.targetHeight ? entry.targetHeight / Math.max(1, item.sourceHeight) : entry.scale ?? item.scaleY ?? item.scale),
      aspectLocked: entry.aspectLocked,
      sourceScale: entry.sourceScale ?? item.sourceScale,
      opacity: entry.opacity ?? item.opacity,
      visible: entry.visible ?? item.visible,
      anchor: entry.anchor || item.anchor,
      slot: entry.slot || item.slot,
      layer: entry.layer || item.layer,
      source: entry.source || item.source,
      furnitureType: entry.furnitureType || item.furnitureType || inferFurnitureType(entry.fileName || item.fileName || entry.name || item.name),
      kind: entry.kind || item.kind || inferItemKind(entry.fileName || item.fileName || entry.name || item.name, entry.furnitureType || item.furnitureType),
      heightPx: entry.heightPx ?? item.heightPx,
      footprint: entry.footprint || item.footprint,
      footprintPolygon: normalizeLocalPolygon(entry.footprintPolygon ?? item.footprintPolygon),
      shadowPolygon: normalizeLocalPolygon(entry.shadowPolygon ?? item.shadowPolygon),
      shadowAnchor: entry.shadowAnchor ?? item.shadowAnchor,
      wallShadowDepthPx: entry.wallShadowDepthPx ?? item.wallShadowDepthPx,
      wallShadowOffsetY: entry.wallShadowOffsetY ?? item.wallShadowOffsetY,
      castsShadow: entry.castsShadow ?? item.castsShadow,
      receivesShadow: entry.receivesShadow ?? item.receivesShadow,
      occludesSprite: entry.occludesSprite ?? item.occludesSprite,
      sortY: entry.sortY ?? item.sortY,
      cutoutApplied: entry.cutoutApplied ?? item.cutoutApplied,
      speciesId: entry.speciesId ?? item.speciesId,
      action: entry.action ?? item.action,
      frame: entry.frame ?? item.frame
    });
    if (item.aspectLocked === undefined) {
      item.aspectLocked = Math.abs(itemScaleX(item) - itemScaleY(item)) < 0.0001;
    }
    if (item.aspectLocked) {
      item.scaleY = itemScaleX(item);
    }
    item.scale = itemScaleX(item);
    if (item.source === "furniture") {
      item.furnitureType = normalizeFurnitureType(item.furnitureType);
    }
    normalizeItemSemantics(item);
  }
  setStatus(`Layout imported: ${file.name}.${missing.length ? ` Missing assets: ${missing.join(", ")}.` : ""}`);
  refreshList();
  refreshSelectedPanel();
  refreshFaceList();
  refreshSelectedFacePanel();
  render();
  commitHistory();
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

document.getElementById("replaceBaseButton").addEventListener("click", () => {
  const input = document.getElementById("baseInput");
  input.value = "";
  input.click();
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
  commitHistory();
});

document.getElementById("lightProfileShared").addEventListener("click", () => {
  setLightProfile("shared");
  render();
  commitHistory();
});

document.getElementById("lightProfileDay").addEventListener("click", () => {
  setLightProfile("day");
  render();
  commitHistory();
});

document.getElementById("lightProfileNight").addEventListener("click", () => {
  setLightProfile("night");
  render();
  commitHistory();
});

document.getElementById("copyLightToDay").addEventListener("click", () => {
  copyActiveLightToProfile("day");
  render();
  commitHistory();
});

document.getElementById("copyLightToNight").addEventListener("click", () => {
  copyActiveLightToProfile("night");
  render();
  commitHistory();
});

for (const id of ["baseOpacity", "sourceScale", "furnitureImportMode", "baseFit", "showGuides", "showGrid", "gridOpacity", "snapToGrid", "spriteX", "spriteY", "spriteW", "spriteH", "nightTint", "nightDarken", "lightShape", "lightStrength", "lightX", "lightY", "lightDepth", "lightRadius", "lightAngle", "lightSpread", "castShadows", "shadowMode", "shadowAlpha", "shadowLength", "shadowBlur"]) {
  document.getElementById(id).addEventListener("input", () => {
    render();
    if (id === "baseOpacity" || id === "baseFit") refreshList();
    commitHistory();
  });
}

document.getElementById("dayMode").addEventListener("click", () => {
  commitLightInputsToState();
  state.mode = "day";
  updateModeButtons();
  syncLightInputsFromState();
  render();
  commitHistory();
});

document.getElementById("nightMode").addEventListener("click", () => {
  commitLightInputsToState();
  state.mode = "night";
  updateModeButtons();
  syncLightInputsFromState();
  render();
  commitHistory();
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
  commitHistory();
});

document.getElementById("shapeMode").addEventListener("click", () => {
  state.editMode = "shape";
  state.selectedId = null;
  updateEditModeButtons();
  refreshList();
  refreshSelectedPanel();
  render();
  commitHistory();
});

document.getElementById("selectTool").addEventListener("click", () => setToolMode("select"));
document.getElementById("measureHeightTool").addEventListener("click", () => setToolMode("measureHeight"));
document.getElementById("clearMeasure").addEventListener("click", clearMeasurement);

document.getElementById("editZoom").addEventListener("input", (event) => {
  setEditZoom(event.target.value);
});

document.getElementById("zoomOut").addEventListener("click", () => {
  setEditZoom(Math.round(state.editZoom * 100) - 25);
});

document.getElementById("zoomIn").addEventListener("click", () => {
  setEditZoom(Math.round(state.editZoom * 100) + 25);
});

document.getElementById("zoomReset").addEventListener("click", () => {
  setEditZoom(100);
});

document.getElementById("resetView").addEventListener("click", () => {
  state.selectedId = null;
  state.selectedFaceId = null;
  state.selectedPointIndex = null;
  state.selectedPointId = null;
  state.lightSelected = false;
  state.editZoom = 1;
  updateZoomControls();
  updateEditModeButtons();
  refreshList();
  refreshSelectedPanel();
  refreshFaceList();
  refreshSelectedFacePanel();
  render();
  commitHistory();
});

document.getElementById("undoAction").addEventListener("click", undo);
document.getElementById("redoAction").addEventListener("click", redo);

document.getElementById("exportJson").addEventListener("click", () => {
  downloadText("room_layout.json", JSON.stringify(exportLayoutObject(), null, 2), "application/json");
});

document.getElementById("exportGeometry").addEventListener("click", () => {
  downloadText("room_geometry.json", JSON.stringify(exportRoomGeometryObject(), null, 2), "application/json");
});

document.getElementById("exportPreview").addEventListener("click", () => {
  downloadCanvas(`room_${state.mode}_preview.png`);
});

document.getElementById("saveSession").addEventListener("click", async () => {
  try {
    await saveEditorSession();
  } catch (error) {
    console.error(error);
    setStatus(error.message || "Session save failed.");
  }
});

document.getElementById("changeSessionFile").addEventListener("click", async () => {
  try {
    await forgetSessionExportHandle();
    await saveEditorSession({ forcePick: true });
  } catch (error) {
    console.error(error);
    setStatus(error.message || "Session save location change failed.");
  }
});

document.getElementById("loadSession").addEventListener("click", async () => {
  try {
    await loadEditorSession();
  } catch (error) {
    console.error(error);
    setStatus(error.message || "Session load failed.");
  }
});

document.getElementById("clearSession").addEventListener("click", async () => {
  try {
    await clearEditorSession();
  } catch (error) {
    console.error(error);
    setStatus(error.message || "Clearing saved session failed.");
  }
});

previewCanvas.addEventListener("click", () => {
  state.previewZoomed = !state.previewZoomed;
  updatePreviewZoom();
});

document.getElementById("deleteItem").addEventListener("click", () => {
  const item = selectedItem();
  if (!item) return;
  deleteItemById(item.id);
});

document.getElementById("duplicateItem").addEventListener("click", () => {
  const item = selectedItem();
  if (!item) return;
  const copy = { ...item, id: `f${state.nextId++}`, name: `${item.name}_copy`, x: item.x + 4, y: item.y + 4 };
  copy.sortY = autoSortY(copy);
  state.items.push(copy);
  state.selectedId = copy.id;
  refreshList();
  refreshSelectedPanel();
  render();
  commitHistory();
});

canvas.addEventListener("pointerdown", (event) => {
  if (state.toolMode === "measureHeight") {
    const canvasPoint = pointerToCanvas(event);
    state.measure.dragging = true;
    setMeasureStart(canvasPoint);
    canvas.classList.add("dragging");
    canvas.setPointerCapture(event.pointerId);
    updateMeasurePanel();
    render();
    return;
  }

  const canvasPoint = pointerToCanvas(event);
  if (hitTestConeControl(canvasPoint.x, canvasPoint.y)) {
    state.draggingConeControl = true;
    state.lightSelected = true;
    updateEditModeButtons();
    canvas.classList.add("dragging");
    canvas.setPointerCapture(event.pointerId);
    updateConeControlFromCanvasPoint(canvasPoint);
    render();
    return;
  }
  if (hitTestLight(canvasPoint.x, canvasPoint.y)) {
    state.draggingLight = true;
    state.lightSelected = true;
    updateEditModeButtons();
    canvas.classList.add("dragging");
    canvas.setPointerCapture(event.pointerId);
    setLightPositionFromCanvasPoint(canvasPoint);
    render();
    return;
  }
  if (state.editMode === "shape") {
    state.lightSelected = false;
    updateEditModeButtons();
    handleShapePointerDown(event);
    commitHistory();
    return;
  }

  const polygonHit = hitTestItemPolygonPoint(canvasPoint.x, canvasPoint.y);
  if (polygonHit) {
    state.lightSelected = false;
    state.selectedId = polygonHit.item.id;
    state.selectedFaceId = null;
    state.selectedPointIndex = null;
    state.selectedPointId = null;
    state.draggingItemPolygon = true;
    state.dragPolygonKey = polygonHit.key;
    state.dragPolygonIndex = polygonHit.index;
    setActiveItemPolygon(polygonHit.key);
    materializeItemPolygon(polygonHit.item, polygonHit.key);
    syncPolygonEditorField(polygonHit.item, polygonHit.key);
    updateDraggedItemPolygon(canvasPoint);
    canvas.classList.add("dragging");
    canvas.setPointerCapture(event.pointerId);
    updateEditModeButtons();
    render();
    return;
  }

  const p = pointerToTarget(event);
  const item = hitTest(p.x, p.y);
  if (item) {
    state.lightSelected = false;
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
    state.lightSelected = false;
  }
  updateEditModeButtons();
  refreshList();
  refreshSelectedPanel();
  render();
});

canvas.addEventListener("pointermove", (event) => {
  if (state.measure.dragging) {
    setMeasureEnd(pointerToCanvas(event));
    updateMeasurePanel();
    render();
    return;
  }

  if (state.draggingConeControl) {
    updateConeControlFromCanvasPoint(pointerToCanvas(event));
    render();
    return;
  }

  if (state.draggingLight) {
    setLightPositionFromCanvasPoint(pointerToCanvas(event));
    render();
    return;
  }
  if (state.draggingItemPolygon) {
    updateDraggedItemPolygon(pointerToCanvas(event));
    render();
    return;
  }
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
  item.x = snapValue(Math.round(p.x - state.dragOffsetX));
  item.y = snapValue(Math.round(p.y - state.dragOffsetY));
  item.sortY = autoSortY(item);
  refreshList();
  refreshSelectedPanel();
  render();
});

function finishCanvasDrag(event) {
  if (state.measure.dragging) {
    setMeasureEnd(pointerToCanvas(event));
    state.measure.dragging = false;
    canvas.classList.remove("dragging");
    try {
      canvas.releasePointerCapture(event.pointerId);
    } catch (_) {}
    updateMeasurePanel();
    render();
    setStatus(measurementStatusText());
    return;
  }

  if (state.dragging || state.draggingPoint || state.draggingItemPolygon || state.draggingLight || state.draggingConeControl) {
    commitHistory();
  }
  state.dragging = false;
  state.draggingItemPolygon = false;
  state.draggingLight = false;
  state.draggingConeControl = false;
  state.draggingPoint = false;
  state.dragPointId = null;
  state.dragPolygonKey = null;
  state.dragPolygonIndex = null;
  canvas.classList.remove("dragging");
  refreshSelectedFacePanel();
  try {
    canvas.releasePointerCapture(event.pointerId);
  } catch (_) {}
}

canvas.addEventListener("pointerup", finishCanvasDrag);
canvas.addEventListener("pointercancel", finishCanvasDrag);

window.addEventListener("keydown", (event) => {
  if (event.key === "Escape" && state.previewZoomed) {
    state.previewZoomed = false;
    updatePreviewZoom();
    event.preventDefault();
    return;
  }

  if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === "z") {
    event.preventDefault();
    if (event.shiftKey) {
      redo();
    } else {
      undo();
    }
    return;
  }

  if (state.toolMode === "measureHeight") {
    if (event.key === "Escape") {
      setToolMode("select");
      event.preventDefault();
    }
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
      commitHistory();
      return;
    }
  }

  const item = selectedItem();
  if (!item) return;

  if (state.editMode === "furniture" && item.source === "project_sprite") {
    return;
  }

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
  item.x = snapValue(item.x + dx);
  item.y = snapValue(item.y + dy);
  event.preventDefault();
  refreshList();
  refreshSelectedPanel();
  render();
  commitHistory();
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

commitHistory();
refreshList();
refreshSelectedPanel();
refreshFaceList();
refreshSelectedFacePanel();
updateBaseImageMeta();
updateModeButtons();
updateEditModeButtons();
updateToolButtons();
updatePreviewZoom();
updateZoomControls();
updateGridOpacityLabel();
setupHoverTooltips();
updateUndoButtons();
initProjectSpriteControls();
render();
